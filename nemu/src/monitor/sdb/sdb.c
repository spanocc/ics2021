#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

#include <memory/paddr.h>
#include <stdio.h>
#include <stdlib.h>

static int is_batch_mode = false;
void init_regex();
void init_wp_pool();
void isa_reg_display(void);
//uint8_t* guest_to_host(paddr_t paddr); 
word_t paddr_read(paddr_t addr, int len);
word_t expr(char *e, bool *success);
WP* new_wp();
void free_wp(WP *wp);
void delete_wp(int num);
void display_watchpoint();

#ifdef CONFIG_DIFFTEST
  extern bool difftest_open;
  void set_diff();
#endif

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
	nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char*args) {
    int N = 1;
    if(args != NULL) sscanf(args, "%d", &N); // if args only consists of blank , then N is still equal to 1
    //printf("%d\n\n%s\n\n",N,args);
    cpu_exec(N);
    return 0;
}

static int cmd_info(char*args) {
    if(!args) return 0;
    if(!strcmp(args,"r")) isa_reg_display();
    else if(!strcmp(args,"w")) display_watchpoint();
    return 0;
}

static int cmd_x(char*args) {
    if(!args) return 0;
    vaddr_t adr = 0;
    int N = 0;
    char *expression = args;
    while(*expression == ' ') ++expression;
    sscanf(expression, "%d", &N);
    while(*expression != ' ') ++expression;
  //  printf("%d\n",N);
  //  printf("%s\n",expression);
    bool success = true;
    adr = expr(expression, &success);

    if(success == false) {
        printf("Expression is wrong!\n");
        return 0;
    }
    printf("value: 0x%x\n",adr);   
//    sscanf(expression,"%x", &adr);    //      printf("%d %x\n",N,adr);
    for(int i = 0; i < N; ++i) {
      
        uint32_t ret = paddr_read(adr, 4);
        printf("0x%x: ",adr);
        uint8_t* ptu = (uint8_t*)&ret;
        for(int j = 0; j < 4; ++j) {  
            printf("%02x ",*ptu);
            ptu++;
        }
        printf("\n");
        adr += 4;
    }

    return 0;
}
static int cmd_p(char*args) {
    if(!args) return 0;
    uint32_t result = 0;
    bool success = true;
    result = expr(args, &success);
    if(success == false) {
        printf("Expression is wrong!\n");
        return 0;
    }
    printf("value:%u\n",result);
    return 0;
}


static int cmd_w(char*args) {
    if(!args) return 0;
    WP* pwp = new_wp();
    bool success = true;
    uint32_t result = expr(args,&success);
    if(success == false) {
        printf("Expression is wrong!\n");
        return 0;
    }
    strncpy(pwp->WatchName, args, 63);
    pwp->value = result;
    return 0;
}

static int cmd_d(char*args) {
    if(!args) return 0;
    int num;
    sscanf(args, "%d", &num);
    delete_wp(num);
    return 0;
}

static int cmd_detach(char *args) {
#ifdef CONFIG_DIFFTEST
  difftest_open = false;
#endif
  return 0;
} 

static int cmd_attach(char *args) {
#ifdef CONFIG_DIFFTEST
  difftest_open = true;
  set_diff();
#endif
  return 0;
}


static int cmd_save(char *args) {
  if(!args) return 0;
  FILE *fp = fopen(args, "wb");
  if(fp == NULL) {
    printf("open file fault!");
    return 0;
  }
  for(int i = 0; i < 32; ++i) {
    fwrite(&(cpu.gpr[i]._32), 1, 4, fp);
  }
  fwrite(&(cpu.pc), 1, 4, fp);
  fwrite(&(cpu.mepc), 1, 4, fp);
  fwrite(&(cpu.mstatus), 1, 4, fp);
  fwrite(&(cpu.mcause), 1, 4, fp);
  fwrite(&(cpu.mtvec), 1, 4, fp);

  uint8_t *p = guest_to_host(RESET_VECTOR);
  for(int i = 0; i < CONFIG_MSIZE; ++i) {
    fwrite(&p[i], 1, 1, fp);
  }
  return 0;
}

static int cmd_load(char *args) {
  if(!args) return 0;
  FILE *fp = fopen(args, "rb");
  if(fp == NULL) {
    printf("open file fault!");
    return 0;
  }
  int ret = 0;
  for(int i = 0; i < 32; ++i) {
    ret = fread(&(cpu.gpr[i]._32), 1, 4, fp);
  }
  ret = fread(&(cpu.pc), 1, 4, fp);
  ret = fread(&(cpu.mepc), 1, 4, fp);
  ret = fread(&(cpu.mstatus), 1, 4, fp);
  ret = fread(&(cpu.mcause), 1, 4, fp);
  ret = fread(&(cpu.mtvec), 1, 4, fp);

  uint8_t *p = guest_to_host(RESET_VECTOR);
  for(int i = 0; i < CONFIG_MSIZE; ++i) {
    ret = fread(&p[i], 1, 1, fp);
  }
  assert(ret);
  return 0;
}


static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Execute N instructions", cmd_si },
  { "info", "Print informations", cmd_info },
  { "x", "Scan memory", cmd_x },
  { "p", "Expression evaluation", cmd_p },
  { "w", "Set watchpoint", cmd_w },
  { "d", "Delete watchpoint", cmd_d },
  { "detach", "Close Difftest", cmd_detach},
  { "attach", "Open Difftest", cmd_attach},
  { "save", "Save the state to path", cmd_save},
  { "load", "Load the state from path", cmd_load},
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {

#ifndef CONFIG_WATCHPOINT //和监视点一起控制，如果定义了监视点则开启nemu调试
  is_batch_mode = true;
#endif

}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) {  return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}

