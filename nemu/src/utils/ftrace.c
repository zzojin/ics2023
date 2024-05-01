#include "debug.h"
#include "utils.h"
#include <common.h>
#include <stdio.h>
#include <unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define NAME_LENGTH 128
#define FT_CALL 0
#define FT_RET  1

typedef struct {
    char name[NAME_LENGTH];
    vaddr_t addr;
    size_t size;
} finfo;

typedef struct fnode {
    finfo *cur_func;
    finfo *dst_func;
    const char* type;
    struct fnode *next;
    vaddr_t pc;
    vaddr_t target_addr;
    int call_depth;
} fnode;

static finfo funcs[102400];
static unsigned int ind = 0;
static int call_depth = 0;
static fnode* func_stack_head = NULL;
static fnode* func_stack_tail = NULL;
static const char *action_type[] = {"Call", "Ret "};

void init_elf(const char* elf_file) {
    if (elf_file == NULL) {
        Log("elf file path is null");
        return;
    }
    int fd;
    struct stat st;
    fd = open(elf_file, O_RDONLY);
    Assert(fd >= 0, "can not open elf file: %s", elf_file);
    fstat(fd, &st);

    char *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)mem;
    Elf32_Shdr *shdr = (Elf32_Shdr *)(mem + ehdr->e_shoff);

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB || shdr[i].sh_type == SHT_DYNSYM) {
            // Found a symbol table, now find its associated string table
            Elf32_Shdr *strtab_shdr = &shdr[shdr[i].sh_link];
            // 拿到 String table section 相对于 ELF 文件开头的偏移位置
            const char *const strtab = mem + strtab_shdr->sh_offset;
            Elf32_Sym *symtab = (Elf32_Sym *)(mem + shdr[i].sh_offset);
            int num_symbols = shdr[i].sh_size / shdr[i].sh_entsize;

            for (int j = 0; j < num_symbols; j++) {
                if (ELF32_ST_TYPE(symtab[j].st_info) == STT_FUNC) {
                    strncpy(funcs[ind].name, strtab + symtab[j].st_name, NAME_LENGTH);
                    
                    funcs[ind].size = symtab[j].st_size;
                    funcs[ind].addr = symtab[j].st_value;
                    ind++;
                }
            }
        }
    }
    munmap(mem, st.st_size);
    close(fd);
}

int which_func(vaddr_t addr) {
    for (int i = 0; i < ind; i++) {
        if (addr >= funcs[i].addr && addr < funcs[i].addr + funcs[i].size)
            return i;
    }
    return -1;
}

void append(vaddr_t cur, vaddr_t target_addr, int dst_index, const char *type) {
    int cur_index = which_func(cur);
    fnode* newnode = malloc(sizeof(fnode));
    newnode->cur_func = &funcs[cur_index];
    newnode->dst_func = &funcs[dst_index];
    newnode->type = type;
    newnode->pc = cur;
    newnode->target_addr = target_addr;
    newnode->next = NULL;

    if (type == action_type[0]) {
        newnode->call_depth = call_depth;
        call_depth += 2;
    } else {
        call_depth -= 2;
        newnode->call_depth = call_depth;
    }
    Assert(call_depth >= 0, "function call depth less than 0, something wrong");
    IFDEF(CONFIG_FTRACE, log_write("[ftrace] %x: %*s%s [%s@%x]\n", newnode->pc, newnode->call_depth, "", newnode->type, newnode->dst_func->name, newnode->target_addr));

    if (func_stack_head == NULL) {
        func_stack_head = newnode;
        func_stack_tail = func_stack_head;
    } else {
        func_stack_tail->next = newnode;
        func_stack_tail = newnode;
    }
}

void process_jal(int rd, vaddr_t cur, vaddr_t dst) {
    int dst_index = which_func(dst);
    if (rd == 1 && dst_index >= 0) {
        append(cur, dst, dst_index, action_type[0]);
    }
}

/* 当某个函数的 return 语句内容是一个函数调用时，编译器会把这个函数的返回优化掉，返回地址直接设为 x0，称为尾调用
 * TODO: 参考 https://www.cnblogs.com/nosae/p/17066439.html#%E5%AE%9E%E7%8E%B0ftrace，完成尾调用的补充
 * */
void process_jalr(int rd, word_t inst_val, vaddr_t cur, vaddr_t dst, word_t imm) {
    int dst_index = which_func(dst);
    if (dst_index < 0)
        return;
    if (inst_val == 0x00008067) {
        append(cur, dst, dst_index, action_type[1]);         // ret -> jalr x0, 0(x1=ra) 
    } else if (rd == 1) {                                      // dst == funcs[dst_index].addr 条件可以省略
        append(cur, dst, dst_index, action_type[0]);
    }
}

void print_func_stack() {
    log_write("=========== The function stack ===========\n");
    fnode *temp = func_stack_head;
    while (temp != NULL) {
        log_write("%x: %*s%s [%s@%x]\n", temp->pc, temp->call_depth, "", temp->type, temp->dst_func->name, temp->target_addr);
        temp = temp->next;
    }
}

/*void print_func_stack() {*/
    /*printf("=========== The function stack ===========\n");*/
    /*fnode *temp = func_stack_head;*/
    /*while (temp != NULL) {*/
    /*    printf("%x: in %-25s, %s [%x@%s]\n", temp->pc, temp->cur_func->name, temp->type, temp->target_addr, temp->dst_func->name);*/
    /*    temp = temp->next;*/
    /*}*/
/*}*/
