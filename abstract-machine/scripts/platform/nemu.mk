AM_SRCS := platform/nemu/trm.c \
           platform/nemu/ioe/ioe.c \
           platform/nemu/ioe/timer.c \
           platform/nemu/ioe/input.c \
           platform/nemu/ioe/gpu.c \
           platform/nemu/ioe/audio.c \
           platform/nemu/ioe/disk.c \
           platform/nemu/mpe.c

CFLAGS    += -fdata-sections -ffunction-sections
# -T 指定链接器使用的链接器脚本文件，--defsym 定义符号
LDFLAGS   += -T $(AM_HOME)/scripts/linker.ld \
             --defsym=_pmem_start=0x80000000 --defsym=_entry_offset=0x0
# 指定程序入口点是 _start, 并且启用垃圾回收功能，从而移出未被任何部分引用的代码和数据段，可缩减最终二进制文件大小
LDFLAGS   += --gc-sections -e _start   
NEMUFLAGS += -l $(shell dirname $(IMAGE).elf)/nemu-log.txt
#NEMUFLAGS += -b
# IMAGE 是绝对路径，定义在 abstract-machine/Makefile. 因此 run 目标执行时切换到 NEMU_HOME 也不会影响后续程序读取 elf 文件
NEMUFLAGS += -e $(IMAGE).elf 
#-e /home/jasper/ics2023/nanos-lite/build/ramdisk.img 
#当 ramdisk 包含多个文件时，不能直接用 ramdisk 代替程序的原始 elf 文件

# mainargs 可以从 make 命令行输入，是作为镜像的主函数命令行参数。本操作是把 mainargs Makefile 变量放在宏 MAINARGS 中
CFLAGS += -DMAINARGS=\"$(mainargs)\"
CFLAGS += -I$(AM_HOME)/am/src/platform/nemu/include
.PHONY: $(AM_HOME)/am/src/platform/nemu/trm.c

image: $(IMAGE).elf
	@$(OBJDUMP) -d $(IMAGE).elf > $(IMAGE).txt
	@echo + OBJCOPY "->" $(IMAGE_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(IMAGE).elf $(IMAGE).bin
### 下面的命令会切到 nemu 子项目下的 Makefile，nemu/scripts/native.mk 中包含了 run 命令进一步的补充，包括对参数 ARGS 和 IMG
run: image
	$(MAKE) -C $(NEMU_HOME) ISA=$(ISA) run ARGS="$(NEMUFLAGS)" IMG=$(IMAGE).bin

gdb: image
	$(MAKE) -C $(NEMU_HOME) ISA=$(ISA) gdb ARGS="$(NEMUFLAGS)" IMG=$(IMAGE).bin
