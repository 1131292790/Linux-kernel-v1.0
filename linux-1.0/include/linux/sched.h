#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

#define NEW_SWAP

/*
 * define DEBUG if you want the wait-queues to have some extra
 * debugging code. It's not normally used, but might catch some
 * wait-queue coding errors.
 *
 *  #define DEBUG
 */

/*
 *	HZ: ����ϵͳ��ʱ�ӽ���(�δ�)Ƶ��Ϊ 100HZ����ϵͳ�� tick ����Ϊ 10ms��ʱ�ӽ�����ͨ�� sched_init
 * �����г�ʼ�����ⲿӲ����ʱ����ʵ�ֵģ�1 tick = 10ms��
 */
#define HZ 100

/*
 * System setup flags..
 */
extern int hard_math;
extern int x86;
extern int ignore_irq13;
extern int wp_works_ok;

/*
 * Bus types (default is ISA, but people can check others with these..)
 * MCA_bus hardcoded to 0 for now.
 */
extern int EISA_bus;
#define MCA_bus 0

#include <linux/tasks.h>
#include <asm/system.h>

/*
 * User space process size: 3GB. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.
 */
/*
 *	TASK_SIZE: �����û�̬�ռ�Ĵ�С��ÿ����������Ե�ַ�ռ�Ϊ 4GB��0 - 3GB Ϊ������û�̬�ռ䣬
 * 3GB - 4GB Ϊ������ں�̬�ռ䡣
 */
#define TASK_SIZE	0xc0000000

/*
 * Size of io_bitmap in longwords: 32 is ports 0-0x3ff.
 */
#define IO_BITMAP_SIZE	32

/*
 * These are the constant used to fake the fixed-point load-average
 * counting. Some notes:
 *  - 11 bit fractions expand to 22 bits by the multiplies: this gives
 *    a load-average precision of 10 bits integer + 11 bits fractional
 *  - if you want to count load-averages more often, you need more
 *    precision, or rounding will get you. With 2-second counting freq,
 *    the EXP_n values would be 1981, 2034 and 2043 if still using only
 *    11 bit fractions.
 */
extern unsigned long avenrun[];		/* Load averages */

#define FSHIFT		11		/* nr of bits of precision */
#define FIXED_1		(1<<FSHIFT)	/* 1.0 as fixed-point */
#define LOAD_FREQ	(5*HZ)		/* 5 sec intervals */
#define EXP_1		1884		/* 1/exp(5sec/1min) as fixed-point */
#define EXP_5		2014		/* 1/exp(5sec/5min) */
#define EXP_15		2037		/* 1/exp(5sec/15min) */

#define CALC_LOAD(load,exp,n) \
	load *= exp; \
	load += n*(FIXED_1-exp); \
	load >>= FSHIFT;

#define CT_TO_SECS(x)	((x) / HZ)
#define CT_TO_USECS(x)	(((x) % HZ) * 1000000/HZ)

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/time.h>
#include <linux/param.h>
#include <linux/resource.h>
#include <linux/vm86.h>
#include <linux/math_emu.h>

/*
 *	�����״̬:
 *
 *	TASK_RUNNING: ������������������״̬���� TASK_RUNNING��
 *		      1. �������û�̬����
 *		      2. �������ں�̬����
 *		      3. ������׼�����������ھ���̬
 *
 *	TASK_INTERRUPTIBLE: ���ж�˯��״̬����������״̬�µ�������Ա��źŻ��ѣ����Ѻ���� TASK_RUNNING ״̬��
 *
 *	TASK_UNINTERRUPTIBLE: �����ж�˯��״̬����������״̬�µ������ܱ��źŻ��ѣ�ֻ�ܱ� wake_up() ����
 *			      ���ѣ����Ѻ���� TASK_RUNNING ״̬��
 *
 *	TASK_ZOMBIE: ��ʬ̬��������ֹͣ���У���ռ�еĴ󲿷���Դ���Ѿ��ͷţ���������ռ�е� task_struct �ṹ
 *		     �� task_struct �ṹ���ڵ��ڴ�ҳ�滹δ�ͷţ����ڵȴ��丸������ո����񣬸��������ʱ��Ҫ
 *		     �õ�������� task_struct �ṹ�������ջ��ͷŸ������ task_struct �ṹ�� task_struct �ṹ
 *		     ��ռ�е��ڴ�ҳ�棬��������պ󣬸�����������ʧ��
 *
 *	TASK_STOPPED: ��ͣ״̬����ʱֹͣ���С�
 *
 *	TASK_SWAPPING:
 */
#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4
#define TASK_SWAPPING		5

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifdef __KERNEL__

extern void sched_init(void);
extern void show_state(void);
extern void trap_init(void);

asmlinkage void schedule(void);

#endif /* __KERNEL__ */

struct i387_hard_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

struct i387_soft_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long    top;
	struct fpu_reg	regs[8];	/* 8*16 bytes for each FP-reg = 128 bytes */
	unsigned char	lookahead;
	struct info	*info;
	unsigned long	entry_eip;
};

union i387_union {
	struct i387_hard_struct hard;
	struct i387_soft_struct soft;
};

/*
 *	tss_struct: ����״̬��(Task State Segment)���ݽṹ���洢�������������Ϣ�����ڱ���ͻָ�
 * ������ֳ����������ʱ�����������Զ�������ĵ�ǰ�ֳ���Ϣ����������� TSS ���У��������ʱ����
 * �������Զ�������� TSS ���лָ�֮ǰ������ֳ���Ϣ�����л���������������С�
 *
 *	����� TSS �ε���С��СΪ 104 �ֽ�(0x64 λ�ô�)��104 �ֽ����ڵ���Ϣ�ϸ���ѭ����״̬�� TSS
 * �ĸ�ʽ���������Ϣ���Ը��ݾ������������ơ�
 *
 *	TSS ���� 104 �ֽ����ڵ��ֶο��Է�Ϊ��̬�ֶκ;�̬�ֶ�������:
 *
 *	��̬�ֶ�: ���������ʱ�����������Զ����¶�̬�ֶε����ݣ�����ͨ�üĴ����ֶΡ���ѡ����ֶΡ�
 * ��־�Ĵ����ֶΡ�ָ��ָ���ֶΡ���ǰ���������ֶΣ���Щ��Ϣ���������ִ�й����ж�̬�仯�ġ�
 *
 *	��̬�ֶ�: ��Щ�ֶε����������񱻴���ʱ���ã����������ȡ��Щ�ֶε����ݣ���ͨ������������ǣ�
 * ��̬�ֶε�ֵ���������ʱ���ᱻ���£�������ֻ������Ҫ����ʱ�Ż����Ӧ��λ����ȥ��ȡ���ǵ�ֵ��
 * ���� LDT ��ѡ����ֶΡ�ҳĿ¼����ַ�Ĵ����ֶΡ���Ȩ�� 0/1/2 �Ķ�ջָ���ֶΡ��������塢I/O λͼ
 * ����ַ�ֶΣ���Щ��Ϣ�������ִ�й������ǲ��ᶯ̬�仯�ġ�
 *
 *	104 �ֽ�������ֶΣ�����Ҫ�ڳ������еĹ������ֶ���д�ģ�Ҳ��û�ж�̬�;�̬֮�֡�
 *
 *	TSS �θ�ʽ����:
 *
 *	|31	      16|15	       0|
 *	+-------------------------------+----------
 *	|				|
 *	|				|	I387_INFO: �洢���񸡵���صļĴ�����
 *	|	     I387_INFO		|
 *	|				|
 *	|				|
 *	+-------------------------------+----------
 *	|	     ERROR_CODE		|	ERROR_CODE: �洢�����쳣�Ĵ����롣
 *	+-------------------------------+----------
 *	|	      TRAP_NO		|	TRAP_NO: �洢�쳣���ͱ�š�
 *	+-------------------------------+----------
 *	|		CR2		|	CR2: ҳ�������Ե�ַ�ֶΣ��洢����ҳ��������Ե�ַ��
 *	+-------------------------------+----------
 *	|		TR		|	TR: ����Ĵ����ֶΡ��洢����� TSS ��ѡ��������ֶε�ֵ������
 *	+-------------------------------+---------- ����ʱ���ã�����������ʱ������д�������л�ʱ���õ����ֵ��
 *	|				|
 *	|				|	IO_BITMAP: I/O ���λͼ�ֶΣ����ֶεĴ�Сȡ���� IO_BITMAP_SIZE
 *	|				|		   �����á�
 *	|				|
 *	|				|
 *	|				|
 *	|	    IO_BITMAP		|
 *	|				|
 *	|				|     ----------------------------------------------------------------
 *	|				|     |	BIT_MAP: I/O λͼ����ַ�ֶΣ��洢�� TSS �ο�ʼ���� I/O ���
 *	|				|     |		 λͼ���� 16 λƫ��ֵ������ط���ֵ�����񴴽�ʱ����
 *	|				|     |		 �ã�����������ʱ�����������ط���ֵ��
 *	|				|     |	T: ���������־�ֶΣ����ֶ�λ�� TSS �ε�ƫ�� 0x64 ���� bit0
 *	+-------------------------------+------	   �����������˸�λʱ���������л���������Ĳ����������һ��
 * 0x64	|    BIT_MAP	|	      |T|	   �����쳣���ں�δʹ�ø��ֶΡ�
 *	+-------------------------------+----------
 * 0x60	|		|	LDT	|	LDT: LDT ��ѡ����ֶΣ���������� LDT �ε�ѡ�����ÿ�������
 *	+-------------------------------+----------  LDT �������񴴽�ʱ���Ѿ�ȷ�����������������ڲ�����ģ�
 * 0x5C	|		|	GS	|	  |  ����������ʱ������Ĵ˴���ֵ��������Ҫ����������Ӵ˴�
 *	+-------------------------------+	  |  ��ȡ����� LDT �εĶ�ѡ�����
 * 0x58	|		|	FS	|	  -----------------------------------------------------------
 *	+-------------------------------+	GS -> ES: ��ѡ����ֶΡ��������ʱ���������Զ�����Щ�Ĵ�����
 * 0x54	|		|	DS	|		  ��Ϣ�����ڴ˴��������ٴε���ʱ���������Զ��Ӵ˴�
 *	+-------------------------------+		  �ָ���Щ�Ĵ�������Ϣ��
 * 0x50	|		|	SS	|
 *	+-------------------------------+
 * 0x4C	|		|	CS	|
 *	+-------------------------------+
 * 0x48	|		|	ES	|
 *	+-------------------------------+----------
 * 0x44	|	       EDI		|
 *	+-------------------------------+
 * 0x40	|	       ESI		|
 *	+-------------------------------+	EDI -> EAX: ͨ�üĴ����ֶΡ��������ʱ���������Զ�����Щ�Ĵ���
 * 0x3C	|	       EBP		|		    ����Ϣ�����ڴ˴��������ٴε���ʱ���������Զ��Ӵ˴�
 *	+-------------------------------+		    �ָ���Щ�Ĵ�������Ϣ��
 * 0x38	|	       ESP		|
 *	+-------------------------------+	SS:ESP  ָ���������ʱ��ջ��λ�á�����������û�̬������������
 * 0x34	|	       EBX		|		ָ���û�̬ջ�е�ĳһλ�ã�����������ں�̬������������
 *	+-------------------------------+		ָ���ں�̬ջ�е�ĳһλ�á�
 * 0x30	|	       EDX		|
 *	+-------------------------------+
 * 0x2C	|	       ECX		|
 *	+-------------------------------+
 * 0x28	|	       EAX		|
 *	+-------------------------------+----------
 * 0x24	|	      EFLAGS		|	EFLAGS: ��־�Ĵ����ֶΡ��������ʱ�����־�Ĵ��� EFLAGS ��ֵ��
 *	+-------------------------------+----------	�������Զ�����ͻָ� EFLAGS��
 * 0x20	|	       EIP		|	EIP: ָ��ָ���ֶΡ�CS:EIP ָ���������ʱ�Ĵ����ִ��λ�á�����
 *	+-------------------------------+----------  ���Զ�����ͻָ� EIP �Ĵ��������ݡ�
 * 0x1C	|	     CR3(PDBR)		|	CR3: ҳĿ¼����ַ�Ĵ����ֶΡ�ÿ���������Լ���һ��ҳ����¼��
 *	+-------------------------------+----------  ÿ����������Ե�ַ�������ַ��ӳ���ϵ��ÿ��������һ��ҳ
 * 0x18	|		|	SS2	|	  |  Ŀ¼����ҳĿ¼�������񴴽���ʱ����Ѿ�ȷ���ˣ����������
 *	+-------------------------------+	  |  ���������ڲ���ı䡣��ˣ����λ�õ� CR3 ֻ�����񴴽���ʱ��
 * 0x14	|	       ESP2		|	  |  д�룬�������ʱ������������д���������µ���ʱ���������ȡ
 *	+-------------------------------+	  |  ����ط���ֵ�����ص� CR3 �Ĵ����С�
 * 0x10	|		|	SS1	|	  -----------------------------------------------------------
 *	+-------------------------------+	SS2 -> ESP0: ��Ȩ�� 0��1��2 �Ķ�ջָ���ֶΡ�Linux �ں�ֻ�õ���
 * 0x0C	|	       ESP1		|		     ��Ȩ�� 0 �� SS0:ESP0������ָʾ������ں�̬ջ��ջ
 *	+-------------------------------+		     �ס����ֵֻ�����񴴽�ʱ���ã�����������ʱ����
 * 0x08	|		|	SS0	|		     ���ģ�ֻ����������û�̬�����ں�̬ʱ�����������
 *	+-------------------------------+		     �˴���ȡ SS0:ESP0 ��ֵ�����ص� SS:ESP �Ĵ����У�
 * 0x04	|	       ESP0		|		     ��Ϊ�������ں�ִ̬��ʱ��ջ�ĳ�ʼλ�á�
 *	+-------------------------------+----------
 * 0x00	|		|   BACK_LINK	|	BACK_LINK: ��ǰ���������ֶΣ�����ǰһ������� TSS ��ѡ�����
 *	+-------------------------------+		   ���ֶ���������ʹ�� IRET ָ���л���ǰһ������Linux
 *							   �ں˲�δʹ��������ܡ�
 */
struct tss_struct {
	unsigned short	back_link,__blh;
	unsigned long	esp0;
	unsigned short	ss0,__ss0h;
	unsigned long	esp1;
	unsigned short	ss1,__ss1h;
	unsigned long	esp2;
	unsigned short	ss2,__ss2h;
	unsigned long	cr3;
	unsigned long	eip;
	unsigned long	eflags;
	unsigned long	eax,ecx,edx,ebx;
	unsigned long	esp;
	unsigned long	ebp;
	unsigned long	esi;
	unsigned long	edi;
	unsigned short	es, __esh;
	unsigned short	cs, __csh;
	unsigned short	ss, __ssh;
	unsigned short	ds, __dsh;
	unsigned short	fs, __fsh;
	unsigned short	gs, __gsh;
	unsigned short	ldt, __ldth;
	unsigned short	trace, bitmap;
	unsigned long	io_bitmap[IO_BITMAP_SIZE+1];
	unsigned long	tr;
	unsigned long	cr2, trap_no, error_code;
	union i387_union i387;
};

/*
 *	task_struct: ������ƿ飬ÿ��������һ��Ψһ�� task_struct �ṹ������
 */
struct task_struct {
/* these are hardcoded - don't touch */
	volatile long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
			/*
			 *	state: �����״̬��0 ��ʾ���л�����У�>0 ��״̬����δ����״̬��
			 */
	long counter;
			/*
			 *	counter: ��������ʱ��Ƭ����λΪ tick��ʱ��Ƭ���� 0 ʱ�����ó�������������
			 * �����µ�����ִ�С����������������ʵ�ʱ���������� counter ��ֵ��
			 */
	long priority;
			/*
			 *	priority: ��������ȼ�����λΪ tick��priority ������ counter �����ã���
			 * counter ֵԽ�󣬸��������ȵ��ȵĿ�����Խ��ִ�е�ʱ��ҲԽ�����������ֵ��Ϊ
			 * ���ȼ���
			 *
			 *	����ʼ����ʱ counter = priority��
			 */
	unsigned long signal;
			/*
			 *	signal: ������ź�λͼ��ÿһλ����һ���źţ���ʾ�����յ����źš�
			 * �ź�ֵ = λƫ��ֵ + 1��
			 */
	unsigned long blocked;	/* bitmap of masked signals */
			/*
			 *	blocked: ������ź������룬���ź�λͼ���Ӧ���ĸ�λ�� 1����ʾ��������
			 * (����Ӧ)��λ��Ӧ���Ǹ��źš�
			 */
	unsigned long flags;	/* per process flags, defined below */
	int errno;
	int debugreg[8];  /* Hardware debugging registers */
/* various fields */
	struct task_struct *next_task, *prev_task;
			/*
			 *	next_task �� prev_task ���ڽ�ϵͳ�����е��������ӳ�һ��˫��ѭ������
			 */
	struct sigaction sigaction[32];
			/*
			 *	sigaction: �ź�ִ�����Խṹ��ÿ���źŶ�Ӧһ�������Ľṹ���ýṹ�����ź�
			 * ��Ӧ�Ĳ��������ͱ�־����Ϣ��
			 */
	unsigned long saved_kernel_stack;
	unsigned long kernel_stack_page;
			/*
			 *	saved_kernel_stack:
			 *
			 *	kernel_stack_page: ������ں�̬ջ���������ڴ�ҳ��Ļ���ַ��
			 */
	int exit_code, exit_signal;
	int elf_executable:1;
	int dumpable:1;
	int swappable:1;
	int did_exec:1;
	unsigned long start_code,end_code,end_data,start_brk,brk,start_stack,start_mmap;
	unsigned long arg_start, arg_end, env_start, env_end;
	int pid,pgrp,session,leader;
	int	groups[NGROUPS];
	/* 
	 * pointers to (original) parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with 
	 * p->p_pptr->pid)
	 */
	struct task_struct *p_opptr,*p_pptr, *p_cptr, *p_ysptr, *p_osptr;
			/*
			 *	p_opptr: ָ��ԭʼ������
			 *	p_pptr: ָ�����ڸ�����
			 *	p_cptr: ָ�����µ�������
			 *	p_ysptr: ָ����Լ��󴴽�����������
			 *	p_osptr: ָ����Լ��紴������������
			 */
	struct wait_queue *wait_chldexit;	/* for wait4() */
	/*
	 * For ease of programming... Normal sleeps don't need to
	 * keep track of a wait-queue: every task has an entry of its own
	 */
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	unsigned long timeout;
	unsigned long it_real_value, it_prof_value, it_virt_value;
	unsigned long it_real_incr, it_prof_incr, it_virt_incr;
	long utime,stime,cutime,cstime,start_time;
	unsigned long min_flt, maj_flt;
	unsigned long cmin_flt, cmaj_flt;
	struct rlimit rlim[RLIM_NLIMITS]; 
	unsigned short used_math;
	unsigned short rss;	/* number of resident pages */
			/*
			 *	rss: ����פ�ڴ��ҳ����
			 */
	char comm[16];
	struct vm86_struct * vm86_info;
	unsigned long screen_bitmap;
/* file system info */
	int link_count;
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct inode * pwd;
	struct inode * root;
	struct inode * executable;
	struct vm_area_struct * mmap;
	struct shm_desc *shm;
	struct sem_undo *semun;
	struct file * filp[NR_OPEN];
	fd_set close_on_exec;
/* ldt for this task - used by Wine.  If NULL, default_ldt is used */
	struct desc_struct *ldt;
			/*
			 *	ldt: ָ������� LDT ������������� ldt = NULL��������ʹ��ϵͳĬ�ϵ�
			 * LDT ���������� default_ldt��
			 */
/* tss for this task */
	struct tss_struct tss;
			/*
			 *	tss: ����� TSS �Σ������������������Ϣ��
			 */
#ifdef NEW_SWAP
	unsigned long old_maj_flt;	/* old value of maj_flt */
	unsigned long dec_flt;		/* page fault count of the last time */
	unsigned long swap_cnt;		/* number of pages to swap on next pass */
	short swap_table;		/* current page table */
	short swap_page;		/* current page */
#endif NEW_SWAP
	struct vm_area_struct *stk_vma;
};

/*
 * Per process flags
 */
#define PF_ALIGNWARN	0x00000001	/* Print alignment warning msgs */
					/* Not implemented yet, only for 486*/
#define PF_PTRACED	0x00000010	/* set if ptrace (0) has been called. */
#define PF_TRACESYS	0x00000020	/* tracing system calls */

/*
 * cloning flags:
 */
#define CSIGNAL		0x000000ff	/* signal mask to be sent at exit */
#define COPYVM		0x00000100	/* set if VM copy desired (like normal fork()) */
#define COPYFD		0x00000200	/* set if fd's should be copied, not shared (NI) */

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x1fffff (=2MB)
 */
/*
 *	INIT_TASK: ���ڳ�ʼ������ 0 �� task_struct �ṹ������ 0 ��ϵͳ����ԭʼ�ĵ�һ������
 * Ҳ��ϵͳ��Ψһһ���ֶ���ʼ������������ 0 �� fork ������ 1��Ҳ�����Ǹ� init ���̣�����
 * fork ��Խ��Խ��Ľ��̡�
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15,0,0,0,0, \
				/*
				 *	state = TASK_RUNNING��
				 *	counter = 15������ 0 �ĳ�ʼʱ��ƬΪ 15 �� tick��
				 *	priority = 15������ 0 �ĳ�ʼ���ȼ���Ϊ 15 �� tick��
				 */
/* debugregs */ { 0, },            \
/* schedlink */	&init_task,&init_task, \
				/*
				 *	��ʼʱϵͳ��ֻ�� init_task һ�����񣬹� init_task �� next_task ��
				 * prev_task ��ָ���Լ���
				 */
/* signals */	{{ 0, },}, \
/* stack */	0,(unsigned long) &init_kernel_stack, \
				/*
				 *	kernel_stack_page = init_kernel_stack������ 0 ���ں�̬ջ��������
				 * �ڴ�ҳ�����ַ��init_kernel_stack ���鲢��һ����ҳ�߽���룬�˴�ֻ����
				 * �ں�̬ջ����ʼ��ַ��䡣
				 */
/* ec,brk... */	0,0,0,0,0,0,0,0,0,0,0,0,0, \
/* argv.. */	0,0,0,0, \
/* pid etc.. */	0,0,0,0, \
/* suppl grps*/ {NOGROUP,}, \
/* proc links*/ &init_task,&init_task,NULL,NULL,NULL,NULL, \
				/*
				 *	init_task ��ԭʼ����������ڸ������� init_task �Լ���
				 */
/* uid etc */	0,0,0,0,0,0, \
/* timeout */	0,0,0,0,0,0,0,0,0,0,0,0, \
/* min_flt */	0,0,0,0, \
/* rlimits */   { {LONG_MAX, LONG_MAX}, {LONG_MAX, LONG_MAX},  \
		  {LONG_MAX, LONG_MAX}, {LONG_MAX, LONG_MAX},  \
		  {       0, LONG_MAX}, {LONG_MAX, LONG_MAX}}, \
/* math */	0, \
/* rss */	2, \
				/*
				 *	rss = 2������ 0 �� 2 ��ҳ�泣פ�ڴ棬�ֱ������� 0 ���û�̬ջ
				 * user_stack ������ 0 ���ں�̬ջ init_kernel_stack�����������鲢��һ��
				 * ��ҳ�߽���롣
				 */
/* comm */	"swapper", \
/* vm86_info */	NULL, 0, \
/* fs info */	0,-1,0022,NULL,NULL,NULL,NULL, \
/* ipc */	NULL, NULL, \
/* filp */	{NULL,}, \
/* cloe */	{{ 0, }}, \
/* ldt */	NULL, \
/*tss*/	{0,0, \
	 sizeof(init_kernel_stack) + (long) &init_kernel_stack, KERNEL_DS, 0, \
				/*
				 *	tss.esp0 ָ�� init_kernel_stack �����β����Ҳ�������� 0 ���ں�̬ջ
				 * ��ջ�ס�
				 *
				 *	tss.ss0 = KERNEL_DS����ʾ���� 0 ���ں�̬ջλ���ں����ݶ��С�
				 */
	 0,0,0,0,0,0, \
	 (long) &swapper_pg_dir, \
				/*
				 *	tss.cr3 ָ�� swapper_pg_dir����ʾ���� 0 ��ҳĿ¼���� swapper_pg_dir��
				 */
	 0,0,0,0,0,0,0,0,0,0, \
	 USER_DS,0,USER_DS,0,USER_DS,0,USER_DS,0,USER_DS,0,USER_DS,0, \
				/*
				 *	6 ����ѡ�����ֵ���� USER_DS��
				 */
	 _LDT(0),0, \
				/*
				 *	tss.ldt = _LDT(0): ���� 0 �� LDT ��ѡ�����ֵ��
				 */
	 0, 0x8000, \
				/*
				 *	tss.trace = 0 ��ֹ�������塣
				 *	tss.bitmap = 0x8000
				 */
/* ioperm */ 	{~0, }, \
	 _TSS(0), 0, 0,0, \
				/*
				 *	tss.tr = _TSS(0): ���� 0 �� TSS ��ѡ�����ֵ��
				 */
/* 387 state */	{ { 0, }, } \
	} \
}

extern struct task_struct init_task;
extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern unsigned long volatile jiffies;
extern unsigned long itimer_ticks;
extern unsigned long itimer_next;
extern struct timeval xtime;
extern int need_resched;

/*
 *	CURRENT_TIME: ��ǰʱ�䣬����Ϊ�� 1970 �� 1 �� 1 �� 0 ʱ��������������������
 */
#define CURRENT_TIME (xtime.tv_sec)

extern void sleep_on(struct wait_queue ** p);
extern void interruptible_sleep_on(struct wait_queue ** p);
extern void wake_up(struct wait_queue ** p);
extern void wake_up_interruptible(struct wait_queue ** p);

extern void notify_parent(struct task_struct * tsk);
extern int send_sig(unsigned long sig,struct task_struct * p,int priv);
extern int in_group_p(gid_t grp);

extern int request_irq(unsigned int irq,void (*handler)(int));
extern void free_irq(unsigned int irq);
extern int irqaction(unsigned int irq,struct sigaction * sa);

/*
 * Entry into gdt where to find first TSS. GDT layout:
 *   0 - nul
 *   1 - kernel code segment
 *   2 - kernel data segment
 *   3 - user code segment
 *   4 - user data segment
 * ...
 *   8 - TSS #0
 *   9 - LDT #0
 *  10 - TSS #1
 *  11 - LDT #1
 */
/*
 *	��һ������Ҳ�������� 0 �� TSS �κ� LDT �ε��������� GDT ���е�ƫ��λ�á�
 */
#define FIRST_TSS_ENTRY 8
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
/*
 *	_TSS(n)��_LDT(n): �����ѡ�����ֵ����������� n ��������ѡ�� GDT ���и������
 * TSS �κ� LDT �εĶ�ѡ�����ֵ��
 */
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
/*
 *	load_TR(n)��load_ldt(n): ����������� n �õ��Ķ�ѡ�����ֵ���ص� TR �� LDTR �Ĵ����С�
 * TR �� LDTR �е�ѡ�������ѡ�� GDT �������� n ��Ӧ�� TSS �κ� LDT �ε���������
 */
#define load_TR(n) __asm__("ltr %%ax": /* no output */ :"a" (_TSS(n)))
#define load_ldt(n) __asm__("lldt %%ax": /* no output */ :"a" (_LDT(n)))

#define store_TR(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"0" (0),"i" (FIRST_TSS_ENTRY<<3))
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
#define switch_to(tsk) \
__asm__("cmpl %%ecx,_current\n\t" \
	"je 1f\n\t" \
	"cli\n\t" \
	"xchgl %%ecx,_current\n\t" \
	"ljmp %0\n\t" \
	"sti\n\t" \
	"cmpl %%ecx,_last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	: /* no output */ \
	:"m" (*(((char *)&tsk->tss.tr)-4)), \
	 "c" (tsk) \
	:"cx")

#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%1\n\t" \
	"movb %%dh,%2" \
	: /* no output */ \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)), \
	 "d" (base) \
	:"dx")

#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %1,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%1" \
	: /* no output */ \
	:"m" (*(addr)), \
	 "m" (*((addr)+6)), \
	 "d" (limit) \
	:"dx")

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

/*
 * The wait-queues are circular lists, and you have to be *very* sure
 * to keep them correct. Use only these two functions to add/remove
 * entries in the queues.
 */
extern inline void add_wait_queue(struct wait_queue ** p, struct wait_queue * wait)
{
	unsigned long flags;

#ifdef DEBUG
	if (wait->next) {
		unsigned long pc;
		__asm__ __volatile__("call 1f\n"
			"1:\tpopl %0":"=r" (pc));
		printk("add_wait_queue (%08x): wait->next = %08x\n",pc,(unsigned long) wait->next);
	}
#endif
	save_flags(flags);
	cli();
	if (!*p) {
		wait->next = wait;
		*p = wait;
	} else {
		wait->next = (*p)->next;
		(*p)->next = wait;
	}
	restore_flags(flags);
}

extern inline void remove_wait_queue(struct wait_queue ** p, struct wait_queue * wait)
{
	unsigned long flags;
	struct wait_queue * tmp;
#ifdef DEBUG
	unsigned long ok = 0;
#endif

	save_flags(flags);
	cli();
	if ((*p == wait) &&
#ifdef DEBUG
	    (ok = 1) &&
#endif
	    ((*p = wait->next) == wait)) {
		*p = NULL;
	} else {
		tmp = wait;
		while (tmp->next != wait) {
			tmp = tmp->next;
#ifdef DEBUG
			if (tmp == *p)
				ok = 1;
#endif
		}
		tmp->next = wait->next;
	}
	wait->next = NULL;
	restore_flags(flags);
#ifdef DEBUG
	if (!ok) {
		printk("removed wait_queue not on list.\n");
		printk("list = %08x, queue = %08x\n",(unsigned long) p, (unsigned long) wait);
		__asm__("call 1f\n1:\tpopl %0":"=r" (ok));
		printk("eip = %08x\n",ok);
	}
#endif
}

extern inline void select_wait(struct wait_queue ** wait_address, select_table * p)
{
	struct select_table_entry * entry;

	if (!p || !wait_address)
		return;
	if (p->nr >= __MAX_SELECT_TABLE_ENTRIES)
		return;
 	entry = p->entry + p->nr;
	entry->wait_address = wait_address;
	entry->wait.task = current;
	entry->wait.next = NULL;
	add_wait_queue(wait_address,&entry->wait);
	p->nr++;
}

extern void __down(struct semaphore * sem);

extern inline void down(struct semaphore * sem)
{
	if (sem->count <= 0)
		__down(sem);
	sem->count--;
}

extern inline void up(struct semaphore * sem)
{
	sem->count++;
	wake_up(&sem->wait);
}	

static inline unsigned long _get_base(char * addr)
{
	unsigned long __base;
	__asm__("movb %3,%%dh\n\t"
		"movb %2,%%dl\n\t"
		"shll $16,%%edx\n\t"
		"movw %1,%%dx"
		:"=&d" (__base)
		:"m" (*((addr)+2)),
		 "m" (*((addr)+4)),
		 "m" (*((addr)+7)));
	return __base;
}

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

static inline unsigned long get_limit(unsigned long segment)
{
	unsigned long __limit;
	__asm__("lsll %1,%0"
		:"=r" (__limit):"r" (segment));
	return __limit+1;
}

#define REMOVE_LINKS(p) do { unsigned long flags; \
	save_flags(flags) ; cli(); \
	(p)->next_task->prev_task = (p)->prev_task; \
	(p)->prev_task->next_task = (p)->next_task; \
	restore_flags(flags); \
	if ((p)->p_osptr) \
		(p)->p_osptr->p_ysptr = (p)->p_ysptr; \
	if ((p)->p_ysptr) \
		(p)->p_ysptr->p_osptr = (p)->p_osptr; \
	else \
		(p)->p_pptr->p_cptr = (p)->p_osptr; \
	} while (0)

#define SET_LINKS(p) do { unsigned long flags; \
	save_flags(flags); cli(); \
	(p)->next_task = &init_task; \
	(p)->prev_task = init_task.prev_task; \
	init_task.prev_task->next_task = (p); \
	init_task.prev_task = (p); \
	restore_flags(flags); \
	(p)->p_ysptr = NULL; \
	if (((p)->p_osptr = (p)->p_pptr->p_cptr) != NULL) \
		(p)->p_osptr->p_ysptr = p; \
	(p)->p_pptr->p_cptr = p; \
	} while (0)

#define for_each_task(p) \
	for (p = &init_task ; (p = p->next_task) != &init_task ; )

/*
 * This is the ldt that every process will get unless we need
 * something other than this.
 */
extern struct desc_struct default_ldt;

/* This special macro can be used to load a debugging register */

#define loaddebug(register) \
		__asm__("movl %0,%%edx\n\t" \
			"movl %%edx,%%db" #register "\n\t" \
			: /* no output */ \
			:"m" (current->debugreg[register]) \
			:"dx");

#endif
