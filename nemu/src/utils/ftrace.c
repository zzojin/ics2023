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
} fnode;

static finfo funcs[102400];
static unsigned int ind = 0;
static fnode* func_stack_head = NULL;
static fnode* func_stack_tail = NULL;
static const char *action_type[] = {"Call", "Ret"};

void init_elf(const char* elf_file) {
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
    if (rd == 1 && dst_index >= 0 && dst == funcs[dst_index].addr) {
        append(cur, dst, dst_index, action_type[0]);
    }
}

void process_jalr(int rd, word_t inst_val, vaddr_t cur, vaddr_t dst, word_t imm) {
    int dst_index = which_func(dst);
    if (dst_index < 0)
        return;
    if (inst_val == 0x00008067) {
        append(cur, dst, dst_index, action_type[1]);
    }
    else if (rd == 1 && imm == 0 && dst == funcs[dst_index].addr) { 
        append(cur, dst, dst_index, action_type[0]);
    }
}

void print_func_stack() {
    printf("=========== The function stack ===========\n");
    fnode *temp = func_stack_head;
    while (temp != NULL) {
        printf("%x: in %-25s, %s [%x@%s]\n", temp->pc, temp->cur_func->name, temp->type, temp->target_addr, temp->dst_func->name);
        temp = temp->next;
    }
}
