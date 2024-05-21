#include <fs.h>
#include <string.h>

// 定义两种类型的函数指针，分别取别名为 ReadFn 和 WriteFn
typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);
extern size_t ramdisk_read(void *buf, size_t offset, size_t len);
extern size_t ramdisk_write(const void *buf, size_t offset, size_t len);
extern size_t serial_write(const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. 
 * 如 Makefile 所述，files.h 实际是 Navy ramdisk.h 的符号链接（软链接）。传过来一张文件记录表（注意不是文件描述符表）
 * */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
#include "files.h"
};

void init_fs() {
  // TODO: initialize the size of /dev/fb
}

int fs_open(const char *filename, int flags, int mode) {
    for (int i = 0; i < sizeof(file_table) / sizeof(Finfo); i++) {
        if (strcmp(file_table[i].name, filename) == 0) {
            file_table[i].open_offset = 0;
            return i;
        }
    }
    panic("cant find %s in ramdisk", filename);
    return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
    Finfo *f = &file_table[fd];
    size_t real_len = 0;
    if (f->read) {
        return f->read(buf, f->open_offset, len);
    } else {
        real_len = f->open_offset + len <= f->size ? len : f->size - f->open_offset;
        ramdisk_read(buf, f->disk_offset + f->open_offset, real_len);
        f->open_offset += real_len;
    }
    return real_len;
}

size_t fs_write(int fd, const void *buf, size_t len) {
    Finfo *f = &file_table[fd];
    size_t real_len = 0;
    if (f->write) {
        return f->write(buf, f->open_offset, len);
    } else {
        real_len = f->open_offset + len <= f->size ? len : f->size - f->open_offset;
        ramdisk_write(buf, f->disk_offset + f->open_offset, real_len);
        f->open_offset += real_len;
    }
    return real_len;
}

size_t fs_lseek(int fd, size_t offset, int whence) {
    assert(fd > 2 && fd < sizeof(file_table) / sizeof(Finfo));
    Finfo *f = &file_table[fd];
    switch (whence) {
        case SEEK_SET: assert(offset <= f->size); f->open_offset = offset; break;
        case SEEK_CUR: assert(offset <= f->size - f->open_offset); f->open_offset += offset; break;
        case SEEK_END: f->open_offset = f->size + offset; break;
        default: return -1;     
    }
    return f->open_offset;
}

int fs_close(int fd) {
    file_table[fd].open_offset = 0;
    return 0;
}
