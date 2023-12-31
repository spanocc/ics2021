#include <proc.h>

#define MAX_NR_PROC 4

void naive_uload(PCB *pcb, const char *filename);
void context_kload(PCB *new_pcb, void (*entry)(void *), void *arg);
void context_uload(PCB *new_pcb, const char *file_name, char *const argv[], char *const envp[]);

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {}; //大概(每次都不一样)0x8070d000开始的地址
static PCB pcb_boot = {};
PCB *current = NULL;

PCB *fg_pcb = NULL;

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

  // 测试main函数的参数
  // char *argv[10] = {"/bin/exec-test", "Shimamura", "-type", "f", NULL};
  // char *envp[10] = {"ARCH=riscv32-nemu", "HOME=llh", NULL};
  // char *argv[10] = { NULL};  
  // char *argv[10] = { "echo", "abc", NULL};
  // char *envp[10] = { NULL};


  // kload用am的栈（_stack_pointer），uload用heap.end的栈
  context_kload(&pcb[0], hello_fun, "Adachi");
  //context_kload(&pcb[1], hello_fun, "Shimamura");
  //context_uload(&pcb[0], "/bin/hello", NULL, NULL);
  context_uload(&pcb[1], "/bin/bird", NULL, NULL);       //如果两个都是uload，那么这两个用户程序的用户栈是一样的，会相互覆盖，发生错误
  context_uload(&pcb[2], "/bin/nslider", NULL, NULL);

  //前台进程
  fg_pcb = &pcb[1];


  switch_boot_pcb();

  Log("Initializing processes...");

  // load program here
  // naive_uload(NULL, "/bin/bird");
  yield();
}

Context* schedule(Context *prev) {  //assert(prev!=NULL);
  // save the context pointer

  current->cp = prev;   //保存当前进程的上下文(sp)
//printf("adr:%p\n",&(pcb[0].cp));
  // always select pcb[0] as the new process
  // current = &pcb[0];

/*带时钟中断的
  static int time_count = 0;
  if(current == &pcb[1]) time_count++;
  else current = &pcb[1];
  
  if(time_count >= 100) {
    time_count = 0;
    current = &pcb[0];
  }
*/

  current = (current == &pcb[0] ? fg_pcb : &pcb[0]);  //if(current == &pcb[0]) printf("A\n"); else printf("B\n");
  //printf("adr:%p  %p\n",pcb[0].cp,pcb[1].cp);
  // then return the new context
  return current->cp;    

  //return NULL;
}



