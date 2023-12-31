#include <unistd.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>
#include "syscall.h"
#include<stdio.h> //调试
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>


// helper macros
#define _concat(x, y) x ## y
#define concat(x, y) _concat(x, y)
#define _args(n, list) concat(_arg, n) list
#define _arg0(a0, ...) a0
#define _arg1(a0, a1, ...) a1
#define _arg2(a0, a1, a2, ...) a2
#define _arg3(a0, a1, a2, a3, ...) a3
#define _arg4(a0, a1, a2, a3, a4, ...) a4
#define _arg5(a0, a1, a2, a3, a4, a5, ...) a5

// extract an argument from the macro array
#define SYSCALL  _args(0, ARGS_ARRAY)
#define GPR1 _args(1, ARGS_ARRAY)
#define GPR2 _args(2, ARGS_ARRAY)
#define GPR3 _args(3, ARGS_ARRAY)
#define GPR4 _args(4, ARGS_ARRAY)
#define GPRx _args(5, ARGS_ARRAY)

// ISA-depedent definitions
#if defined(__ISA_X86__)
# define ARGS_ARRAY ("int $0x80", "eax", "ebx", "ecx", "edx", "eax")
#elif defined(__ISA_MIPS32__)
# define ARGS_ARRAY ("syscall", "v0", "a0", "a1", "a2", "v0")
#elif defined(__ISA_RISCV32__) || defined(__ISA_RISCV64__)
# define ARGS_ARRAY ("ecall", "a7", "a0", "a1", "a2", "a0")
#elif defined(__ISA_AM_NATIVE__)
# define ARGS_ARRAY ("call *0x100000", "rdi", "rsi", "rdx", "rcx", "rax")
#elif defined(__ISA_X86_64__)
# define ARGS_ARRAY ("int $0x80", "rdi", "rsi", "rdx", "rcx", "rax")
#else
#error _syscall_ is not implemented
#endif


extern char etext, edata, end;
intptr_t prog_brk = (intptr_t)&end;

intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2) {  //printf("SYS:%x\n",(intptr_t)&end);
  register intptr_t _gpr1 asm (GPR1) = type;
  register intptr_t _gpr2 asm (GPR2) = a0;
  register intptr_t _gpr3 asm (GPR3) = a1;
  register intptr_t _gpr4 asm (GPR4) = a2;
  register intptr_t ret asm (GPRx);
  asm volatile (SYSCALL : "=r" (ret) : "r"(_gpr1), "r"(_gpr2), "r"(_gpr3), "r"(_gpr4));
  return ret;
}

void _exit(int status) {
  _syscall_(SYS_exit, status, 0, 0);
  while (1);
}

int _open(const char *path, int flags, mode_t mode) {    //printf("SYS:%x %x\n",prog_brk,(intptr_t)&end);
  intptr_t ret = _syscall_(SYS_open, (intptr_t)path, flags, mode);
  //_exit(SYS_open);
  return ret;
}
//./libs/libc/include/_syslist.h:#define _write write

int _write(int fd, void *buf, size_t count) {
  intptr_t ret = _syscall_(SYS_write, fd, (intptr_t)buf, count);
  //_exit(SYS_write);
  return ret;
}

void *_sbrk(intptr_t increment) {      //char buf[128]; //printf("SYS:%x %x\n",prog_brk,(intptr_t)&end);
  intptr_t new_brk = prog_brk + increment;
  intptr_t old_brk = prog_brk;	                   // sprintf(buf,"PPP%x %x %d\n",new_brk,old_brk,increment);  write(1,buf,50);
  intptr_t ret = _syscall_(SYS_brk, new_brk, 0, 0);
  if(ret == 0) {
      prog_brk = new_brk;
      return (char *)old_brk;
  }   
  return (void *)-1;
}

int _read(int fd, void *buf, size_t count) {
  intptr_t ret = _syscall_(SYS_read, fd, (intptr_t)buf, count);
  //_exit(SYS_read);
  return ret;
}

int _close(int fd) {
  intptr_t ret = _syscall_(SYS_close, 0, 0, 0);
  //_exit(SYS_close);
  return ret;
}

off_t _lseek(int fd, off_t offset, int whence) {
  intptr_t ret = _syscall_(SYS_lseek, fd, offset, whence);
  //_exit(SYS_lseek);
  return ret;
}

int _gettimeofday(struct timeval *tv, struct timezone *tz) {
  intptr_t ret = _syscall_(SYS_gettimeofday, (intptr_t)tv, (intptr_t)tz, 0);
  //_exit(SYS_gettimeofday);
  //printf("zazhe\n");
  return ret;
}

int _execve(const char *fname, char * const argv[], char *const envp[]) {
  intptr_t ret = _syscall_(SYS_execve, (intptr_t)fname, (intptr_t)argv, (intptr_t)envp);
  //_exit(SYS_execve);
  errno = -ret;
  return -1;
}

// Syscalls below are not used in Nanos-lite.
// But to pass linking, they are defined as dummy functions.

int _fstat(int fd, struct stat *buf) {
  return -1;
}

int _stat(const char *fname, struct stat *buf) {
  assert(0);
  return -1;
}

int _kill(int pid, int sig) {
  _exit(-SYS_kill);
  return -1;
}

pid_t _getpid() {
  _exit(-SYS_getpid);
  return 1;
}

pid_t _fork() {
  assert(0);
  return -1;
}

pid_t vfork() {
  assert(0);
  return -1;
}

int _link(const char *d, const char *n) {
  assert(0);
  return -1;
}

int _unlink(const char *n) {
  assert(0);
  return -1;
}

pid_t _wait(int *status) {
  assert(0);
  return -1;
}

clock_t _times(void *buf) {
  assert(0);
  return 0;
}

int pipe(int pipefd[2]) {
  assert(0);
  return -1;
}

int dup(int oldfd) {
  assert(0);
  return -1;
}

int dup2(int oldfd, int newfd) {
  return -1;
}

unsigned int sleep(unsigned int seconds) {
  assert(0);
  return -1;
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
  assert(0);
  return -1;
}

int symlink(const char *target, const char *linkpath) {
  assert(0);
  return -1;
}

int ioctl(int fd, unsigned long request, ...) {
  return -1;
}
