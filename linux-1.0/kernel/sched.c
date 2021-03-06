/*
 *  linux/kernel/sched.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */

#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/ptrace.h>
#include <linux/segment.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define TIMER_IRQ 0

#include <linux/timex.h>

/*
 * kernel variables
 */
long tick = 1000000 / HZ;               /* timer interrupt period */
/*
 *	xtime: 保存系统当前的时间，这个时间值表示的是从 1970 年 1 月 1 日 0 时起到现在所
 * 经过的秒数和微秒数，这个值随着时间的推移而不断变化，表示系统当前的时间在不断更新。
 */
volatile struct timeval xtime;		/* The current time */
int tickadj = 500/HZ;			/* microsecs */

/*
 * phase-lock loop variables
 */
int time_status = TIME_BAD;     /* clock synchronization status */
long time_offset = 0;           /* time adjustment (us) */
long time_constant = 0;         /* pll time constant */
long time_tolerance = MAXFREQ;  /* frequency tolerance (ppm) */
long time_precision = 1; 	/* clock precision (us) */
long time_maxerror = 0x70000000;/* maximum error */
long time_esterror = 0x70000000;/* estimated error */
long time_phase = 0;            /* phase offset (scaled us) */
long time_freq = 0;             /* frequency offset (scaled ppm) */
long time_adj = 0;              /* tick adjust (scaled 1 / HZ) */
long time_reftime = 0;          /* time at last adjustment (s) */

long time_adjust = 0;
long time_adjust_step = 0;

int need_resched = 0;

/*
 * Tell us the machine setup..
 */
int hard_math = 0;		/* set by boot/head.S */
int x86 = 0;			/* set by boot/head.S to 3 or 4 */
int ignore_irq13 = 0;		/* set if exception 16 works */
int wp_works_ok = 0;		/* set if paging hardware honours WP */ 
		/* wp_works_ok = 1 表示页写保护功能正常 */

/*
 * Bus types ..
 */
int EISA_bus = 0;

extern int _setitimer(int, struct itimerval *, struct itimerval *);
unsigned long * prof_buffer = NULL;
unsigned long prof_len = 0;

#define _S(nr) (1<<((nr)-1))

extern void mem_use(void);

extern int timer_interrupt(void);
asmlinkage int system_call(void);

/*
 *	init_kernel_stack: 4KB，任务 0 (init_task) 的内核态栈，任务 0 的用户态栈是 user_stack。
 */
static unsigned long init_kernel_stack[1024];
/*
 *	init_task: 任务 0 的 task_struct 结构，任务 0 是系统中最原始的第一个任务，也是系统中
 * 唯一一个人工创建的任务，后续任务的 task_struct 结构会在 fork 时申请空闲页面来存放，任务 0
 * 的 task_struct 结构需要手动设置。
 */
struct task_struct init_task = INIT_TASK;

/*
 *	jiffies: 系统的滴答(tick)计数值，表示从系统启动到现在的 tick 数，系统初始化时设置滴答周期
 * 为 10ms，每隔 10ms jiffies 的值会加 1。
 */
unsigned long volatile jiffies=0;

/*
 *	current: 永远指向当前正在运行的任务，初始时系统中运行的任务为 init_task。
 *	last_task_used_math: 指向最后一个使用数学协处理器的任务。
 */
struct task_struct *current = &init_task;
struct task_struct *last_task_used_math = NULL;

/*
 *	task[NR_TASKS]: 任务指针数组，每个元素指向一个任务的 task_struct 结构，任务的最大个数为 NR_TASKS，
 * 系统创建一个任务时，必须先要从 task 数组中获取一个空闲的元素。当 task 数组中的所有元素都用完时，系统将
 * 无法再创建出新的任务。
 *
 *	task 数组的下标就是任务的任务号，任务号的范围为 0 -> NR_TASKS - 1。任务号与进程号不同，进程号由
 * 变量 last_pid 给出。
 */
struct task_struct * task[NR_TASKS] = {&init_task, };

/*
 *	user_stack: 4KB 大小的数组当做栈来使用。系统启动时，会用于内核初始化过程中的内核栈，内核初始化完成，
 * 执行完 move_to_user_mode 之后，这个数组将用作任务 0 的用户态栈。
 */
long user_stack [ PAGE_SIZE>>2 ] ;

/*
 *	stack_start: 用于设置 SS:ESP。在 head.S 中，由指令 [ lss _stack_start,%esp ] 来设置。
 *
 *	设置后:
 *		SS = KERNEL_DS 用于选择内核数据段，表示栈的位置在内核数据段中。
 *		ESP 指向数组 user_stack 的尾部，指示栈的起始位置。
 */
struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , KERNEL_DS };

/*
 *	kstat: 内核统计结构变量。
 */
struct kernel_stat kstat =
	{ 0, 0, 0, { 0, 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 * int 0x80 entry points.. Moved away from the header file, as
 * iBCS2 may also want to use the '<linux/sys.h>' headers..
 */
#ifdef __cplusplus
extern "C" {
#endif

int sys_ni_syscall(void)
{
	return -EINVAL;
}

fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
sys_olduname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
sys_setreuid,sys_setregid, sys_sigsuspend, sys_sigpending,
sys_sethostname, sys_setrlimit, sys_getrlimit, sys_getrusage,
sys_gettimeofday, sys_settimeofday, sys_getgroups, sys_setgroups,
sys_select, sys_symlink, sys_lstat, sys_readlink, sys_uselib,
sys_swapon, sys_reboot, sys_readdir, sys_mmap, sys_munmap, sys_truncate,
sys_ftruncate, sys_fchmod, sys_fchown, sys_getpriority, sys_setpriority,
sys_profil, sys_statfs, sys_fstatfs, sys_ioperm, sys_socketcall,
sys_syslog, sys_setitimer, sys_getitimer, sys_newstat, sys_newlstat,
sys_newfstat, sys_uname, sys_iopl, sys_vhangup, sys_idle, sys_vm86,
sys_wait4, sys_swapoff, sys_sysinfo, sys_ipc, sys_fsync, sys_sigreturn,
sys_clone, sys_setdomainname, sys_newuname, sys_modify_ldt,
sys_adjtimex, sys_mprotect, sys_sigprocmask, sys_create_module,
sys_init_module, sys_delete_module, sys_get_kernel_syms, sys_quotactl,
sys_getpgid, sys_fchdir, sys_bdflush };

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);

#ifdef __cplusplus
}
#endif

/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 *
 * Careful.. There are problems with IBM-designed IRQ13 behaviour.
 * Don't touch unless you *really* know how it works.
 */
asmlinkage void math_state_restore(void)
{
	__asm__ __volatile__("clts");
	if (last_task_used_math == current)
		return;
	timer_table[COPRO_TIMER].expires = jiffies+50;
	timer_active |= 1<<COPRO_TIMER;	
	if (last_task_used_math)
		__asm__("fnsave %0":"=m" (last_task_used_math->tss.i387));
	else
		__asm__("fnclex");
	last_task_used_math = current;
	if (current->used_math) {
		__asm__("frstor %0": :"m" (current->tss.i387));
	} else {
		__asm__("fninit");
		current->used_math=1;
	}
	timer_active &= ~(1<<COPRO_TIMER);
}

#ifndef CONFIG_MATH_EMULATION

asmlinkage void math_emulate(long arg)
{
  printk("math-emulation not enabled and no coprocessor found.\n");
  printk("killing %s.\n",current->comm);
  send_sig(SIGFPE,current,1);
  schedule();
}

#endif /* CONFIG_MATH_EMULATION */

unsigned long itimer_ticks = 0;
unsigned long itimer_next = ~0;
static unsigned long lost_ticks = 0;

/*
 *  'schedule()' is the scheduler function. It's a very simple and nice
 * scheduler: it's not perfect, but certainly works for most things.
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 *
 * The "confuse_gcc" goto is used only to get better assembly code..
 * Djikstra probably hates me.
 */
/*
 *	schedule: 内核调度程序，选择下一个将要运行的任务并调度这个任务运行，当前任务暂停
 * 在 switch_to 中。
 *
 *	任务 0 是 idle 任务，当没有其它任务运行时将调度运行任务 0，任务 0 不能被杀死，也
 * 不能睡眠，内核不会使用任务 0 的状态信息。
 */
asmlinkage void schedule(void)
{
	int c;
	struct task_struct * p;
	struct task_struct * next;
	unsigned long ticks;

/* check alarm, wake up any interruptible tasks that have got a signal */

	cli();
	ticks = itimer_ticks;
	itimer_ticks = 0;
	itimer_next = ~0;
	sti();
	need_resched = 0;

	/*
	 *	for: 从 init_task 开始，扫描系统中除 init_task 以外的所有任务，完成两项工作。
	 * 一是处理所有任务的真实间隔定时器，二是唤醒所有已收到信号或超时定时到期的可中断任务。
	 */
	p = &init_task;
	for (;;) {
		if ((p = p->next_task) == &init_task)
			goto confuse_gcc1;
				/*
				 *	所有任务已扫描完毕。
				 */
		if (ticks && p->it_real_value) {
			if (p->it_real_value <= ticks) {
				send_sig(SIGALRM, p, 1);
				if (!p->it_real_incr) {
					p->it_real_value = 0;
					goto end_itimer;
				}
				do {
					p->it_real_value += p->it_real_incr;
				} while (p->it_real_value <= ticks);
			}
			p->it_real_value -= ticks;
			if (p->it_real_value < itimer_next)
				itimer_next = p->it_real_value;
		}
end_itimer:
		if (p->state != TASK_INTERRUPTIBLE)
			continue;
				/*
				 *	后面的判断只对可中断睡眠状态的任务有效，TASK_UNINTERRUPTIBLE 状态的
				 * 任务不能被信号唤醒，只能被 wake_up 函数显示唤醒。
				 */
		if (p->signal & ~p->blocked) {
			p->state = TASK_RUNNING;
			continue;
		}
				/*
				 *	任务收到了信号(有可能不止一个)，且收到的信号中存在没有被屏蔽的信号，
				 * 则置任务的状态为就绪态，使其参与系统调度并运行，任务重新运行后，操作系统
				 * 会在执行到 ret_from_sys_call 时去处理任务收到的信号。
				 */
		if (p->timeout && p->timeout <= jiffies) {
			p->timeout = 0;
			p->state = TASK_RUNNING;
		}
				/*
				 *	如果设置过任务的超时定时并且任务的超时定时已经到期，则复位超时定时值
				 * 并将任务置为就绪态使其参与系统调度并执行。
				 */
	}
confuse_gcc1:

/* this is the scheduler proper: */
#if 0
	/* give processes that go to sleep a bit higher priority.. */
	/* This depends on the values for TASK_XXX */
	/* This gives smoother scheduling for some things, but */
	/* can be very unfair under some circumstances, so.. */
 	if (TASK_UNINTERRUPTIBLE >= (unsigned) current->state &&
	    current->counter < current->priority*2) {
		++current->counter;
	}
#endif
	/*
	 *	for: 从 init_task 开始，扫描系统中除 init_task 以外的所有任务，寻找处于就绪状态
	 * 且运行时间片 counter 最大的那个任务，这个任务将是下一个要运行的任务。
	 */
	c = -1;
	next = p = &init_task;
	for (;;) {
		if ((p = p->next_task) == &init_task)
			goto confuse_gcc2;
		if (p->state == TASK_RUNNING && p->counter > c)
			c = p->counter, next = p;
	}
			/*
			 *	如果找到，则 next 指向这个任务，c 是这个任务的运行时间片 counter。如果系统中
			 * 已经没有处于就绪状态的任务，则 next 指向 init_task，c == -1，这时下一个将要运行的
			 * 任务就是init_task，也就是系统的 idle 任务。
			 */
confuse_gcc2:
	/*
	 *	if (c == 0): 表示系统中有处于就绪态的任务，但是所有处于就绪态的任务的运行时间片
	 * 都已经用完了，这时就需要为所有任务重新分配时间片了。
	 */
	if (!c) {
		for_each_task(p)
			p->counter = (p->counter >> 1) + p->priority;
	}
			/*
			 *	counter = counter/2 + priority: 新的 counter 值与任务的优先权值有关。
			 *
			 *	这里将对除 init_task 以外的所有的任务都重新设置 counter 值。这时，不在就绪
			 * 状态的这些任务的 counter 值将会变大。这样，等它们重新回到就绪态时，它们将更有可
			 * 能被优先调度执行。也就是说一个任务睡的越久，当它醒来以后的优先级就会越高。
			 *
			 *	任务的运行时间片只有在所有就绪任务的运行时间片为 0 时才会重新设置，这样，当
			 * 一个任务的运行时间片用完以后，它就要一直等，它的优先级将会变的很低。
			 *
			 *	重新设置完任务的 counter 之后，系统将调度执行 next 指向的那个任务，这时的 next
			 * 指向之前的扫描中扫到的第一个处于就绪态且 counter 值为 0 的任务。
			 */
	if(current != next)
		kstat.context_swtch++;
			/* 任务切换的次数增 1 */

	switch_to(next);
			/*
			 *	切换下一个任务执行，当前任务 current 将暂停在 switch_to 中，等再次调度执行时
			 * 将从 switch_to 中继续向下执行。next 指向下一个要执行的任务。
			 */

	/* Now maybe reload the debug registers */
	if(current->debugreg[7]){
		loaddebug(0);
		loaddebug(1);
		loaddebug(2);
		loaddebug(3);
		loaddebug(6);
	};
}

asmlinkage int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return -ERESTARTNOHAND;
}

/*
 * wake_up doesn't wake up stopped processes - they have to be awakened
 * with signals or similar.
 *
 * Note that this doesn't need cli-sti pairs: interrupts may not change
 * the wait-queue structures directly, but only call wake_up() to wake
 * a process. The process itself must remove the queue once it has woken.
 */
/*
 *	wake_up: 唤醒等待队列 *q 上的处于可中断或不可中断睡眠状态的所有任务。
 *
 *	1. 等待队列上处于停止状态的任务不能被唤醒，它们只能被信号唤醒。
 *
 *	2. 这里只是唤醒等待队列上的任务，并不会将它们从等待队列上删除。
 *
 *	3. 这里不需要使用开关中断的操作，中断中有可能会调用 wake_up，比如从块设备
 * 中读取数据结束之后在中断中调用 wake_up 唤醒等待块数据的任务。但中断中仅仅是唤醒
 * 任务，并不会更改等待队列的结构，当任务一旦被唤醒并重新运行后，就必须将自己从等待
 * 队列中删除。
 */
void wake_up(struct wait_queue **q)
{
	struct wait_queue *tmp;
	struct task_struct * p;

	if (!q || !(tmp = *q))
		return;
			/* 等待队列不存在或等待队列中没有等待的任务 */

	/*
	 *	do - while: 循环扫描等待队列中的所有任务，唤醒处于可中断睡眠状态
	 * 或不可中断睡眠状态的任务。
	 */
	do {
		if ((p = tmp->task) != NULL) {
			if ((p->state == TASK_UNINTERRUPTIBLE) ||
			    (p->state == TASK_INTERRUPTIBLE)) {
				p->state = TASK_RUNNING;
				if (p->counter > current->counter)
					need_resched = 1;
						/*
						 *	1. 将被唤醒的任务的状态置为就绪态，使其参与调度并执行。
						 *
						 *	2. 任务的 counter 值越大，表示任务的优先级越高。这里
						 * 只是将需要调度的标志置位，并不会立即产生调度，只有在检测
						 * need_resched 的地方才会重新产生调度。因此系统对高优先级
						 * 任务的响应并不是实时的。
						 *
						 *	虽然不是实时调度，但是时间也不会很长，在处理信号的
						 * 地方会检测 need_resched，而每隔 10ms 的定时器中断就会触发
						 * 一次信号处理。
						 */
			}
		}
		if (!tmp->next) {
			printk("wait_queue is bad (eip = %08lx)\n",((unsigned long *) q)[-1]);
			printk("        q = %p\n",q);
			printk("       *q = %p\n",*q);
			printk("      tmp = %p\n",tmp);
			break;
					/* 等待队列是一个循环队列，正常情况下不会出现没有下一个元素的情况 */
		}
		tmp = tmp->next;
	} while (tmp != *q);
}

/*
 *	wake_up_interruptible: 唤醒等待队列 *q 上的处于可中断睡眠状态的所有任务。
 *
 *	1. 等待队列上处于不可中断睡眠状态的任务不能被唤醒，它们只能被 wake_up 唤醒。
 *
 *	2. 等待队列上处于停止状态的任务不能被唤醒，它们只能被信号唤醒。
 *
 *	3. 这里只是唤醒等待队列上的任务，并不会将它们从等待队列上删除。任务一旦被
 * 唤醒并重新运行后，就必须将自己从等待队列中删除。
 */
void wake_up_interruptible(struct wait_queue **q)
{
	struct wait_queue *tmp;
	struct task_struct * p;

	if (!q || !(tmp = *q))
		return;
	do {
		if ((p = tmp->task) != NULL) {
			if (p->state == TASK_INTERRUPTIBLE) {
				p->state = TASK_RUNNING;
				if (p->counter > current->counter)
					need_resched = 1;
			}
		}
		if (!tmp->next) {
			printk("wait_queue is bad (eip = %08lx)\n",((unsigned long *) q)[-1]);
			printk("        q = %p\n",q);
			printk("       *q = %p\n",*q);
			printk("      tmp = %p\n",tmp);
			break;
		}
		tmp = tmp->next;
	} while (tmp != *q);
}

void __down(struct semaphore * sem)
{
	struct wait_queue wait = { current, NULL };
	add_wait_queue(&sem->wait, &wait);
	current->state = TASK_UNINTERRUPTIBLE;
	while (sem->count <= 0) {
		schedule();
		current->state = TASK_UNINTERRUPTIBLE;
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&sem->wait, &wait);
}

static inline void __sleep_on(struct wait_queue **p, int state)
{
	unsigned long flags;
	struct wait_queue wait = { current, NULL };

	if (!p)
		return;
	if (current == task[0])
		panic("task[0] trying to sleep");
	current->state = state;		/* 设置当前任务的睡眠状态 */
	add_wait_queue(p, &wait);	/* 将当前任务加入到等待队列 *p 中 */
	save_flags(flags);
	sti();				/* 开中断 ??? */
	schedule();			/* 调度新任务运行 */
	remove_wait_queue(p, &wait);	/* 当前任务被唤醒之后必须将自己从等待队列中删除 */
	restore_flags(flags);
}

/*
 *	interruptible_sleep_on: 当前任务进入可中断睡眠状态，且睡眠在 *p 表示的等待队列上。
 * 退出时表示任务已从睡眠状态中醒来并重新运行。让任务醒来的办法可以是信号唤醒，也可以是
 * 其它任务调用 wake_up_interruptible() 或 wake_up() 显示唤醒。
 *
 */
void interruptible_sleep_on(struct wait_queue **p)
{
	__sleep_on(p,TASK_INTERRUPTIBLE);
}

/*
 *	sleep_on: 当前任务进入不可中断睡眠状态，且睡眠在 *p 表示的等待队列上。
 * 退出时表示任务已从睡眠状态中醒来并重新运行。让任务醒来的唯一办法是其它任务
 * 调用 wake_up() 显示唤醒。
 */
void sleep_on(struct wait_queue **p)
{
	__sleep_on(p,TASK_UNINTERRUPTIBLE);
}

static struct timer_list * next_timer = NULL;

void add_timer(struct timer_list * timer)
{
	unsigned long flags;
	struct timer_list ** p;

	if (!timer)
		return;
	timer->next = NULL;
	p = &next_timer;
	save_flags(flags);
	cli();
	while (*p) {
		if ((*p)->expires > timer->expires) {
			(*p)->expires -= timer->expires;
			timer->next = *p;
			break;
		}
		timer->expires -= (*p)->expires;
		p = &(*p)->next;
	}
	*p = timer;
	restore_flags(flags);
}

int del_timer(struct timer_list * timer)
{
	unsigned long flags;
	unsigned long expires = 0;
	struct timer_list **p;

	p = &next_timer;
	save_flags(flags);
	cli();
	while (*p) {
		if (*p == timer) {
			if ((*p = timer->next) != NULL)
				(*p)->expires += timer->expires;
			timer->expires += expires;
			restore_flags(flags);
			return 1;
		}
		expires += (*p)->expires;
		p = &(*p)->next;
	}
	restore_flags(flags);
	return 0;
}

unsigned long timer_active = 0;
struct timer_struct timer_table[32];

/*
 * Hmm.. Changed this, as the GNU make sources (load.c) seems to
 * imply that avenrun[] is the standard name for this kind of thing.
 * Nothing else seems to be standardized: the fractional size etc
 * all seem to differ on different machines.
 */
unsigned long avenrun[3] = { 0,0,0 };

/*
 * Nr of active tasks - counted in fixed-point numbers
 */
static unsigned long count_active_tasks(void)
{
	struct task_struct **p;
	unsigned long nr = 0;

	for(p = &LAST_TASK; p > &FIRST_TASK; --p)
		if (*p && ((*p)->state == TASK_RUNNING ||
			   (*p)->state == TASK_UNINTERRUPTIBLE ||
			   (*p)->state == TASK_SWAPPING))
			nr += FIXED_1;
	return nr;
}

static inline void calc_load(void)
{
	unsigned long active_tasks; /* fixed-point */
	static int count = LOAD_FREQ;

	if (count-- > 0)
		return;
	count = LOAD_FREQ;
	active_tasks = count_active_tasks();
	CALC_LOAD(avenrun[0], EXP_1, active_tasks);
	CALC_LOAD(avenrun[1], EXP_5, active_tasks);
	CALC_LOAD(avenrun[2], EXP_15, active_tasks);
}

/*
 * this routine handles the overflow of the microsecond field
 *
 * The tricky bits of code to handle the accurate clock support
 * were provided by Dave Mills (Mills@UDEL.EDU) of NTP fame.
 * They were originally developed for SUN and DEC kernels.
 * All the kudos should go to Dave for this stuff.
 *
 * These were ported to Linux by Philip Gladstone.
 */
static void second_overflow(void)
{
	long ltemp;
	/* last time the cmos clock got updated */
	static long last_rtc_update=0;
	extern int set_rtc_mmss(unsigned long);

	/* Bump the maxerror field */
	time_maxerror = (0x70000000-time_maxerror < time_tolerance) ?
	  0x70000000 : (time_maxerror + time_tolerance);

	/* Run the PLL */
	if (time_offset < 0) {
		ltemp = (-(time_offset+1) >> (SHIFT_KG + time_constant)) + 1;
		time_adj = ltemp << (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
		time_offset += (time_adj * HZ) >> (SHIFT_SCALE - SHIFT_UPDATE);
		time_adj = - time_adj;
	} else if (time_offset > 0) {
		ltemp = ((time_offset-1) >> (SHIFT_KG + time_constant)) + 1;
		time_adj = ltemp << (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
		time_offset -= (time_adj * HZ) >> (SHIFT_SCALE - SHIFT_UPDATE);
	} else {
		time_adj = 0;
	}

	time_adj += (time_freq >> (SHIFT_KF + SHIFT_HZ - SHIFT_SCALE))
	    + FINETUNE;

	/* Handle the leap second stuff */
	switch (time_status) {
		case TIME_INS:
		/* ugly divide should be replaced */
		if (xtime.tv_sec % 86400 == 0) {
			xtime.tv_sec--; /* !! */
			time_status = TIME_OOP;
			printk("Clock: inserting leap second 23:59:60 GMT\n");
		}
		break;

		case TIME_DEL:
		/* ugly divide should be replaced */
		if (xtime.tv_sec % 86400 == 86399) {
			xtime.tv_sec++;
			time_status = TIME_OK;
			printk("Clock: deleting leap second 23:59:59 GMT\n");
		}
		break;

		case TIME_OOP:
		time_status = TIME_OK;
		break;
	}
	if (xtime.tv_sec > last_rtc_update + 660)
	  if (set_rtc_mmss(xtime.tv_sec) == 0)
	    last_rtc_update = xtime.tv_sec;
}

/*
 * disregard lost ticks for now.. We don't care enough.
 */
/*
 *	系统定时器的中断下半部处理函数。
 */
static void timer_bh(void * unused)
{
	unsigned long mask;
	struct timer_struct *tp;

	cli();
	while (next_timer && next_timer->expires == 0) {
		void (*fn)(unsigned long) = next_timer->function;
		unsigned long data = next_timer->data;
		next_timer = next_timer->next;
		sti();
		fn(data);
		cli();
	}
	sti();
	
	for (mask = 1, tp = timer_table+0 ; mask ; tp++,mask += mask) {
		if (mask > timer_active)
			break;
		if (!(mask & timer_active))
			continue;
		if (tp->expires > jiffies)
			continue;
		timer_active &= ~mask;
		tp->fn();
		sti();
	}
}

/*
 * The int argument is really a (struct pt_regs *), in case the
 * interrupt wants to know from where it was called. The timer
 * irq uses this to decide if it should update the user or system
 * times.
 */
static void do_timer(struct pt_regs * regs)
{
	unsigned long mask;
	struct timer_struct *tp;

	long ltemp;

	/* Advance the phase, once it gets to one microsecond, then
	 * advance the tick more.
	 */
	time_phase += time_adj;
	if (time_phase < -FINEUSEC) {
		ltemp = -time_phase >> SHIFT_SCALE;
		time_phase += ltemp << SHIFT_SCALE;
		xtime.tv_usec += tick + time_adjust_step - ltemp;
	}
	else if (time_phase > FINEUSEC) {
		ltemp = time_phase >> SHIFT_SCALE;
		time_phase -= ltemp << SHIFT_SCALE;
		xtime.tv_usec += tick + time_adjust_step + ltemp;
	} else
		xtime.tv_usec += tick + time_adjust_step;

	if (time_adjust)
	{
	    /* We are doing an adjtime thing. 
	     *
	     * Modify the value of the tick for next time.
	     * Note that a positive delta means we want the clock
	     * to run fast. This means that the tick should be bigger
	     *
	     * Limit the amount of the step for *next* tick to be
	     * in the range -tickadj .. +tickadj
	     */
	     if (time_adjust > tickadj)
	       time_adjust_step = tickadj;
	     else if (time_adjust < -tickadj)
	       time_adjust_step = -tickadj;
	     else
	       time_adjust_step = time_adjust;
	     
	    /* Reduce by this step the amount of time left  */
	    time_adjust -= time_adjust_step;
	}
	else
	    time_adjust_step = 0;

	if (xtime.tv_usec >= 1000000) {
	    xtime.tv_usec -= 1000000;
	    xtime.tv_sec++;
	    second_overflow();
	}

	jiffies++;
	calc_load();
	if ((VM_MASK & regs->eflags) || (3 & regs->cs)) {
		current->utime++;
		if (current != task[0]) {
			if (current->priority < 15)
				kstat.cpu_nice++;
			else
				kstat.cpu_user++;
		}
		/* Update ITIMER_VIRT for current task if not in a system call */
		if (current->it_virt_value && !(--current->it_virt_value)) {
			current->it_virt_value = current->it_virt_incr;
			send_sig(SIGVTALRM,current,1);
		}
	} else {
		current->stime++;
		if(current != task[0])
			kstat.cpu_system++;
#ifdef CONFIG_PROFILE
		if (prof_buffer && current != task[0]) {
			unsigned long eip = regs->eip;
			eip >>= 2;
			if (eip < prof_len)
				prof_buffer[eip]++;
		}
#endif
	}
	if (current == task[0] || (--current->counter)<=0) {
		current->counter=0;
		need_resched = 1;
	}
	/* Update ITIMER_PROF for the current task */
	if (current->it_prof_value && !(--current->it_prof_value)) {
		current->it_prof_value = current->it_prof_incr;
		send_sig(SIGPROF,current,1);
	}
	for (mask = 1, tp = timer_table+0 ; mask ; tp++,mask += mask) {
		if (mask > timer_active)
			break;
		if (!(mask & timer_active))
			continue;
		if (tp->expires > jiffies)
			continue;
		mark_bh(TIMER_BH);
	}
	cli();
	itimer_ticks++;
	if (itimer_ticks > itimer_next)
		need_resched = 1;
	if (next_timer) {
		if (next_timer->expires) {
			next_timer->expires--;
			if (!next_timer->expires)
				mark_bh(TIMER_BH);
		} else {
			lost_ticks++;
			mark_bh(TIMER_BH);
		}
	}
	sti();
}

asmlinkage int sys_alarm(long seconds)
{
	struct itimerval it_new, it_old;

	it_new.it_interval.tv_sec = it_new.it_interval.tv_usec = 0;
	it_new.it_value.tv_sec = seconds;
	it_new.it_value.tv_usec = 0;
	_setitimer(ITIMER_REAL, &it_new, &it_old);
	return(it_old.it_value.tv_sec + (it_old.it_value.tv_usec / 1000000));
}

asmlinkage int sys_getpid(void)
{
	return current->pid;
}

asmlinkage int sys_getppid(void)
{
	return current->p_opptr->pid;
}

asmlinkage int sys_getuid(void)
{
	return current->uid;
}

asmlinkage int sys_geteuid(void)
{
	return current->euid;
}

asmlinkage int sys_getgid(void)
{
	return current->gid;
}

asmlinkage int sys_getegid(void)
{
	return current->egid;
}

asmlinkage int sys_nice(long increment)
{
	int newprio;

	if (increment < 0 && !suser())
		return -EPERM;
	newprio = current->priority - increment;
	if (newprio < 1)
		newprio = 1;
	if (newprio > 35)
		newprio = 35;
	current->priority = newprio;
	return 0;
}

static void show_task(int nr,struct task_struct * p)
{
	static char * stat_nam[] = { "R", "S", "D", "Z", "T", "W" };

	printk("%-8s %3d ", p->comm, (p == current) ? -nr : nr);
	if (((unsigned) p->state) < sizeof(stat_nam)/sizeof(char *))
		printk(stat_nam[p->state]);
	else
		printk(" ");
	if (p == current)
		printk(" current  ");
	else
		printk(" %08lX ", ((unsigned long *)p->tss.esp)[3]);
	printk("%5lu %5d %6d ",
		p->tss.esp - p->kernel_stack_page, p->pid, p->p_pptr->pid);
	if (p->p_cptr)
		printk("%5d ", p->p_cptr->pid);
	else
		printk("      ");
	if (p->p_ysptr)
		printk("%7d", p->p_ysptr->pid);
	else
		printk("       ");
	if (p->p_osptr)
		printk(" %5d\n", p->p_osptr->pid);
	else
		printk("\n");
}

void show_state(void)
{
	int i;

	printk("                         free                        sibling\n");
	printk("  task             PC    stack   pid father child younger older\n");
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i])
			show_task(i,task[i]);
}

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	bh_base[TIMER_BH].routine = timer_bh;
			/* 初始化系统定时器的中断下半部。 */
	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");

	set_tss_desc(gdt+FIRST_TSS_ENTRY,&init_task.tss);
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&default_ldt,1);
			/*
			 *	在 GDT 表中设置任务 0 的 TSS 段和 LDT 段的段描述符。这两个描述符
			 * 用于描述任务 0 (init_task)的这两个段在线性地址空间中的基地址，段大小
			 * 等信息。
			 *
			 *	任务的 TSS 段是必须的，该段主要用于在任务切换时保存和恢复任务的
			 * 上下文信息，位于任务的 task_struct 结构中。任务的 LDT 段是非必须的，
			 * 没有 LDT 段的话，LDT 段描述符设置成 default_ldt，内容是 0，没有实际意义。
			 */

	set_system_gate(0x80,&system_call);
			/*
			 *	system_call 是系统调用的总入口，int 0x80 用于触发系统调用，系统调用
			 * 触发后，会先进入 sys_call.S 中的 _system_call 处，再执行具体的系统调用
			 * 处理函数。
			 */

	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1 ; i<NR_TASKS ; i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
			/*
			 *	将 GDT 表中除了任务 0 以外的所有任务的 TSS 段和 LDT 段的描述符全
			 * 置 0，这些描述符会在后续创建新任务的时候填充。
			 */

/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
			/*
			 *	清除 EFLAGS 中的 NT 位，这样当执行 iret 指令时就不会引起任务切换。
			 */

	load_TR(0);
	load_ldt(0);
			/*
			 *	将任务 0 的 TSS 段选择符加载到任务寄存器 TR 中，将任务 0 的 LDT
			 * 段选择符加载到局部描述符表寄存器 LDTR 中，参数 0 是任务号。手动加载
			 * TR 和 LDTR 只在初始化的时候加载一次，以后 TR 在任务切换的时候自动加载，
			 * LDTR 根据 TSS 中的 LDT 项自动加载。
			 */

	outb_p(0x34,0x43);		/* binary, mode 2, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
			/*
			 *	初始化定时器，每 10ms 发出一个定时器中断，作为操作系统的时钟节拍。
			 * 也就是操作系统的滴答(tick)，即 1 tick = 10ms。
			 */
	if (request_irq(TIMER_IRQ,(void (*)(int)) do_timer)!=0)
		panic("Could not allocate timer IRQ!");
			/*
			 *	为定时器中断绑定中断属性结构，定时器中断的处理函数为 do_timer，
			 * 定时器中断下半部的处理函数在最开始已经设置了，为 timer_bh。
			 */
}
