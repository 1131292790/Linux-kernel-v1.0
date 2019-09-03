/*
 *  linux/kernel/fork.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s).
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/segment.h>
#include <linux/ptrace.h>
#include <linux/malloc.h>
#include <linux/ldt.h>

#include <asm/segment.h>
#include <asm/system.h>

asmlinkage void ret_from_sys_call(void) __asm__("ret_from_sys_call");

/* These should maybe be in <linux/tasks.h> */

#define MAX_TASKS_PER_USER (NR_TASKS/2)
#define MIN_TASKS_LEFT_FOR_ROOT 4

extern int shm_fork(struct task_struct *, struct task_struct *);

/*
 *	last_pid: ���̺ţ���ʼֵΪ 0�������̺Ŵ� 1 ��ʼ�����̺ŵ���Ч��ΧΪ 1 - 32767��ѭ��ʹ�á�
 */
long last_pid=0;

/*
 *	find_empty_process: ����������������ܣ�һ��Ϊ�½���ȡ�ò��ظ��Ľ��̺� last_pid������Ϊ
 * �½���Ѱ��һ������ţ��������ҵ�������š�
 */
static int find_empty_process(void)
{
	int free_task;
	int i, tasks_free;
	int this_user_tasks;

repeat:
	if ((++last_pid) & 0xffff8000)
		last_pid=1;
			/*
			 *	�� ++last_pid �·���һ�����̺ţ����̺ŵ���Ч��ΧΪ 1 - 32767��
			 */
	this_user_tasks = 0;
	tasks_free = 0;
	free_task = -EAGAIN;

		/*
		 *	ɨ������ task ���飬�鿴 task �������Ƿ��п��е�Ԫ�أ�������·������
		 * ���̺��Ƿ���á�
		 */
	i = NR_TASKS;
	while (--i > 0) {
		if (!task[i]) {
			free_task = i;
			tasks_free++;
			continue;
		}
				/*
				 *	task �����е�Ԫ�ؿ��У�ÿ�η����ȥ����������ǿ������������С
				 * ���Ǹ�����š�
				 */
		if (task[i]->uid == current->uid)
			this_user_tasks++;
				/*
				 *	task[i] ��ָʾ�������뵱ǰ�������е���������ͬһ���û�
				 */
		if (task[i]->pid == last_pid || task[i]->pgrp == last_pid ||
		    task[i]->session == last_pid)
			goto repeat;
				/*
				 *	���̺��ѱ�ĳһ������ռ�ã���������������̺Ų����¼����̺��Ƿ���á�
				 */
	}
	if (tasks_free <= MIN_TASKS_LEFT_FOR_ROOT ||
	    this_user_tasks > MAX_TASKS_PER_USER)
		if (current->uid)
			return -EAGAIN;
				/*
				 *	ϵͳ�п��е�������Ѿ������ˣ������뵱ǰ�������е���������ͬһ���û�
				 * �������Ѿ��ܶ��ˡ�����������£���������Ϊ��ǰ�û��ٴ��������ˡ�
				 */
	return free_task;
			/*
			 *	���ؿ��е�����ţ����̺Ų���Ҫ���أ��� last_pid ����������Ѿ�û�п��е������
			 * �������ٴ��������ˣ��򷵻� -EAGAIN��
			 */
}

static struct file * copy_fd(struct file * old_file)
{
	struct file * new_file = get_empty_filp();
	int error;

	if (new_file) {
		memcpy(new_file,old_file,sizeof(struct file));
		new_file->f_count = 1;
		if (new_file->f_inode)
			new_file->f_inode->i_count++;
		if (new_file->f_op && new_file->f_op->open) {
			error = new_file->f_op->open(new_file->f_inode,new_file);
			if (error) {
				iput(new_file->f_inode);
				new_file->f_count = 0;
				new_file = NULL;
			}
		}
	}
	return new_file;
}

int dup_mmap(struct task_struct * tsk)
{
	struct vm_area_struct * mpnt, **p, *tmp;

	tsk->mmap = NULL;
	tsk->stk_vma = NULL;
	p = &tsk->mmap;
	for (mpnt = current->mmap ; mpnt ; mpnt = mpnt->vm_next) {
		tmp = (struct vm_area_struct *) kmalloc(sizeof(struct vm_area_struct), GFP_KERNEL);
		if (!tmp)
			return -ENOMEM;
		*tmp = *mpnt;
		tmp->vm_task = tsk;
		tmp->vm_next = NULL;
		if (tmp->vm_inode)
			tmp->vm_inode->i_count++;
		*p = tmp;
		p = &tmp->vm_next;
		if (current->stk_vma == mpnt)
			tsk->stk_vma = tmp;
	}
	return 0;
}

/*
 *	IS_CLONE: �ж��û��ռ䴥����ϵͳ������ fork ���� clone����������������Ե�ַ�ռ��
 * �����ǲ�һ���ġ�
 *
 *	copy_vm: ���� clone_flags ��״̬��copy �� clone ���Ե�ַ�ռ䡣���Ե�ַ�ռ�����ձ���
 * ��ʽ�������ַ�ռ䣬�����Ե�ַ�ռ��������ַ�ռ�֮���Ψһ���Ӿ���ҳ�����Ե�ַ�ռ�ͨ��
 * ҳ���е�ӳ���ϵת��Ϊһһ��Ӧ�������ַ�ռ䡣
 *
 *	��ˣ��������Ե�ַ�ռ�ʵ���Ͼ��Ǹ���ҳ�������ַ�ռ䡣
 */
#define IS_CLONE (regs.orig_eax == __NR_clone)
#define copy_vm(p) ((clone_flags & COPYVM)?copy_page_tables(p):clone_page_tables(p))

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in its entirety.
 */
/*
 *	sys_fork: ϵͳ���� fork ��Ӧ��ϵͳ���ô�����������������(�ӽ���)������ͽ���
 * �ĸ�����ͨ�õġ�
 *
 *	ϵͳ���� clone Ҳ����ִ�����������(sys.h ���� #define sys_clone sys_fork)��
 * �� sys_fork ����Ҫ���� clone �Ĵ������̣�clone ���ڴ���������(�ӽ��̻����߳�)��
 *
 *	���: struct pt_regs������ϵͳ����ʱ���б��������ļĴ�����
 *
 *	����ֵ: ִ�гɹ�ʱ�����ӽ��̵Ľ��̺ţ�ִ��ʧ��ʱ���� -EAGAIN��
 */
asmlinkage int sys_fork(struct pt_regs regs)
{
	struct pt_regs * childregs;
	struct task_struct *p;
	int i,nr;
	struct file *f;
	unsigned long clone_flags = COPYVM | SIGCHLD;
			/*
			 *	Ĭ�ϵĿ�¡��־������ fork ϵͳ���á������ clone ϵͳ���ã��������־����
			 * ������� clone ���ô���Ĳ����������á�
			 */

	if(!(p = (struct task_struct*)__get_free_page(GFP_KERNEL)))
		goto bad_fork;
			/*
			 *	��ȡһҳ�����ڴ�ҳ�����ڴ��������� task_struct �ṹ��������� task_struct
			 * �ṹ���ڴ�ҳ�����ʼ����ʼ��š�
			 */
	nr = find_empty_process();
	if (nr < 0)
		goto bad_fork_free;
			/*
			 *	��ȡ����ţ�nr < 0 ��ʾϵͳ���Ѿ�û�п��е�����ţ����߲������ٴ����������ˡ�
			 */
	task[nr] = p;
	*p = *current;
			/*
			 *	1. task[nr] ָ��������� task_struct �ṹ��
			 *
			 *	2. ��������� task_struct �ṹ���Ƹ������񣬺�������Ը��ƺ�� task_struct
			 * �ṹ�е�������һЩ�޸ģ���Ϊ�����������ṹ��������� task_struct �ṹ��û���޸�
			 * �Ĳ��ֽ��͸����񱣳�һ�¡�
			 */
	p->did_exec = 0;
			/*  */
	p->kernel_stack_page = 0;
			/*  */
	p->state = TASK_UNINTERRUPTIBLE;
			/*
			 *	�������״̬��Ϊ�����ж�˯��״̬����ֹ�������ڻ�δ��ʼ����֮ǰ������ִ�С�
			 */
	p->flags &= ~(PF_PTRACED|PF_TRACESYS);
			/*  */
	p->pid = last_pid;
			/*
			 *	�����������Ӧ�Ľ��̺�(�̺߳�)��ע��������ŵ�����
			 */
	p->swappable = 1;
			/*  */
	p->p_pptr = p->p_opptr = current;
	p->p_cptr = NULL;
	SET_LINKS(p);
			/*
			 *	1. ������մ���ʱ����ԭʼ����������ڸ������� current��������ʱû���Լ���
			 * ������
			 *
			 *	2. ���մ��������������ӵ�����������: һ��ϵͳ����������ı��������С�������
			 * �������븸�����������������ɵ������С�
			 */
	p->signal = 0;
			/*
			 *	�����񲻼̳и������Ѿ��յ����źš�
			 */
	p->it_real_value = p->it_virt_value = p->it_prof_value = 0;
	p->it_real_incr = p->it_virt_incr = p->it_prof_incr = 0;
			/*  */
	p->leader = 0;		/* process leadership doesn't inherit */
			/*
			 *	���̵��쵼Ȩ�ǲ��ܼ̳еģ��������顢�Ự����̳У����ӽ��̸մ���ʱ�븸����
			 * ����ͬ�Ľ����鼰�Ự��
			 */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
			/*
			 *	�������е�ʱ����ص�ͳ���� 0��
			 */
	p->min_flt = p->maj_flt = 0;
	p->cmin_flt = p->cmaj_flt = 0;
			/*  */
	p->start_time = jiffies;
			/*
			 *	����������ʼ���ڵ�ʱ��Ϊϵͳ��ǰ�ĵδ�����
			 */
/*
 * set up new TSS and kernel stack
 */
	if (!(p->kernel_stack_page = __get_free_page(GFP_KERNEL)))
		goto bad_fork_cleanup;
			/*
			 *	��ȡһҳ�����ڴ�ҳ��������������ں�̬ջ��
			 */
	p->tss.es = KERNEL_DS;
	p->tss.cs = KERNEL_CS;
	p->tss.ss = KERNEL_DS;
	p->tss.ds = KERNEL_DS;
	p->tss.fs = USER_DS;
	p->tss.gs = KERNEL_DS;
	p->tss.ss0 = KERNEL_DS;
	p->tss.esp0 = p->kernel_stack_page + PAGE_SIZE;
			/*
			 *	������Ķ�ѡ���: ��Щ��ѡ��������������һ�α���������ʱ���ص���ѡ���
			 * �Ĵ����У���Ϊ������ĳ�ʼ�ֳ���
			 *
			 *	������տ�ʼ����ʱ��λ�����źŴ���ĵط����źŴ��������ں˿ռ䣬���:
			 * CS = KERNEL_CS ���ڷ�����������ں˴���Σ�ES = DS = GS = KERNEL_DS ���ڷ���������
			 * ���ں����ݶΣ�FS = USER_DS ���ڷ�����������û����ݶΡ�SS = KERNEL_DS ��ʾ������
			 * �տ�ʼ����ʱʹ�õ�ջλ���ں����ݶ��У�����������ں�̬ջ��
			 *
			 *	SS0:ESP0 ָʾ��������ں�̬ջ�ĳ�ʼλ�ã����ں�̬ջ�����ڴ�ҳ���β����
			 * ����������SS0 �� ESP0 ��ֵ������������û�̬�����ں�̬ʱ���������Զ����ص�
			 * SS:ESP �С�
			 */
	p->tss.tr = _TSS(nr);
			/*
			 *	����������� TSS ��ѡ�����ֵ�����ֵ���������л�ʱ�õ���
			 */
	childregs = ((struct pt_regs *) (p->kernel_stack_page + PAGE_SIZE)) - 1;
	p->tss.esp = (unsigned long) childregs;
	p->tss.eip = (unsigned long) ret_from_sys_call;
	*childregs = regs;
			/*
			 *	esp �� eip ��ֵ�������������һ������ʱ���ص� ESP �� EIP �Ĵ����У�����ָʾ
			 * �������һ������ʱ��ջָ��ʹ���λ�á�
			 *
			 *	eip = ret_from_sys_call: �������һ������ʱ���� ret_from_sys_call ����Ҳ����
			 * �źŴ���ĵط���ʼִ�С�
			 *
			 *	Ϊ����������Դ� ret_from_sys_call ���������У����������˳��ں�̬���ص��û�̬��
			 * ��Ҫ����������ں�̬ջ�еļĴ�������Ϣ���Ƶ���������ں�̬ջ�У����� esp ָ��ǰ
			 * ջ��������������������һ��ִ��ʱ���ֳ���
			 */
	childregs->eax = 0;
			/*
			 *	�������ں�̬ջ�� EAX ��λ�õ�ֵ�� 0�����λ�ñ���ϵͳ���õķ���ֵ����ʾ fork
			 * �� clone ����ʱ������ķ���ֵΪ 0��
			 */
	p->tss.back_link = 0;
			/*
			 *	back_link: Linux �ں˲�ʹ��������ܡ�
			 */
	p->tss.eflags = regs.eflags & 0xffffcfff;	/* iopl is always 0 for a new process */
			/*
			 *	������̳и�����ı�־�Ĵ�����״̬�����ǲ��̳и������ IOPL(I/O ��Ȩ��)����
			 * �մ������������� I/O ��Ȩ������ 0��
			 */

	/*
	 *	if (IS_CLONE): ����� clone ϵͳ����:
	 *
	 *	clone ϵͳ����һ����Ҫ���� 3 ������: eax �Ĵ������ڴ���ϵͳ���ú� __NR_clone��
	 * ebx �Ĵ������ڴ�����������û�̬ջָ�룬ecx �Ĵ������ڴ��� clone ϵͳ���õı�־��
	 *
	 *	1. ���� clone ϵͳ���ñ�־��ԭ��: clone �� fork ����ϵͳ���ù���ͬһ��ϵͳ����
	 * ������ sys_fork����� sys_fork ����Ҫ���� clone �ķ�֧���� clone �� fork ������
	 * ��ַ�ռ�Ĵ���ʽ������ͬ���� sys_fork ��Ĭ��ʹ�õ� clone_flag ��Ϊ fork ϵͳ����
	 * ׼���ģ�������Ҫ�� clone ϵͳ�����д������ֱ�־��
	 *
	 *	2. �����������û�̬ջָ���ԭ��: ͨ�� clone ���ַ�ʽ�����������������֮Ϊ��
	 * �̣߳������񽫺͸�������ͬһ�����Ե�ַ�ռ䣬����ͬһ��ҳ����������ͬһ������
	 * �ڴ�ռ䡣����������£�������ʼ���в����ں�̬���ص��û�̬ʱ����Ҫʹ���������Լ�
	 * ���û�̬ջ�������������������û�̬ջָ�룬��ô�����񽫻�͸�����ʹ��ͬһ���û�̬
	 * ջָ�룬����������͸����񽫲���ͬһ���û�̬ջ����һ��������⡣
	 *	���Ե�ַ�ռ��е�ջָ����ͬ�������ַ�ռ��е�ջָ��Ҳһ����ͬ��
	 *
	 *	clone Ҳ���Դ������̣���ʱ��������� 0 ���ɡ�
	 *
	 *	3. fork ����Ҫ�����û�̬ջָ���ԭ��: ͨ�� fork ���ַ�ʽ�����������������֮Ϊ
	 * �ӽ��̣������������ĸ��Ƹ���������Ե�ַ�ռ䣬����������͸�����ͻ��и��Զ�����
	 * ���Ե�ַ�ռ䣬��˾ͻ��ж����������ڴ�ռ䡣�������񷵻ص��û�̬ʱ����Ȼջָ���λ��
	 * �͸�������ͬ������Ҳ�������ڲ�ͬ���Ե�ַ�ռ��е���ͬ��ַ���ѣ��������ַ�ռ��еĵ�ַ
	 * һ������ͬ������������͸�����ʹ�õ��ǲ�ͬ���û�̬ջ��
	 */
	if (IS_CLONE) {
		if (regs.ebx)
			childregs->esp = regs.ebx;
		clone_flags = regs.ecx;
		if (childregs->esp == regs.esp)
			clone_flags |= COPYVM;
	}
			/*
			 *	�� clone ϵͳ���ã����������:
			 *
			 *	1. ���������û�̬ջָ��Ϊ 0 ��ַ�����ʾ��������Ҫ���Ƹ���������Ե�ַ�ռ䣬
			 * ��������¶����Ե�ַ�ռ�Ĵ���ͺ� fork һ�£�ʵ���Ͼ��Ǵ����ӽ��̣����ַ�ʽ������
			 * ���ӽ������� fork ���������ӽ��̵Ĳ���ɴ���� clone_flags ��������
			 *
			 *	2. �����������Ч���û�̬ջָ�룬���ʾ�������븸������ͬһ�����Ե�ַ�ռ䣬
			 * �������ʵ���Ͼ��Ǵ������̡߳���ʱ��Ҫ���������񷵻��û�̬ʱ��ջָ�룬���������ں�
			 * ̬ջ�е� OLDESP λ�õ�ֵ�滻Ϊ clone �����ջָ�뼴�ɡ�
			 */

	p->exit_signal = clone_flags & CSIGNAL;
			/*
			 *	�����������˳�ʱ���źš��������������: ������� fork �����ӽ��̣����ӽ��̵�
			 * �˳��ź�Ϊ SIGCHLD��������� clone �����ӽ��̣����ӽ��̵��˳��ź����û����롣���
			 * ���� clone �������̣߳������̵߳��˳��ź����û����롣
			 */
	p->tss.ldt = _LDT(nr);
	if (p->ldt) {
		p->ldt = (struct desc_struct*) vmalloc(LDT_ENTRIES*LDT_ENTRY_SIZE);
		if (p->ldt != NULL)
			memcpy(p->ldt, current->ldt, LDT_ENTRIES*LDT_ENTRY_SIZE);
	}
			/*
			 *	1. ����������� LDT �εĶ�ѡ�����
			 *
			 *	2. ������������Լ��� LDT �Σ�����������Ҫ�����ĸ���һ�ݸ������ LDT �Ρ����
			 * û�У�������������񶼽�����ϵͳĬ�ϵ� LDT �� default_ldt���������ٸ����ˡ�
			 */
	p->tss.bitmap = offsetof(struct tss_struct,io_bitmap);
	for (i = 0; i < IO_BITMAP_SIZE+1 ; i++) /* IO bitmap is actually SIZE+1 */
		p->tss.io_bitmap[i] = ~0;
			/*
			 *	1. ����������� TSS ���е� BIT_MAP �ֶε�ֵ����ֵ��ʾ�� TSS �ο�ʼ���� I/O
			 * ���λͼ���� 16 λƫ��ֵ��ʵ���Ͼ��� TSS ���е� IO_BITMAP �ֶ��� tss_struct �ṹ
			 * ���е�ƫ�ơ�
			 *
			 *	2. ��������� TSS ���е� IO_BITMAP �ֶεı���λȫ���� 1����ʾ���е� I/O �˿�
			 * ��������ʱ�����ܷ��ʡ�
			 */
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0 ; frstor %0":"=m" (p->tss.i387));
			/*
			 *	������ʹ����ѧЭ�������������Ǹ��������������ѧЭ����������Ϣ�п���
			 * ��δ���µ�������� i387 �ṹ�У���ʱ�������Ƶĸ������ i387 �ṹ�е���Ϣ�п���
			 * �������µġ��ʴ˴���Ҫ�����������ѧЭ�������ĵ�ǰ������Ϣ���Ƹ�������� i387
			 * �ṹ��������� i387 �ṹ����Ϣ���������ط����¡�
			 */
	p->semun = NULL; p->shm = NULL;
			/*  */
	if (copy_vm(p) || shm_fork(current, p))
		goto bad_fork_cleanup;
			/*
			 *	1. ���������Ƹ���������Ե�ַ�ռ䣬���Ե�ַ�ռ�����ձ�����ʽ���������ַ
			 * �ռ䣬���ｫ����������¡���Ƹ������ҳ�������ڴ�ҳ�潫���ù����дʱ���Ƶ�
			 * ��ʽ��
			 *
			 *	2. TODO:
			 */
	if (clone_flags & COPYFD) {
		for (i=0; i<NR_OPEN;i++)
			if ((f = p->filp[i]) != NULL)
				p->filp[i] = copy_fd(f);
	} else {
		for (i=0; i<NR_OPEN;i++)
			if ((f = p->filp[i]) != NULL)
				f->f_count++;
	}
			/*  */
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
			/*  */
	dup_mmap(p);
			/*  */
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	if (p->ldt)
		set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,p->ldt, 512);
	else
		set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&default_ldt, 1);
			/*
			 *	1. �� GDT ����Ϊ������������ TSS �κ� LDT �εĶ������������������������������
			 * TSS �κ� LDT �ε���Ϣ(TSS �κ� LDT �����������ڴ����ַ���δ�С��)����������� TSS
			 * ��ѡ����� LDT ��ѡ�����ѡ���Ӧ����������
			 *
			 *	2. ����ϵͳ�� GDT ����Ϊÿһ������Ԥ���� LDT �����������������п��ܻ�û�� LDT
			 * �δ��ڡ�������������Լ��� LDT �δ��ڣ������Լ��� LDT �ε���Ϣ���������������������
			 * û���Լ��� LDT �δ��ڣ�����ϵͳĬ�ϵġ���ҹ��õ� LDT �� default_ldt ����Ϣ��������
			 * ������
			 */

	p->counter = current->counter >> 1;
	p->state = TASK_RUNNING;	/* do this last, just in case */
			/*
			 *	1. ����������տ�ʼ����ʱ��ʱ��Ƭ��Ϊ������ǰʱ��Ƭ��һ�롣�ڵ��ȳ����У�
			 * counter ��ֵԽ������Խ�ȱ����ȵ��������������ã���Ϊ���ø�������������֮ǰ���С�
			 * ���Ǹ�����һ������������֮ǰ��?
			 *
			 *	2. �����������״̬������֮�������񽫲���ϵͳ���Ȳ����С�
			 */
	return p->pid;
			/*
			 *	�����񽫷����������Ӧ�Ľ��̺�(�̺߳�)��
			 */
bad_fork_cleanup:
	task[nr] = NULL;
	REMOVE_LINKS(p);
	free_page(p->kernel_stack_page);
bad_fork_free:
	free_page((long) p);
bad_fork:
	return -EAGAIN;
}
