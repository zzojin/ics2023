#include "debug.h"
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
#else
# error Unsupported ISA
#endif

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
static uintptr_t loader(PCB *pcb, const char *filename) {
  // load elf header 
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);
  fs_read(fd, (void *)&ehdr, sizeof(Elf_Ehdr));
  assert((*(uint32_t *)ehdr.e_ident == 0x464c457f));

  // load program headers，通常Program headers 紧跟elf header，不需要调整 open_offset，但是还是设置了 phoff 指定 pheader 距离文件开头的偏移
  Elf_Phdr phdr[ehdr.e_phnum];
  fs_lseek(fd, ehdr.e_phoff, SEEK_SET);
  fs_read(fd, (void *)phdr, sizeof(Elf_Phdr) * ehdr.e_phnum);

  // load segments
  for (int i = 0; i < ehdr.e_phnum; i++) {
      if (phdr[i].p_type == PT_LOAD) {
          fs_lseek(fd, phdr[i].p_offset, SEEK_SET);
          fs_read(fd, (void *)phdr[i].p_vaddr, phdr[i].p_memsz);
          // bss 置为 0
          memset((void*)(phdr[i].p_vaddr+phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
      }
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
    Area area;
    area.start = pcb->stack;
    area.end = pcb->stack  + STACK_SIZE;
    pcb->cp = kcontext(area, entry, arg);
}

static int len(char *const str[]) {
    int i = 0;
    if (str == NULL)
        return 0;
    for (; str[i] != NULL; i++) {}
    return i;
}

void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  // 现在需要把入参存入用户栈，用户栈的栈底设为 heap.end
  // 更新：用 new_page 分配新的空间作为用户栈
  char *sp = new_page(USER_STACK_PG_NUM) + USER_STACK_PG_NUM * PGSIZE;
  //char *sp = heap.end;
  int argc = len(argv);
  char* args[argc];
  
  // 拷贝字符串到栈上
  for (int i = 0; i < argc; ++i) {
    sp -= (strlen(argv[i]) + 1);     // +1 为了补充末尾的\0
    strcpy(sp, argv[i]);
    args[i] = sp;
  }
  int envc = len(envp);
  char* envs[envc];
  for (int i = 0; i < envc; ++i) {
    sp -= (strlen(envp[i]) + 1);     // +1 为了补充末尾的\0
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
  // TODO：loader 需要放在参数压栈的后面，还没搞清楚是为什么
  AddrSpace addr;
  uintptr_t entry = loader(pcb, filename);
    Area area;
    area.start = pcb->stack;
    area.end = pcb->stack  + STACK_SIZE;
    // 用户进程创建其内核栈
  Context *ctx = ucontext(&addr, area, (void*)entry);
  pcb->cp = ctx;
  // 现在栈顶指向 argc，将栈顶的地址赋给 a0
  //printf("new user stack top=%p\n", sp_2);
  ctx->GPRx = (uintptr_t) sp_2;                 // GPRX = a0, 到时候在 navy-apps 会将这个赋给sp, 这样用户程序就能读取用户栈里的入参和环境变量了
}
