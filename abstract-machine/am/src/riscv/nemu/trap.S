#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)
#define MAP(c, f) c(f)

#if __riscv_xlen == 32
#define LOAD  lw
#define STORE sw
#define XLEN  4
#else
#define LOAD  ld
#define STORE sd
#define XLEN  8
#endif

#define REGS(f) \
      f( 1)       f( 3) f( 4) f( 5) f( 6) f( 7) f( 8) f( 9) \
f(10) f(11) f(12) f(13) f(14) f(15) f(16) f(17) f(18) f(19) \
f(20) f(21) f(22) f(23) f(24) f(25) f(26) f(27) f(28) f(29) \
f(30) f(31)

#define PUSH(n) STORE concat(x, n), (n * XLEN)(sp);
#define POP(n)  LOAD  concat(x, n), (n * XLEN)(sp);

#define CONTEXT_SIZE  ((32 + 3 + 1) * XLEN)
#define OFFSET_SP     ( 2 * XLEN)
#define OFFSET_CAUSE  (32 * XLEN)
#define OFFSET_STATUS (33 * XLEN)
#define OFFSET_EPC    (34 * XLEN)
#define OFFSET_NP    (35 * XLEN)

.align 3
.globl __am_asm_trap                     //每个进程用的是自己的内核栈，进入用户态时，保存的是自己的内核栈，进入内核态时，保存上下文用的也是自己的内核栈，就是PCB数组里的那个内核栈
__am_asm_trap:

	csrrw sp, mscratch, sp        // (1) atomically exchange sp and mscratch
  bnez sp, save_context_user    // (2) take the branch if we trapped from user
  csrr sp, mscratch             // (3) if we trapped from kernel, restore the original sp

	//  c->np = KERNEL
	addi sp, sp, -CONTEXT_SIZE
	MAP(REGS, PUSH)  
	li t1, 0
	jal save_together
	
	//  c->np = USER
save_context_user:
  addi sp, sp, -CONTEXT_SIZE          //;给上下文创建栈空间

  MAP(REGS, PUSH)                     //;寄存器压栈  sp栈顶指针不保存 
	li t1, 1  

save_together:
	csrr t0, mscratch       
	STORE t0, OFFSET_SP(sp)       // c->sp = $sp;    
	STORE t1, OFFSET_NP(sp)       // c->np

	csrrwi x0, mscratch, 0


  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc

  STORE t0, OFFSET_CAUSE(sp)          
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  # set mstatus.MPRV to pass difftest
  li a0, (1 << 17)
  or t1, t1, a0  
  csrw mstatus, t1

  mv a0, sp                          //;把栈顶指针（context指针）作为参数传递给__am_irq_handle，x86中的 pushl %esp指令同理
  jal __am_irq_handle

  mv sp, a0                         // 返回值（新进程的contest指针）赋值给sp  (内核栈)

	LOAD t0, OFFSET_SP(sp)	
	LOAD t1, OFFSET_NP(sp)
	beqz t1, switch2
	csrw mscratch, t0


switch2:
  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  MAP(REGS, POP)

  addi sp, sp, CONTEXT_SIZE

	STORE sp, (-34 * XLEN)(sp)

  mret
