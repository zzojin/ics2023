# Makefile for AbstractMachine Kernels and Libraries

### *Get a more readable version of this Makefile* by `make html` (requires python-markdown)
html:
	cat Makefile | sed 's/^\([^#]\)/    \1/g' | markdown_py > Makefile.html
.PHONY: html

## 1. Basic Setup and Checks

### Default to create a bare-metal kernel image
ifeq ($(MAKECMDGOALS),)
  MAKECMDGOALS  = image
  .DEFAULT_GOAL = image
endif

### Override checks when `make clean/clean-all/html`
ifeq ($(findstring $(MAKECMDGOALS),clean|clean-all|html),)

### Print build info message
$(info # Building $(NAME)-$(MAKECMDGOALS) [$(ARCH)])

### Check: environment variable `$AM_HOME` looks sane
ifeq ($(wildcard $(AM_HOME)/am/include/am.h),)
  $(error $$AM_HOME must be an AbstractMachine repo)
endif

### Check: environment variable `$ARCH` must be in the supported list, $$ARCH 在最终的报错信息中会直接显示成 $ARCH
ARCHS = $(basename $(notdir $(shell ls $(AM_HOME)/scripts/*.mk)))
ifeq ($(filter $(ARCHS), $(ARCH)), )
  $(error Expected $$ARCH in {$(ARCHS)}, Got "$(ARCH)")
endif

### Extract instruction set architecture (`ISA`) and platform from `$ARCH`. Example: `ARCH=x86_64-qemu -> ISA=x86_64; PLATFORM=qemu`
ARCH_SPLIT = $(subst -, ,$(ARCH))
ISA        = $(word 1,$(ARCH_SPLIT))
PLATFORM   = $(word 2,$(ARCH_SPLIT))

### Check if there is something to build
ifeq ($(flavor SRCS), undefined)
  $(error Nothing to build)
endif

### Checks end here
endif

## 2. General Compilation Targets

### Create the destination directory (`build/$ARCH`)
WORK_DIR  = $(shell pwd)
DST_DIR   = $(WORK_DIR)/build/$(ARCH)
$(shell mkdir -p $(DST_DIR))

### Compilation targets (a binary image or archive)
IMAGE_REL = build/$(NAME)-$(ARCH)
IMAGE     = $(abspath $(IMAGE_REL))
ARCHIVE   = $(WORK_DIR)/build/$(NAME)-$(ARCH).a

### Collect the files to be linked: object files (`.o`) and libraries (`.a`)
### SRCS and NAME 都是由客户程序那边的 Makefile 生成的，详见am-kernals/test/cpu-tests/Makefile, am/Makefile 中的 AM_SRCS，后期该变量会整合成 SRCS。所以这个 Makefile 更像是中间的中枢处理站。从各地方的 Makefile 搜集源文件，编译得到 image 和 ARGS 变量，再切到 nemu 目录下编译 nemu 构建 run 目标，nemu 可执行文件执行包含了客户程序和运行时环境代码的镜像
OBJS      = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(SRCS))))
LIBS     := $(sort $(LIBS) am klib) # lazy evaluation ("=") causes infinite recursions
# 包含目标文件以及 am/build 目录下的一个静态库文件 LIB-ARCH.a， LIB=am
# 以及 klib/build/ 目录下的一个静态库文件 LIB-ARCH.a，这种情况下 LIB=klib 
# -$(ARCH).a 的连接符，就是用来与最后的 $(LIBS)字符连上构成一个完整的库文件名
LINKAGE   = $(OBJS) \
  $(addsuffix -$(ARCH).a, $(join \
    $(addsuffix /build/, $(addprefix $(AM_HOME)/, $(LIBS))), \
    $(LIBS) ))

## 3. General Compilation Flags

### (Cross) compilers, e.g., mips-linux-gnu-g++
AS        = $(CROSS_COMPILE)gcc
CC        = $(CROSS_COMPILE)gcc
CXX       = $(CROSS_COMPILE)g++
LD        = $(CROSS_COMPILE)ld
AR        = $(CROSS_COMPILE)ar
OBJDUMP   = $(CROSS_COMPILE)objdump
OBJCOPY   = $(CROSS_COMPILE)objcopy
READELF   = $(CROSS_COMPILE)readelf

### Compilation flags
INC_PATH += $(WORK_DIR)/include $(addsuffix /include/, $(addprefix $(AM_HOME)/, $(LIBS)))
INCFLAGS += $(addprefix -I, $(INC_PATH))

ARCH_H := arch/$(ARCH).h
# $(ARCH) 是读取 Makefile 变量，tr a-z A-Z: 小写全都转成大写，-换成下划线
CFLAGS   += -O2 -MMD -Wall -Werror $(INCFLAGS) \
            -D__ISA__=\"$(ISA)\" -D__ISA_$(shell echo $(ISA) | tr a-z A-Z)__ \
            -D__ARCH__=$(ARCH) -D__ARCH_$(shell echo $(ARCH) | tr a-z A-Z | tr - _) \
            -D__PLATFORM__=$(PLATFORM) -D__PLATFORM_$(shell echo $(PLATFORM) | tr a-z A-Z | tr - _) \
            -DARCH_H=\"$(ARCH_H)\" \
            -fno-asynchronous-unwind-tables -fno-builtin -fno-stack-protector \
            -Wno-main -U_FORTIFY_SOURCE
CXXFLAGS +=  $(CFLAGS) -ffreestanding -fno-rtti -fno-exceptions
ASFLAGS  += -MMD $(INCFLAGS)
LDFLAGS  += -z noexecstack

## 4. Arch-Specific Configurations
#$(info ARCH is $(ARCH))

### Paste in arch-specific configurations (e.g., from `scripts/x86_64-qemu.mk`)
# native.mk 没有引用其他的 mk 文件, 直接定义 run image gdb 等编译目标
# riscv32-nemu 引用了 nemu.mk and riscv32.mk。编译的目标 run image gdb 等和参数都定义在其中
-include $(AM_HOME)/scripts/$(ARCH).mk

#$(info CC is $(CC), ARCH is $(ARCH), cross compile is $(CROSS_COMPILE))
#$(info common cflags $(COMMON_CFLAGS))

### Fall back to native gcc/binutils if there is no cross compiler
ifeq ($(wildcard $(shell which $(CC))),)
  $(info #  $(CC) not found; fall back to default gcc and binutils)
  CROSS_COMPILE :=
endif

## 5. Compilation Rules

### Rule (compile): a single `.c` -> `.o` (gcc)
$(DST_DIR)/%.o: %.c
### mkdir 是 shell 命令，dir 是内置函数。内置函数和变量为了避免被解释成普通字符串，使用时 $() or ${}
### -c 只编译不链接，$(realpath $<) 将第一个依赖（这种模式和场景下，一般是 c 文件）转写成绝对路径
	@mkdir -p $(dir $@) && echo + CC $<
	@$(CC) -std=gnu11 $(CFLAGS) -c -o $@ $(realpath $<)

### Rule (compile): a single `.cc` -> `.o` (g++)
$(DST_DIR)/%.o: %.cc
	@mkdir -p $(dir $@) && echo + CXX $<
	@$(CXX) -std=c++17 $(CXXFLAGS) -c -o $@ $(realpath $<)

### Rule (compile): a single `.cpp` -> `.o` (g++)
$(DST_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@) && echo + CXX $<
	@$(CXX) -std=c++17 $(CXXFLAGS) -c -o $@ $(realpath $<)

### Rule (compile): a single `.S` -> `.o` (gcc, which preprocesses and calls as)
$(DST_DIR)/%.o: %.S
	@mkdir -p $(dir $@) && echo + AS $<
	@$(AS) $(ASFLAGS) -c -o $@ $(realpath $<)

### Rule (recursive make): build a dependent library (am, klib, ...)
### $(MAKE) 保证子 make 与当前 make 相同的版本，archive 告诉子 make 执行的目标。即递归下去实际要干的活
### 注意因为会递归潜入到每个 lib 下面去做编译，所以 OBJS 和  ARCHIVE 这些都是会根据实际情况变化的。详见 am/Makefile klib/Makefile 等. 所以 archive 子 make 进程能够灵活处理各模块的编译。例如 LIBS 包含 am klib，这项规则能够分别进入这两个子模块目录下，执行 archive 
$(LIBS): %:
	@$(MAKE) -s -C $(AM_HOME)/$* archive

### Rule (link): objects (`*.o`) and libraries (`*.a`) -> `IMAGE.elf`, the final ELF binary to be packed into image (ld)
### 这条规则的使用位置隐藏在 scripts/platform/nemu.mk 中的 image target 规则，这条规则将与本文件中的 image:image-dep 发生依赖合并
$(IMAGE).elf: $(OBJS) am $(LIBS)
	@echo + LD "->" $(IMAGE_REL).elf
	@echo "Linking flags: $(LDFLAGS)"
	@$(LD) $(LDFLAGS) -o $(IMAGE).elf --start-group $(LINKAGE) --end-group #--verbose -L/opt/riscv/sysroot/usr/lib32/ilp32 -lc

### Rule (archive): objects (`*.o`) -> `ARCHIVE.a` (ar)
$(ARCHIVE): $(OBJS)
	@echo + AR "->" $(shell realpath $@ --relative-to .)
	@$(AR) rcs $(ARCHIVE) $(OBJS)

### Rule (`#include` dependencies): paste in `.d` files generated by gcc on `-MMD`
-include $(addprefix $(DST_DIR)/, $(addsuffix .d, $(basename $(SRCS))))

## 6. Miscellaneous

### Build order control
### run 目标在 scripts/platform/nemu.mk 中有更详细的定义。条件是 image 目标，然后切换目录到 $NEMU_HOME 下， 读取目录下的 Makefile，执行编译 run 目标，并指定 image。这样 NEMU 就能执行交叉编译好的程序代码
### 事实上，run 目标通过 make -C 跨目录跳转，不断且不受限制地切换到其他目录的 Makefile，以至于最后本 Makefile 其实提供的仅仅是 image，而 run 目标被偷龙换凤之后，由跳转后目录下的 Makefile 重新安排后续操作
image: image-dep
archive: $(ARCHIVE)
image-dep: $(OBJS) am $(LIBS)
	@echo \# Creating image [$(ARCH)]
.PHONY: image image-dep archive run $(LIBS)

### Clean a single project (remove `build/`)
clean:
	rm -rf Makefile.html $(WORK_DIR)/build/
.PHONY: clean

### Clean all sub-projects within depth 2 (and ignore errors)
### CLEAN_ALL 是一个变量，最终获得包含 Makefile 文件的子目录列表
CLEAN_ALL = $(dir $(shell find . -mindepth 2 -name Makefile))
### clean-all 是一个目标，依赖于 $(CLEAN_ALL) clean 等两个目标 
clean-all: $(CLEAN_ALL) clean
### 对于 CLEAN_ALL 找到的每个目录，都有一个隐含的目标需要被执行，具体的执行命令定义如下: 递归地执行 make clean 命令，-s 使得 make 在执行命令时不输出错误意外的信息，-C 指定了 make 应该在哪个目录执行
$(CLEAN_ALL):
	-@$(MAKE) -s -C $@ clean
.PHONY: clean-all $(CLEAN_ALL)
