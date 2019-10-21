/*
 *  linux/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>

#include <asm/segment.h>

/*
 *	_S(nr): ��ȡ�ź� nr ��Ӧ���ź�λͼ��
 */
#define _S(nr) (1<<((nr)-1))

/*
 *	_BLOCKABLE: ��ʾ�ɱ��������źţ����� SIGKILL �� SIGSTOP ����������ź�
 * �����Ա�������
 */
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

extern int core_dump(long signr,struct pt_regs * regs);

asmlinkage int do_signal(unsigned long oldmask, struct pt_regs * regs);

struct sigcontext_struct {
	unsigned short gs, __gsh;
	unsigned short fs, __fsh;
	unsigned short es, __esh;
	unsigned short ds, __dsh;
	unsigned long edi;
	unsigned long esi;
	unsigned long ebp;
	unsigned long esp;
	unsigned long ebx;
	unsigned long edx;
	unsigned long ecx;
	unsigned long eax;
	unsigned long trapno;
	unsigned long err;
	unsigned long eip;
	unsigned short cs, __csh;
	unsigned long eflags;
	unsigned long esp_at_signal;
	unsigned short ss, __ssh;
	unsigned long i387;
	unsigned long oldmask;
	unsigned long cr2;
};

/*
 *	sys_sigprocmask: ϵͳ���� sigprocmask ��Ӧ��ϵͳ���ô����������ڸ��ĵ�ǰ�����
 * �����źż���ͬʱ���ص�ǰ�����ԭ�����źż���
 *
 *	���:	how --- ��θ��ĵı�־�����ӡ�ɾ�����������á�
 *		set --- ���ڸ��ĵ�ǰ����������źż�����ָ��ָ��Ŀռ�λ���û�̬�ռ��С�
 *		oset --- ���ڱ��浱ǰ�����ԭ�����źż�����ָ��ָ��Ŀռ�λ���û�̬�ռ��С�
 */
asmlinkage int sys_sigprocmask(int how, sigset_t *set, sigset_t *oset)
{
	sigset_t new_set, old_set = current->blocked;
	int error;

	/*
	 *	if: set ���Դ� NULL���� NULL ��ʾ�����ĵ�ǰ����������źż��������û�ֻ��
	 * ���ȡһ�µ�ǰ����������źż����ѡ�
	 */
	if (set) {
		error = verify_area(VERIFY_READ, set, sizeof(sigset_t));
		if (error)
			return error;
		new_set = get_fs_long((unsigned long *) set) & _BLOCKABLE;
				/*
				 *	1. ��֤ set ָ��Ĵ�������źż����û�̬�ռ��Ƿ�ɶ���
				 *
				 *	2. ��Ҫ���ĵ������źż��� set ָ����û��ռ��ж�ȡ�� new_set ��ʾ��
				 * �ں˿ռ��У��� SIGKILL �� SIGSTOP ���ܱ�������
				 */
		switch (how) {
		case SIG_BLOCK:
			current->blocked |= new_set;
			break;
				/* �ڵ�ǰ�����ԭ�����źż�������ָ���������źż� */
		case SIG_UNBLOCK:
			current->blocked &= ~new_set;
			break;
				/* �ӵ�ǰ�����ԭ�����źż���ɾ��ָ���������źż� */
		case SIG_SETMASK:
			current->blocked = new_set;
			break;
				/* �����õ�ǰ����������źż� */
		default:
			return -EINVAL;
		}
	}

	/*
	 *	if: oset ���Դ� NULL���� NULL ��ʾ�û������ȡ��ǰ�����ԭ�����źż���
	 */
	if (oset) {
		error = verify_area(VERIFY_WRITE, oset, sizeof(sigset_t));
		if (error)
			return error;
		put_fs_long(old_set, (unsigned long *) oset);
	}
			/*
			 *	1. ��֤ oset ָ������ڱ��浱ǰ�����ԭ�����źż����û�̬�ռ��Ƿ��д��
			 *
			 *	2. ����ǰ�����ԭ�����źż��� old_set ��ʾ���ں˿ռ���д�뵽 oset ָ���
			 * �û��ռ��С�
			 */
	return 0;
}

/*
 *	sys_sgetmask: ϵͳ���� sgetmask ��Ӧ��ϵͳ���ô����������ڻ�ȡ��ǰ�����
 * �ź������롣
 */
asmlinkage int sys_sgetmask(void)
{
	return current->blocked;
}

/*
 *	sys_ssetmask: ϵͳ���� ssetmask ��Ӧ��ϵͳ���ô����������ڸ���ǰ��������
 * �µ��ź������룬SIGKILL �� SIGSTOP ���ܱ�������
 *
 *	���:	newmask --- �µ��ź������롣
 *	����:	��ǰ�����ԭ�ź������롣
 */
asmlinkage int sys_ssetmask(int newmask)
{
	int old=current->blocked;

	current->blocked = newmask & _BLOCKABLE;
	return old;
}

/*
 *	sys_sigpending: ϵͳ���� sigpending ��Ӧ��ϵͳ���ô����������ڻ�ȡ��ǰ�����Ѿ�
 * �յ��ĵ������ε��źţ���Щ�źŴ���δ�����״̬��
 *
 *	���:	set --- ָ�����ڱ����źŵ��û�̬�ռ��ָ�롣
 */
asmlinkage int sys_sigpending(sigset_t *set)
{
	int error;
	/* fill in "set" with signals pending but blocked. */
	error = verify_area(VERIFY_WRITE, set, 4);
			/*
			 *	��֤ set ָ������ڱ����źŵ��û�̬�ռ��Ƿ��д��
			 */
	if (!error)
		put_fs_long(current->blocked & current->signal, (unsigned long *)set);
			/*
			 *	����ǰ�����Ѿ��յ��ĵ������ε��ź�д�뵽 set ָ����û��ռ��С�
			 */
	return error;
}

/*
 * atomically swap in the new signal mask, and wait for a signal.
 */
asmlinkage int sys_sigsuspend(int restart, unsigned long oldmask, unsigned long set)
{
	unsigned long mask;
	struct pt_regs * regs = (struct pt_regs *) &restart;

	mask = current->blocked;
	current->blocked = set & _BLOCKABLE;
	regs->eax = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(mask,regs))
			return -EINTR;
	}
}

/*
 * POSIX 3.3.1.3:
 *  "Setting a signal action to SIG_IGN for a signal that is pending
 *   shall cause the pending signal to be discarded, whether or not
 *   it is blocked" (but SIGCHLD is unspecified: linux leaves it alone).
 *
 *  "Setting a signal action to SIG_DFL for a signal that is pending
 *   and whose default action is to ignore the signal (for example,
 *   SIGCHLD), shall cause the pending signal to be discarded, whether
 *   or not it is blocked"
 *
 * Note the silly behaviour of SIGCHLD: SIG_IGN means that the signal
 * isn't actually ignored, but does automatic child reaping, while
 * SIG_DFL is explicitly said by POSIX to force the signal to be ignored..
 */
/*
 *	�����Ѿ�������ź�(�ź��Ѳ���������δ����)�����źŵĲ�������Ϊ SIG_IGN ��������
 * ������źű��������������Ƿ�����(�� SIGCHLD �ź�δָ����Linux ����������������)��
 *
 *	�����Ѿ�������Ĭ�ϲ����� Ignore (���� SIGCHLD)���źţ����źŵĲ�������Ϊ SIG_DFL
 * �������ѹ�����źű��������������Ƿ�������
 *
 *	ע�� SIGCHLD �źŵĻ�����Ϊ: SIG_IGN ��ζ���ź�ʵ����û�б����ԣ����ǽ����������
 * �Զ����գ��� SIG_DFL �� POSIX ��ȷ��˵����ǿ�Ƶĺ����źš�
 */

/*
 *	check_pending: ��Ⲣ�����źŵĹ���״̬�������������źŵ����Խṹ�Ժ�����Ⲣ
 * �����źŵĹ���״̬��
 */
static void check_pending(int signum)
{
	struct sigaction *p;

	p = signum - 1 + current->sigaction;
	if (p->sa_handler == SIG_IGN) {
		if (signum == SIGCHLD)
			return;
		current->signal &= ~_S(signum);
		return;
	}
			/*
			 *	����ź� signum �Ĵ��������������ó��� SIG_IGN������Ҫ�������Ѿ��յ�����δ
			 * ����� signum �źŶ�����
			 *
			 *	����: SIGCHLD �źŲ��ܱ�������
			 */
	if (p->sa_handler == SIG_DFL) {
		if (signum != SIGCONT && signum != SIGCHLD && signum != SIGWINCH)
			return;
		current->signal &= ~_S(signum);
		return;
	}	
			/*
			 *	����ź� signum �Ĵ��������������ó��� SIG_DFL������Ҫ�������Ѿ��յ�����δ
			 * ����� signum �źŶ�����
			 *
			 *	����: SIGCONT��SIGCHLD��SIGWINCH �źŲ��ܱ�������
			 */
}

/*
 *	sys_signal: ϵͳ���� signal ��Ӧ��ϵͳ���ô����������ڲ���һ���źţ�Ҳ����Ϊָ��
 * ���źŰ�װ�µ��źž��(�����µ��źŴ�����)���¾���������û��Զ���ĺ�����Ҳ������
 * SIG_DFL �� SIG_IGN��
 *
 *	signal �������ɿ�����ĳЩ����ʱ�̿��ܻ�����źŶ�ʧ��ԭ����: sys_signal �л�ǿ��
 * ���� SA_ONESHOT ��־�������ϵͳ��ִ���û��Զ�����źŴ�����֮ǰ�Ƚ��źŵľ������Ϊ
 * Ĭ�Ͼ����ͬʱ������ SA_NOMASK ��־�����������ִ���źŴ������Ĺ������ٴ��յ����źš�
 *
 *	��ˣ����źŲ�����ִ���źŵĴ�����ʱ�������������źŵĴ�����֮ǰ�����ź�����
 * һ�β��������Ǵ�ʱϵͳ�Ѿ��Ѹ��źŵĴ��������ó���Ĭ�ϵľ��������������¾��п��ܻ�
 * ����ٴβ���������źŶ�ʧ��
 *
 *	�źŶ�ʧֻ���п��ܣ�������һ���ᶪʧ���źŶ�ʧ��ʱ������:
 *	1. �źŲ�����ϵͳ׼��ִ���û��Զ�����źŴ����������ڴ�֮ǰ�����źŵľ������Ϊ
 * Ĭ�Ͼ����
 *	2. ִ���û��Զ���Ĵ�����������û��Ϊ���ź����������µľ������ʱ���ź�����һ��
 * ������
 *	3. ��ΪĳЩ����ʱ�������(����������жϵ�)��������û��Ϊ���ź��������þ��֮ǰ��һ
 * �ν����ں�̬�������˳��ں�̬֮ǰ����ڶ����յ��ĸ��źš�
 *	4. ��ʱ����֮ǰ�Ѿ������źŵľ�����ó���Ĭ��ֵ����ô�ڶ����յ����źžͲ���ִ���û�
 * �Զ�����źŴ���������������źŶ�ʧ��
 *
 *
 *	���:	signum --- ָ�����źš�
 *		handler --- �¾����
 *
 *	����:	���źŵ�ԭ�����
 */
asmlinkage int sys_signal(int signum, unsigned long handler)
{
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL || signum==SIGSTOP)
		return -EINVAL;
	if (handler >= TASK_SIZE)
		return -EFAULT;
			/*
			 *	1. ϵͳ���֧�� 32 ���źţ��ź�ֵΪ 1 - 32��SIGKILL �� SIGSTOP �������źŲ�
			 * �����û�����
			 *
			 *	2. �û��Զ�����źŴ���������λ��������û�̬�ռ��С�
			 */
	tmp.sa_handler = (void (*)(int)) handler;
	tmp.sa_mask = 0;
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
	tmp.sa_restorer = NULL;
	handler = (long) current->sigaction[signum-1].sa_handler;
	current->sigaction[signum-1] = tmp;
			/*
			 *	1. ����źŶ�Ӧ�� sigaction �ṹ��sa_mask == 0 ��ʾ���źŵĴ�����ִ���ڼ䲻
			 * ���������κ��źš�sa_flags = SA_ONESHOT | SA_NOMASK ��ʾ�û��Զ�����źŴ�����
			 * ִ��һ��֮��ͻָ�ΪĬ�ϵ��źŴ�����������������ִ���źŴ������Ĺ������ٴ��յ�
			 * ���źš�
			 *
			 *	2. �����źŵ�ԭ�����
			 *
			 *	3. ���������źŵ� sigaction �ṹ��
			 */
	check_pending(signum);
			/*
			 *	���������źŵ� sigaction �ṹ�����Ⲣ�����źŵĹ���״̬
			 */
	return handler;
			/* signal ϵͳ���÷����źŵ�ԭ��� */
}

/*
 *	sys_sigaction: ϵͳ���� sigaction ��Ӧ��ϵͳ���ô����������ڲ���һ���źţ�Ҳ����
 * Ϊָ�����źŰ�װ�µ����Խṹ��
 *
 *	sigaction �� signal ��ͬ��signal ֻ�ܰ�װ�źŵľ�������ܸ����źŵı�־��sigaction
 * �����û��������źŵ��������Խṹ��
 *
 *	�ӿɿ���������sigaction һ���ǿɿ��ģ������û������� SA_ONESHOT ��־����ʱ sigaction
 * �ͺ� signal һ������ò��ɿ��ˡ�
 *
 *
 *	���:	signum --- ָ�����źš�
 *		action --- ָ�������Խṹ��ָ�룬�����������Խṹ������û��ռ��С�
 *		oldaction --- �û����������ڱ���ԭ���Խṹ��ָ�룬��ָ��ָ��Ŀռ�λ���û��ռ��С�
 *
 *	����:	�ɹ����� 0��ʧ�ܷ��ض�Ӧ�Ĵ����롣
 */
asmlinkage int sys_sigaction(int signum, const struct sigaction * action,
	struct sigaction * oldaction)
{
	struct sigaction new_sa, *p;

	if (signum<1 || signum>32 || signum==SIGKILL || signum==SIGSTOP)
		return -EINVAL;
	p = signum - 1 + current->sigaction;
			/*
			 *	1. ϵͳ���֧�� 32 ���źţ��ź�ֵΪ 1 - 32��SIGKILL �� SIGSTOP �������źŲ�
			 * �����û�����
			 *
			 *	2. p ָ��ָ���źŶ�Ӧ�� sigaction �ṹ��
			 */

	/*
	 *	if: ָ�������Խṹ��ָ����Ч�����ָ����Դ� NULL���� NULL ��ʾ��Ϊ�ź�
	 * ��װ�µ����Խṹ�������û�ֻ�����ȡһ���źŵ����Խṹ���ѡ�
	 */
	if (action) {
		int err = verify_area(VERIFY_READ, action, sizeof(*action));
		if (err)
			return err;
		memcpy_fromfs(&new_sa, action, sizeof(struct sigaction));
				/*
				 *	1. ��֤ action ָ��Ĵ�������Խṹ���û�̬�ռ��Ƿ�ɶ���
				 *
				 *	2. ���źŵ������Խṹ�� action ָ����û��ռ��и��Ƶ� new_sa ��ʾ��
				 * �ں˿ռ��С�
				 */
		if (new_sa.sa_flags & SA_NOMASK)
			new_sa.sa_mask = 0;
		else {
			new_sa.sa_mask |= _S(signum);
			new_sa.sa_mask &= _BLOCKABLE;
		}
				/*
				 *	1. ����û������� SA_NOMASK ��־����˵���û�������ִ���źŴ�������
				 * �������ٴ��յ��ź�(���źź������źŶ�����)���� sa_mask �� 0����ʾ��ִ��
				 * �źŴ������Ĺ����в������κ��źš�
				 *
				 *	2. ����û�û������ SA_NOMASK ��־����˵���û�������ִ���źŴ�����
				 * �Ĺ������ٴ��յ����ź�(�����ź��������)������Ҫ�� sa_mask �м�����źţ�
				 * ��ʾ��ִ���źŴ������Ĺ������������źš�����û��� sa_mask �л�����������
				 * �źţ���ͬʱ��������Щ�źš�
				 */
		if (TASK_SIZE <= (unsigned long) new_sa.sa_handler)
			return -EFAULT;
				/*
				 *	�û��Զ�����źŴ���������λ��������û�̬�ռ��С�
				 */
	}

	/*
	 *	if: ����ԭ���Խṹ��ָ����Ч�����ָ����Դ� NULL���� NULL ��ʾ�û�����
	 * ��ȡ�źŵ�ԭ���Խṹ��
	 */
	if (oldaction) {
		int err = verify_area(VERIFY_WRITE, oldaction, sizeof(*oldaction));
		if (err)
			return err;
		memcpy_tofs(oldaction, p, sizeof(struct sigaction));
				/*
				 *	1. ��֤ oldaction ָ������ڱ���ԭ���Խṹ���û�̬�ռ��Ƿ��д��
				 *
				 *	2. ���źŵ�ԭ���Խṹ�� p ָ����ں˿ռ��и��Ƶ� oldaction ָ���
				 * �û��ռ��С�
				 */
	}

	if (action) {
		*p = new_sa;
		check_pending(signum);
	}
			/*
			 *	1. Ϊָ���ź����������µ����Խṹ��
			 *
			 *	2. ���������źŵ� sigaction �ṹ�����Ⲣ�����źŵĹ���״̬
			 */
	return 0;
}

asmlinkage int sys_waitpid(pid_t pid,unsigned long * stat_addr, int options);

/*
 * This sets regs->esp even though we don't actually use sigstacks yet..
 */
/*
 *	sys_sigreturn: ϵͳ���� sigreturn ��Ӧ��ϵͳ���ô�������sigreturn ������һ���ź�
 * ���źŴ�����ִ�����֮��ִ�У����ڴ�����źŵ��ź�ջ֡�лָ��źŴ�����ִ�����֮��
 * ��ԭʼ�ֳ���
 *
 *	���:	__unused --- ʵ���ϣ��û���û��Ϊ sigreturn ���ò�������ˣ����� sys_sigreturn
 * �Ժ󣬵�ǰ�ں�̬ջ��ջ���б����ֵ��Ҳ���� esp0 ���� EBX �ͻᱻ���� sys_sigreturn �Ĳ�����
 * ���������ﲢ��������ʹ�����������ֻ���������������ȡһ�µ�ǰ�ں�̬ջ��ջ��(esp0)���ѡ�
 */
asmlinkage int sys_sigreturn(unsigned long __unused)
{
#define COPY(x) regs->x = context.x
#define COPY_SEG(x) \
if ((context.x & 0xfffc) && (context.x & 3) != 3) goto badframe; COPY(x);
#define COPY_SEG_STRICT(x) \
if (!(context.x & 0xfffc) || (context.x & 3) != 3) goto badframe; COPY(x);
			/*
			 *	1. COPY(x): ���ڸ����������ݡ�
			 *
			 *	2. COPY_SEG(x): ���ڸ�����ͨ��ѡ��� DS��ES��FS��GS��
			 *
			 *	3. COPY_SEG_STRICT(x): ���ڸ��ƴ����ѡ��� CS �Ͷ�ջ��ѡ��� SS��
			 *
			 *	4. ���ƶ�ѡ���ʱ���ж�������ͨ������ѡ����е� Index��TI��RPL �ֶ����ж�
			 * Ҫ���ƵĶ�ѡ����Ƿ���һ����Ч�Ķ�ѡ�����
			 */

	struct sigcontext_struct context;
	struct pt_regs * regs;

	regs = (struct pt_regs *) &__unused;
	if (verify_area(VERIFY_READ, (void *) regs->esp, sizeof(context)))
		goto badframe;
	memcpy_fromfs(&context,(void *) regs->esp, sizeof(context));
			/*
			 *	1. regs ָ��ǰ������ں�̬ջ�� esp0 ����
			 *
			 *	2. ��֤�����е�ǰ�ź�ջ֡���û�̬�ռ��Ƿ�ɶ�����������ں�̬�Ժ��ں�̬ջ��
			 * regs->esp �б��������������ں�̬֮ǰ��ջָ�룬���� sigreturn ϵͳ���ã����ջָ��
			 * ָ��ǰ�ź�ջ֡��ջ������Ҳ���� frame + 2 ��λ�ô���
			 *
			 *	3. ����ǰ�ź�ջ֡�� regs->esp ָ����û�̬�ռ��������ĸ��Ƶ��ں�̬�ռ��е�
			 * context �ṹ�С�
			 */

	/*
	 *	below: ���潫�ӵ�ǰ�ź�ջ֡�лָ���ǰ�źŴ�����ִ�����֮���ԭʼ�ֳ���
	 *
	 *	1. ��������ں�̬ʱϵͳ�������ں�̬ջ�ױ���һ����������ں�̬֮ǰ��ԭʼ
	 * �ֳ������� sigreturn ϵͳ���ã�ԭʼ�ֳ��еĴ���ָ�뽫ָ��ǰջ֡�� frame + 26
	 * λ�ô���ջָ�뽫ָ��ǰջ֡�� frame + 2 λ�ô�����Ȼ�����ԭʼ�ֳ�����Ч�ģ�
	 * ���񷵻ص��û�̬��Ҳ�޷��������С�
	 *
	 *	2. ��������ԭ�򣬾���Ҫ��������ں�̬���ص��û�̬֮ǰΪ������һ����Ч��
	 * ԭʼ�ֳ����������Ч��ԭʼ�ֳ����ڵ�ǰ�źŵ��ź�ջ֡�У�������Чԭʼ�ֳ��ķ���
	 * Ҳ�������ź�ջ֡�е�ԭʼ�ֳ��������ڱ������ں�̬ջ�׵�ԭʼ�ֳ��������Ժ󣬵�
	 * ������ں�̬���ص��û�̬ʱ���ͻ᷵�ص���Ч��ԭʼ�ֳ��м������С�
	 */
	current->blocked = context.oldmask & _BLOCKABLE;
	COPY_SEG(ds);
	COPY_SEG(es);
	COPY_SEG(fs);
	COPY_SEG(gs);
	COPY_SEG_STRICT(ss);
	COPY_SEG_STRICT(cs);
	COPY(eip);
	COPY(ecx); COPY(edx);
	COPY(ebx);
	COPY(esp); COPY(ebp);
	COPY(edi); COPY(esi);
	regs->eflags &= ~0xCD5;
	regs->eflags |= context.eflags & 0xCD5;
			/*
			 *	1. ���ź�ջ֡�� oldmask �лָ�����������롣
			 *
			 *	2. ���ź�ջ֡�лָ��Ĵ�����Ϣ(ds ---> esi)��context.eip ��ԭʼ�ֳ��Ĵ���ָ�룬
			 * context.esp ��ջ֡�� frame + 9 λ�ô���ԭʼ�ֳ���ջ��ָ�롣
			 *
			 *	3. ���ź�ջ֡�� eflags �лָ�ԭʼ�ֳ��ı�־�Ĵ���������ֻ��ջ֡�б���ı�־
			 * �Ĵ����лָ� CF PF AF ZF SF DF OF ��״̬��������־�����е�״̬Ϊ׼��
			 */
	regs->orig_eax = -1;		/* disable syscall checks */
	return context.eax;
			/*
			 *	1. ��Ϊ sigreturn ϵͳ����ֻ��ִ��һ�Σ����������������ʽ� ORIG_EAX ������
			 * ��ֵ�� __NR_sigreturn ����Ϊ -1�����������Ժ󣬵� sys_sigreturn �˳���ִ�к����
			 * �źŴ�������(ret_from_sys_call)ʱ���Ͳ����ټ���Ƿ���Ҫ����ϵͳ�����ˡ�
			 *
			 *	2. ���ź�ջ֡�лָ�ԭʼ����ֵ��
			 */
badframe:
	do_exit(SIGSEGV);
}

/*
 * Set up a signal frame... Make the stack look the way iBCS2 expects
 * it to look.
 */
/*
 *	����һ���ź�ջ֡��ʹ��ջ�������� iBCS2 ��������������
 */


/*
 *	setup_frame: Ϊһ���ź������ź�ջ֡���ź�ջ֡�Ĳ��ּ������� struct sigcontext_struct ��������
 * ����ź�ջ֡����������������û�̬ջ�У�Ҳ����������λ���û�̬�ռ���ź��Լ��������ź�ջ�С�
 *
 *	һ���źŵ��ź�ջ֡������Ϊ�źŴ�������ִ�ж�׼���ģ��෴���������źŴ�����ִ�����֮��
 *
 *	�ź�ջ֡�б�������źŴ�����ִ�����֮����Ҫ�ָ����ֳ���Ϣ�����źŴ�����ִ�н������ڷ���
 * ʱ�ᴥ��ϵͳȥִ���źŷ��س��� sys_sigreturn���źŷ��س���ִ��ʱ��ӵ�ǰ�ź�ջ֡�лָ��źŴ�����
 * ִ�����֮���ԭʼ�ֳ��������ص�ԭʼ�ֳ��м������С�
 *
 *	����ж���ź�ջ֡����˵���ж���źŴ�������Ҫ����ִ�С���ǰ�źŴ�����ִ�����֮��ϵͳ
 * ����ӵ�ǰ�ź�ջ֡�лָ���һ���źŴ��������ֳ���ִ����һ���źŴ��������Դ����ƣ������һ���ź�
 * ������ִ�����֮��ϵͳ��������һ���ź�ջ֡�лָ���������ں�̬֮ǰ��ԭʼ�ֳ������ص�ԭʼ�ֳ�
 * �м������С�
 *
 *	���:	sa --- ָ���źŶ�Ӧ�� sigaction �ṹ���������ֻ���ڻ�ȡ�ź��Լ��������ź�ջָ�롣
 *
 *		fp --- �����������������: һ�Ǵ��ݵ�ǰ�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ���
 *			ջ��ָ�롣���Ƿ��ص�ǰ�ź�ջ֡��ջ����ַ��
 *
 *		eip --- ��ǰ�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ��Ĵ���ָ�롣
 *
 *		regs --- ��ǰ�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ��ļĴ�����Ϣ��
 *
 *		signr --- �źŵ��ź�ֵ������Ϊ���ݸ��źŴ������Ĳ���ʹ�á�
 *
 *		oldmask --- ��ǰ�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ������������롣
 *
 *
 *	���ź�ջ֡�洢��������û�̬ջʱ�Ĳ������¡�:
 *
 *	�ϸ񽲣�һ���źŵ��ź�ջ֡�Ǵ� frame + 2 �� frame + 23��Ϊ�˷���������
 * ���ｫ frame + 0 �� frame + 31 ��Ϊһ���źŵ��ź�ջ֡��
 *
 *							/----->	���������һ���ź�ջ֡���Ǿ���ǰһ���ź�ջ֡
 *							|	�� frame + 0 ������������ź�ջ֡���Ǿ�������
 *			+-----------------------+	|	�ڽ����ں�̬֮ǰ��ջָ�� SS:ESP ��ָʾ���û�̬
 *			|	........	|	|	ջ��λ�á�
 *			+-----------------------+	|
 *		   /-->	|			|-------/
 *	--------   |	+-----------------------+----------	<=== �ź�ջ֡�ĵײ�
 *	+31	   |	|			|
 *		   |	+-----------------------+
 *	+...	   |	|			|
 *		   |	+-----------------------+
 *	+26	   |	|			|
 *		   |	+-----------------------+
 *	+25	   |	|0x80cd		0x0000	|	<=== frame + 24 �� frame + 25 ����λ�ô��� 8 ���ֽ���
 *		   |	+-----------------------+		������ 3 ��ָ��� 3 ��ָ��ڱ��źŴ�����
 *	+24	/---->	|__NR_sigreturn	0xb858	|		ִ����Ϻ�ִ�С�
 *		|  |	+-----------------------+
 *	+23	|  |	|	CR2		|	<=== ���ﱣ��������ִ�й����в�����ҳ��������Ե�ַ��
 *		|  |	+-----------------------+
 *	+22	|  |	|	oldmask		|	<=== ���ﱣ�汾�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ
 *		|  |	+-----------------------+		�ֳ������������롣
 *	+21	|  |	|	0		|	<=== ��ǰ����ĸ���״ָ̬�룬�ù�����δʵ�֡���ֵ��Ч��
 *		|  |	+-----------------------+
 *	+20	|  |	|	regs->ss	|
 *		|  |	+-----------------------+
 *	+19	|  |	|	regs->esp	|	<=== ���ź�ջ֡ sigcontext_struct �У����λ�õĳ�Ա
 *		|  |	+-----------------------+		������ esp_at_signal��
 *	+18	|  |	|	regs->eflags	|
 *		|  |	+-----------------------+
 *	+17	|  |	|	regs->cs	|
 *		|  |	+-----------------------+
 *	+16	|  |	|	eip		|	<=== ���ﱣ�������������ں�̬֮ǰ�Ĵ���ָ���ǰһ��
 *		|  |	+-----------------------+		�ź�ջ֡��Ӧ���źŴ���������ڵ�ַ��
 *	+15	|  |	|	error_code	|
 *		|  |	+-----------------------+	<=== ���ﱣ��������ִ�й����в������쳣���ͱ�ź͵���
 *	+14	|  |	|	trap_no		|		�쳣�Ĵ����롣
 *		|  |	+-----------------------+
 *	+13	|  |	|	regs->eax	|
 *		|  |	+-----------------------+
 *	+12	|  |	|	regs->ecx	|
 *		|  |	+-----------------------+
 *	+11	|  |	|	regs->edx	|
 *		|  |	+-----------------------+
 *	+10	|  |	|	regs->ebx	|
 *		|  |	+-----------------------+
 *	+9	|  \---	|	*fp		|	<=== ���ﱣ�������������ں�̬֮ǰ���û�̬ջָ���
 *		|	+-----------------------+		ǰһ���ź�ջ֡��ջ����ַ����Ҫע�����: ��
 *	+8	|	|	regs->ebp	|		�ź�ջ֡�ṹ sigcontext_struct �У����λ��
 *		|	+-----------------------+		�ĳ�Ա������ esp��
 *	+7	|	|	regs->esi	|
 *		|	+-----------------------+
 *	+6	|	|	regs->edi	|
 *		|	+-----------------------+
 *	+5	|	|	regs->ds	|
 *		|	+-----------------------+
 *	+4	|	|	regs->es	|
 *		|	+-----------------------+
 *	+3	|	|	regs->fs	|
 *		|	+-----------------------+
 *	+2	|	|	regs->gs	|
 *		|	+-----------------------+----------
 *	+1	|	|	signr		|	<=== �ź�ֵ�������źŴ������Ĳ�����
 *		|	+-----------------------+
 * frame+0	\-----	|	__CODE		|	<=== ����ط���������ź�ջ֡�� frame + 24 λ�ô��ĵ�ַ��
 *			+-----------------------+		�����ַ�ᱻ�źŴ���������ʱ�� ret ָ�����
 *		/-----	|	........	|		����ָ�� eip �Ĵ����С���ˣ��źŵ��źŴ�����
 *		|	+-----------------------+		ִ�н���ʱ��CS:EIP ��ָ��ǰ�ź�ջ֡�� frame +
 *		|						24 ��λ�ã��������������λ�ÿ�ʼִ��Ԥ�ȱ�����
 *		|						���������ָ�
 *		|
 *		\---->	frame + 0 ����: �źŴ�����ִ�й�����ʹ�õ�ջ���źŴ�����ִ����ϣ������˳�ʱ
 *					ջָ��ص� frame + 0 ����
 *
 */
static void setup_frame(struct sigaction * sa, unsigned long ** fp, unsigned long eip,
	struct pt_regs * regs, int signr, unsigned long oldmask)
{
	unsigned long * frame;

#define __CODE ((unsigned long)(frame+24))
#define CODE(x) ((unsigned long *) ((x)+__CODE))
	frame = *fp;
	if (regs->ss != USER_DS)
		frame = (unsigned long *) sa->sa_restorer;
			/*
			 *	1. ��������£�һ���źŵ��ź�ջ֡��ʹ��������û�̬ջ��
			 *
			 *	2. ���������û�̬ջ����������û����ݶΣ���ÿ���źŴ���ʹ�ö������ź�ջ��
			 * �ź�ջ֡���������ź�ջ��ջ�ס�
			 */
	frame -= 32;
	if (verify_area(VERIFY_WRITE,frame,32*4))
		do_exit(SIGSEGV);
			/*
			 *	��֤���ڴ洢�ź�ջ֡���û�̬�ռ��Ƿ��д���ź�ջ֡�̶� 32 * 4 ���ֽڡ�����ź�
			 * ջ֡�ռ䲻��д���������޷���ȷ�����յ����źţ�������ֱ���˳���
			 */
/* set up the "normal" stack seen by the signal handler (iBCS2) */
	put_fs_long(__CODE,frame);
	put_fs_long(signr, frame+1);
			/*
			 *	1. ���ź�ջ֡�� frame + 24 λ�ô��ĵ�ַ������ջ֡�Ķ����������ַ�����źŴ���
			 * ����ִ�����֮��ķ��ص�ַ��
			 *
			 *	2. ���ź�ֵ signr �������ź�ջ֡�У�����ź�ֵ���Ǵ��ݸ��źŴ�������Ψһ������
			 *
			 *	����ϵͳִ���źŴ�������ͨ��ֱ����ת�ķ�ʽ��Ҳ���Ǵ�������ں�ֱ̬�ӷ��ص�
			 * �û�̬���źŴ������еķ�ʽ�������Ǻ������õķ�ʽ�����Դ�������������ջ���Զ�ѹ��
			 * �źŴ���������Ҫ�Ĳ������źŴ�����ִ�����֮��ķ��ص�ַ��
			 *
			 *	��Ȼֱ����ת���źŴ�������ʱ�������������Զ������������ص�ַѹջ�������ź�
			 * ������β���� ret ָ�����źŴ���������ʱ��Ȼ��ִ�У���Ҳ���˳��źŴ�������Ψһ
			 * ��ʽ������ִ�� ret ʱ������������ӵ�ǰջָ�� SS:ESP ָʾ��λ�ô�����һ�����ص�ַ��
			 * ����ָ�� CS:EIP �С�
			 *
			 *	Ϊ���źŴ���������һ����ȷ�Ĳ�����ҲΪ���źŴ������ܹ���ȷ���˳�����������
			 * ����Ҫ���ź�ջ֡�Ķ����˹�Ԥ��ѹ���źŴ������Ĳ��������ص�ַ��
			 *
			 *	���������Ժ󣬵���ʼִ���źŴ�����ʱ����������ջָ�� SS:ESP ��ָ�� frame ����
			 * �źŴ���������� ESP + 4 ��λ�ã�Ҳ���� frame + 1 ��λ�ô�ȥ��ȡ���ݸ����Ĳ�����
			 * �źŴ�����ִ�еĹ����У�ջָ�뽫���� frame ���»�����źŴ�����ִ����ϲ�׼��
			 * ִ�� ret ָ��ʱ��ջָ�뽫�ص� frame ����
			 *
			 *	�����ִ�� ret ָ��ʱ���������Ὣջָ��ָ��� frame ���ķ��ص�ַ __CODE ����
			 * ������ָ�� CS:EIP �С���ʱ������ָ�뽫ָ���ź�ջ֡�� frame + 24 ��λ�ô�������ִ��
			 * Ԥ�ȴ洢������� 3 ��ָ���ʱջָ�� SS:ESP ��ָ�� frame + 1 ��λ�á�
			 */
	put_fs_long(regs->gs, frame+2);
	put_fs_long(regs->fs, frame+3);
	put_fs_long(regs->es, frame+4);
	put_fs_long(regs->ds, frame+5);
	put_fs_long(regs->edi, frame+6);
	put_fs_long(regs->esi, frame+7);
	put_fs_long(regs->ebp, frame+8);
	put_fs_long((long)*fp, frame+9);
			/*
			 *	�����źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ���ջָ�뱣���� frame + 9 ��λ�ô���
			 *
			 *	������ź�ջ֡�ǵ�һ���ź�ջ֡�������ﱣ�������������ں�̬֮ǰ���û�̬ջָ�룬
			 * ������ǣ������ﱣ�����ǰһ���ź�ջ֡��ջ����ַ������ط�����ĵ�ַ��Ϣ���Ǵӵ�ǰ
			 * ջ֡Ѱ��ǰһ��ջ֡��Ψһͨ����
			 *
			 *	��Ҫע�����: ���ź�ջ֡�ṹ sigcontext_struct �У����λ�õĳ�Ա������ esp��
			 * �� frame + 19 λ�õ������� esp_at_signal��
			 */
	put_fs_long(regs->ebx, frame+10);
	put_fs_long(regs->edx, frame+11);
	put_fs_long(regs->ecx, frame+12);
	put_fs_long(regs->eax, frame+13);
	put_fs_long(current->tss.trap_no, frame+14);
	put_fs_long(current->tss.error_code, frame+15);
			/*
			 *	���浱ǰ�����Ѿ��������쳣���ͱ�ź͵����쳣�Ĵ����롣
			 */
	put_fs_long(eip, frame+16);
			/*
			 *	�����źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ��Ĵ���ָ�뱣���� frame + 16 ��
			 * λ�ô���
			 *
			 *	������ź�ջ֡�ǵ�һ���ź�ջ֡�������ﱣ�������������ں�̬֮ǰ�Ĵ���ָ�룬
			 * ������ǣ������ﱣ�����ǰһ���ź�ջ֡��Ӧ���źŴ���������ڵ�ַ��
			 */
	put_fs_long(regs->cs, frame+17);
	put_fs_long(regs->eflags, frame+18);
	put_fs_long(regs->esp, frame+19);
	put_fs_long(regs->ss, frame+20);
	put_fs_long(0,frame+21);		/* 387 state pointer - not implemented*/
/* non-iBCS2 extensions.. */
	put_fs_long(oldmask, frame+22);
	put_fs_long(current->tss.cr2, frame+23);
			/*
			 *	1. ���汾�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ������������롣
			 *
			 *	2. ���浼��ҳ��������Ե�ַ��
			 */
/* set up the return code... */
	put_fs_long(0x0000b858, CODE(0));	/* popl %eax ; movl $,%eax */
	put_fs_long(0x80cd0000, CODE(4));	/* int $0x80 */
	put_fs_long(__NR_sigreturn, CODE(2));
			/*
			 *	���ź�ջ֡�е� frame + 24 �� frame + 25 ����λ�ô��� 8 ���ֽ���Ԥ��д�� 3 ��
			 * ָ��� 3 ��ָ��ֱ���:
			 *
			 *	popl %eax;
			 *	movl $__NR_sigreturn, %eax;
			 *	int $0x80;
			 *
			 *	���źŴ�����ִ����ϲ�ִ�� ret ָ��غ󣬴���ָ�� CS:EIP ��ָ��ջ֡�е�
			 * frame + 24 λ�ô�������������ִ��Ԥ�ȱ���������� 3 ��ָ���ʱ��ջָ�� SS:ESP
			 * ָ�� frame + 1 λ�ô���
			 *
			 *	��һ��ָ��: ��ջ�б�����źŴ������Ĳ��� signr ���������ԣ���ʱջָ�뽫ָ��
			 * frame + 2 λ�ô���
			 *	�ڶ���ָ��: �� eax �Ĵ�����װ�� sigreturn ϵͳ���õĵ��úš�
			 *	������ָ��: ����ϵͳ���ã����񽫴��û�̬�����ں�̬��ִ�� sys_sigreturn��
			 *
			 *	�����������ϵͳ���ò������ں�̬�󣬴������ڵ�ǰ������ں�̬ջ���Զ������
			 * ��ǰ������û�̬�ֳ��ֱ���: �û�̬ջָ��Ϊ frame + 2���û�̬����ָ��Ϊ frame + 26��
			 * ������ָ�뽫���ڵ�������ں�̬���ص��û�̬ʱ�ָ��û�̬���ֳ���
			 *
			 *	ʵ���ϣ�������ָ��ָʾ���û�̬�ֳ�����Ч�ģ�����Ҳû��ʲô��ϵ����Ϊϵͳ��
			 * ִ�� sys_sigreturn ʱ��ӵ�ǰ������ź�ջ֡�лָ���ȷ��ԭʼ�ֳ����ָ��Ժ󣬵�����
			 * ���ں�̬���ص��û�̬ʱ�����᷵�ص���ȷ��ԭʼ�ֳ��в��������С�
			 */
	*fp = frame;
			/*
			 *	����ǰ�ź�ջ֡��ջ����ַ���س�ȥ��
			 */
#undef __CODE
#undef CODE
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals that
 * the kernel can handle, and then we build all the user-level signal handling
 * stack-frames in one go after that.
 */
/*
 *	do_signal: �źŴ����������ڴ���ǰ������������ current ���źš�
 *
 *	ע��: init ��һ������Ľ��̣��������ղ��봦����źš���ˣ���ʹ�����ʹ��
 * SIGKILL Ҳ����ɱ�� init��
 *
 *	���:	oldmask --- ��ǰ������ź������롣
 *		regs --- ָ����������ں�̬ʱ�����ջ֡��ջ�� esp0 ����
 */
asmlinkage int do_signal(unsigned long oldmask, struct pt_regs * regs)
{
	unsigned long mask = ~current->blocked;
	unsigned long handler_signal = 0;
	unsigned long *frame = NULL;
	unsigned long eip = 0;
	unsigned long signr;
	struct sigaction * sa;

	/*
	 *	while: ѭ������ǰ�����Ѿ��յ��ĵ�δ�������������źţ�ÿ��ѭ��ֻ�����ź�ֵ
	 * ��С���Ǹ��źţ�ֱ�����е��źŶ��������Ϊֹ��
	 *
	 *	ѭ����ʼʱ signr �б�����ǻ�δ����������źš�
	 */
	while ((signr = current->signal & mask)) {
		__asm__("bsf %2,%1\n\t"
			"btrl %1,%0"
			:"=m" (current->signal),"=r" (signr)
			:"1" (signr));
				/*
				 *	1. bsf: �ӻ�δ����������ź� signr ��Ѱ���ź�ֵ��С���Ǹ��źţ�����
				 * ���źŵ�λƫ��ֵ���·��� signr �У���һ������������źš�
				 *
				 *	2. btrl: �����źŴ� current->signal ���������ʾ���ź��ѱ�����
				 */
		sa = current->sigaction + signr;
		signr++;
				/*
				 *	sa ָ����źŶ�Ӧ�� sigaction �ṹ��
				 *	signr ��������ź�ֵ��(�ź�ֵ = λƫ��ֵ + 1)
				 */
		if ((current->flags & PF_PTRACED) && signr != SIGKILL) {
			current->exit_code = signr;
			current->state = TASK_STOPPED;
			notify_parent(current);
			schedule();
			if (!(signr = current->exit_code))
				continue;
			current->exit_code = 0;
			if (signr == SIGSTOP)
				continue;
			if (_S(signr) & current->blocked) {
				current->signal |= _S(signr);
				continue;
			}
			sa = current->sigaction + signr - 1;
		}

		/*
		 *	if: �źŵĴ������� SIG_IGN����ʾ���źŽ������ԣ���ʲôҲ����������
		 * ������һ���źš�
		 */
		if (sa->sa_handler == SIG_IGN) {
			if (signr != SIGCHLD)
				continue;
			/* check for SIGCHLD: it's special */
			while (sys_waitpid(-1,NULL,WNOHANG) > 0)
				/* nothing */;
			continue;
					/*
					 *	SIGCHLD �ź���һ�������������� check_pending ��˵����
					 *
					 *	�����ǰ�����յ��� SIGCHLD �źţ���˵����ǰ�������������˳�
					 * �ˣ������п��ܻ��ж���������˳���������Ҫ�� sys_waitpid ѭ������
					 * ���˳�����Щ���������е������������Ϻ󽫼������������źš�
					 */
		}

		/*
		 *	if: �źŵĴ������� SIG_DFL����ʾ���źŽ���Ĭ�ϴ������̡�
		 */
		if (sa->sa_handler == SIG_DFL) {
			if (current->pid == 1)
				continue;
					/* ������ init �����յ����ź� */
			switch (signr) {
			/*
			 *	���� 3 ���źŵ�Ĭ�ϴ����Ǻ��ԣ�SIGCONT �ź���Ϊ���õ�ǰ����
			 * �ָ����У�����ǰ���������Ѿ�������״̬�ˣ�����ֱ�Ӻ��������ɡ�
			 */
			case SIGCONT: case SIGCHLD: case SIGWINCH:
				continue;

			/*
			 *	���� 4 ���źŵ�Ĭ�ϴ�����ֹͣ��ǰ��������У���ǰ�����ٴ�
			 * ���Ȼ����Ժ󣬼��������������źš�
			 */
			case SIGSTOP: case SIGTSTP: case SIGTTIN: case SIGTTOU:
				if (current->flags & PF_PTRACED)
					continue;
						/* ��ǰ�����ϵͳ���ù��̱����٣���������� */
				current->state = TASK_STOPPED;
				current->exit_code = signr;
				if (!(current->p_pptr->sigaction[SIGCHLD-1].sa_flags & 
						SA_NOCLDSTOP))
					notify_parent(current);
				schedule();
				continue;
						/*
						 *	1. ���õ�ǰ����Ϊֹͣ״̬���������˳��룬��ʾ������
						 * �յ��� XX �źŶ��˳�ִ�С�
						 *
						 *	2. �����ǰ����ĸ�����û������ SA_NOCLDSTOP ��־��
						 * �򽫵�ǰ�����˳�ִ��ʱӦ�÷��͸���������źŷ��͸�������
						 * ��ʾ��ǰ�������˳�ִ�У��˳�ԭ���� exit_code �С�
						 *
						 *	3. ֹͣ��ǰ�����ִ�в�����������ִ�У���ǰ�����ٴ�
						 * ���Ȼ���֮����������������źš�
						 */

			/*
			 *	���� 6 ���źŵ�Ĭ�ϴ������Ȳ��� core_dump �ļ���Ȼ����ִ��
			 * do_exit �õ�ǰ�����˳���
			 */
			case SIGQUIT: case SIGILL: case SIGTRAP:
			case SIGIOT: case SIGFPE: case SIGSEGV:
				if (core_dump(signr,regs))
					signr |= 0x80;
				/* fall through */
			/*
			 *	ʣ��������źŵ�Ĭ�ϴ���(���� SIGKILL)����ֱ��ִ�� do_exit
			 * �õ�ǰ�����˳���
			 */
			default:
				current->signal |= _S(signr & 0x7f);
				do_exit(signr);
			}
		}

		/*
		 * OK, we're invoking a handler
		 */
		/*
		 *	1. ��һ���źŵĴ���ִ�е����˵�����ź����û��Զ�����źŴ�������Ҫִ�С�
		 *
		 *	2. ��ǰ���������ͨ��ϵͳ���ý����ں�̬��ִ�е��źŴ���ĵط��������������:
		 * һ�ǵ�ǰ�����ϵͳ��������ִ����ϣ�����ϵͳ���õ�ִ�й��̱��жϣ�����ϵͳ����
		 * ��ΪĳЩԭ��Ҫ˯�ߵȴ�����˯�ߵĹ����б��źŴ�ϡ������Ҫ�ж�ϵͳ�����ǲ��Ǳ�
		 * �ж��ˣ�����ǣ�����Ҫȷ���Ƿ���Ҫ����������жϵ�ϵͳ���á�
		 *
		 *	3. ��ǰ�����������ִ���ж϶������ں�̬��ִ�е��źŴ���ĵط������ϵͳ����
		 * û���κι�ϵ��Ҳ�������κθ�����ϵͳ������صĶ�����
		 */
		if (regs->orig_eax >= 0) {
			if (regs->eax == -ERESTARTNOHAND ||
			   (regs->eax == -ERESTARTSYS && !(sa->sa_flags & SA_RESTART)))
				regs->eax = -EINTR;
		}
				/*
				 *	1. ��������� ORIG_EAX(0x2C) ����ϵͳ���ú���Ч( >= 0 )������Ҫ���
				 * �Ƿ���Ҫ������ϵͳ���á�������жϴ����β��ִ�е������ ORIG_EAX ����
				 * ������ -1����ʱ������������ϵͳ���õ��κβ�����
				 *
				 *	2. ��������� EAX(0x18) ����ϵͳ���õķ���ֵ�� -ERESTARTNOHAND����
				 * ��������ϵͳ���ã���Ϊ��ʱ�Ѿ�������һ���û��Զ�����źŴ�������Ҫִ�С�
				 *
				 *	3. ���ϵͳ���õķ���ֵ�� -ERESTARTSYS������Ҫ�жϸ��ź��Ƿ�������
				 * SA_RESTART ��ʾ�����û������ SA_RESTART ��־�����ʾϵͳ���ñ����ź��ж�
				 * ʱ��������ϵͳ���á�
				 *
				 *	4. ���ϵͳ���ò��ܱ�������������ϵͳ���õķ���ֵΪ -EINTR����ʾϵͳ
				 * ���ñ��жϡ�
				 *
				 *	5. ֻҪ��һ���źŵ���ϵͳ���ò��ܱ���������ôϵͳ���þͲ���������
				 */
		handler_signal |= 1 << (signr-1);
		mask &= ~sa->sa_mask;
				/*
				 *	1. �����źű����� handler_signal �У��������źŶ�������Ϻ���ͳһ����
				 * ��Щ��Ҫִ���û��Զ��崦�������źš�
				 *
				 *	2. mask �����ε���ǰ�źŵ� sa_mask ��ָ������Щ�źţ���Ϊ��ǰ�źŵ�
				 * ������ִ��ʱ��Ҫ���� sa_mask ��ָ������Щ�źţ���������ʱ�Ͳ��ٴ�����Щ
				 * �ź��ˡ�
				 */
	}

	/*
	 *	��ǰ�������յ��ĵ�δ�������������źŶ��Ѿ�������ϡ���Ҫִ���û��Զ���
	 * ���������źŶ������� handler_signal �С����潫��һ�δ�����Щ�źš�
	 */

	if (regs->orig_eax >= 0 &&
	    (regs->eax == -ERESTARTNOHAND ||
	     regs->eax == -ERESTARTSYS ||
	     regs->eax == -ERESTARTNOINTR)) {
		regs->eax = regs->orig_eax;
		regs->eip -= 2;
	}
			/*
			 *	����Ƿ���Ҫ����ϵͳ���á��������Ҫ����ϵͳ���ã������񷵻ص��û�̬ʱ������
			 * ָ�� eip ָ�� "int $0x80" ָ������һ��ָ�eax �Ĵ����б������ϵͳ���õķ���ֵ��
			 * ���������� "int $0x80" �ĺ���һ��ָ���ʼ��������ִ���û�̬�Ĵ��롣
			 *
			 *	�����Ҫ����ϵͳ���ã���:
			 *
			 *	1. �޸� EAX(0x18) ����ϵͳ���÷���ֵΪ ORIG_EAX(0x2C) �������ϵͳ���úš�
			 *
			 *	2. �޸� EIP(0x30) ���Ĵ���ָ�룬ʹ������ָ�� "int $0x80" ָ�
			 *
			 *	��ʱ�������񷵻ص��û�̬ʱ������ָ�� eip ����ָ�� "int $0x80" ָ�eax �Ĵ���
			 * �б�����Ǳ��жϵ�ϵͳ���úţ�������������ִ�� "int $0x80" ָ�Ҳ�����������жϵ�
			 * ϵͳ���á����ң�����ϵͳ�����Ƿ����û�̬������ִ�еģ�����ִ���û�̬���κδ��룬����
			 * �û�������֪��ϵͳ���ñ���������
			 */
	if (!handler_signal)		/* no handler will be called - return 0 */
		return 0;
			/*
			 *	û����Ҫִ���Զ��崦�������źţ���ֱ�ӷ��أ�������źŴ������̽�����������
			 * ���ص��û�̬ʱ������ִ���û�̬�Ĵ��������ϵͳ���á�
			 *
			 *	�������ҪΪִ���û��Զ�����źŴ�������׼����
			 */

/*
 ******************************************************************************
 *	���潫Ϊÿһ����Ҫִ���źŴ��������ź������ź�ջ֡���ź�ջ֡���Դ洢
 * ��������û�̬ջ�ϣ�Ҳ���Դ洢���ź��Լ��������ź�ջ�ϣ������ִ洢��ʽ��ԭ��
 * ��һ���ģ��������ֻ�����洢�������û�̬ջ�ϵķ�ʽ��
 ******************************************************************************
 */

	eip = regs->eip;
	frame = (unsigned long *) regs->esp;
			/*
			 *	1. eip ����������ں�̬֮ǰ�Ĵ���ָ�롣����ϵͳ���õ�����������Ҫ����ϵͳ
			 * ���ã��� eip �Ѿ�������Ϊָ�� "int $0x80" ָ���Ϊ����Ҫִ���û��Զ�����źŴ���
			 * ���������Ծ���Ҫֱ�Ӵ��ں�̬���ص��û�̬���źŴ�������ȥ�������������ں�̬֮ǰ
			 * �Ĵ���ָ�����Ҫ��ʱ�������������źŴ�����ִ�����֮�������ָ���������ں�̬֮ǰ
			 * �Ĵ���λ�á�
			 *
			 *	2. frame ����������ں�̬֮ǰ��ջָ�룬����Ҫִ���û��Զ�����źŴ�������
			 * ���Ծ���Ҫ��������û�̬ջ�д� frame ��λ�ÿ�ʼΪ�ź������ź�ջ֡��ÿ���źŴ�����
			 * ����Ҫ����һ��ջ֡�����е�ջ֡��������û�̬ջ���������С������������ں�̬֮ǰ��
			 * ջָ��Ҳ��Ҫ��ʱ�������������źŴ�����ִ�����֮�������ָ���������ں�̬֮ǰ��
			 * ջλ�á�
			 */

	/*
	 *	for: ���ź�ֵ��С���źſ�ʼ�����α������е��źţ���Ϊ��Ҫִ���źŴ�������
	 * �ź������ź�ջ֡��ֱ��������Ҫִ���źŴ��������źű�����Ϊֹ��
	 *
	 *	��������ջ֡�����ź�ֵ��С���źţ���������źŶ�Ӧ���źŴ�����ȴ���ִ�С�
	 */
	signr = 1;
	sa = current->sigaction;
	for (mask = 1 ; mask ; sa++,signr++,mask += mask) {
		if (mask > handler_signal)
			break;
		if (!(mask & handler_signal))
			continue;
				/*
				 *	1. ������Ҫִ���źŴ��������ź��ѱ�����ϣ����˳������ź�ջ֡�����̡�
				 *
				 *	2. �����ǰ�źŲ���Ҫִ���Զ�����źŴ������������������һ���źš�
				 */
		setup_frame(sa,&frame,eip,regs,signr,oldmask);
		eip = (unsigned long) sa->sa_handler;
				/*
				 *	��ǰ�ź���Ҫִ���źŴ���������Ϊ�ź������ź�ջ֡:
				 *
				 *	1. ���� eip: eip �ǵ�ǰ�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ��Ĵ���
				 * ָ�룬���ָ�뽫�ᱻ�����ڵ�ǰ�źŵ��ź�ջ֡�С�
				 *
				 *	��������û�̬ջ�ϣ�����ź�ջ֡���������У���һ���ź�ջ֡�б������
				 * ǰһ���ź�ջ֡��Ӧ���źŴ���������ڵ�ַ����һ���ź�ջ֡�б����������
				 * ����ϵͳ����֮ǰ�Ĵ���λ�ã��������һ���ź�ջ֡��Ӧ���źŴ����������
				 * ��ַ����Ϊ������ں�̬���ص��û�̬ʱ��ֱ�ӷ��ص�����źŴ�������ȥ������
				 * ������ڵ�ַ����Ҫ���档
				 *
				 *	2. ���� frame: frame �ǵ�ǰ�źŴ�����ִ�����֮��Ҫ�ָ���ԭʼ�ֳ���
				 * ջ��ָ�룬���ָ�뽫�ᱻ�����ڵ�ǰ�źŵ��ź�ջ֡�С�
				 *
				 *	��������û�̬ջ�ϣ�����ź�ջ֡���������У���һ���ź�ջ֡�б������
				 * ǰһ���ź�ջ֡��ջ��λ�ã���һ���ź�ջ֡�б��������������ں�̬֮ǰ��ջ
				 * ָ�롣���һ���ź�ջ֡��ջ��λ�ò���Ҫ���档
				 *
				 *	3. ���� regs: regs ָ����ں�̬ջ������������ں�̬֮ǰ�������ֳ���ִ��
				 * �źŴ�����ʱ������Ҫ���ں�̬�˳������ص��û�̬���źŴ��������ֳ���ȥ��
				 * ������Ҫ����������ں�̬֮ǰ�������ֳ���ʱ�������ź�ջ֡�У����źŴ�����
				 * ִ�����֮���ٴӵ�ǰ�źŵ��ź�ջ֡�����ָ���������ں�̬֮ǰ��ԭʼ�ֳ���
				 *
				 *	ʵ���ϣ�ÿ���ź�ջ֡�ж�������һ����������ں�̬֮ǰ��ԭʼ�ֳ�������
				 * ֻ�е�һ���ź�ջ֡�б������������ԭʼ�ֳ��������ջ֡�б�����ֳ��еĲ���
				 * �ֶ����޸Ĺ��ģ��������ջ֡�б�����ǲ��ֵ�ԭʼ�ֳ��������ֳ����������ź�
				 * ��������ԭʼ�ֳ���
				 *
				 *	4. ���� signr: �źŶ�Ӧ���ź�ֵ��Ҫ�������ź�ջ֡�У����Ǵ��ݸ��źŴ���
				 * ������Ψһ������
				 *
				 *	5. ���� oldmask: oldmask �������ԭ�ź������룬���źŴ�������ִ�й���
				 * �У�sa_mask ��ֵ��Ӱ�쵽��ǰ������ź������룬�����Ҫ�������ԭ�ź�������
				 * ��ʱ������������Ȼ����һ���ź�ջ֡�б������ǰһ���źŴ�����ִ��ʱ���ź�
				 * �����룬��һ���ź�ջ֡�б�����������ԭʼ�ź������룬���һ���ź�ջ֡��Ӧ
				 * ��������ֱ�������� current->blocked �С�
				 *
				 *	6. ���� sa: �������ź�ջ֡�Ĺ����У��źŵ� sigaction �ṹ��������ͨ��
				 * ��Ա sa_handler Ϊ�ź��ṩ�������ź�ջָ�롣
				 */
		if (sa->sa_flags & SA_ONESHOT)
			sa->sa_handler = NULL;
				/*
				 *	SA_ONESHOT ��־Ҫ���û��Զ�����źŴ�����ִ�й�һ��֮��ͻָ�ΪĬ��
				 * ���źŴ����� SIG_DFL��
				 */
/* force a supervisor-mode page-in of the signal handler to reduce races */
		__asm__("testb $0,%%fs:%0": :"m" (*(char *) eip));
				/*
				 *	����źŴ������Ĵ��벻���ڴ��У���ôִ����������ָ��ʱ�ͻ����ȱҳ
				 * �쳣����ʱ�������ͻᴦ������쳣������ʹ���źŴ������Ĵ��뱻���ص��ڴ��У�
				 * �����źŴ�������ڴ�������ֽ�ֵ�ǲ��� 0 �޹ؽ�Ҫ��
				 */
		regs->cs = USER_CS; regs->ss = USER_DS;
		regs->ds = USER_DS; regs->es = USER_DS;
		regs->gs = USER_DS; regs->fs = USER_DS;
				/*
				 *	�������񷵻ص��û�̬֮��Ķ�ѡ�����ʹ�������źŴ��������ֳ�����Ϊ
				 * �źŴ�����ֻ����������û�̬��ִ�У������еĶ�ѡ�����Ҫ����Ϊ�û�̬�µ�
				 * ��ѡ�����
				 */
		current->blocked |= sa->sa_mask;
		oldmask |= sa->sa_mask;
				/*
				 *	1. �źŴ�����ִ�й����������� sa_mask ָ������Щ�źš�
				 *
				 *	2. ��������ź�ջ֡��Ҫ���ã�������ĵ�ǰ��������Ҫ��Ϊԭ�����뱣��
				 * ����һ���ź�ջ֡�С�
				 */
	}

	regs->esp = (unsigned long) frame;
	regs->eip = eip;		/* "return" to the first handler */
			/*
			 *	���е��ź�ջ֡����������ϣ����ｫ���ý����źŴ������ķ�ʽ��
			 *
			 *	regs->esp �� regs->eip �б������������ں�̬���ص��û�̬���ջ��λ�úʹ���
			 * λ�á����ｫ����ָ������Ϊ���һ���ź�ջ֡����Ӧ���źŴ���������ڵ�ַ����ջָ��
			 * ����Ϊ���һ���ź�ջ֡��ջ����ַ��
			 *
			 *	���������Ժ󣬵�������ں�̬���ص��û�̬ʱ��ֱ����ת��ָ�����źŴ�������
			 * ��ڵ�ַ��������ִ���źŴ������������Ƿ��ص���������ں�̬֮ǰ��ԭʼ�ֳ��С�
			 *
			 *	��Ҫע�����: ������ں�̬���ص��û�̬ʱ�����źŴ������ķ�ʽ��ֱ����ת��
			 * ��ʽ�����Ǻ������õķ�ʽ����˴�����������ջ���Զ�ѹ���źŴ�������Ҫ�Ĳ�����
			 * �źŴ�����ִ�����֮��ķ��ص�ַ�����Ծ���Ҫ�������ź�ջ֡��ʱ���ֶ���������
			 * ���ص�ַѹջ��
			 */
	current->tss.trap_no = current->tss.error_code = 0;
	return 1;
			/*
			 *	1. ��ǰ������쳣���ͱ�ź͵����쳣�Ĵ������Ѿ��洢���ź�ջ֡���ˣ��������Ҫ��
			 * ���������ִ���źŴ�����ʱ���ź�ջ֡�л�ȡ��������Ϣ��
			 *
			 *	2. ������źŴ������̽�����������Ҫ���õ���Ϣ���Ѿ�������ϣ�������������˳�
			 * �����ص��û�̬ʱ��ֱ�ӽ����źŴ�����ִ�����̡�
			 */

/*
 *	����:
 *
 *	1. ��������ں�̬֮ǰ��ԭʼ�ֳ��Ѿ��������˵�һ�����õ��ź�ջ֡�У�����ź�ջ֡
 * ��Ӧ���źŴ����������һ��ִ�С�
 *
 *	2. ������Ҫִ���û��Զ��崦�������źŵ�ջ֡���Ѿ����ú��ˣ����α����������
 * �û�̬ջ�ϡ�
 *
 *	3. �����ں�̬ջ�ױ�����û�̬�ֳ�(����ָ���ջָ��)�Ѿ����ĳ��˵�һ��Ҫִ�е�
 * �źŴ�����(���һ�������ź�ջ֡)���ֳ���
 *
 *	���źŴ�������ִ����������:
 *
 *	1. �����˳��ں�̬�����ص��û�̬��ֱ����ת����һ���źŴ�������ȥִ�С�
 *
 *	2. ��һ���źŴ�����ִ�����֮�󣬴��� sigreturn ϵͳ���á�
 *
 *	3. �������½����ں�̬����ִ�� sys_sigreturn���ӵ�һ���źŴ�������Ӧ���ź�ջ֡
 * �лָ��ڶ����źŴ��������ֳ���
 *
 *	4. �����˳��ں�̬�����ص��û�̬��ֱ����ת���ڶ����źŴ�������ȥִ�С�
 *
 *	5. �������ƣ�ֱ�����һ���źŴ�����ִ����ϡ�
 *
 *	6. ���һ���źŴ�����ִ�����֮�󣬴��� sigreturn ϵͳ���á�
 *
 *	7. �������½����ں�̬����ִ�� sys_sigreturn�������һ���źŴ�������Ӧ���ź�ջ֡
 * �лָ���������ں�̬֮ǰ��ԭʼ�ֳ���
 *
 *	8. �����˳��ں�̬�����ص��û�̬���ӽ����ں�̬֮ǰ���ֳ�������ִ�С�
 */
}
