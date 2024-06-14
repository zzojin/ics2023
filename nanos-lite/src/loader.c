#include "am.h"
#include "debug.h"
#include "klib-macros.h"
#include "memory.h"
#include <proc.h>
#include <elf.h>
#include <fs.h>
#include <stdio.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#if defined(__ISA_AM_NATIVE__)
# define EXPECT_TYPE EM_X86_64
#elif defined(__ISA_X86__)
# define EXPECT_TYPE EM_386  // see /usr/include/elf.h to get the right type
#elif defined(__ISA_MIPS32__)
# define EXPECT_TYPE EM_MIPS
#elif defined(__ISA_RISCV32__)
# define EXPECT_TYPE EM_RISCV
#define PTE_V 0x01
#define PTE_R 0x02
#define PTE_W 0x04
#define PTE_X 0x08
#define PTE_U 0x10
#define PTE_A 0x40
#define PTE_D 0x80

#else
# error Unsupported ISA
#endif

#define min(x, y) ((x < y) ? x : y)

extern size_t ramdisk_read(void *buf, size_t offset, size_t len);
extern size_t ramdisk_write(const void *buf, size_t offset, size_t len);
extern size_t get_ramdisk_size();

/*static uintptr_t loader(PCB *pcb, const char *filename) {
 *  // load elf header 
 *  Elf_Ehdr ehdr;
 *  ramdisk_read((void *)&ehdr, 0, sizeof(Elf_Ehdr));
 *  assert((*(uint32_t *)ehdr.e_ident == 0x464c457f));
 *
 *  // load program headers
 *  Elf_Phdr phdr[ehdr.e_phnum];
 *  ramdisk_read(phdr, ehdr.e_phoff, sizeof(Elf_Phdr)*ehdr.e_phnum);
 *  
 *  // load segments
 *  for (int i = 0; i < ehdr.e_phnum; i++) {
 *      if (phdr[i].p_type == PT_LOAD) {
 *          ramdisk_read((void *)phdr[i].p_vaddr, phdr[i].p_offset, phdr[i].p_memsz);
 *          // bss 置为 0
 *          memset((void*)(phdr[i].p_vaddr+phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
 *      }
 *  }
 *  return ehdr.e_entry;
 *}*/

// ramdisk 被 fs 系列函数替代，以支持多文件的 ramdisk，fs 底层的实现依赖于 ramdisk_read write
/*#define PG_MASK (~0xfff)
 *#define IS_ALIGN(vaddr) ((vaddr) == ((vaddr)&PG_MASK))
 *#define OFFSET(vaddr) ((vaddr) & (~PG_MASK))*/
#define PG_MASK (0xfff)
#define IS_ALIGN(vaddr) ((0) == ((vaddr)&PG_MASK))
#define OFFSET(vaddr) ((vaddr) & (PG_MASK))

static uintptr_t loader(PCB *pcb, const char *filename) {
  // load elf header 
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);     // 到 PA 最后阶段，这个函数执行失败时只会返回 -1
  if (fd < 0)
      assert(0);
  fs_read(fd, (void *)&ehdr, sizeof(Elf_Ehdr));
  assert((*(uint32_t *)ehdr.e_ident == 0x464c457f));

  // load program headers，通常Program headers 紧跟elf header，不需要调整 open_offset，但是还是设置了 phoff 指定 pheader 距离文件开头的偏移
  Elf_Phdr phdr[ehdr.e_phnum];
  fs_lseek(fd, ehdr.e_phoff, SEEK_SET);
  fs_read(fd, (void *)phdr, sizeof(Elf_Phdr) * ehdr.e_phnum);

  // load segments 普通模式
  /*for (int i = 0; i < ehdr.e_phnum; i++) {
   *    if (phdr[i].p_type == PT_LOAD) {
   *        fs_lseek(fd, phdr[i].p_offset, SEEK_SET);
   *        fs_read(fd, (void *)phdr[i].p_vaddr, phdr[i].p_memsz);
   *        // bss 置为 0
   *        memset((void*)(phdr[i].p_vaddr+phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
   *    }
   *}*/
  // load segments，虚拟内存空间
  for (int i = 0; i < ehdr.e_phnum; i++) {
      if (phdr[i].p_type != PT_LOAD)
        continue;
     
      uint32_t file_size = phdr[i].p_filesz;
      uint32_t p_vaddr = phdr[i].p_vaddr;
      uint32_t mem_size = phdr[i].p_memsz;
      int unprocessed_size = file_size;
      int read_len = 0;
      void *pg_p = NULL;
      fs_lseek(fd, phdr[i].p_offset, SEEK_SET);

      if (!IS_ALIGN(p_vaddr)) {
          pg_p = new_page(1);
          read_len = min(PGSIZE - OFFSET(p_vaddr), unprocessed_size);
          unprocessed_size -= read_len;
          assert(fs_read(fd, pg_p + OFFSET(p_vaddr), read_len) >= 0);
          map(&pcb->as, (void *)p_vaddr, pg_p, PTE_R | PTE_W | PTE_X | PTE_V);
          p_vaddr += read_len;
      }
      for (; p_vaddr < phdr[i].p_vaddr + file_size; p_vaddr += PGSIZE) {
          assert(IS_ALIGN(p_vaddr));
          pg_p = new_page(1);
          //memset(pg_p, 0, PGSIZE);    // 我觉得这个清零操作可以省去，直接读程序覆盖内存
          read_len = min(PGSIZE, unprocessed_size);
          unprocessed_size -= read_len;
          assert(fs_read(fd, pg_p, read_len) >= 0);
          map(&pcb->as, (void *)p_vaddr, pg_p, PTE_R | PTE_W | PTE_X | PTE_V);
      }
      if (file_size == mem_size)
          continue;
      memset(pg_p + read_len, 0, PGSIZE - read_len);
      for (; p_vaddr < phdr[i].p_vaddr + mem_size; p_vaddr += PGSIZE) {
          assert(IS_ALIGN(p_vaddr));
          pg_p = new_page(1);
          memset(pg_p, 0, PGSIZE);
          map(&pcb->as, (void *)p_vaddr, pg_p, PTE_R | PTE_W | PTE_X | PTE_V);
      }
      // TODO: max_brk ?
      pcb->max_brk = p_vaddr;
  }
  assert(fs_close(fd) == 0);
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", (char *)entry);
  ((void(*)())entry) ();
}

void context_kload(PCB *pcb, void (*entry)(void *), void *arg) {
    Area kstack = RANGE(pcb, (char *)pcb + STACK_SIZE);
    pcb->cp = kcontext(kstack, entry, arg);
}

static int len(char *const str[]) {
    int i = 0;
    if (str == NULL)
        return 0;
    for (; str[i] != NULL; i++) {}
    return i;
}

static size_t ceil_4_bytes(size_t size){
  if (size & 0x3)
    return (size & (~0x3)) + 0x4;
  return size;
}

void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
    AddrSpace *as = &pcb->as;
    protect(as);
  // 更新：用 new_page 分配新的空间作为用户栈
  char *sp = new_page(USER_STACK_PG_NUM) + USER_STACK_PG_NUM * PGSIZE;
  // 建立用户栈的物理地址与虚拟地址空间的顶部的映射
  for (size_t i = USER_STACK_PG_NUM; i > 0; --i)
    map(as, (void *)(pcb->as.area.end - i * PGSIZE), sp - i * PGSIZE, PTE_R | PTE_W | PTE_X | PTE_V | PTE_U);
  uint32_t map_offset = sp - (char *)(pcb->as.area.end);

  int argc = len(argv);
  char* args[argc];
  // 拷贝字符串到栈上
  for (int i = 0; i < argc; ++i) {
    sp -= (ceil_4_bytes(strlen(argv[i]) + 1));     // +1 为了补充末尾的\0
    strcpy(sp, argv[i]);
    args[i] = sp;
  }
  int envc = len(envp);
  char* envs[envc];
  for (int i = 0; i < envc; ++i) {
    sp -= (ceil_4_bytes(strlen(envp[i]) + 1));     // +1 为了补充末尾的\0
    strcpy(sp, argv[i]);
    envs[i] = sp;
  }
  // 压入指针数组，每个元素都是一个指向字符串首字符地址的指针。设置 NULL 以做指针数组的结尾标识。
  // 现在压入argv[] and envp[]
  char **sp_2 = (char **)sp;
  --sp_2;
  *sp_2 = NULL;
  for (int i = envc - 1; i >= 0; i--) {
      --sp_2;
      *sp_2 = envs[i];
  }
  --sp_2;
  *sp_2 = NULL;
  for (int i = argc - 1; i >= 0; i--) {
      --sp_2;
      *sp_2 = args[i];
  }
  --sp_2;
  *((int*)sp_2) = argc;
  // TODO：loader 需要放在参数压栈的后面，还没搞清楚是为什么, PA4-2：运行在虚拟地址空间上的用户进程，经测验，将 loader 放到前面并没有出现什么问题
  uintptr_t entry = loader(pcb, filename);
  Area kstack = RANGE(pcb, (char *)pcb + STACK_SIZE);
  // 用户进程创建其内核栈,在内核栈栈底创建一个context并返回其地址，同时，将栈顶的 cp 指针，指向栈底的 context 结构体地址。
  // 结构体中初始化一些控制寄存器，还有将 as 创建的页目录首地址赋给 ctx->pdir
  Context *ctx = ucontext(as, kstack, (void*)entry);
  pcb->cp = ctx;
  // 现在栈顶指向 argc，将栈顶的地址转成虚拟地址，赋给 a0
  ctx->GPRx = (uintptr_t) sp_2 - map_offset;                 // GPRX = a0, 到时候在 navy-apps start.S 切换到用户空间时会将这个赋给sp, 这样用户程序就能读取用户栈里的入参和环境变量了
}
