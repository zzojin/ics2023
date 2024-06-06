#include "am.h"
#include "debug.h"
#include "fs.h"
#include <proc.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void naive_uload(PCB *pcb, const char *filename);
void context_kload(PCB *pcb, void (*entry)(void *), void *arg);
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]);

void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    Log("Hello World from Nanos-lite with arg '%s' for the %dth time!", (char *)arg, j);
    j ++;
    yield();
  }
}

void init_proc() {
    context_kload(&pcb[0], hello_fun, "zhoujin");
    /*char c[10] = "--skip";
     *char *argv[2] = {c, NULL};
     *context_uload(&pcb[1], "/bin/pal", argv, NULL);*/
    char *argv_exec_test[2] = {"/bin/exec-test", NULL};
    /*printf("pcb0 address=%p\n", &pcb[0]);
     *printf("pcb1 address=%p\n", &pcb[1]);*/
    context_uload(&pcb[1], "/bin/exec-test", argv_exec_test, NULL);
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
    //printf("pcb address = %p, context address = %p\n", current, current->cp);
    // then return the new context
    return current->cp;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    int fd = fs_open(path, 0, 0);
    if (fd == -1) {
        Log("can not open %s", path);
        return -1;
    } else {
        fs_close(fd);
        Log("Loading %s", path);
    }
    context_uload(current, path, argv, envp);
    switch_boot_pcb();
    yield();
    return 0;
}
