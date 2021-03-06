/*
 *  linux/mm/memory.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */

/*
 * Real VM (paging to/from disk) started 18.12.91. Much more work and
 * thought has to go into this. Oh, well..
 * 19.12.91  -  works, somewhat. Sometimes I get faults, don't know why.
 *		Found it. Everything seems to work now.
 * 20.12.91  -  Ok, making the swap-device changeable like the root.
 */

#include <asm/system.h>
#include <linux/config.h>

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>

/*
 *	high_memory: 内存最高端，分页管理的内存的最高位置。
 */
unsigned long high_memory = 0;

extern unsigned long pg0[1024];		/* page table for 0-4MB for everybody */

extern void sound_mem_init(void);
extern void die_if_kernel(char *,struct pt_regs *,long);

int nr_swap_pages = 0;
/*
 *	nr_free_pages: 内存中空闲页面的数目，随着页面的申请和释放动态变化。
 */
int nr_free_pages = 0;
/*
 *	free_page_list: 空闲页面链表的起始位置，初始化后指向物理内存的最后一个页面，
 * 空闲物理内存页面最开始的 4 字节存放指向下一个空闲页面的指针，所有的空闲页面以单
 * 链表的形式链接在 free_page_list 上。
 *	空闲页面的申请和释放都在链表头部操作，这样效率最高。
 */
unsigned long free_page_list = 0;
/*
 * The secondary free_page_list is used for malloc() etc things that
 * may need pages during interrupts etc. Normal get_free_page() operations
 * don't touch it, so it stays as a kind of "panic-list", that can be
 * accessed when all other mm tricks have failed.
 */
/*
 *	secondary_page_list: 二级空闲页面链表，在这个链表上的空闲页面通常用于中断中
 * 使用页面的情况。平常获取空闲页的操作不会使用到这个链表，因此这个链表可以理解为是
 * 一种专用链表或备用链表，当所有的其它内存页面获取策略都失败时，才可以访问这个链表
 * 上的页面。
 *
 *	nr_secondary_pages: 二级空闲页面的个数。
 *
 *	刚开始系统初始化后这个链表上并没有空闲页面，因为刚开始系统中的页面是足够的，
 * 所以就没有必要预留页面。后续当有页面释放时，会优先放入到这个链表上作为预留页面。
 */
int nr_secondary_pages = 0;
unsigned long secondary_page_list = 0;

/*
 *	copy_page: 将 from 表示的页面中的内容完整的复制到 to 表示的页面中。
 */
#define copy_page(from,to) \
__asm__("cld ; rep ; movsl": :"S" (from),"D" (to),"c" (1024):"cx","di","si")

/*
 *	mem_map: 指向内存页面管理结构的起始位置。每个内存页面由 2 个字节来管理，
 * 所有管理结构从 mem_map 指向的位置处依次向后存放。
 */
unsigned short * mem_map = NULL;

#define CODE_SPACE(addr,p) ((addr) < (p)->end_code)

/*
 * oom() prints a message (so that the user knows why the process died),
 * and gives the process an untrappable SIGSEGV.
 */
/*
 *	oom: 内存不足。入参 task 表示内存不足时将要杀死的那个任务。
 */
void oom(struct task_struct * task)
{
	printk("\nout of memory\n");
	task->sigaction[SIGKILL-1].sa_handler = NULL;
	task->blocked &= ~(1<<(SIGKILL-1));
	send_sig(SIGKILL,task,1);
			/*
			 *	解除任务 task 对 SIGKILL 信号的屏蔽并向 task 发送 SIGKILL 信号杀死 task。
			 */
}

/*
 *	free_one_table: 释放一个页表及页表项指向的所有物理内存页面。
 *
 *	入参: 指向该页表对应的页目录项的指针。
 */
static void free_one_table(unsigned long * page_dir)
{
	int j;
	unsigned long pg_table = *page_dir;
			/*
			 *	获取页目录项的值。页目录项的高 20 bit 是该页目录项指向的页表的基地址
			 * 低 12 bit 是页表的属性。
			 */
	unsigned long * page_table;

	if (!pg_table)
		return;
			/*
			 *	页目录项的值为空，说明该页目录项没有对应的页表存在。
			 */
	*page_dir = 0;
			/*
			 *	将页目录表中的页目录项的值清空，断开页目录项与页表的连接。
			 */
	if (pg_table >= high_memory || !(pg_table & PAGE_PRESENT)) {
		printk("Bad page table: [%p]=%08lx\n",page_dir,pg_table);
		return;
	}
			/*
			 *	要释放的页表已经超出了内存最高端，或者页表的属性显示页表不存在，这种
			 * 状态下的页表是一个异常的页表，无法释放。
			 */
	if (mem_map[MAP_NR(pg_table)] & MAP_PAGE_RESERVED)
		return;
			/*
			 *	页表所占用的物理内存页面是系统保留的页面，这种类型的页面不能释放。
			 */

	page_table = (unsigned long *) (pg_table & PAGE_MASK);
			/*
			 *	page_table: 指向页表中的第一个页表项。
			 */
	/*
	 *	for: 循环扫描一个页表中的所有页表项，对每一个页表项，释放该页表项
	 * 指向的物理内存页面。
	 */
	for (j = 0 ; j < PTRS_PER_PAGE ; j++,page_table++) {
		unsigned long pg = *page_table;
				/*
				 *	获取页表项的值，页表项的高 20 bit 是该页表项指向的物理内存
				 * 页面的基地址，低 12 bit 是物理内存页面的属性。
				 */
		if (!pg)
			continue;
				/*
				 *	页表项的值为 0，说明该页表项没有指向的物理内存页面存在，则
				 * 继续扫描下一个页表项。
				 */
		*page_table = 0;
				/*
				 *	将页表中的页表项的值清空，断开页表项与物理内存页面的连接。
				 */
		if (pg & PAGE_PRESENT)
			free_page(PAGE_MASK & pg);
		else
			swap_free(pg);
				/*
				 *	如果物理内存页面的属性显示物理内存页面存在，则直接释放该页面，
				 * 否则说明该页表项指向的物理内存页面位于交换设备中，则从交换设备中
				 * 释放该页面。
				 */
	}
	free_page(PAGE_MASK & pg_table);
			/*
			 *	页表中的所有页表项指向的物理内存页面都已经释放完了，最后释放页表占用
			 * 的物理内存页面。
			 */
}

/*
 * This function clears all user-level page tables of a process - this
 * is needed by execve(), so that old pages aren't in the way. Note that
 * unlike 'free_page_tables()', this function still leaves a valid
 * page-table-tree in memory: it just removes the user pages. The two
 * functions are similar, but there is a fundamental difference.
 */
/*
 *	clear_page_tables: 清除任务的用户级页表。释放一个任务的用户空间对应的页表及页表项
 * 指向的物理内存页面，保留任务的页目录表、内核空间对应的页表及内核空间所占用的物理内存页面。
 *
 *	本函数是 execve() 所必需的，与 free_page_tables 不同，本函数执行完后，任务在内存中
 * 仍然保留一个有效的页表树，只是用户空间的那部分页表及页面不存在了。free_page_tables 执行
 * 完后，任务的整个页表树将不复存在。
 *
 *	入参: 指向要清除页表的那个任务的 task_struct 结构的指针。
 */
void clear_page_tables(struct task_struct * tsk)
{
	int i;
	unsigned long pg_dir;
	unsigned long * page_dir;

	if (!tsk)
		return;
	if (tsk == task[0])
		panic("task[0] (swapper) doesn't support exec()\n");
			/*
			 *	任务 0 中是不能通过 exec 函数来加载执行新程序的。
			 */
	pg_dir = tsk->tss.cr3;
			/*
			 *	从任务的 TSS 段中获取任务的页目录表所在物理内存页面的基地址。
			 */
	page_dir = (unsigned long *) pg_dir;
			/*
			 *	page_dir: 指向页目录表，也表示指向页目录表中的第一个页目录项。
			 */
	if (!page_dir || page_dir == swapper_pg_dir) {
		printk("Trying to clear kernel page-directory: not good\n");
		return;
	}
			/*
			 *	任务没有页目录表，或任务的页目录表是内核页目录表，而内核的页目录表
			 * 是不允许被释放的。
			 */

	/*
	 *	if: 页目录表所在物理内存页面的引用计数大于 1，说明还有其它任务在
	 * 使用这个页目录表。这时页目录表的用户空间的页表及用户空间的物理内存页面
	 * 的映射关系还不能解除，它们所占用的物理内存页面还不能释放，否则会影响到
	 * 其它任务。
	 *
	 *	但是任务 tsk 的用户空间的页表及物理内存页面必须清除，因此只需为
	 * 任务 tsk 重新设置一个页目录表即可。
	 */
	if (mem_map[MAP_NR(pg_dir)] > 1) {
		unsigned long * new_pg;

		if (!(new_pg = (unsigned long*) get_free_page(GFP_KERNEL))) {
			oom(tsk);
			return;
		}
				/*
				 *	重新申请一页空闲内存用于任务 tsk 的新页目录表。
				 */
		for (i = 768 ; i < 1024 ; i++)
			new_pg[i] = page_dir[i];
				/*
				 *	将任务 tsk 的原页目录表中内核空间的页目录项复制到新页目录表中。
				 * 这样，任务 tsk 的内核空间的映射关系将与原来保持一致。
				 */
		free_page(pg_dir);
		tsk->tss.cr3 = (unsigned long) new_pg;
		return;
				/*
				 *	释放任务 tsk 的原页目录表所占用的物理内存页面，这种情况下 free_page
				 * 中会将页面的引用计数减 1 并直接返回，并不会真正释放物理内存页面。
				 *
				 *	在任务 tsk 的 TSS 段中重新设置任务 tsk 的页目录表。新页目录表的用户
				 * 空间的页目录项全是 0，用户空间此时没有任何映射关系存在，故直接返回。
				 */
	}

	/*
	 *	for: 循环扫描任务 tsk 的页目录表的用户空间对应的页目录项，对每一个
	 * 用户空间的页目录项，释放该页目录项指向的页表及页表项指向的物理内存页面。
	 */
	for (i = 0 ; i < 768 ; i++,page_dir++)
		free_one_table(page_dir);
	invalidate();
			/* 刷新 TLB */
	return;
}

/*
 * This function frees up all page tables of a process when it exits.
 */
/*
 *	free_page_tables: 释放页表，释放一个任务所占用的页目录表、页表及页表项指向的物理内存页面。
 *
 *	入参: 指向要释放页表的那个任务的 task_struct 结构的指针。
 */
void free_page_tables(struct task_struct * tsk)
{
	int i;
	unsigned long pg_dir;
	unsigned long * page_dir;

	if (!tsk)
		return;
	if (tsk == task[0]) {
		printk("task[0] (swapper) killed: unable to recover\n");
		panic("Trying to free up swapper memory space");
	}
			/*
			 *	任务 0 是系统的 idle 任务，这个任务是系统中必须存在的一个任务，
			 * 故不允许释放任务 0 的页表。
			 */
	pg_dir = tsk->tss.cr3;
			/*
			 *	从任务的 TSS 段中获取任务的页目录表所在物理内存页面的基地址。
			 */
	if (!pg_dir || pg_dir == (unsigned long) swapper_pg_dir) {
		printk("Trying to free kernel page-directory: not good\n");
		return;
	}
			/*
			 *	任务没有页目录表，或任务的页目录表是内核页目录表，而内核的页目录表
			 * 是不允许被释放的。
			 */
	tsk->tss.cr3 = (unsigned long) swapper_pg_dir;
			/*
			 *	任务的页目录表要释放了，所以任务的 TSS 段中不能再保存一个已经不存在
			 * 的页目录表，那就用内核的页目录表来代替吧，这里将解除任务与原页目录表的
			 * 连接关系。
			 */
	if (tsk == current)
		__asm__ __volatile__("movl %0,%%cr3": :"a" (tsk->tss.cr3));
			/*
			 *	当前正在运行的任务自己释放自己的页表，这时因为当前任务的页目录表在
			 * 上面已经重新设置为内核的页目录表了，所以需要给 CR3 重新加载当前任务的新
			 * 页目录表，这个重新加载动作会刷新 TLB。
			 *
			 *	这里需要重新加载的原因: 释放当前任务页表的代码及后续的代码都在当前
			 * 任务的内核空间中执行，这些代码的正常执行需要一个正确的页目录表，而所有的
			 * 任务共用内核空间，因此所有任务的内核空间的映射关系是完全一致的，所以这里
			 * 可以直接用内核空间的页目录表。
			 */
	if (mem_map[MAP_NR(pg_dir)] > 1) {
		free_page(pg_dir);
		return;
	}
			/*
			 *	页目录表所在物理内存页面的引用计数大于 1，说明还有其它任务在使用这个
			 * 页目录表。这时页目录表、页表及物理内存页面的映射关系还不能解除，它们所占用
			 * 的物理内存页面还不能释放，否则会影响到其它任务。
			 *
			 *	因为这个任务与页目录表的连接关系在前面已经解除了，因此这里直接释放
			 * 页目录表所在物理内存页面即可，在这种情况下 free_page 中会将页面的引用计数
			 * 减 1 并直接返回，并不会真正释放物理内存页面。
			 */

	page_dir = (unsigned long *) pg_dir;
			/*
			 *	page_dir: 指向页目录表中的第一个页目录项。
			 */
	/*
	 *	for: 循环扫描页目录表中的所有页目录项，对每一个页目录项，释放该
	 * 页目录项指向的页表及页表项指向的物理内存页面。
	 */
	for (i = 0 ; i < PTRS_PER_PAGE ; i++,page_dir++)
		free_one_table(page_dir);
	free_page(pg_dir);
			/*
			 *	所有的页表及页表项指向的物理内存页面都释放完了，最后释放页目录表
			 * 所占用的物理内存页面。
			 */
	invalidate();
			/* 刷新 TLB */
}

/*
 * clone_page_tables() clones the page table for a process - both
 * processes will have the exact same pages in memory. There are
 * probably races in the memory management with cloning, but we'll
 * see..
 */
/*
 *	clone_page_tables: 克隆页表，实际上是克隆第一级页表，也就是页目录表，克隆
 * 不是复制，克隆是让两个任务使用同一个页目录表。页目录表相同，也就意味着页表相同，
 * 线性地址与物理地址的映射关系也相同，最终使得两个任务的物理地址空间也相同。
 *
 *	因此，父任务通过克隆的方式创建子任务时，父任务和子任务共用同一个线性地址空间，
 * 共用同一套页表，共用同一个物理地址空间，即共用同一套物理内存页面。但是父任务和子
 * 任务分别有自己的 task_struct 结构和内核态栈，task_struct 结构和内核态栈所占用的
 * 内存页面是相互独立的。
 *
 *	用这种方式创建出来的子任务，称之为子线程。
 *
 *	入参: 指向子任务的 task_struct 结构的指针。
 */
int clone_page_tables(struct task_struct * tsk)
{
	unsigned long pg_dir;

	pg_dir = current->tss.cr3;
			/*
			 *	从当前正在运行的任务(父任务)的 TSS 段中获取其页目录表所在物理
			 * 内存页面的基地址。
			 */
	mem_map[MAP_NR(pg_dir)]++;
			/*
			 *	子任务也将使用这个页目录表，因此需要将页目录表所在物理内存页面
			 * 的引用计数增 1。
			 */
	tsk->tss.cr3 = pg_dir;
			/*
			 *	在子任务的 TSS 段中设置子任务的页目录表基地址，使其指向父任务
			 * 的页目录表。
			 */
	return 0;
}

/*
 * copy_page_tables() just copies the whole process memory range:
 * note the special handling of RESERVED (ie kernel) pages, which
 * means that they are always shared by all processes.
 */
/*
 *	copy_page_tables: 复制页表，为子任务复制父任务的页目录表及页表，页表项指向的物理内存页面将
 * 采用写时复制的方式。
 *
 *	复制不是克隆，复制是将父任务的线性地址空间完整的复制一份给子任务，而不是父任务和子任务共用
 * 同一个线性地址空间。因此，刚复制完之后，子任务就是父任务的一个一模一样的副本。但是父任务和子任务
 * 拥有自己独立的页目录表及页表，也有意味着它们有相互独立的线性地址与物理地址的映射关系，最终使得
 * 两个任务有相互独立的物理地址空间。
 *
 *	因为使用了写时复制技术，故刚开始复制之后，虽然父任务和子任务是两个相互独立的线性地址空间，
 * 但是它们暂时会映射到同一个物理地址空间上。随着时间的推移，通过写时复制，父任务和子任务的物理地址
 * 空间将逐渐的分离开来。
 *
 *	因此，父任务通过复制的方式创建子任务时，父任务和子任务有相互独立的线性地址空间，有相互
 * 独立的页目录表及页表，最终使得它们拥有相互独立的物理内存空间。父任务和子任务只有在刚创建的
 * 时候是在同一个点上，之后它们将向不同的分支相互独立的继续向前发展。同时，父任务和子任务分别
 * 有自己的 task_struct 结构和内核态栈，task_struct 结构和内核态栈所占用的内存页面是相互独立的。
 *
 *	用这种方式创建出来的子任务，称之为子进程。
 *
 *	入参: 指向子任务的 task_struct 结构的指针。
 *
 *
 *	页目录表、页表、物理内存页面的关系如下，每个页目录表和页表都占用一页物理内存页面，页目录项和
 * 页表项具有相同的格式(head.S 中)。高 20 bit 是页面地址，低 12 bit 是页面属性。页目录项的高 20 bit
 * 保存的是该页目录项指向的页表的基地址，页表项的高 20 bit 保存的是该页表项指向的物理内存页面的基地址。
 * 页面属性中各标志的含义及线性地址到物理地址变换的过程在 head.S 中。
 *
 *	页目录表(4KB):			页表(4KB):			物理内存页面(4KB):
 *
 *	+---------------+	/====>	+---------------+	  /==>	+---------------+
 *	|   页目录项	|  ====/	|    页表项	|	 /	|		|
 *	+---------------+		+---------------+	/	|		|
 *	|  ..........	|		|  ..........	|      /	|		|
 *	+---------------+		+---------------+     /		|		|
 *	|   页目录项	|		|    页表项	|  ==/		|		|
 *	+---------------+		+---------------+		|		|
 *	|  ..........	|		|  ..........	|		|		|
 *	+---------------+		+---------------+		+---------------+
 *
 *
 *					+---------------+	/====>	+---------------+
 *					|    页表项	|  ====/	|		|
 *					+---------------+		|		|
 *					|  ..........	|		|		|
 *					+---------------+		|		|
 *					|    页表项	|		|		|
 *					+---------------+		|		|
 *					|  ..........	|		|		|
 *					+---------------+		+---------------+
 */
int copy_page_tables(struct task_struct * tsk)
{
	int i;
	unsigned long old_pg_dir, *old_page_dir;
	unsigned long new_pg_dir, *new_page_dir;

	if (!(new_pg_dir = get_free_page(GFP_KERNEL)))
		return -ENOMEM;
			/*
			 *	申请一页空闲内存页面用于存放子任务的页目录表，子任务页目录表中的内容后续
			 * 将会从父任务的页目录表中复制。
			 */
	old_pg_dir = current->tss.cr3;
	tsk->tss.cr3 = new_pg_dir;
			/*
			 *	获取父任务的页目录表所在物理内存页面的基地址。
			 *	设置子任务的页目录表所在物理内存页面的基地址。
			 */
	old_page_dir = (unsigned long *) old_pg_dir;
	new_page_dir = (unsigned long *) new_pg_dir;
			/*
			 *	old_page_dir: 指向父任务的页目录表的第一个页目录项。
			 *	new_page_dir: 指向子任务的页目录表的第一个页目录项。
			 */

	/*
	 *	for: 循环扫描父任务页目录表中的所有页目录项。对每一个页目录项，复制页目录项
	 * 指向的页表及页表中的页表项指向的物理内存页面给子任务，复制完后，为子任务建立其
	 * 页目录项和页表的映射关系。
	 */
	for (i = 0 ; i < PTRS_PER_PAGE ; i++,old_page_dir++,new_page_dir++) {
		int j;
		unsigned long old_pg_table, *old_page_table;
		unsigned long new_pg_table, *new_page_table;

		old_pg_table = *old_page_dir;
				/*
				 *	获取父任务页目录表中的页目录项的值，页目录项的高 20 bit 是该页目录
				 * 项指向的页表的基地址，低 12 bit 是页表的属性。
				 */
		if (!old_pg_table)
			continue;
				/*
				 *	页目录项的值为 0，则表示该页目录项没有对应的页表存在，则继续获取下
				 * 一个页目录项。否则继续向下，处理页目录项指向的页表。
				 */
		if (old_pg_table >= high_memory || !(old_pg_table & PAGE_PRESENT)) {
			printk("copy_page_tables: bad page table: "
				"probable memory corruption");
			*old_page_dir = 0;
			continue;
		}
				/*
				 *	页目录项指向的页表已经超出了内存最高端，或者页目录项中页表的属性显示
				 * 页表不存在，这种状态下的页目录项是一个异常的页目录项，所以在此处将该页目录
				 * 项的内容清空，并重新获取下一个页目录项。
				 */
		if (mem_map[MAP_NR(old_pg_table)] & MAP_PAGE_RESERVED) {
			*new_page_dir = old_pg_table;
			continue;
		}
				/*
				 *	页目录项指向的页表所在的物理内存页面是系统保留的内存页面，这种类型的
				 * 页表，没有任务会去更改页表中的页表项。因此，子任务将不会复制这种类型的页表，
				 * 只是简单的让子任务的页目录项指向父任务的页表，子任务和父任务共用同一个页表。
				 *
				 *	否则，子任务就需要复制父任务的页表。
				 */

		if (!(new_pg_table = get_free_page(GFP_KERNEL))) {
			free_page_tables(tsk);
			return -ENOMEM;
		}
				/*
				 *	申请一页空闲内存页面用于存放子任务的一个页表，页表中的内容后续将会从
				 * 父任务对应的页表中复制。
				 *
				 *	如果已经分配不出空闲的内存页面，则将本次 copy_page_tables 中之前已经
				 * 复制的表全部释放并退出。
				 */
		old_page_table = (unsigned long *) (PAGE_MASK & old_pg_table);
		new_page_table = (unsigned long *) (PAGE_MASK & new_pg_table);
				/*
				 *	old_page_table: 指向父任务的一个页表中的第一个页表项。
				 *	new_page_table: 指向子任务的一个页表中的第一个页表项。
				 */

		/*
		 *	for: 扫描父任务一个页表中的所有页表项，用写时复制的方式将所有的页表项
		 * 及页表项指向的物理内存页面复制给子任务，同时建立子任务的页表项与物理内存
		 * 页面的映射关系。
		 */
		for (j = 0 ; j < PTRS_PER_PAGE ; j++,old_page_table++,new_page_table++) {
			unsigned long pg;
			pg = *old_page_table;
					/*
					 *	获取父任务一个页表中的页表项的值，页表项的高 20 bit 是该页表项
					 * 指向的物理内存页面的基地址，低 12 bit 是物理内存页面的属性。
					 */
			if (!pg)
				continue;
					/*
					 *	页表项的值为 0，则表示该页表项没有对应的物理内存页面存在，则
					 * 继续获取下一个页表项。
					 */
			if (!(pg & PAGE_PRESENT)) {
				*new_page_table = swap_duplicate(pg);
				continue;
			}
					/*
					 *	页表项中物理内存页面的属性显示物理内存页面不存在，但是物理内存
					 * 页面的地址是存在的。这种情况下，该物理内存页面是被交换出去了，位于
					 * 交换设备上。
					 *
					 *	TODO:
					 */
			if ((pg & (PAGE_RW | PAGE_COW)) == (PAGE_RW | PAGE_COW))
				pg &= ~PAGE_RW;
					/*
					 *	如果物理内存页面的属性值为可读写，则将其值设置为只读。
					 */
			*new_page_table = pg;
					/*
					 *	设置子任务的页表项，使其与父任务的页表项指向同一个物理内存
					 * 页面，且子任务对该页面的访问属性为只读。
					 */
			if (mem_map[MAP_NR(pg)] & MAP_PAGE_RESERVED)
				continue;
					/*
					 *	如果该页面是系统保留的内存页面，则父任务对该页面也只有只读
					 * 属性，且保留页面的引用计数也不需要增 1，这种情况下后续的代码就不
					 * 需要再执行了，直接处理下一个页表项即可。
					 */
			*old_page_table = pg;
					/*
					 *	更新父任务的页表项，使父任务对该页表项指向的物理内存页面的
					 * 访问属性变为只读。
					 */
			mem_map[MAP_NR(pg)]++;
					/*
					 *	子任务现在也使用该物理内存页面，故需将该物理内存页面的访问
					 * 计数增 1。
					 *
					 *	此时，父任务和子任务的页表项都指向同一个物理内存页面，父任务
					 * 和子任务中相同的线性地址会转换出相同的物理地址，所以它们将访问同一
					 * 个物理内存页面。
					 *
					 *	但是，父任务和子任务对该物理内存页面都只有读权限，没有写权限，
					 * 如果父任务和子任务不对该内存页面进行写操作，而只是读操作的话，那么
					 * 它们将永远共享这一个物理内存页面。
					 *
					 *	当父任务或子任务任何一个对该页面进行写操作时，因为没有写权限，
					 * 所以就会产生页写保护异常，进而执行页异常处理函数 page_fault。
					 *
					 *	异常处理函数中，操作系统会为执行写操作的那个任务重新分配一个
					 * 物理内存页面，然后将该页面中的内容全部复制到新分配的页面中，并重新
					 * 设置父任务和子任务的页表项，让执行写操作的那个任务的页表项指向新
					 * 分配的那个物理内存页面，原来共享的那个内存页面留给另一个任务使用，
					 * 并解除页面的写保护。
					 *
					 *	这时，父任务和子任务就会有各自独立的物理内存页面，且各自对各自
					 * 的页面具有读写权限。当异常处理退出时，执行写操作的那个任务的写操作
					 * 将会重新执行，此时，这个任务已经有了自己独立的页面，且具有可写权限，
					 * 写操作将执行成功。
					 *
					 *	这个机制就是写时复制机制，即任务在开始创建的时候只复制页目录
					 * 表和页表，而不复制物理内存页面，只是让父任务和子任务共享相同的物理
					 * 内存页面，但是父任务和子任务对该页面的访问属性都变成了只读。当有
					 * 一方需要执行写操作时，才去复制物理内存页面，然后才执行真正的写操作。
					 *
					 *	写时复制，将内存页面的复制工作推迟到有写操作时才进行，这样使得
					 * 任务创建的速度就变得很快。另外，对于那些永远不会写的页面，比如代码
					 * 所占的页面，父任务和子任务将永远共享这些页面，这样也会减小任务对于
					 * 物理内存页面的使用量。
					 */
		}

		*new_page_dir = new_pg_table | PAGE_TABLE;
				/*
				 *	操作系统将父任务的一个页表复制给子任务之后，建立子任务的页目录项与
				 * 新复制的页表的映射关系。
				 */
	}

	invalidate();
			/*
			 *	页目录表及页表的复制工作已经完成，因为父任务对物理内存页面的访问属性有改变
			 * (可读写 -> 只读)，所以需要刷新 TLB，使 TLB 中缓存的父任务的线性地址到物理地址的
			 * 映射关系失效。
			 */
	return 0;
}

/*
 * a more complete version of free_page_tables which performs with page
 * granularity.
 */
int unmap_page_range(unsigned long from, unsigned long size)
{
	unsigned long page, page_dir;
	unsigned long *page_table, *dir;
	unsigned long poff, pcnt, pc;

	if (from & ~PAGE_MASK) {
		printk("unmap_page_range called with wrong alignment\n");
		return -EINVAL;
	}
	size = (size + ~PAGE_MASK) >> PAGE_SHIFT;
	dir = PAGE_DIR_OFFSET(current->tss.cr3,from);
	poff = (from >> PAGE_SHIFT) & (PTRS_PER_PAGE-1);
	if ((pcnt = PTRS_PER_PAGE - poff) > size)
		pcnt = size;

	for ( ; size > 0; ++dir, size -= pcnt,
	     pcnt = (size > PTRS_PER_PAGE ? PTRS_PER_PAGE : size)) {
		if (!(page_dir = *dir))	{
			poff = 0;
			continue;
		}
		if (!(page_dir & PAGE_PRESENT)) {
			printk("unmap_page_range: bad page directory.");
			continue;
		}
		page_table = (unsigned long *)(PAGE_MASK & page_dir);
		if (poff) {
			page_table += poff;
			poff = 0;
		}
		for (pc = pcnt; pc--; page_table++) {
			if ((page = *page_table) != 0) {
				*page_table = 0;
				if (1 & page) {
					if (!(mem_map[MAP_NR(page)] & MAP_PAGE_RESERVED))
						if (current->rss > 0)
							--current->rss;
					free_page(PAGE_MASK & page);
				} else
					swap_free(page);
			}
		}
		if (pcnt == PTRS_PER_PAGE) {
			*dir = 0;
			free_page(PAGE_MASK & page_dir);
		}
	}
	invalidate();
	return 0;
}

int zeromap_page_range(unsigned long from, unsigned long size, int mask)
{
	unsigned long *page_table, *dir;
	unsigned long poff, pcnt;
	unsigned long page;

	if (mask) {
		if ((mask & (PAGE_MASK|PAGE_PRESENT)) != PAGE_PRESENT) {
			printk("zeromap_page_range: mask = %08x\n",mask);
			return -EINVAL;
		}
		mask |= ZERO_PAGE;
	}
	if (from & ~PAGE_MASK) {
		printk("zeromap_page_range: from = %08lx\n",from);
		return -EINVAL;
	}
	dir = PAGE_DIR_OFFSET(current->tss.cr3,from);
	size = (size + ~PAGE_MASK) >> PAGE_SHIFT;
	poff = (from >> PAGE_SHIFT) & (PTRS_PER_PAGE-1);
	if ((pcnt = PTRS_PER_PAGE - poff) > size)
		pcnt = size;

	while (size > 0) {
		if (!(PAGE_PRESENT & *dir)) {
				/* clear page needed here?  SRB. */
			if (!(page_table = (unsigned long*) get_free_page(GFP_KERNEL))) {
				invalidate();
				return -ENOMEM;
			}
			if (PAGE_PRESENT & *dir) {
				free_page((unsigned long) page_table);
				page_table = (unsigned long *)(PAGE_MASK & *dir++);
			} else
				*dir++ = ((unsigned long) page_table) | PAGE_TABLE;
		} else
			page_table = (unsigned long *)(PAGE_MASK & *dir++);
		page_table += poff;
		poff = 0;
		for (size -= pcnt; pcnt-- ;) {
			if ((page = *page_table) != 0) {
				*page_table = 0;
				if (page & PAGE_PRESENT) {
					if (!(mem_map[MAP_NR(page)] & MAP_PAGE_RESERVED))
						if (current->rss > 0)
							--current->rss;
					free_page(PAGE_MASK & page);
				} else
					swap_free(page);
			}
			*page_table++ = mask;
		}
		pcnt = (size > PTRS_PER_PAGE ? PTRS_PER_PAGE : size);
	}
	invalidate();
	return 0;
}

/*
 * maps a range of physical memory into the requested pages. the old
 * mappings are removed. any references to nonexistent pages results
 * in null mappings (currently treated as "copy-on-access")
 */
int remap_page_range(unsigned long from, unsigned long to, unsigned long size, int mask)
{
	unsigned long *page_table, *dir;
	unsigned long poff, pcnt;
	unsigned long page;

	if (mask) {
		if ((mask & (PAGE_MASK|PAGE_PRESENT)) != PAGE_PRESENT) {
			printk("remap_page_range: mask = %08x\n",mask);
			return -EINVAL;
		}
	}
	if ((from & ~PAGE_MASK) || (to & ~PAGE_MASK)) {
		printk("remap_page_range: from = %08lx, to=%08lx\n",from,to);
		return -EINVAL;
	}
	dir = PAGE_DIR_OFFSET(current->tss.cr3,from);
	size = (size + ~PAGE_MASK) >> PAGE_SHIFT;
	poff = (from >> PAGE_SHIFT) & (PTRS_PER_PAGE-1);
	if ((pcnt = PTRS_PER_PAGE - poff) > size)
		pcnt = size;

	while (size > 0) {
		if (!(PAGE_PRESENT & *dir)) {
			/* clearing page here, needed?  SRB. */
			if (!(page_table = (unsigned long*) get_free_page(GFP_KERNEL))) {
				invalidate();
				return -1;
			}
			*dir++ = ((unsigned long) page_table) | PAGE_TABLE;
		}
		else
			page_table = (unsigned long *)(PAGE_MASK & *dir++);
		if (poff) {
			page_table += poff;
			poff = 0;
		}

		for (size -= pcnt; pcnt-- ;) {
			if ((page = *page_table) != 0) {
				*page_table = 0;
				if (PAGE_PRESENT & page) {
					if (!(mem_map[MAP_NR(page)] & MAP_PAGE_RESERVED))
						if (current->rss > 0)
							--current->rss;
					free_page(PAGE_MASK & page);
				} else
					swap_free(page);
			}

			/*
			 * the first condition should return an invalid access
			 * when the page is referenced. current assumptions
			 * cause it to be treated as demand allocation in some
			 * cases.
			 */
			if (!mask)
				*page_table++ = 0;	/* not present */
			else if (to >= high_memory)
				*page_table++ = (to | mask);
			else if (!mem_map[MAP_NR(to)])
				*page_table++ = 0;	/* not present */
			else {
				*page_table++ = (to | mask);
				if (!(mem_map[MAP_NR(to)] & MAP_PAGE_RESERVED)) {
					++current->rss;
					mem_map[MAP_NR(to)]++;
				}
			}
			to += PAGE_SIZE;
		}
		pcnt = (size > PTRS_PER_PAGE ? PTRS_PER_PAGE : size);
	}
	invalidate();
	return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 */
unsigned long put_page(struct task_struct * tsk,unsigned long page,
	unsigned long address,int prot)
{
	unsigned long *page_table;

	if ((prot & (PAGE_MASK|PAGE_PRESENT)) != PAGE_PRESENT)
		printk("put_page: prot = %08x\n",prot);
	if (page >= high_memory) {
		printk("put_page: trying to put page %08lx at %08lx\n",page,address);
		return 0;
	}
	page_table = PAGE_DIR_OFFSET(tsk->tss.cr3,address);
	if ((*page_table) & PAGE_PRESENT)
		page_table = (unsigned long *) (PAGE_MASK & *page_table);
	else {
		printk("put_page: bad page directory entry\n");
		oom(tsk);
		*page_table = BAD_PAGETABLE | PAGE_TABLE;
		return 0;
	}
	page_table += (address >> PAGE_SHIFT) & (PTRS_PER_PAGE-1);
	if (*page_table) {
		printk("put_page: page already exists\n");
		*page_table = 0;
		invalidate();
	}
	*page_table = page | prot;
/* no need for invalidate */
	return page;
}

/*
 * The previous function doesn't work very well if you also want to mark
 * the page dirty: exec.c wants this, as it has earlier changed the page,
 * and we want the dirty-status to be correct (for VM). Thus the same
 * routine, but this time we mark it dirty too.
 */
unsigned long put_dirty_page(struct task_struct * tsk, unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

	if (page >= high_memory)
		printk("put_dirty_page: trying to put page %08lx at %08lx\n",page,address);
	if (mem_map[MAP_NR(page)] != 1)
		printk("mem_map disagrees with %08lx at %08lx\n",page,address);
	page_table = PAGE_DIR_OFFSET(tsk->tss.cr3,address);
	if (PAGE_PRESENT & *page_table)
		page_table = (unsigned long *) (PAGE_MASK & *page_table);
	else {
		if (!(tmp = get_free_page(GFP_KERNEL)))
			return 0;
		if (PAGE_PRESENT & *page_table) {
			free_page(tmp);
			page_table = (unsigned long *) (PAGE_MASK & *page_table);
		} else {
			*page_table = tmp | PAGE_TABLE;
			page_table = (unsigned long *) tmp;
		}
	}
	page_table += (address >> PAGE_SHIFT) & (PTRS_PER_PAGE-1);
	if (*page_table) {
		printk("put_dirty_page: page already exists\n");
		*page_table = 0;
		invalidate();
	}
	*page_table = page | (PAGE_DIRTY | PAGE_PRIVATE);
/* no need for invalidate */
	return page;
}

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * Note that we do many checks twice (look at do_wp_page()), as
 * we have to be careful about race-conditions.
 *
 * Goto-purists beware: the only reason for goto's here is that it results
 * in better assembly code.. The "default" path will see no jumps at all.
 */
/*
 *	__do_wp_page: 解除页面写保护。
 *
 *	入参:
 *	error_code --- 引起页异常的错误码，未使用。
 *	address --- 引起页异常的线性地址。
 *	tsk --- 引起页异常的任务。
 *	user_esp --- 未使用。
 */
static void __do_wp_page(unsigned long error_code, unsigned long address,
	struct task_struct * tsk, unsigned long user_esp)
{
	unsigned long *pde, pte, old_page, prot;
	unsigned long new_page;

	new_page = __get_free_page(GFP_KERNEL);
			/*
			 *	申请一页空闲内存页面。
			 */
	pde = PAGE_DIR_OFFSET(tsk->tss.cr3,address);
	pte = *pde;
			/*
			 *	pde: 指向页目录表中负责映射线性地址 address 的那个页目录项。
			 *
			 *	pte: 获取页目录表中负责映射线性地址 address 的那个页目录项的值，该页目录项的
			 * 值指示了页表的基地址和页表的属性。
			 */
	if (!(pte & PAGE_PRESENT))
		goto end_wp_page;
	if ((pte & PAGE_TABLE) != PAGE_TABLE || pte >= high_memory)
		goto bad_wp_pagetable;
			/*
			 *	检测页表是否存在且有效。
			 */
	pte &= PAGE_MASK;
	pte += PAGE_PTR(address);
	old_page = *(unsigned long *) pte;
			/*
			 *	pte: 指向页表中负责映射线性地址 address 的那个页表项。
			 *
			 *	old_page: 获取页表中负责映射线性地址 address 的那个页表项的值，该页表项的值
			 * 指示了线性地址 address 映射后的物理地址所在的物理内存页面的基地址和页面的属性。
			 *
			 *	线性地址 address 所在的物理内存页面，也就是那个被写保护的页面，称为原页面。
			 */
	if (!(old_page & PAGE_PRESENT))
		goto end_wp_page;
	if (old_page >= high_memory)
		goto bad_wp_page;
	if (old_page & PAGE_RW)
		goto end_wp_page;
			/*
			 *	检测原页面是否存在、是否有效、是否有可写权限。
			 */
	tsk->min_flt++;
			/*  */
	prot = (old_page & ~PAGE_MASK) | PAGE_RW;
	old_page &= PAGE_MASK;
			/*
			 *	port: 低 12bit 有效，是新页面的属性字段的值，新页面的属性里将包含原页面的所有
			 * 属性，并新增可写属性。
			 */

	/*
	 *	if: 原页面的引用计数不为 1，说明原页面被多个任务共享。
	 */
	if (mem_map[MAP_NR(old_page)] != 1) {
		if (new_page) {
			if (mem_map[MAP_NR(old_page)] & MAP_PAGE_RESERVED)
				++tsk->rss;
					/*  */
			copy_page(old_page,new_page);
			*(unsigned long *) pte = new_page | prot;
			free_page(old_page);
					/*
					 *	对于原页面被多个任务共享的情况，解除页写保护的方式为:
					 *
					 *	1. 将原页面中的内容完整的复制一份到新页面中。
					 *
					 *	2. 重新设置任务 tsk 的页表中负责映射线性地址 address 的页表项
					 * 的值，使其指向新页面，进而断开该页表项与原页面的连接关系，并重新设
					 * 置页表项中的属性字段为新页面的属性。
					 *
					 *	这时，当异常返回并重新执行引起异常的那条指令时，线性地址
					 * address 所映射的物理地址就会在新页面中，且新页面具有可写权限。
					 *
					 *	3. 任务 tsk 已经映射好了新页面，则需要释放其占有的原页面，这里
					 * 的释放动作只是将原页面的引用计数减 1 而已，并不会真正的释放原页面。
					 */
			invalidate();
					/*
					 *	任务 tsk 的页表已更新，则需要刷新 TLB。
					 */
			return;
		}

		free_page(old_page);
		oom(tsk);
		*(unsigned long *) pte = BAD_PAGE | prot;
		invalidate();
		return;
				/*
				 *	没有为任务 tsk 申请到新页面，对于这种情况，解除页写保护的方式为:
				 *
				 *	1. 释放任务 tsk 占有的原页面，这里只是将原页面的引用计数减 1 而已。
				 *
				 *	2. 重新设置任务 tsk 的页表中负责映射线性地址 address 的页表项的值，
				 * 使其与系统给定的无效内存页面建立映射关系，进而断开该页表项与原页面的连
				 * 接关系。
				 *
				 *	实际上，这里没能为 tsk 申请到新的内存页面，所以也就没有办法为任务
				 * tsk 正确的解除页写保护异常，但是页写保护异常又必须解除，否则退出异常后
				 * 又会触发同一个页写保护异常。
				 *
				 *	另外，在这种情况下会调用 oom 报内存不足，oom 中会发送信号将 tsk
				 * 杀死，所以这里只需要为任务 tsk 解除页写保护异常即可，任务 tsk 之后的
				 * 运行状态已无关紧要，因此才会用系统给定的无效内页页面来临时解除任务 tsk
				 * 的页写保护异常。
				 */
	}

	/*
	 *	below: 原页面的引用计数已经为 1，说明任务 tsk 已独占原页面，只是任务 tsk
	 * 对原页面没有写权限而已。(共享原页面的其它任务都已经解除了页写保护，并重新建立了
	 * 新的映射关系，将原页面留给了最后一个解除页写保护的任务使用)
	 *
	 *	对于这种情况，解除页写保护的方式是: 在原页面对应的页表项中为原页面新增可写
	 * 属性，使任务 tsk 对原页面有可写权限即可。
	 */
	*(unsigned long *) pte |= PAGE_RW;
	invalidate();
	if (new_page)
		free_page(new_page);
	return;

bad_wp_page:
	printk("do_wp_page: bogus page at address %08lx (%08lx)\n",address,old_page);
	*(unsigned long *) pte = BAD_PAGE | PAGE_SHARED;
	send_sig(SIGKILL, tsk, 1);
	goto end_wp_page;
			/*
			 *	任务 tsk 中的线性地址 address 映射后的物理地址所在的内存页面无效，则重新设置
			 * 无效页面对应的页表项的值，使该页表项与系统给定的无效页面建立映射关系，断开与原无效
			 * 页面的连接关系。
			 *
			 *	任务已经映射好的页面无效，这是一种异常的状态，故此处发信号直接将任务杀死。
			 */
bad_wp_pagetable:
	printk("do_wp_page: bogus page-table at address %08lx (%08lx)\n",address,pte);
	*pde = BAD_PAGETABLE | PAGE_TABLE;
	send_sig(SIGKILL, tsk, 1);
			/*
			 *	任务 tsk 中负责映射线性地址 address 的那个页表无效，则重新设置无效页表对应的
			 * 页目录项的值，使该页目录项与系统给定的无效页表建立映射关系，断开与原无效页表的连接
			 * 关系。
			 *
			 *	任务的页表存在但无效，这是一种异常的状态，故此处发送信号直接将任务杀死。
			 */
end_wp_page:
	if (new_page)
		free_page(new_page);
	return;
}

/*
 * check that a page table change is actually needed, and call
 * the low-level function only in that case..
 */
/*
 *	do_wp_page: 处理页写保护异常。
 *
 *	入参:
 *	error_code --- 引起页异常的错误码。
 *	address --- 引起页异常的线性地址。
 *	tsk --- 引起页异常的任务。
 *	user_esp ---
 */
void do_wp_page(unsigned long error_code, unsigned long address,
	struct task_struct * tsk, unsigned long user_esp)
{
	unsigned long page;
	unsigned long * pg_table;

	pg_table = PAGE_DIR_OFFSET(tsk->tss.cr3,address);
	page = *pg_table;
	if (!page)
		return;
			/*
			 *	1. pg_table 指向任务 tsk 的页目录表中负责映射线性地址 address 的页目录项。
			 *
			 *	2. page 是页目录项的值，page 的高 20bit 是该页目录项指向的页表的物理内存
			 * 基地址，低 12bit 是页表的属性。
			 */

	/*
	 *	if: 页表的属性显示页表存在且页表的基地址是一个有效的物理内存地址。
	 */
	if ((page & PAGE_PRESENT) && page < high_memory) {
		pg_table = (unsigned long *) ((page & PAGE_MASK) + PAGE_PTR(address));
		page = *pg_table;
				/*
				 *	1. pg_table 指向任务 tsk 的页表中负责映射线性地址 address 的页表项。
				 *
				 *	2. page 是页表项的值，page 的高 20bit 是该页表项指向的物理内存页面
				 * 基地址，低 12bit 是物理内存页面的属性。
				 *	任务 tsk 的线性地址 address 映射后的物理地址就在这个页面中。
				 */
		if (!(page & PAGE_PRESENT))
			return;
		if (page & PAGE_RW)
			return;
				/*
				 *	物理内存页面不存在，则无法解除页写保护。物理内存页面已经有可写属性，
				 * 则无需再解除页写保护。
				 */
		if (!(page & PAGE_COW)) {
			if (user_esp && tsk == current) {
				current->tss.cr2 = address;
				current->tss.error_code = error_code;
				current->tss.trap_no = 14;
				send_sig(SIGSEGV, tsk, 1);
				return;
			}
		}
		if (mem_map[MAP_NR(page)] == 1) {
			*pg_table |= PAGE_RW | PAGE_DIRTY;
			invalidate();
			return;
		}
				/*
				 *	该物理内存页面的引用计数已经为 1，说明该页面已经被任务 tsk 独占，只是
				 * tsk 对该页面没有写权限而已。
				 *
				 *	对于这种情况，解除页写保护的方式是: 将该物理内存页面对应的页表项中的
				 * 页面属性置为可写，使任务对该页面有可写权限即可。
				 */

		__do_wp_page(error_code, address, tsk, user_esp);
				/*
				 *	对于物理内存页面被多个任务共享的情况，解除页面写保护。
				 */
		return;
	}

	printk("bad page directory entry %08lx\n",page);
	*pg_table = 0;
			/*
			 *	页目录项指向的页表异常，则断开该页目录项与页表的连接。
			 */
}

/*
 *	__verify_write: 验证 start 指向的大小为 size 字节的当前正在运行任务的用户态
 * 线性地址空间对应的物理内存页面是否可写，如果不可写，则处理页写保护的问题。
 */
int __verify_write(unsigned long start, unsigned long size)
{
	size--;
	size += start & ~PAGE_MASK;
	size >>= PAGE_SHIFT;
	start &= PAGE_MASK;
			/*
			 *	1. 将 size 由需要验证的字节数转换为需要验证的页面个数。
			 *
			 *	2. start 转换为线性地址空间中页面的起始地址。
			 */
	do {
		do_wp_page(1,start,current,0);
		start += PAGE_SIZE;
	} while (size--);
			/*
			 *	循环验证线性地址空间中的所有页面，检测它们对应的物理内存页面是否被页写保护，
			 * 如果有页写保护，则解除页写保护。这里不关注缺页的问题。
			 */
	return 0;
}

static inline void get_empty_page(struct task_struct * tsk, unsigned long address)
{
	unsigned long tmp;

	if (!(tmp = get_free_page(GFP_KERNEL))) {
		oom(tsk);
		tmp = BAD_PAGE;
	}
	if (!put_page(tsk,tmp,address,PAGE_PRIVATE))
		free_page(tmp);
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable or library.
 *
 * We may want to fix this to allow page sharing for PIC pages at different
 * addresses so that ELF will really perform properly. As long as the vast
 * majority of sharable libraries load at fixed addresses this is not a
 * big concern. Any sharing of pages between the buffer cache and the
 * code space reduces the need for this as well.  - ERY
 */
static int try_to_share(unsigned long address, struct task_struct * tsk,
	struct task_struct * p, unsigned long error_code, unsigned long newpage)
{
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;

	from_page = (unsigned long)PAGE_DIR_OFFSET(p->tss.cr3,address);
	to_page = (unsigned long)PAGE_DIR_OFFSET(tsk->tss.cr3,address);
/* is there a page-directory at from? */
	from = *(unsigned long *) from_page;
	if (!(from & PAGE_PRESENT))
		return 0;
	from &= PAGE_MASK;
	from_page = from + PAGE_PTR(address);
	from = *(unsigned long *) from_page;
/* is the page clean and present? */
	if ((from & (PAGE_PRESENT | PAGE_DIRTY)) != PAGE_PRESENT)
		return 0;
	if (from >= high_memory)
		return 0;
	if (mem_map[MAP_NR(from)] & MAP_PAGE_RESERVED)
		return 0;
/* is the destination ok? */
	to = *(unsigned long *) to_page;
	if (!(to & PAGE_PRESENT))
		return 0;
	to &= PAGE_MASK;
	to_page = to + PAGE_PTR(address);
	if (*(unsigned long *) to_page)
		return 0;
/* share them if read - do COW immediately otherwise */
	if (error_code & PAGE_RW) {
		if(!newpage)	/* did the page exist?  SRB. */
			return 0;
		copy_page((from & PAGE_MASK),newpage);
		to = newpage | PAGE_PRIVATE;
	} else {
		mem_map[MAP_NR(from)]++;
		from &= ~PAGE_RW;
		to = from;
		if(newpage)	/* only if it existed. SRB. */
			free_page(newpage);
	}
	*(unsigned long *) from_page = from;
	*(unsigned long *) to_page = to;
	invalidate();
	return 1;
}

/*
 * share_page() tries to find a process that could share a page with
 * the current one. Address is the address of the wanted page relative
 * to the current data space.
 *
 * We first check if it is at all feasible by checking executable->i_count.
 * It should be >1 if there are other tasks sharing this inode.
 */
int share_page(struct vm_area_struct * area, struct task_struct * tsk,
	struct inode * inode,
	unsigned long address, unsigned long error_code, unsigned long newpage)
{
	struct task_struct ** p;

	if (!inode || inode->i_count < 2 || !area->vm_ops)
		return 0;
	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p)
			continue;
		if (tsk == *p)
			continue;
		if (inode != (*p)->executable) {
			  if(!area) continue;
			/* Now see if there is something in the VMM that
			   we can share pages with */
			if(area){
			  struct vm_area_struct * mpnt;
			  for (mpnt = (*p)->mmap; mpnt; mpnt = mpnt->vm_next) {
			    if (mpnt->vm_ops == area->vm_ops &&
			       mpnt->vm_inode->i_ino == area->vm_inode->i_ino&&
			       mpnt->vm_inode->i_dev == area->vm_inode->i_dev){
			      if (mpnt->vm_ops->share(mpnt, area, address))
				break;
			    };
			  };
			  if (!mpnt) continue;  /* Nope.  Nuthin here */
			};
		}
		if (try_to_share(address,tsk,*p,error_code,newpage))
			return 1;
	}
	return 0;
}

/*
 * fill in an empty page-table if none exists.
 */
static inline unsigned long get_empty_pgtable(struct task_struct * tsk,unsigned long address)
{
	unsigned long page;
	unsigned long *p;

	p = PAGE_DIR_OFFSET(tsk->tss.cr3,address);
	if (PAGE_PRESENT & *p)
		return *p;
	if (*p) {
		printk("get_empty_pgtable: bad page-directory entry \n");
		*p = 0;
	}
	page = get_free_page(GFP_KERNEL);
	p = PAGE_DIR_OFFSET(tsk->tss.cr3,address);
	if (PAGE_PRESENT & *p) {
		free_page(page);
		return *p;
	}
	if (*p) {
		printk("get_empty_pgtable: bad page-directory entry \n");
		*p = 0;
	}
	if (page) {
		*p = page | PAGE_TABLE;
		return *p;
	}
	oom(current);
	*p = BAD_PAGETABLE | PAGE_TABLE;
	return 0;
}

void do_no_page(unsigned long error_code, unsigned long address,
	struct task_struct *tsk, unsigned long user_esp)
{
	unsigned long tmp;
	unsigned long page;
	struct vm_area_struct * mpnt;

	page = get_empty_pgtable(tsk,address);
	if (!page)
		return;
	page &= PAGE_MASK;
	page += PAGE_PTR(address);
	tmp = *(unsigned long *) page;
	if (tmp & PAGE_PRESENT)
		return;
	++tsk->rss;
	if (tmp) {
		++tsk->maj_flt;
		swap_in((unsigned long *) page);
		return;
	}
	address &= 0xfffff000;
	tmp = 0;
	for (mpnt = tsk->mmap; mpnt != NULL; mpnt = mpnt->vm_next) {
		if (address < mpnt->vm_start)
			break;
		if (address >= mpnt->vm_end) {
			tmp = mpnt->vm_end;
			continue;
		}
		if (!mpnt->vm_ops || !mpnt->vm_ops->nopage) {
			++tsk->min_flt;
			get_empty_page(tsk,address);
			return;
		}
		mpnt->vm_ops->nopage(error_code, mpnt, address);
		return;
	}
	if (tsk != current)
		goto ok_no_page;
	if (address >= tsk->end_data && address < tsk->brk)
		goto ok_no_page;
	if (mpnt && mpnt == tsk->stk_vma &&
	    address - tmp > mpnt->vm_start - address &&
	    tsk->rlim[RLIMIT_STACK].rlim_cur > mpnt->vm_end - address) {
		mpnt->vm_start = address;
		goto ok_no_page;
	}
	tsk->tss.cr2 = address;
	current->tss.error_code = error_code;
	current->tss.trap_no = 14;
	send_sig(SIGSEGV,tsk,1);
	if (error_code & 4)	/* user level access? */
		return;
ok_no_page:
	++tsk->min_flt;
	get_empty_page(tsk,address);
}

/*
 * This routine handles page faults.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 */
/*
 *	do_page_fault: 页错误的中断 C 处理函数，在 sys_call.S 中的 page_fault 函数中调用。
 *
 *	入参: regs --- 指向进入中断时保存在当前正在运行的任务的内核态栈中的寄存器的信息。
 *	      error_code --- 页错误的错误码。
 *
 *
 *	页异常的错误码格式:
 *
 *	|   31						    3	|   2	|   1	|   0   |
 *	+-------------------------------------------------------------------------------+
 *	|			保留				|  U/S 	|  R/W 	|   P	|
 *	+-------------------------------------------------------------------------------+
 *
 *	BIT0: P --- P = 0 表示缺页异常，P = 1 表示页写保护异常。
 *	BIT1: R/W --- R/W = 0 表示异常由读操作引起，R/W = 1 表示异常由写操作引起。
 *	BIT2: U/S --- U/S = 0 表示 CPU 正在执行超级用户代码，U/S = 1 表示 CPU 正在执行一般
 * 用户代码。
 */
asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long error_code)
{
	unsigned long address;
	unsigned long user_esp = 0;
	unsigned int bit;

	/* get the address */
	__asm__("movl %%cr2,%0":"=r" (address));
			/*
			 *	address = CR2。当出现页异常时，处理器会把引起页异常的线性地址保存在
			 * CR2 寄存器中。
			 */

	/*
	 *	if: 页异常发生在任务的用户态空间中。
	 */
	if (address < TASK_SIZE) {
		/*
		 *	if: 发生异常时 CPU 正在执行一般用户代码。
		 */
		if (error_code & 4) {	/* user mode access? */
			if (regs->eflags & VM_MASK) {
				bit = (address - 0xA0000) >> PAGE_SHIFT;
				if (bit < 32)
					current->screen_bitmap |= 1 << bit;
			} else 
				user_esp = regs->esp;
		}
		if (error_code & 1)
			do_wp_page(error_code, address, current, user_esp);	/* 处理页写保护异常 */
		else
			do_no_page(error_code, address, current, user_esp);	/* 处理缺页异常 */
		return;
	}

	/*
	 *	below: 页异常发生在任务的内核态空间中。
	 */
	address -= TASK_SIZE;
			/*
			 *	address = 发生异常时的物理地址。
			 */
	/*
	 *	if: 用于在 mem_init 中测试页写保护功能是否正常。
	 */
	if (wp_works_ok < 0 && address == 0 && (error_code & PAGE_PRESENT)) {
		wp_works_ok = 1;
		pg0[0] = PAGE_SHARED;
		printk("This processor honours the WP bit even when in supervisor mode. Good.\n");
		return;
	}
	if (address < PAGE_SIZE) {
		printk("Unable to handle kernel NULL pointer dereference");
		pg0[0] = PAGE_SHARED;
	} else
		printk("Unable to handle kernel paging request");
	printk(" at address %08lx\n",address);
	die_if_kernel("Oops", regs, error_code);
	do_exit(SIGKILL);
}

/*
 * BAD_PAGE is the page that is used for page faults when linux
 * is out-of-memory. Older versions of linux just did a
 * do_exit(), but using this instead means there is less risk
 * for a process dying in kernel mode, possibly leaving a inode
 * unused etc..
 *
 * BAD_PAGETABLE is the accompanying page-table: it is initialized
 * to point to BAD_PAGE entries.
 *
 * ZERO_PAGE is a special page that is used for zero-initialized
 * data and COW.
 */
/*
 *	BAD_PAGE 是 Linux 在内存不足时用于页错误的页面。旧版本的 Linux 只做了一个 do_exit()，
 * 但是用 BAD_PAGE 代替 do_exit 意味着进程在内核模式下死亡的风险更小，可能会留下一个未使用的
 * inode 等等。
 *
 *	BAD_PAGETABLE 是附带的页表: 它的所有页表项都被初始化为指向 BAD_PAGE。
 *
 *	ZERO_PAGE 是一个特殊的页面，它被用于初始化零数据和 COW。
 */

/*
 *	__bad_pagetable: 将系统设置的位于 0x4000 地址处的无效页表 _empty_bad_page_table
 * 中的所有页表项全部设置为 (BAD_PAGE + PAGE_TABLE)，使每一个页表项都与无效页面
 * _empty_bad_page 建立映射关系，并返回无效页表的基地址。
 */
unsigned long __bad_pagetable(void)
{
	extern char empty_bad_page_table[PAGE_SIZE];

	__asm__ __volatile__("cld ; rep ; stosl":
		:"a" (BAD_PAGE + PAGE_TABLE),
		 "D" ((long) empty_bad_page_table),
		 "c" (PTRS_PER_PAGE)
		:"di","cx");
	return (unsigned long) empty_bad_page_table;
}

/*
 *	__bad_page: 将系统设置的位于 0x3000 地址处的无效内存页面 _empty_bad_page
 * 清 0，并返回无效内存页面的基地址。
 */
unsigned long __bad_page(void)
{
	extern char empty_bad_page[PAGE_SIZE];

	__asm__ __volatile__("cld ; rep ; stosl":
		:"a" (0),
		 "D" ((long) empty_bad_page),
		 "c" (PTRS_PER_PAGE)
		:"di","cx");
	return (unsigned long) empty_bad_page;
}

unsigned long __zero_page(void)
{
	extern char empty_zero_page[PAGE_SIZE];

	__asm__ __volatile__("cld ; rep ; stosl":
		:"a" (0),
		 "D" ((long) empty_zero_page),
		 "c" (PTRS_PER_PAGE)
		:"di","cx");
	return (unsigned long) empty_zero_page;
}

void show_mem(void)
{
	int i,free = 0,total = 0,reserved = 0;
	int shared = 0;

	printk("Mem-info:\n");
	printk("Free pages:      %6dkB\n",nr_free_pages<<(PAGE_SHIFT-10));
	printk("Secondary pages: %6dkB\n",nr_secondary_pages<<(PAGE_SHIFT-10));
	printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));
	i = high_memory >> PAGE_SHIFT;
	while (i-- > 0) {
		total++;
		if (mem_map[i] & MAP_PAGE_RESERVED)
			reserved++;
		else if (!mem_map[i])
			free++;
		else
			shared += mem_map[i]-1;
	}
	printk("%d pages of RAM\n",total);
	printk("%d free pages\n",free);
	printk("%d reserved pages\n",reserved);
	printk("%d pages shared\n",shared);
	show_buffers();
}

/*
 * paging_init() sets up the page tables - note that the first 4MB are
 * already mapped by head.S.
 *
 * This routines also unmaps the page at virtual kernel address 0, so
 * that we can trap those pesky NULL-reference errors in the kernel.
 */
/*
 *	paging_init: 映射所有的物理内存空间，将所有内存空间一对一映射到线性地址 0 和
 * 线性地址 0xC0000000 开始的两个空间内。
 *
 *	页目录表位于 0x1000 处，第一个页表位于 0x2000 处，之后是内核代码及数据所占用
 * 的空间。剩余的页表在映射时将从 start_mem 处开始依次存放，最后返回的 start_mem 将
 * 跳过这些页表，这些页表是内核的页表，将永远存在。
 *
 *	内核的这种映射方式，如果不考虑线性基地址，则线性地址和物理地址是一一对应的，
 * 物理地址 + 线性基地址就是线性地址。比如物理内存有 16MB，则线性地址 0xC0000000 开始
 * 的 16MB 空间将与物理内存的 16MB 空间一一对应。
 */
unsigned long paging_init(unsigned long start_mem, unsigned long end_mem)
{
	unsigned long * pg_dir;
	unsigned long * pg_table;
	unsigned long tmp;
	unsigned long address;

/*
 * Physical page 0 is special; it's not touched by Linux since BIOS
 * and SMM (for laptops with [34]86/SL chips) may need it.  It is read
 * and write protected to detect null pointer references in the
 * kernel.
 */
/*
 *	物理页面 0 是特殊的，Linux不会触及它，因为 BIOS 和 SMM (对于带有
 * [34]86/SL 芯片的笔记本电脑)可能需要它。它具有读写保护，可以检测内核
 * 中的空指针引用。
 */
#if 0
	memset((void *) 0, 0, PAGE_SIZE);
#endif
	start_mem = PAGE_ALIGN(start_mem);
			/*
			 *	start_mem 对齐到下一个 4kB 的边界处，因为要从这个位置开始存放
			 * 页表，所以需要在页边界处对齐。
			 *	内核的页目录表在 0x1000(swapper_pg_dir) 地址处，第一个页表在
			 * 0x2000(pg0) 地址处，从第二个页表开始的后续所有页表都在 start_mem
			 * 开始的连续的内存页面中。
			 */
	address = 0;
		/*
		 *	从物理内存的 0 地址开始，一直到 end_mem，所有的内存空间都要被同步映射
		 * 到 0x00000000 和 0xC0000000 开始的两个线性地址空间中。
		 */
	pg_dir = swapper_pg_dir;	/* pg_dir 指向页目录表的第 0 个页目录项 */
	while (address < end_mem) {
		tmp = *(pg_dir + 768);		/* at virtual addr 0xC0000000 */
				/*
				 *	tmp: 循环获取从页目录表的第 768 项开始的页目录项的内容。
				 * 第 768 项页目录项和第 0 项页目录项在 head.S 中已经映射过了，
				 * 此处直接将第 768 项页目录项的内容重新放到第 0 项即可。
				 */
		if (!tmp) {
			/*
			 *	页目录项的内容为空，该页目录项还没有指向的页表，即该页目录项对应
			 * 的 4MB 的线性地址空间还未被映射。则从 start_mem 处分 4kB 的空间作为页
			 * 表使用，start_mem 向后偏移 4kB。
			 */
			tmp = start_mem | PAGE_TABLE;
			*(pg_dir + 768) = tmp;	/* 建立页目录项与页表的映射关系 */
			start_mem += PAGE_SIZE;
		}
		*pg_dir = tmp;			/* also map it in at 0x0000000 for init */
			/* 
			 *	将 tmp 同时放入线性地址 0 开始的页目录项中，表示线性地址 0 开始
			 * 的线性空间和线性地址 0xC0000000 开始的线性空间映射到同一物理内存空间。
			 */
		pg_dir++;	/* pg_dir 指向下一个页目录项 */
		pg_table = (unsigned long *) (tmp & PAGE_MASK);
				/*
				 *	pg_table 指向页目录项对应的页表的第 0 个页表项。
				 */
		for (tmp = 0 ; tmp < PTRS_PER_PAGE ; tmp++,pg_table++) {
			/*
			 *	循环建立一个页表中的每个页表项与 4kB 的物理内存页面之间的映射关系。
			 */
			if (address < end_mem)
				*pg_table = address | PAGE_SHARED;
			else
				*pg_table = 0;
			address += PAGE_SIZE;
		}
	}
	invalidate();	/* 更新了页目录表和页表，需要刷新 TLB。 */
	return start_mem;	/* 跳过页表所占用的空间 */
}

/*
 *	mem_init: 内存初始化，主要完成以下工作
 *
 *	1. 初始化内存页面: 所有物理内存按页面管理，开辟内存页面管理结构所需要的空间并按所
 * 管理的页面的状态填充对应的页面管理结构。最后将空闲页面链接到空闲页面链表 free_page_list
 * 上，输出内存页面的信息。
 *
 *	2. 测试页写保护功能是否正常，并将物理页面 0 置为无效，使其具有读写保护，用于检测
 * 内核中的空指针引用。
 */
void mem_init(unsigned long start_low_mem,
	      unsigned long start_mem, unsigned long end_mem)
{
	int codepages = 0;
	int reservedpages = 0;
	int datapages = 0;
	unsigned long tmp;
	unsigned short * p;
	extern int etext;	/* 内核代码段结束的位置，由链接程序设置 */

	cli();
	end_mem &= PAGE_MASK;	/* 内存结束位置对齐到页边界起始处，后面不足一页的部分被丢弃 */
	high_memory = end_mem;	/* 设置分页管理的内存的最高位置 */

	start_mem +=  0x0000000f;
	start_mem &= ~0x0000000f;	/* start_mem 对齐到下一个 16 字节的边界处 */
	tmp = MAP_NR(end_mem);	/* 获取整个内存对应的页面个数 */
	mem_map = (unsigned short *) start_mem;	/* 从 start_mem 处开始存放内存页面管理结构 */
	p = mem_map + tmp;
	start_mem = (unsigned long) p;	/* 跳过内存页面管理结构所占的内存空间 */

	while (p > mem_map)
		*--p = MAP_PAGE_RESERVED;
			/* 初始时将所有的内存页面置为已使用状态 */

	start_low_mem = PAGE_ALIGN(start_low_mem);
	start_mem = PAGE_ALIGN(start_mem);	/* 对齐到下一个页面边界 */

	while (start_low_mem < 0xA0000) {
		mem_map[MAP_NR(start_low_mem)] = 0;
		start_low_mem += PAGE_SIZE;
			/*
			 *	从 start_low_mem 到 640KB 之间的内存页面管理结构置 0，表示这些页面
			 * 未使用，start_low_mem 是传入的 low_memory_start，这个值不管哪种情况，都是
			 * 小于 512KB 的。
			 */
	}
	while (start_mem < end_mem) {
		mem_map[MAP_NR(start_mem)] = 0;
		start_mem += PAGE_SIZE;
			/*
			 *	从 start_mem 到内存结束位置之间的内存页面全部置为未使用状态，根据
			 * 最开始的设置，不管哪种情况，start_mem 都是在 1MB 以上的位置。
			 */
	}
	/*
	 *	上面的两个 while，留下了 640KB - 1MB 之间的页面，这些页面的状态仍然是已使用状态，
	 * 这些内存是预留给显存和 BIOS 使用的，所以不能当做空闲页面来使用。
	 */

#ifdef CONFIG_SOUND
	sound_mem_init();
#endif
	free_page_list = 0;
	nr_free_pages = 0;

	/*
	 *	for: 扫描整个内存页面，统计页面使用情况，并将未使用的页面以单链表的形式链接
	 * 在 free_page_list 上。
	 */
	for (tmp = 0 ; tmp < end_mem ; tmp += PAGE_SIZE) {
		if (mem_map[MAP_NR(tmp)]) {
			/* 内存页面已使用 */
			if (tmp >= 0xA0000 && tmp < 0x100000)
				reservedpages++;	/* 640KB - 1MB 之间的内存页面保留 */
			else if (tmp < (unsigned long) &etext)
				codepages++;	/* 内核代码所占用的页面个数 */
			else
				datapages++;	/* 内核数据所占用的页面个数 */
			continue;
		}

		/* 页面空闲 */
		*(unsigned long *) tmp = free_page_list;
		free_page_list = tmp;
		nr_free_pages++;
			/*
			 *	页面最开始的 4 个字节存放指向下一个空闲页面的指针，初始化后，
			 * 低端内存的页面位于链表的尾部，链表头部是最高端内存的那个页面。
			 */
	}

	tmp = nr_free_pages << PAGE_SHIFT;
	printk("Memory: %luk/%luk available (%dk kernel code, %dk reserved, %dk data)\n",
		tmp >> 10,
		end_mem >> 10,
		codepages << (PAGE_SHIFT-10),
		reservedpages << (PAGE_SHIFT-10),
		datapages << (PAGE_SHIFT-10));

/* test if the WP bit is honoured in supervisor mode */
	/*
	 *	测试页写保护功能是否正常，页面写保护是写时复制的基础。pg0[0] 原来的值是
	 * 0x00000007，这是一个页表项，该页表项映射的物理内存空间是 0 - 4KB，属性是 7，
	 * 表示该物理内存页面存在且可读写。
	 *
	 *	1. 先将 pg0[0] 指向的物理内存页面的属性设置为只读。
	 *	2. 刷新 TLB，使 TLB 之前缓存地址转换关系失效。这样，下一次地址转换时 MMU
	 * 需要从内存中重新读取页目录表及页表来做转换并重新在 TLB 中缓存地址转换关系。
	 *	3. 执行一个操作: 向 0 地址处写 0。这时因为物理内存 0 开始的页面的属性已经
	 * 变更为只读，不可写，所以会产生页写保护异常。
	 *	4. 处理器转而执行页写保护异常，最后会执行到函数 do_page_fault，在这个函数
	 * 里有一个分支: if (wp_works_ok < 0 && address == 0 && (error_code & PAGE_PRESENT))，
	 * 这个分支就是用来处理现在这种情况的，这时会执行 wp_works_ok = 1 和
	 * pg0[0] = PAGE_SHARED。将页面的属性重新改回可读写，并设置页写保护功能正常标志。
	 *	5. 页写保护处理结束，重新执行引起页写保护的指令，执行成功。
	 *	6. 继续向下执行。
	 */
	wp_works_ok = -1;
	pg0[0] = PAGE_READONLY;
	invalidate();
	__asm__ __volatile__("movb 0,%%al ; movb %%al,0": : :"ax", "memory");

	/*
	 *	在此之前，页表项 pg0[0] 指向的物理内存页面，也就是物理内存 0 开始
	 * 的页面一直是可读写的，现在将其属性清空，页面不存在，从此以后，对于物理
	 * 地址 0 开始的页面的访问将会引发缺页中断，最后会执行到 do_page_fault，
	 * 其中有一个分支: if (address < PAGE_SIZE)，专门处理这种情况。
	 *
	 *	这就是将物理页面 0 空出来的目的，它具有读写保护，可以检测内核中的
	 * 空指针引用，这个功能从此处开始正式生效。
	 */
	pg0[0] = 0;
	invalidate();

	if (wp_works_ok < 0)
		wp_works_ok = 0;	/* 页写保护功能异常 */
	return;
}

void si_meminfo(struct sysinfo *val)
{
	int i;

	i = high_memory >> PAGE_SHIFT;
	val->totalram = 0;
	val->freeram = 0;
	val->sharedram = 0;
	val->bufferram = buffermem;
	while (i-- > 0)  {
		if (mem_map[i] & MAP_PAGE_RESERVED)
			continue;
		val->totalram++;
		if (!mem_map[i]) {
			val->freeram++;
			continue;
		}
		val->sharedram += mem_map[i]-1;
	}
	val->totalram <<= PAGE_SHIFT;
	val->freeram <<= PAGE_SHIFT;
	val->sharedram <<= PAGE_SHIFT;
	return;
}


/* This handles a generic mmap of a disk file */
void file_mmap_nopage(int error_code, struct vm_area_struct * area, unsigned long address)
{
	struct inode * inode = area->vm_inode;
	unsigned int block;
	unsigned long page;
	int nr[8];
	int i, j;
	int prot = area->vm_page_prot;

	address &= PAGE_MASK;
	block = address - area->vm_start + area->vm_offset;
	block >>= inode->i_sb->s_blocksize_bits;

	page = get_free_page(GFP_KERNEL);
	if (share_page(area, area->vm_task, inode, address, error_code, page)) {
		++area->vm_task->min_flt;
		return;
	}

	++area->vm_task->maj_flt;
	if (!page) {
		oom(current);
		put_page(area->vm_task, BAD_PAGE, address, PAGE_PRIVATE);
		return;
	}
	for (i=0, j=0; i< PAGE_SIZE ; j++, block++, i += inode->i_sb->s_blocksize)
		nr[j] = bmap(inode,block);
	if (error_code & PAGE_RW)
		prot |= PAGE_RW | PAGE_DIRTY;
	page = bread_page(page, inode->i_dev, nr, inode->i_sb->s_blocksize, prot);

	if (!(prot & PAGE_RW)) {
		if (share_page(area, area->vm_task, inode, address, error_code, page))
			return;
	}
	if (put_page(area->vm_task,page,address,prot))
		return;
	free_page(page);
	oom(current);
}

void file_mmap_free(struct vm_area_struct * area)
{
	if (area->vm_inode)
		iput(area->vm_inode);
#if 0
	if (area->vm_inode)
		printk("Free inode %x:%d (%d)\n",area->vm_inode->i_dev, 
				 area->vm_inode->i_ino, area->vm_inode->i_count);
#endif
}

/*
 * Compare the contents of the mmap entries, and decide if we are allowed to
 * share the pages
 */
int file_mmap_share(struct vm_area_struct * area1, 
		    struct vm_area_struct * area2, 
		    unsigned long address)
{
	if (area1->vm_inode != area2->vm_inode)
		return 0;
	if (area1->vm_start != area2->vm_start)
		return 0;
	if (area1->vm_end != area2->vm_end)
		return 0;
	if (area1->vm_offset != area2->vm_offset)
		return 0;
	if (area1->vm_page_prot != area2->vm_page_prot)
		return 0;
	return 1;
}

struct vm_operations_struct file_mmap = {
	NULL,			/* open */
	file_mmap_free,		/* close */
	file_mmap_nopage,	/* nopage */
	NULL,			/* wppage */
	file_mmap_share,	/* share */
	NULL,			/* unmap */
};
