/*
 *  linux/kernel/exit.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#define DEBUG_PROC_TREE

#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/resource.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>

#include <asm/segment.h>
extern void shm_exit (void);
extern void sem_exit (void);

int getrusage(struct task_struct *, int, struct rusage *);

/*
 *	generate: Ϊ���� p �����ź� sig���ź�һ�����ɣ����ʾ�źŷ��ͳɹ���
 */
static int generate(unsigned long sig, struct task_struct * p)
{
	unsigned long mask = 1 << (sig-1);		/* �źŶ�Ӧ���ź�λͼ */
	struct sigaction * sa = sig + p->sigaction - 1;	/* �źŶ�Ӧ�� sigaction �ṹ */

	/* always generate signals for traced processes ??? */
	if (p->flags & PF_PTRACED) {
		p->signal |= mask;
		return 1;
	}
			/*
			 *	������� p �Ǳ����ٵ������򲻹�ʲô�źţ������͸�����
			 */
	/* don't bother with ignored signals (but SIGCHLD is special) */
	if (sa->sa_handler == SIG_IGN && sig != SIGCHLD)
		return 0;
			/*
			 *	������� p ���ź� sig �Ĵ����Ǻ��ԣ���ô��û�������� p ���� sig �źŵı�Ҫ�ˡ�
			 * ���� SIGCHLD �źűȽ����⣬������ check_pending ����˵����
			 */
	/* some signals are ignored by default.. (but SIGCONT already did its deed) */
	if ((sa->sa_handler == SIG_DFL) &&
	    (sig == SIGCONT || sig == SIGCHLD || sig == SIGWINCH))
		return 0;
			/*
			 *	�� 3 ���źŵ�Ĭ�ϴ����Ǻ��ԣ���û��Ҫ�ٷ����ˡ�
			 */
	p->signal |= mask;
			/* ������ p д���ź� sig���źŷ��ͳɹ� */
	return 1;
}

/*
 *	send_sig: ��ǰ���� current ������ p �����ź� sig��Ȩ��Ϊ priv��
 *
 *	priv: �Ƿ�ǿ�Ʒ����źš�priv == 1 ��ʾǿ�Ʒ����źţ�����Ҫ����������û����Ի򼶱�
 * priv == 0 ��ʾ��ǿ�Ʒ����źţ�����ǰ��Ҫ�жϵ�ǰ�����Ƿ��������� p �����źŵ�Ȩ����
 */
int send_sig(unsigned long sig,struct task_struct * p,int priv)
{
	if (!p || sig > 32)
		return -EINVAL;
			/* ������Ч */
	if (!priv && ((sig != SIGCONT) || (current->session != p->session)) &&
	    (current->euid != p->euid) && (current->uid != p->uid) && !suser())
		return -EPERM;
			/*
			 *	1. ��ǿ�Ʒ����źš�
			 *	2. Ҫ���͵��źŲ��� SIGCONT �� ��ǰ���������� p ����ͬһ���Ự�С�
			 *	3. ��ǰ���������� p ���ڲ�����ͬһ���û���
			 *	4. ��ǰ���������� p ����ͬһ���û������ġ�
			 *	5. ��ǰ���������û����ǳ����û���
			 *
			 *	������������������˵����ǰ���� current ��Ȩ������ p �����źš�
			 */
	if (!sig)
		return 0;
			/* �ź�ֵ�� 1 ��ʼ */

	/*
	 *	if:	SIGKILL --- ɱ������
	 *		SIGCONT --- �ô���ֹͣ״̬������ָ����С�
	 */
	if ((sig == SIGKILL) || (sig == SIGCONT)) {
		if (p->state == TASK_STOPPED)
			p->state = TASK_RUNNING;
				/*
				 *	ϵͳֻ�ܴ���ǰ�������е�������źţ���˶��ڴ���ֹͣ״̬��������Ҫ
				 * ������Ϊ����̬��ʹ�������Ƚ����������С�Ҳ����˵����Ҫɱ������ҲҪ����
				 * ������������Ȼ������ɱ��
				 *
				 *	���ڿ��ж�˯��״̬��������ڵ��ȳ����л��ѡ����ڲ����ж�˯��״̬������
				 * ���ܱ��źŻ��ѣ�ֻ�ܱ� wake_up ��ʾ���ѡ�
				 */
		p->exit_code = 0;
		p->signal &= ~( (1<<(SIGSTOP-1)) | (1<<(SIGTSTP-1)) |
				(1<<(SIGTTIN-1)) | (1<<(SIGTTOU-1)) );
				/*
				 *	1. �����Ѿ������˳����е�״̬�ˣ�����Ҫ��λ������˳��롣
				 *
				 *	2. ȥ�������Ѿ��յ��ĵ���δ����Ļᵼ���������ֹͣ״̬���źš���Ϊ
				 * ������Щ�ź�ʱ�����������½���ֹͣ״̬����Ҫ���͵��źų�ͻ��
				 */
	}

	/* Depends on order SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU */
	if ((sig >= SIGSTOP) && (sig <= SIGTTOU)) 
		p->signal &= ~(1<<(SIGCONT-1));
			/*
			 *	���Ҫ���͵��ź����� 4 ���ź��е�����һ������˵��Ҫ�ý����źŵ����� p ֹͣ
			 * ���С������Ҫ������ p �Ѿ��յ�����δ����Ŀ���������������е��ź� SIGCONT ȥ����
			 */
	/* Actually generate the signal */
	generate(sig,p);
			/* �����źţ����������ź� */
	return 0;
}

/*
 *	notify_parent: ����֪ͨ tsk �����ڸ��������� tsk �Ѿ��˳��������ڸ����������ա�
 * ��������ڸ�����ʵ���Ͼ���ԭʼ������������ڸ������ԭʼ��������ͬһ���������
 * �ڵ��� notify_parent ֪֮ͨǰ�����ڸ���������Ϊԭʼ������
 */
void notify_parent(struct task_struct * tsk)
{
	if (tsk->p_pptr == task[1])
		tsk->exit_signal = SIGCHLD;
	send_sig(tsk->exit_signal, tsk->p_pptr, 1);
	wake_up_interruptible(&tsk->p_pptr->wait_chldexit);
			/*
			 *	1. ������� tsk �ĸ������� 1 ������(init ����)�������� tsk ���˳��źŸ�Ϊ
			 * SIGCHLD��???
			 *
			 *	2. �������˳�ʱӦ�÷��͸���������ź�(exit_signal)���͸��丸����
			 *
			 *	3. ���Ѹ������ wait_chldexit ������˯�ߵȴ��������������������еȴ�����
			 * ��Ҳ�Ǹ������Լ���
			 */
}

/*
 *	release: �ͷ����� p ��ռ�е����һ����Դ��ʹ�䳹����ʧ��
 *
 *	�ڴ�֮ǰ������ p �Լ��Ѿ��ͷ��˴󲿷���Դ������˽�ʬ̬���޷������У�ʣ������һ��
 * ��Դֻ�����丸������յ�ʱ�����ͷ��ˡ�
 */
void release(struct task_struct * p)
{
	int i;

	if (!p)
		return;
	if (p == current) {
		printk("task releasing itself\n");
		return;
	}

	/*
	 *	for: ������ 0 ������(idle)�������������Ѱ������ p��
	 */
	for (i=1 ; i<NR_TASKS ; i++)
		if (task[i] == p) {
			task[i] = NULL;
			REMOVE_LINKS(p);
			free_page(p->kernel_stack_page);
			free_page((long) p);
			return;
					/*
					 *	1. �ͷ�������ռ�������(task[i])��
					 *
					 *	2. ��������丸������������������ɵ�������ɾ����
					 *
					 *	3. �ͷ�������ں�̬ջ��ռ�õ������ڴ�ҳ�档
					 *
					 *	4. �ͷ������ task_struct �ṹ��ռ�õ������ڴ�ҳ�档
					 *
					 *	��Щ��Դ�ͷ�֮������ͳ�����ʧ�ˡ�
					 */
		}
	panic("trying to release non-existent task");
}

#ifdef DEBUG_PROC_TREE
/*
 * Check to see if a task_struct pointer is present in the task[] array
 * Return 0 if found, and 1 if not found.
 */
int bad_task_ptr(struct task_struct *p)
{
	int 	i;

	if (!p)
		return 0;
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] == p)
			return 0;
	return 1;
}
	
/*
 * This routine scans the pid tree and make sure the rep invarient still
 * holds.  Used for debugging only, since it's very slow....
 *
 * It looks a lot scarier than it really is.... we're doing nothing more
 * than verifying the doubly-linked list found in p_ysptr and p_osptr, 
 * and checking it corresponds with the process tree defined by p_cptr and 
 * p_pptr;
 */
void audit_ptree(void)
{
	int	i;

	for (i=1 ; i<NR_TASKS ; i++) {
		if (!task[i])
			continue;
		if (bad_task_ptr(task[i]->p_pptr))
			printk("Warning, pid %d's parent link is bad\n",
				task[i]->pid);
		if (bad_task_ptr(task[i]->p_cptr))
			printk("Warning, pid %d's child link is bad\n",
				task[i]->pid);
		if (bad_task_ptr(task[i]->p_ysptr))
			printk("Warning, pid %d's ys link is bad\n",
				task[i]->pid);
		if (bad_task_ptr(task[i]->p_osptr))
			printk("Warning, pid %d's os link is bad\n",
				task[i]->pid);
		if (task[i]->p_pptr == task[i])
			printk("Warning, pid %d parent link points to self\n",
				task[i]->pid);
		if (task[i]->p_cptr == task[i])
			printk("Warning, pid %d child link points to self\n",
				task[i]->pid);
		if (task[i]->p_ysptr == task[i])
			printk("Warning, pid %d ys link points to self\n",
				task[i]->pid);
		if (task[i]->p_osptr == task[i])
			printk("Warning, pid %d os link points to self\n",
				task[i]->pid);
		if (task[i]->p_osptr) {
			if (task[i]->p_pptr != task[i]->p_osptr->p_pptr)
				printk(
			"Warning, pid %d older sibling %d parent is %d\n",
				task[i]->pid, task[i]->p_osptr->pid,
				task[i]->p_osptr->p_pptr->pid);
			if (task[i]->p_osptr->p_ysptr != task[i])
				printk(
		"Warning, pid %d older sibling %d has mismatched ys link\n",
				task[i]->pid, task[i]->p_osptr->pid);
		}
		if (task[i]->p_ysptr) {
			if (task[i]->p_pptr != task[i]->p_ysptr->p_pptr)
				printk(
			"Warning, pid %d younger sibling %d parent is %d\n",
				task[i]->pid, task[i]->p_osptr->pid,
				task[i]->p_osptr->p_pptr->pid);
			if (task[i]->p_ysptr->p_osptr != task[i])
				printk(
		"Warning, pid %d younger sibling %d has mismatched os link\n",
				task[i]->pid, task[i]->p_ysptr->pid);
		}
		if (task[i]->p_cptr) {
			if (task[i]->p_cptr->p_pptr != task[i])
				printk(
			"Warning, pid %d youngest child %d has mismatched parent link\n",
				task[i]->pid, task[i]->p_cptr->pid);
			if (task[i]->p_cptr->p_ysptr)
				printk(
			"Warning, pid %d youngest child %d has non-NULL ys link\n",
				task[i]->pid, task[i]->p_cptr->pid);
		}
	}
}
#endif /* DEBUG_PROC_TREE */

/*
 * This checks not only the pgrp, but falls back on the pid if no
 * satisfactory prgp is found. I dunno - gdb doesn't work correctly
 * without this...
 */
int session_of_pgrp(int pgrp)
{
	struct task_struct *p;
	int fallback;

	fallback = -1;
	for_each_task(p) {
 		if (p->session <= 0)
 			continue;
		if (p->pgrp == pgrp)
			return p->session;
		if (p->pid == pgrp)
			fallback = p->session;
	}
	return fallback;
}

/*
 * kill_pg() sends a signal to a process group: this is what the tty
 * control characters do (^C, ^Z etc)
 */
/*
 *	kill_pg: �����Ϊ pgrp �Ľ������е����н��̷����ź� sig��Ȩ��Ϊ priv��
 */
int kill_pg(int pgrp, int sig, int priv)
{
	struct task_struct *p;
	int err,retval = -ESRCH;
	int found = 0;

	if (sig<0 || sig>32 || pgrp<=0)
		return -EINVAL;

	/*
	 *	����ϵͳ�г� init_task ��������������������� pgrp �е����н��̡�
	 */
	for_each_task(p) {
		if (p->pgrp == pgrp) {
			if ((err = send_sig(sig,p,priv)) != 0)
				retval = err;
			else
				found++;
		}
				/*
				 *	���̵�������������� pgrp ��ͬ��������̷����ź� sig��
				 */
	}
	return(found ? 0 : retval);
			/*
			 *	1. �������û�н��̣��򷵻� 0��
			 *
			 *	2. ��������н��̣������н��̵��źž�����ʧ�ܣ��򷵻ط���ʧ�ܵĴ����롣
			 *
			 *	3. ��������н��̣���������һ�����̵��źŷ��ͳɹ����򷵻������źŷ��ͳɹ���
			 * ���̵ĸ�����
			 */
}

/*
 * kill_sl() sends a signal to the session leader: this is used
 * to send SIGHUP to the controlling process of a terminal when
 * the connection is lost.
 */
/*
 *	kill_sl: ��Ự��Ϊ sess �ĻỰ�е�������̷����ź� sig��Ȩ��Ϊ priv��
 */
int kill_sl(int sess, int sig, int priv)
{
	struct task_struct *p;
	int err,retval = -ESRCH;
	int found = 0;

	if (sig<0 || sig>32 || sess<=0)
		return -EINVAL;
	/* ����ϵͳ�г� init_task ������������������һỰ��Ϊ sess �ĻỰ�е�������� */
	for_each_task(p) {
		if (p->session == sess && p->leader) {
			if ((err = send_sig(sig,p,priv)) != 0)
				retval = err;
			else
				found++;
		}
	}
	return(found ? 0 : retval);
}

/*
 *	kill_proc: ����̺�Ϊ pid �Ľ��̷����ź� sig��Ȩ��Ϊ priv��
 */
int kill_proc(int pid, int sig, int priv)
{
 	struct task_struct *p;

	if (sig<0 || sig>32)
		return -EINVAL;
	/* ����ϵͳ�г� init_task ������������������ҽ��̺�Ϊ pid �Ľ��� */
	for_each_task(p) {
		if (p && p->pid == pid)
			return send_sig(sig,p,priv);
	}
	return(-ESRCH);
}

/*
 * POSIX specifies that kill(-1,sig) is unspecified, but what we have
 * is probably wrong.  Should make it like BSD or SYSV.
 */
/*
 *	sys_kill: ϵͳ���� kill ��Ӧ��ϵͳ���ô��������������κν��̻�����鷢��
 * �κ��źţ�������ֻ��ɱ�����̡�
 *
 *	���� pid �ǽ��̺ţ�sig ��Ҫ���͵��źţ����� pid �Ĳ�ͬ�������¼������:
 *
 *	1. pid > 0: ���ź� sig �������͸����̺��� pid �Ľ��̡�
 *
 *	2. pid == 0: ���ź� sig �������͸���ǰ���������Ľ������е����н��̡�
 *
 *	3. pid == -1: ���ź� sig �������͸�ϵͳ�г� 0 �Ž��̺� 1 �Ž��̼���ǰ������������н��̡�
 *
 *	4. pid < -1: ���ź� sig �������͸������� -pid �е����н��̡�
 */
asmlinkage int sys_kill(int pid,int sig)
{
	int err, retval = 0, count = 0;

	if (!pid)
		return(kill_pg(current->pgrp,sig,0));
	if (pid == -1) {
		struct task_struct * p;
		for_each_task(p) {
			/* for_each_task ���˵� 0 �Ž��̣�������˵� 1 �Ž��̺͵�ǰ���� */
			if (p->pid > 1 && p != current) {
				++count;
				if ((err = send_sig(sig,p,0)) != -EPERM)
					retval = err;
			}
		}
		return(count ? retval : -ESRCH);
	}
	if (pid < 0) 
		return(kill_pg(-pid,sig,0));	/* pid < -1 */
	/* Normal kill */
	return(kill_proc(pid,sig,0));		/* pid > 0 */
}

/*
 * Determine if a process group is "orphaned", according to the POSIX
 * definition in 2.2.2.52.  Orphaned process groups are not to be affected
 * by terminal-generated stop signals.  Newly orphaned process groups are 
 * to receive a SIGHUP and a SIGCONT.
 * 
 * "I ask you, have you ever known what it is to be an orphan?"
 */
int is_orphaned_pgrp(int pgrp)
{
	struct task_struct *p;

	for_each_task(p) {
		if ((p->pgrp != pgrp) || 
		    (p->state == TASK_ZOMBIE) ||
		    (p->p_pptr->pid == 1))
			continue;
		if ((p->p_pptr->pgrp != pgrp) &&
		    (p->p_pptr->session == p->session))
			return 0;
	}
	return(1);	/* (sighing) "Often!" */
}

static int has_stopped_jobs(int pgrp)
{
	struct task_struct * p;

	for_each_task(p) {
		if (p->pgrp != pgrp)
			continue;
		if (p->state == TASK_STOPPED)
			return(1);
	}
	return(0);
}

static void forget_original_parent(struct task_struct * father)
{
	struct task_struct * p;

	for_each_task(p) {
		if (p->p_opptr == father)
			if (task[1])
				p->p_opptr = task[1];
			else
				p->p_opptr = task[0];
	}
}

NORET_TYPE void do_exit(long code)
{
	struct task_struct *p;
	int i;

fake_volatile:
	if (current->semun)
		sem_exit();
	if (current->shm)
		shm_exit();
	free_page_tables(current);
	for (i=0 ; i<NR_OPEN ; i++)
		if (current->filp[i])
			sys_close(i);
	forget_original_parent(current);
	iput(current->pwd);
	current->pwd = NULL;
	iput(current->root);
	current->root = NULL;
	iput(current->executable);
	current->executable = NULL;
	/* Release all of the old mmap stuff. */
	
	{
		struct vm_area_struct * mpnt, *mpnt1;
		mpnt = current->mmap;
		current->mmap = NULL;
		while (mpnt) {
			mpnt1 = mpnt->vm_next;
			if (mpnt->vm_ops && mpnt->vm_ops->close)
				mpnt->vm_ops->close(mpnt);
			kfree(mpnt);
			mpnt = mpnt1;
		}
	}

	if (current->ldt) {
		vfree(current->ldt);
		current->ldt = NULL;
		for (i=1 ; i<NR_TASKS ; i++) {
			if (task[i] == current) {
				set_ldt_desc(gdt+(i<<1)+FIRST_LDT_ENTRY, &default_ldt, 1);
				load_ldt(i);
			}
		}
	}

	current->state = TASK_ZOMBIE;
	current->exit_code = code;
	current->rss = 0;
	/* 
	 * Check to see if any process groups have become orphaned
	 * as a result of our exiting, and if they have any stopped
	 * jobs, send them a SIGUP and then a SIGCONT.  (POSIX 3.2.2.2)
	 *
	 * Case i: Our father is in a different pgrp than we are
	 * and we were the only connection outside, so our pgrp
	 * is about to become orphaned.
 	 */
	if ((current->p_pptr->pgrp != current->pgrp) &&
	    (current->p_pptr->session == current->session) &&
	    is_orphaned_pgrp(current->pgrp) &&
	    has_stopped_jobs(current->pgrp)) {
		kill_pg(current->pgrp,SIGHUP,1);
		kill_pg(current->pgrp,SIGCONT,1);
	}
	/* Let father know we died */
	notify_parent(current);
	
	/*
	 * This loop does two things:
	 * 
  	 * A.  Make init inherit all the child processes
	 * B.  Check to see if any process groups have become orphaned
	 *	as a result of our exiting, and if they have any stopped
	 *	jobs, send them a SIGHUP and then a SIGCONT.  (POSIX 3.2.2.2)
	 */
	while ((p = current->p_cptr) != NULL) {
		current->p_cptr = p->p_osptr;
		p->p_ysptr = NULL;
		p->flags &= ~(PF_PTRACED|PF_TRACESYS);
		if (task[1] && task[1] != current)
			p->p_pptr = task[1];
		else
			p->p_pptr = task[0];
		p->p_osptr = p->p_pptr->p_cptr;
		p->p_osptr->p_ysptr = p;
		p->p_pptr->p_cptr = p;
		if (p->state == TASK_ZOMBIE)
			notify_parent(p);
		/*
		 * process group orphan check
		 * Case ii: Our child is in a different pgrp 
		 * than we are, and it was the only connection
		 * outside, so the child pgrp is now orphaned.
		 */
		if ((p->pgrp != current->pgrp) &&
		    (p->session == current->session) &&
		    is_orphaned_pgrp(p->pgrp) &&
		    has_stopped_jobs(p->pgrp)) {
			kill_pg(p->pgrp,SIGHUP,1);
			kill_pg(p->pgrp,SIGCONT,1);
		}
	}
	if (current->leader)
		disassociate_ctty(1);
	if (last_task_used_math == current)
		last_task_used_math = NULL;
#ifdef DEBUG_PROC_TREE
	audit_ptree();
#endif
	schedule();
/*
 * In order to get rid of the "volatile function does return" message
 * I did this little loop that confuses gcc to think do_exit really
 * is volatile. In fact it's schedule() that is volatile in some
 * circumstances: when current->state = ZOMBIE, schedule() never
 * returns.
 *
 * In fact the natural way to do all this is to have the label and the
 * goto right after each other, but I put the fake_volatile label at
 * the start of the function just in case something /really/ bad
 * happens, and the schedule returns. This way we can try again. I'm
 * not paranoid: it's just that everybody is out to get me.
 */
	goto fake_volatile;
}

asmlinkage int sys_exit(int error_code)
{
	do_exit((error_code&0xff)<<8);
}

/*
 *	sys_wait4: ϵͳ���� wait4 ��Ӧ��ϵͳ���ô����������ڵȴ�ָ�����������˳�(��ֹ)��
 * ��ָ�����������˳�ʱ������ռ�е���Դ���л��գ�����ʹ�䳹����ʧ��
 *
 *	1. һ�����񳹵���ʧ���������׶�: ��һ���׶��������Լ��˳�(��ֹ)�׶Σ�������׶���
 * ���������Լ����ͷ�����ռ�еĴ󲿷���Դ��ֻ����һС������Դ��������ִֹ�У���ɽ�ʬ̬��
 * Ȼ��ȴ��丸���������ա��ڶ��׶��Ǹ�������ս׶Σ�������׶��ｫ�ɸ��������ͷ���������
 * ��һС������Դ��������ռ�е�������Դ�ͷ����֮�����񽫳�����ʧ��
 *
 *	2. ��ˣ�wait4 ϵͳ���õĹ�����: ���ȵȴ�ָ�����������˳���Ҳ���ǵȴ������񳹵�
 * ��ʧ�ĵ�һ�׶ν������������״̬��ɽ�ʬ̬��Ȼ��ָ�����������˳���ִ�������񳹵���ʧ
 * �ĵڶ��׶Σ����ո�������ʹ�䳹����ʧ��
 *
 *	3. �ȴ��������˳������������:
 *	һ�� sys_wait4 ִ��ʱָ�����������Ѿ��˳��ˣ�Ҳ����״̬�Ѿ������ TASK_ZOMBIE��
 * ��ʱ��ϵͳ����������״̬�Ѿ����֣����Ի�ֱ�ӻ����������˳�ϵͳ���á�
 *	���� sys_wait4 ִ��ʱָ����������δ�˳�����ʱ����Ҫ����ǰ����������ȴ���ֱ��
 * ָ�����������˳����յ��жϱ�ϵͳ���õ��ź�Ϊֹ��
 *	����ָ����������ѹ���Ͳ����ڣ���ʱϵͳ���û�ֱ���˳������� -ECHILD��
 *
 *	4. �û�ָ���� options ��������� TASK_STOPPED ״̬��Ӱ�� sys_wait4 ��ִ�����̡�
 *
 *	5. wait4 ϵͳ����ÿ��ֻ�ܵȴ�һ���������˳������Ҫ�ȴ�����������˳�������Ҫ���
 * ���� wait4 ϵͳ���á���Ȼ���˳����������ǵ�ǰ���� current ��������
 *
 *
 *	���:	pid --- �������Ӧ�Ľ��̺š�
 *		stat_addr --- ָ��ǰ������û�̬�ռ��ָ�룬����û�̬�ռ��������û�����
 *			��������˳��롣
 *		options --- �ȴ��������˳�ʱ��ѡ�
 *		ru --- ָ��ǰ������û�̬�ռ��ָ�룬����û�̬�ռ��������û������������
 *			��Դʹ����Ϣ��
 *
 *	���ݲ��� pid �Ĳ�ͬ�������¼������:
 *
 *	1. pid > 0: ��ʾ��ǰ�������ڵȴ����̺ŵ��� pid ���������˳���
 *
 *	2. pid == 0: ��ʾ��ǰ�������ڵȴ�������ŵ��ڵ�ǰ���������ŵ��κ�һ���������˳���
 * Ҳ���ǵȴ��뵱ǰ���� current ͬ����κ�һ���������˳���
 *
 *	3. pid == -1: ��ʾ��ǰ�������ڵȴ��κ�һ���������˳���
 *
 *	4. pid < -1: ��ʾ��ǰ�������ڵȴ�������ŵ��� -pid ���κ�һ���������˳���
 */
asmlinkage int sys_wait4(pid_t pid,unsigned long * stat_addr, int options, struct rusage * ru)
{
	int flag, retval;
	struct wait_queue wait = { current, NULL };
	struct task_struct *p;

	if (stat_addr) {
		flag = verify_area(VERIFY_WRITE, stat_addr, 4);
		if (flag)
			return flag;
	}
			/*
			 *	��֤ stat_addr ָ������ڱ����������˳�״̬���û�̬�ռ��Ƿ��д��
			 */
	add_wait_queue(&current->wait_chldexit,&wait);
			/*
			 *	����ǰ������뵽�ȴ��������˳��ĵȴ������ϡ������ǰ������������û���˳���
			 * ����˯��״̬�ȴ��������������˳����� notify_parent ֪ͨ�������ʱ��ỽ�ѹ���
			 * wait_chldexit �����ϵĵ�ǰ����
			 */
repeat:
	/*
	 *	for: �ӵ�ǰ������������������ʼɨ�赱ǰ���������������ֱ���ҵ�ָ��
	 * ���������ɨ�������е�������Ϊֹ��
	 */
	flag=0;
 	for (p = current->p_cptr ; p ; p = p->p_osptr) {
		if (pid>0) {
			if (p->pid != pid)
				continue;
					/* pid > 0: Ѱ�ҽ��̺ŵ��� pid �������� */
		} else if (!pid) {
			if (p->pgrp != current->pgrp)
				continue;
					/* pid == 0: Ѱ���븸����λ��ͬһ�������������һ�������� */
		} else if (pid != -1) {
			if (p->pgrp != -pid)
				continue;
					/* pid < -1: Ѱ���� -pid ͬ�������һ�������� */
		}
					/*
					 *	pid == -1: ����һ��������
					 */

		/* wait for cloned processes iff the __WCLONE flag is set */
		if ((p->exit_signal != SIGCHLD) ^ ((options & __WCLONE) != 0))
			continue;
					/*
					 *	���ҵ�������������һ��������:
					 *
					 *	1. ����û������� __WCLONE ��־�����ҵ�����������˳��ź�Ϊ
					 * SIGCHLD���������ǰ�����񣬼���Ѱ����һ������������������
					 *
					 *	�û����� __WCLONE ��ʾֻ�ȴ�ͨ�� clone ��ʽ�������������˳���
					 * Ҳ����ֻ�ȴ����߳��˳��������߳��˳�ʱ�ǲ����� SIGCHLD �ź���֪ͨ
					 * �丸����ġ�
					 *
					 *	2. ����û�δ���� __WCLONE ��־�����ҵ�����������˳��źŲ���
					 * SIGCHLD���������ǰ�����񣬼���Ѱ����һ������������������
					 *
					 *	�û�δ���� __WCLONE ��ʾֻ�ȴ�ͨ�� fork ��ʽ�������������˳���
					 * Ҳ����ֻ�ȴ��ӽ����˳������ӽ����˳�ʱ��Ҫ�� SIGCHLD �ź���֪ͨ��
					 * ������
					 */

		/*
		 *	flag = 1: �ҵ���һ������������������
		 */
		flag = 1;
		switch (p->state) {
			/* ѡ������������ֹͣ״̬ */
			case TASK_STOPPED:
				if (!p->exit_code)
					continue;
				if (!(options & WUNTRACED) && !(p->flags & PF_PTRACED))
					continue;
						/*
						 *	1. �����������˳����Ѿ��������ˣ������Ѱ����һ��
						 * ���������������񣬷������Ҫ�жϱ�ϵͳ���ô������Ƿ�
						 * ��Ҫ���Ϸ��ء�
						 *
						 *	2. ����û�û������ WUNTRACED ��־���������ϵͳ����
						 * δ�����٣����ʾ��ϵͳ���ô����������������أ���˼���
						 * Ѱ����������������������
						 *
						 *	3. ����û������� WUNTRACED ��־�����ʾ������������
						 * ��������ֹͣ״̬ʱ����ϵͳ���ô�������Ҫ���Ϸ��أ���ʱ
						 * ���뽫��������ִ�в������˳�ϵͳ���ô�������
						 *	��Ȼ���������ϵͳ���ñ�����ʱҲ��Ҫ���Ϸ��ء�
						 */
				if (stat_addr)
					put_fs_long((p->exit_code << 8) | 0x7f,
						stat_addr);
				p->exit_code = 0;
				if (ru != NULL)
					getrusage(p, RUSAGE_BOTH, ru);
				retval = p->pid;
				goto end_wait4;
						/*
						 *	1. ����������˳���д�뵽 stat_addr ָ����û��ռ��У�
						 * ���ֽڱ����˳��룬���ֽڱ���״̬��Ϣ 0x7F��0x7F ��ʾ������
						 * ����ֹͣ״̬��
						 *
						 *	2. ��������˳����ѱ�����(���ظ����û�)������Ҫ���˳���
						 * �����������˳������Ϊ�˸��߱�����Ϊʲô�˳������ڱ����Ѿ�
						 * ֪������Ϊʲô�˳�������Ҳ��û�б�Ҫ�ٱ����˳�ԭ���ˡ�
						 *
						 *	��������һ���������������ִ�б�ϵͳ����ʱ���������
						 * �����״̬û�иı䣬һֱ�� TASK_STOPPED����ʱ�ͻ����ʼ��
						 * �˳����жϵĵط�������������
						 *
						 *	3. �����������Դʹ����Ϣд�뵽 ru ָ����û��ռ��С�
						 *
						 *	4. �����������Ӧ�Ľ��̺š�
						 */

			/* ѡ�����������ڽ�ʬ״̬����Ҳ�Ǳ�ϵͳ����������״̬ */
			case TASK_ZOMBIE:
				current->cutime += p->utime + p->cutime;
				current->cstime += p->stime + p->cstime;
				current->cmin_flt += p->min_flt + p->cmin_flt;
				current->cmaj_flt += p->maj_flt + p->cmaj_flt;
						/*
						 *	1. ���������û�̬���ں�̬������ʱ��ֱ��ۼӵ��������У�
						 * �����ʱ�仹����������������������ʱ�䡣
						 *
						 *	2.
						 */
				if (ru != NULL)
					getrusage(p, RUSAGE_BOTH, ru);
				flag = p->pid;
				if (stat_addr)
					put_fs_long(p->exit_code, stat_addr);
						/*
						 *	1. �����������Դʹ����Ϣд�뵽 ru ָ����û��ռ��С�
						 *
						 *	2. ��ʱ�����������Ӧ�Ľ��̺ţ����ں��淵�ظý��̺š�
						 *
						 *	3. ����������˳���д�뵽 stat_addr ָ����û��ռ��С�
						 */
				if (p->p_opptr != p->p_pptr) {
					REMOVE_LINKS(p);
					p->p_pptr = p->p_opptr;
					SET_LINKS(p);
					notify_parent(p);
				} else
					release(p);
#ifdef DEBUG_PROC_TREE
				audit_ptree();
#endif
				retval = flag;
				goto end_wait4;
						/*
						 *	1. ����������ԭʼ�����������ڸ�����(current)����ͬ
						 * һ������: ���Ƚ�������������ڸ���������������ɵ�������
						 * ɾ����Ȼ�����ø�����������ڸ�����Ϊԭʼ�������ٽ�����
						 * �������²��뵽���ڸ�����(ԭʼ������)�������������У����
						 * ֪ͨ������������ڸ�����(ԭʼ������)�����������ո�������
						 *
						 *	Ҳ����˵: ���� TASK_ZOMBIE ״̬������������ջ���Ӧ��
						 * ����ԭʼ������(������)����ɣ����ڸ�������Ȩ���ո�������
						 *
						 *	2. ����������ԭʼ����������ڸ�������ͬһ��������
						 * �����ڸ�����ֱ�ӻ���������ռ�еĻ�û���ͷŵ�������Դ������
						 * ֮��������񽫳�����ʧ��
						 *
						 *	3. ���շ����������Ӧ�Ľ��̺š�
						 */
			default:
				continue;
						/*
						 *	�����������������״̬������Ҫ�������Ѱ����һ������
						 * ������������
						 */
		}
	}

	/*
	 *	if: ����������������δ�˳�������Ҫ�ȴ����˳���
	 */
	if (flag) {
		retval = 0;
		if (options & WNOHANG)
			goto end_wait4;
		current->state=TASK_INTERRUPTIBLE;
		schedule();
				/*
				 *	1. WNOHANG ��־Ҫ��ָ����������û���˳�(��ֹ)ʱ���������أ���ʱ��
				 * ����ֵ�� 0��
				 *
				 *	2. ����û�û������ WNOHANG ��־������Ҫ����ǰ����������ȴ���ֱ��
				 * �������������������˳������ߵ�ǰ�����յ����жϱ�ϵͳ���õ��ź�Ϊֹ��
				 */
		current->signal &= ~(1<<(SIGCHLD-1));
		retval = -ERESTARTSYS;
		if (current->signal & ~current->blocked)
			goto end_wait4;
		goto repeat;
				/*
				 *	��ǰ�����ٴε���ִ�У���Ҫ����������ѵ�ԭ��:
				 *
				 *	1. ����յ��� SIGCHLD ���������δ�������źţ�����Ҫ�жϱ���ϵͳ���ã�
				 * ת��ȥ�����յ����źţ���ʱ��ϵͳ���õķ���ֵΪ -ERESTARTSYS����ʾ��Ҫ����
				 * ��ϵͳ���ã�����ϵͳ�����Ƿ��ܹ������ɹ���Ҫ���źŵ� SA_RESTART ��־��
				 *
				 *	2. ����˵��������ָ����������������µ��������˳��ˣ������Ҫ��ͷ��ʼ
				 * ������������ļ�⼰�������̡�
				 */
	}

	retval = -ECHILD;
			/*
			 *	û�з���Ҫ�����������ڣ���ϵͳ����ֱ���˳������� -ECHILD��
			 */
end_wait4:
	remove_wait_queue(&current->wait_chldexit,&wait);
	return retval;
			/*
			 *	ϵͳ���ô�����ִ����ϣ���ǰ�����Ѳ��ٵȴ��������˳����򽫵�ǰ�����
			 * �ȴ��������˳��ĵȴ�������ɾ����
			 */
}

/*
 * sys_waitpid() remains for compatibility. waitpid() should be
 * implemented by calling sys_wait4() from libc.a.
 */
/*
 *	sys_waitpid: ϵͳ���� waitpid ��Ӧ��ϵͳ���ô����������ڵȴ�ָ�����������˳�(��ֹ)
 * �������˳���������waitpid ϵͳ���ò���ȡָ�������������Դʹ�������
 */
asmlinkage int sys_waitpid(pid_t pid,unsigned long * stat_addr, int options)
{
	return sys_wait4(pid, stat_addr, options, NULL);
}
