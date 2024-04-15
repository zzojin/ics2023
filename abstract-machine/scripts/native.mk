AM_SRCS := native/trm.c \
           native/ioe.c \
           native/cte.c \
           native/trap.S \
           native/vme.c \
           native/mpe.c \
           native/platform.c \
           native/ioe/input.c \
           native/ioe/timer.c \
           native/ioe/gpu.c \
           native/ioe/audio.c \
           native/ioe/disk.c \

CFLAGS  += -fpie
ASFLAGS += -fpie -pie
comma = ,
LDFLAGS_CXX = $(addprefix -Wl$(comma), $(LDFLAGS))

# g++ 能够编译 C 源码文件
# --whole-archive 包含文件中的全部符号，不管有没有被用到
#  -no-whole-archive 仅包含文件中被其他文件用到了的符号。默认链接器采用的是 -no-whole-archive 策略
image:
	@echo + LD "->" $(IMAGE_REL)
	@g++ -pie -o $(IMAGE) -Wl,--whole-archive $(LINKAGE) -Wl,-no-whole-archive $(LDFLAGS_CXX) -lSDL2 -ldl
# 直接执行 x86 平台编译的可执行文件
run: image
	$(IMAGE)

gdb: image
	gdb -ex "handle SIGUSR1 SIGUSR2 SIGSEGV noprint nostop" $(IMAGE)
