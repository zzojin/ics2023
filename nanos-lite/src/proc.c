#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;
void naive_uload(PCB *pcb, const char *filename);
void context_kload(PCB *pcb, void (*entry)(void *), void *arg);
void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  int j = 1;
  while (j < 10) {
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (uintptr_t)arg, j);
    j ++;
    yield();
  }
}

void init_proc() {
    context_kload(&pcb[0], hello_fun, NULL);
    context_kload(&pcb[1], hello_fun, "zhoujin");
  switch_boot_pcb();

  Log("Initializing processes...");

  // load program here
  // naive_uload(NULL, "/bin/hello");
  // naive_uload(NULL, "/bin/timer-test");
   //naive_uload(NULL, "/bin/event-test");
  //naive_uload(NULL, "/bin/bmp-test");
  //naive_uload(NULL, "/bin/fixedptc-test");
  //naive_uload(NULL, "/bin/nslider");
  //naive_uload(NULL, "/bin/menu");
    //naive_uload(NULL, "/bin/nterm");
    //naive_uload(NULL, "/bin/bird");
    //naive_uload(NULL, "/bin/pal");
}

Context* schedule(Context *prev) {
    // save the context pointer
    current->cp = prev;

    // always select pcb[0] as the new process
    //current = &pcb[0];

    current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);

    // then return the new context
    return current->cp;
}
