#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();
void isa_reg_display(void);
uint8_t* guest_to_host(paddr_t paddr); 
word_t expr(char *e, bool *success);

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
    return 0;
}

static int cmd_x(char*args) {
    if(!args) return 0;
    paddr_t adr = 0;
    int N = 0;
    char *expression = args;
    while(*expression == ' ') ++expression;
    sscanf(expression, "%d", &N);
    while(*expression != ' ') ++expression;
    printf("%d\n",N);
    printf("%s\n",expression);
    bool success = true;
    adr = expr(expression, &success);

    if(success == false) {
        printf("Expression is wrong!\n");
        return 0;
    }
   
//    sscanf(expression,"%x", &adr);    //      printf("%d %x\n",N,adr);

    for(int i = 0; i < N; ++i) {
        printf("0x%x: ",adr);
        for(int j = 0; j < 4; ++j) {
            uint8_t *ret = guest_to_host(adr+j);  
            printf("%02x ",*ret);
        }
        printf("\n");
        adr += 4;
    }

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
  { "x", "Scan memory", cmd_x }
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
  is_batch_mode = true;
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
