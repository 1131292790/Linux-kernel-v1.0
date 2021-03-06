/*
 *	linux/kernel/irq.c
 *
 *	Copyright (C) 1992 Linus Torvalds
 *
 * This file contains the code used by various IRQ handling routines:
 * asking for different IRQ's should be done through these routines
 * instead of just grabbing them. Thus setups with different IRQ numbers
 * shouldn't result in any weird surprises, and installing new handlers
 * should be easier.
 */

/*
 * IRQ's are in fact implemented a bit like signal handlers for the kernel.
 * The same sigaction struct is used, and with similar semantics (ie there
 * is a SA_INTERRUPT flag etc). Naturally it's not a 1:1 relation, but there
 * are similarities.
 *
 * sa_handler(int irq_NR) is the default function called.
 * sa_mask is 0 if nothing uses this IRQ
 * sa_flags contains various info: SA_INTERRUPT etc
 * sa_restorer is the unused
 */

#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>

#define CR0_NE 32

/*
 *	cache_21: 8 位，每一位代表主 8259A 芯片上的一个中断的请求状态: 1 表示中断请求被屏蔽，
 * 中断控制器不能发出该中断的中断请求信号。0 表示中断请求被允许，中断控制器可以发出该中断的
 * 中断请求信号。
 *
 *	cache_A1: 8 位，每一位代表从 8259A 芯片上的一个中断的请求状态，状态表示与 cache_21 一致。
 *
 *	初始时，cache_21 和 cache_A1 都是 0xFF，表示所有中断的中断请求信号处于屏蔽状态。
 */
static unsigned char cache_21 = 0xff;
static unsigned char cache_A1 = 0xff;

unsigned long intr_count = 0;	/* 中断计数，用于判断是否在中断中及中断是否被嵌套。 */
unsigned long bh_active = 0;	/* 中断下半部活跃标志，每一位代表一个中断下半部活跃。 */
unsigned long bh_mask = 0xFFFFFFFF;
struct bh_struct bh_base[32]; 	/* 每一个中断下半部对应一个 bh_struct 结构。 */

/*
 *	disable_irq: 屏蔽中断号 irq_nr 对应的外部硬件中断，使中断控制器不能发出该中断的中断请求信号，
 * 其它中断的中断请求信号不受影响，处理器也可正常响应其它中断的中断请求信号。
 */
void disable_irq(unsigned int irq_nr)
{
	unsigned long flags;
	unsigned char mask;

	mask = 1 << (irq_nr & 7);
	save_flags(flags);
	if (irq_nr < 8) {
		cli();
		cache_21 |= mask;
		outb(cache_21,0x21);
		restore_flags(flags);
		return;
	}
	cli();
	cache_A1 |= mask;
	outb(cache_A1,0xA1);
	restore_flags(flags);
}

/*
 *	enable_irq: 使能中断号 irq_nr 对应的外部硬件中断，使中断控制器可以发出该中断的中断请求信号，
 * 其它中断的中断请求信号不受影响。
 */
void enable_irq(unsigned int irq_nr)
{
	unsigned long flags;
	unsigned char mask;

	mask = ~(1 << (irq_nr & 7));
	save_flags(flags);
	if (irq_nr < 8) {
		cli();
		cache_21 &= mask;
		outb(cache_21,0x21);
		restore_flags(flags);
		return;
	}
	cli();
	cache_A1 &= mask;
	outb(cache_A1,0xA1);
	restore_flags(flags);
}

/*
 * do_bottom_half() runs at normal kernel priority: all interrupts
 * enabled.  do_bottom_half() is atomic with respect to itself: a
 * bottom_half handler need not be re-entrant.
 */
asmlinkage void do_bottom_half(void)
{
	unsigned long active;
	unsigned long mask, left;
	struct bh_struct *bh;

	bh = bh_base;
	active = bh_active & bh_mask;
	for (mask = 1, left = ~0 ; left & active ; bh++,mask += mask,left += left) {
		if (mask & active) {
			void (*fn)(void *);
			bh_active &= ~mask;
			fn = bh->routine;
			if (!fn)
				goto bad_bh;
			fn(bh->data);
		}
	}
	return;
bad_bh:
	printk ("irq.c:bad bottom half entry\n");
}

/*
 * This builds up the IRQ handler stubs using some ugly macros in irq.h
 *
 * These macros create the low-level assembly IRQ routines that do all
 * the operations that are needed to keep the AT interrupt-controller
 * happy. They are also written to be fast - and to disable interrupts
 * as little as humanly possible.
 *
 * NOTE! These macros expand to three different handlers for each line: one
 * complete handler that does all the fancy stuff (including signal handling),
 * and one fast handler that is meant for simple IRQ's that want to be
 * atomic. The specific handler is chosen depending on the SA_INTERRUPT
 * flag when installing a handler. Finally, one "bad interrupt" handler, that
 * is used when no handler is present.
 */
/*
 *	使用宏 BUILD_IRQ 实现 16 个中断的三种类型的中断处理函数，分别是普通中断、
 * 快速中断和无效中断。
 *	实现后这里每个中断都有 3 个函数，共有 48 个函数。
 */
BUILD_IRQ(FIRST,0,0x01)
BUILD_IRQ(FIRST,1,0x02)
BUILD_IRQ(FIRST,2,0x04)
BUILD_IRQ(FIRST,3,0x08)
BUILD_IRQ(FIRST,4,0x10)
BUILD_IRQ(FIRST,5,0x20)
BUILD_IRQ(FIRST,6,0x40)
BUILD_IRQ(FIRST,7,0x80)
BUILD_IRQ(SECOND,8,0x01)
BUILD_IRQ(SECOND,9,0x02)
BUILD_IRQ(SECOND,10,0x04)
BUILD_IRQ(SECOND,11,0x08)
BUILD_IRQ(SECOND,12,0x10)
BUILD_IRQ(SECOND,13,0x20)
BUILD_IRQ(SECOND,14,0x40)
BUILD_IRQ(SECOND,15,0x80)

/*
 * Pointers to the low-level handlers: first the general ones, then the
 * fast ones, then the bad ones.
 */
/*
 *	这三个数组里存放的是上面用 BUILD_IRQ 宏实现的 48 个函数的地址。
 */
static void (*interrupt[16])(void) = {
	IRQ0_interrupt, IRQ1_interrupt, IRQ2_interrupt, IRQ3_interrupt,
	IRQ4_interrupt, IRQ5_interrupt, IRQ6_interrupt, IRQ7_interrupt,
	IRQ8_interrupt, IRQ9_interrupt, IRQ10_interrupt, IRQ11_interrupt,
	IRQ12_interrupt, IRQ13_interrupt, IRQ14_interrupt, IRQ15_interrupt
};

static void (*fast_interrupt[16])(void) = {
	fast_IRQ0_interrupt, fast_IRQ1_interrupt,
	fast_IRQ2_interrupt, fast_IRQ3_interrupt,
	fast_IRQ4_interrupt, fast_IRQ5_interrupt,
	fast_IRQ6_interrupt, fast_IRQ7_interrupt,
	fast_IRQ8_interrupt, fast_IRQ9_interrupt,
	fast_IRQ10_interrupt, fast_IRQ11_interrupt,
	fast_IRQ12_interrupt, fast_IRQ13_interrupt,
	fast_IRQ14_interrupt, fast_IRQ15_interrupt
};

static void (*bad_interrupt[16])(void) = {
	bad_IRQ0_interrupt, bad_IRQ1_interrupt,
	bad_IRQ2_interrupt, bad_IRQ3_interrupt,
	bad_IRQ4_interrupt, bad_IRQ5_interrupt,
	bad_IRQ6_interrupt, bad_IRQ7_interrupt,
	bad_IRQ8_interrupt, bad_IRQ9_interrupt,
	bad_IRQ10_interrupt, bad_IRQ11_interrupt,
	bad_IRQ12_interrupt, bad_IRQ13_interrupt,
	bad_IRQ14_interrupt, bad_IRQ15_interrupt
};

/*
 * Initial irq handlers.
 */
/*
 *	16 个外设中断的中断属性结构，每个中断属性结构中:
 *
 *	sa_handler:	表示具体的中断处理函数。
 *	sa_mask:	表示中断对应的中断属性结构是否被安装。1 = 已安装，0 = 未安装。
 *	sa_flags:	表示该中断是快速中断还是普通中断。SA_INTERRUPT = 快速中断，0 = 普通中断。
 *	sa_restorer:	未使用，无效。
 */
static struct sigaction irq_sigaction[16] = {
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL }
};

/*
 * do_IRQ handles IRQ's that have been installed without the
 * SA_INTERRUPT flag: it uses the full signal-handling return
 * and runs with other interrupts enabled. All relatively slow
 * IRQ's should use this format: notably the keyboard/timer
 * routines.
 */
asmlinkage void do_IRQ(int irq, struct pt_regs * regs)
{
	struct sigaction * sa = irq + irq_sigaction;

	kstat.interrupts++;
	sa->sa_handler((int) regs);
}

/*
 * do_fast_IRQ handles IRQ's that don't need the fancy interrupt return
 * stuff - the handler is also running with interrupts disabled unless
 * it explicitly enables them later.
 */
asmlinkage void do_fast_IRQ(int irq)
{
	struct sigaction * sa = irq + irq_sigaction;

	kstat.interrupts++;
	sa->sa_handler(irq);
}

/*
 *	为中断绑定中断属性结构，传入中断编号(0 - 15)和对应的中断属性结构，
 * 同时会设置 IDT 表中该中断对应的中断门描述符，最后使能中断。
 */
int irqaction(unsigned int irq, struct sigaction * new_sa)
{
	struct sigaction * sa;
	unsigned long flags;

	if (irq > 15)
		return -EINVAL;
			/* 16 个中断编号为 0 - 15 */

	sa = irq + irq_sigaction;
	if (sa->sa_mask)
		return -EBUSY;
			/* 该中断的中断属性结构已安装，不能重复安装 */
	if (!new_sa->sa_handler)
		return -EINVAL;
			/* 中断必须要有中断处理函数 */

	save_flags(flags);
	cli();
			/* 保存 EFLAGS 并关外部中断 */

	*sa = *new_sa;
			/* 在数组 irq_sigaction 中安装中断对应的中断属性结构 */
	sa->sa_mask = 1;
			/* 设置中断属性结构已安装标志 */

	if (sa->sa_flags & SA_INTERRUPT)
		set_intr_gate(0x20+irq,fast_interrupt[irq]);
	else
		set_intr_gate(0x20+irq,interrupt[irq]);
			/* 设置 IDT 表中该中断对应的中断门描述符 */

	if (irq < 8) {
		cache_21 &= ~(1<<irq);
		outb(cache_21,0x21);
	} else {
		cache_21 &= ~(1<<2);
		cache_A1 &= ~(1<<(irq-8));
		outb(cache_21,0x21);
		outb(cache_A1,0xA1);
	}
			/* 使能中断 */

	restore_flags(flags);
			/* 恢复 EFLAGS */
	return 0;
}

/*
 *	请求中断，传入中断编号(0 - 15)和对应的中断处理函数，本质上还是为中断绑定
 * 中断属性结构。只是调用 request_irq 会将中断设置为普通中断并强制绑定。
 */
int request_irq(unsigned int irq, void (*handler)(int))
{
	struct sigaction sa;

	sa.sa_handler = handler;
	sa.sa_flags = 0;	/* 设置该中断为普通中断 */
	sa.sa_mask = 0;		/* 如果之前该中断的属性结构已经绑定过，将会强制重新绑定 */
	sa.sa_restorer = NULL;
	return irqaction(irq,&sa);
}

void free_irq(unsigned int irq)
{
	struct sigaction * sa = irq + irq_sigaction;
	unsigned long flags;

	if (irq > 15) {
		printk("Trying to free IRQ%d\n",irq);
		return;
	}
	if (!sa->sa_mask) {
		printk("Trying to free free IRQ%d\n",irq);
		return;
	}
	save_flags(flags);
	cli();
	if (irq < 8) {
		cache_21 |= 1 << irq;
		outb(cache_21,0x21);
	} else {
		cache_A1 |= 1 << (irq-8);
		outb(cache_A1,0xA1);
	}
	set_intr_gate(0x20+irq,bad_interrupt[irq]);
	sa->sa_handler = NULL;
	sa->sa_flags = 0;
	sa->sa_mask = 0;
	sa->sa_restorer = NULL;
	restore_flags(flags);
}

/*
 * Note that on a 486, we don't want to do a SIGFPE on a irq13
 * as the irq is unreliable, and exception 16 works correctly
 * (ie as explained in the intel litterature). On a 386, you
 * can't use exception 16 due to bad IBM design, so we have to
 * rely on the less exact irq13.
 *
 * Careful.. Not only is IRQ13 unreliable, but it is also
 * leads to races. IBM designers who came up with it should
 * be shot.
 */
static void math_error_irq(int cpl)
{
	outb(0,0xF0);
	if (ignore_irq13)
		return;
	math_error();
}

static void no_action(int cpl) { }

static struct sigaction ignore_IRQ = {
	no_action,
	0,
	SA_INTERRUPT,	/* 表示该中断是快速中断 */
	NULL
};

/*
 *	外设中断初始化，共 16 个中断，编号为 0 - 15，对应于 IDT 表中的中断号为 32 - 47。
 */
void init_IRQ(void)
{
	int i;

	for (i = 0; i < 16 ; i++)
		set_intr_gate(0x20+i,bad_interrupt[i]);
			/*
			 *	16 个中断先设置一个默认的中断门，后续在合适的地方重新初始化
			 * 每个中断门，默认的中断门是无效的，中断在使用前必须重新设置中断门。
			 */

	if (irqaction(2,&ignore_IRQ))
		printk("Unable to get IRQ2 for cascade\n");
			/*
			 *	为 2 号中断绑定中断属性结构，每个外设中断都会有一个中断属性
			 * 结构，中断属性结构借用的是信号属性的结构。中断在使用前必须为中断
			 * 绑定对应的中断属性结构。
			 *
			 * 	16 个中断信号是通过两个芯片级联的方式实现的，其中从芯片的 INT
			 * 引脚连接到主芯片的 IR2 引脚上，所以 2 号中断不对应某一个外设中断，
			 * 系统实际上只有 15 个有效的外设中断，因此需要将 2 号中断忽略。
			 */

	if (request_irq(13,math_error_irq))
		printk("Unable to get IRQ13 for math-error handler\n");
			/* 为 13 号中断绑定中断属性结构，13 号中断是数学协处理器中断。 */

	/* intialize the bottom half routines. */
	for (i = 0; i < 32; i++) {
		bh_base[i].routine = NULL;
		bh_base[i].data = NULL;
	}
			/* 初始化中断下半部数据机构 */
	bh_active = 0;
	intr_count = 0;
}
