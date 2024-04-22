#include <common.h>

#define MAX_IRINGBUF 16

typedef struct {
  word_t pc;
  uint32_t inst;
} ItraceNode;

ItraceNode iringbuf[MAX_IRINGBUF];
int p_cur = 0;
bool full = false;

void trace_inst(word_t pc, uint32_t inst) {
  iringbuf[p_cur].pc = pc;
  iringbuf[p_cur].inst = inst;
  p_cur = (p_cur + 1) % MAX_IRINGBUF;
  full = full || p_cur == 0;
}

void display_inst() {
#ifdef CONFIG_ITRACE    
  if (!full && !p_cur) return;

  int end = p_cur;
  int i = full ? p_cur : 0;
  printf("====== The nearest %d instructions ======\n", MAX_IRINGBUF);

  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  char buf[256]; // 128 should be enough!
  char *p;
  do {
    p = buf;
    p += sprintf(buf, "%s" FMT_WORD ": %08x ", (i+1) % MAX_IRINGBUF == end?" --> ":"     ", iringbuf[i].pc, iringbuf[i].inst);
    
    disassemble(p, buf + sizeof(buf) - p, iringbuf[i].pc, (uint8_t *)&iringbuf[i].inst, 4);

    if ((i+1)%MAX_IRINGBUF==end) printf(ANSI_FG_RED);
    puts(buf);
  } while ((i = (i+1)%MAX_IRINGBUF) != end);
  puts(ANSI_NONE);
#endif
}

