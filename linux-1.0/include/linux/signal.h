#ifndef _LINUX_SIGNAL_H
#define _LINUX_SIGNAL_H

/*
 *	sigset_t: �����źż����ͣ�Ҳ����һЩ�źŵļ��ϡ�
 */
typedef unsigned int sigset_t;		/* 32 bits */

/*
 *	NSIG: �ں�֧�ֵ��ź����࣬�� 32 �֡�
 */
#define _NSIG             32
#define NSIG		_NSIG

/*
 *	below: �����źţ��ź�ֵΪ 1 - 32���ź����ź�λͼ�е�ƫ��Ϊ 0 - 31��
 *
 *	�����ź�: ��ʾ�û�ͨ��ϵͳ�ṩ��ϵͳ���ýӿ������źŵĴ�������ʹ�õ��źŲ���ʱ
 * ִ���û��Զ�����źŴ�������
 *	�����ź�: ��ʾ������������յ��źţ����ǲ������źš�
 *	�����ź�: ��ʾ������������յ��źţ�ϵͳҲ�ᴦ��������źţ�ֻ��ϵͳ����ķ�ʽ��
 * ʲôҲ������
 *
 *	�źŵ�Ĭ�ϲ��������� 5 �����:
 *	Abort --- ������ֹ��
 *	Dump --- ������ֹ������ core dump �ļ���
 *	Ignore --- ���ԣ�ʲôҲ������
 *	Continue --- �ָ��������ִ�С�
 *	Stop --- ֹͣ��������С�
 */
#define SIGHUP		 1
				/*
				 *	SIGHUP --- hang up�����ڹҶϿ����ն˻���̣�Ĭ�ϲ����� Abort��
				 */
#define SIGINT		 2
				/*
				 *	SIGINT --- interrupt�����Լ��̵��жϣ�ͨ����������Ὣ���� Ctrl + C
				 * �󶨣�Ĭ�ϲ����� Abort��
				 */
#define SIGQUIT		 3
				/*
				 *	SIGQUIT --- quit�����Լ��̵��˳��жϣ�ͨ����������Ὣ���� Ctrl + \
				 * �󶨣�Ĭ�ϲ����� Dump��
				 */
#define SIGILL		 4
				/*
				 *	SIGILL --- illegal instruction����ʾ��������ִ����һ���Ƿ�ָ�
				 * Ĭ�ϲ����� Dump��
				 */
#define SIGTRAP		 5
				/*
				 *	SIGTRAP --- trap�����ڵ��ԣ����ٶϵ㣬Ĭ�ϲ����� Dump��
				 */
#define SIGABRT		 6
#define SIGIOT		 6
				/*
				 *	SIGABRT --- abort����ʾ�쳣������
				 *	SIGIOT --- IO trap��ͬ SIGABRT����������ͬһ���źš�Ĭ�ϲ����� Dump��
				 */
#define SIGUNUSED	 7
				/*
				 *	SIGUNUSED --- unused��û��ʹ��
				 */
#define SIGFPE		 8
				/*
				 *	SIGFPE --- floating point exception����ʾ�����쳣��Ĭ�ϲ����� Dump��
				 */
#define SIGKILL		 9
				/*
				 *	SIGKILL --- kill��ǿ����ֹ���̣����źŲ��ܱ�����Ҳ���ܱ����ԣ�����
				 * ��û���κλ�������������Ĭ�ϲ����� Abort��
				 */
#define SIGUSR1		10
				/*
				 *	SIGUSR1 --- user defined signal 1���û��ź� 1��Ĭ�ϲ����� Abort��
				 */
#define SIGSEGV		11
				/*
				 *	SIGSEGV --- segmentation violation����Ч�ڴ����ã�Ĭ�ϲ����� Dump��
				 */
#define SIGUSR2		12
				/*
				 *	SIGUSR2 --- user defined signal 2���û��ź� 2��Ĭ�ϲ����� Abort��
				 */
#define SIGPIPE		13
				/*
				 *	SIGPIPE --- pipe���ܵ�д�����޶��ߣ�Ĭ�ϲ����� Abort��
				 */
#define SIGALRM		14
				/*
				 *	SIGALRM --- alarm��ʵʱ��ʱ���������û�ͨ�� alarm ϵͳ���������õ�
				 * ��ʱʱ�䵽�ڣ���ͨ�� setitimer ϵͳ�������õ���ʵ�����ʱ�����ڣ�Ĭ�ϲ���
				 * �� Abort��
				 */
#define SIGTERM		15
				/*
				 *	SIGTERM --- terminate��������ֹ�����ں��Ƶ�Ҫ��һ��������ֹ������
				 * kill ��Ĭ���źš��� SIGKILL ��ͬ�����źſ��Ա����������������˳�����ǰ
				 * ����������Ĭ�ϲ����� Abort��
				 */
#define SIGSTKFLT	16
				/*
				 *	SIGSTKFLT --- stack fault on coprocessor��Э��������ջ����Ĭ�ϲ���
				 * �� Abort��
				 */
#define SIGCHLD		17
				/*
				 *	SIGCHLD --- child�����ӽ��̷�������ʾ�ӽ�����ֹͣ����ֹ��Ĭ�ϲ���
				 * �� Ignore��
				 */
#define SIGCONT		18
				/*
				 *	SIGCONT --- continue���ָ�����ֹͣ״̬�Ľ��̼���ִ�У�Ĭ�ϲ�����
				 * Continue��
				 */
#define SIGSTOP		19
				/*
				 *	SIGSTOP --- stop��ֹͣ���̵�ִ�У����źŲ��ܱ�����Ҳ���ܱ����ԣ�
				 * Ĭ�ϲ����� Stop��
				 */
#define SIGTSTP		20
				/*
				 *	SIGTSTP --- terminal stop��tty ����ֹͣ���̣��ɺ��ԣ�Ĭ�ϲ����� Stop��
				 */
#define SIGTTIN		21
				/*
				 *	SIGTTIN --- tty input on background����̨������ͼ��һ�����ٱ����Ƶ�
				 * �ն��϶�ȡ���ݣ�Ĭ�ϲ����� Stop��
				 */
#define SIGTTOU		22
				/*
				 *	SIGTTOU --- tty output on background����̨������ͼ��һ�����ٱ����Ƶ�
				 * �ն���������ݣ�Ĭ�ϲ����� Stop��
				 */

/*
 * Most of these aren't used yet (and perhaps never will),
 * so they are commented out.
 */


#define SIGIO		23
#define SIGPOLL		SIGIO
				/*
				 *	SIGIO --- io�������첽 IO ģʽ������ IO ����ʱ�������ź�֪ͨ���̣�
				 * Ĭ�ϲ����� Abort��
				 *	SIGPOLL --- ��ָ����ʱ�䷢���ڿ�ѡ����豸��ʱ���������źţ�Ĭ��
				 * ������ Abort��
				 */
#define SIGURG		SIGIO
				/*
				 *	SIGURG --- ���������ӽ��յ���������ʱ��������źţ�Ĭ�ϲ����� Ignore��
				 */
#define SIGXCPU		24
				/*
				 *	SIGXCPU --- ���̳����� cpu ��ʱ�����ƣ�Ĭ�ϲ����� Dump��
				 */
#define SIGXFSZ		25
				/*
				 *	SIGXFSZ --- ���̳������ļ���С���ƣ�Ĭ�ϲ����� Dump��
				 */


#define SIGVTALRM	26
#define SIGPROF		27
				/*
				 *	SIGVTALRM --- ͨ�� setitimer ϵͳ�������õ���������ʱ�����ڣ�Ĭ��
				 * ������ Abort��
				 *
				 *	SIGPROF --- ͨ�� setitimer ϵͳ�������õ� PROF �����ʱ�����ڣ�Ĭ��
				 * ������ Abort��
				 */

#define SIGWINCH	28
				/*
				 *	SIGWINCH --- ���ڳߴ�ı䣬Ĭ�ϲ����� Ignore��
				 */

/*
#define SIGLOST		29
*/
#define SIGPWR		30
				/*
				 *	SIGPWR --- ��Դ�쳣��Ĭ�ϲ����� Abort��
				 */

/* Arggh. Bad user source code wants this.. */
#define SIGBUS		SIGUNUSED
				/*
				 *	SIGBUS --- �����쳣��Ĭ�ϲ����� Dump��
				 */

/*
 * sa_flags values: SA_STACK is not currently supported, but will allow the
 * usage of signal stacks by using the (now obsolete) sa_restorer field in
 * the sigaction structure as a stack pointer. This is now possible due to
 * the changes in signal handling. LBT 010493.
 * SA_INTERRUPT is a no-op, but left due to historical reasons. Use the
 * SA_RESTART flag to get restarting signals (which were the default long ago)
 */
/*
 *	������ struct sigaction �� sa_flags ��ֵ:
 *
 *	SA_NOCLDSTOP --- ����������ֹͣ״̬ʱ��������Ͳ����յ��� SIGCHLD �ź�������
 * ʵ���ϣ�����������ֹͣ״̬���������� SIGCHLD �ź�֮ǰ�����ȼ�⸸����� SIGCHLD
 * �źŶ�Ӧ sigaction �ṹ�е� sa_flags����������������� SA_NOCLDSTOP����������Ͳ�����
 * �������� SIGCHLD �źš�
 *
 *	SA_STACK --- ִ���źŴ�����ʱʹ�ö�����ջ(�ź�ջ)��ϵͳ��ǰ��ʱ��֧�ָñ�־��
 * �����ź�ջ����ͨ�� struct sigaction �ṹ�е� sa_restorer �ֶ������á�
 *
 *	SA_RESTART --- ���øñ�־��ʾ��ϵͳ���ñ��ź��жϺ󣬽��������жϵ�ϵͳ���ã�����
 * Ĭ������£�����ϵͳ�ǲ����������жϵ�ϵͳ���õġ�
 *
 *	SA_INTERRUPT --- ����������ʷԭ������������һ����־��ԭ�����øñ�־��ʾ��ϵͳ����
 * ���ź��жϺ󣬲��������������жϵ�ϵͳ���á����ڸñ�־�ѱ� SA_RESTART ���档
 *
 *	SA_NOMASK --- �������κ��źţ�������ִ���źŴ������Ĺ��������յ��ź�(�����źŶ�
 * ������)�������øñ�־������ʾ���������źţ�����ֻ���� sigaction �ṹ��Ӧ���Ǹ��źš�
 *
 *	SA_ONESHOT --- �ñ�־��ʾ�û��Զ�����źŴ�����ִ�й�һ��֮��ͻָ�ΪĬ�ϵ��ź�
 * ������ SIG_DFL������û������Զ�����źŴ�����������Ч���Ͳ�Ҫ���øñ�־��
 */
#define SA_NOCLDSTOP	1
#define SA_STACK	0x08000000
#define SA_RESTART	0x10000000
#define SA_INTERRUPT	0x20000000
#define SA_NOMASK	0x40000000
#define SA_ONESHOT	0x80000000

/*
 *	���º궨���� sys_sigprocmask ��ʹ�ã�����ϵͳ���� sigprocmask ��Ӧ��ϵͳ���ô���
 * ������sigprocmask ���ڸı䵱ǰ����������źż���ͬʱ���ص�ǰ�����ԭ�����źż���
 *
 *	SIG_BLOCK --- �������źż��м���ָ���źż���
 *
 *	SIG_UNBLOCK --- �������źż���ɾ��ָ���źż���
 *
 *	SIG_SETMASK --- �������������źż���
 */
#define SIG_BLOCK          0	/* for blocking signals */
#define SIG_UNBLOCK        1	/* for unblocking signals */
#define SIG_SETMASK        2	/* for setting the signal mask */

/* Type of a signal handler.  */
typedef void (*__sighandler_t)(int);

/*
 *	���º����ڶ����źŵĴ�����:
 *
 *	SIG_DFL --- Ĭ�ϵ��źŴ����������źŵĴ����� sa_handler == SIG_DFL����ϵͳ
 * ��ִ�и��źŵ�Ĭ�ϲ����������ź� SIGINT ��Ĭ�ϲ�������ֹ����
 *
 *	SIG_IGN --- �����źŵĴ����������źŵĴ����� sa_handler == SIG_IGN����ϵͳ
 * �����Ը��źţ������κδ���
 *
 *	SIG_ERR --- �źŴ����ش�������źŵĴ����� sa_handler == SIG_ERR����ϵͳ��
 * ���ش��󣬴������� errno ������
 */
#define SIG_DFL	((__sighandler_t)0)	/* default signal handling */
#define SIG_IGN	((__sighandler_t)1)	/* ignore signal */
#define SIG_ERR	((__sighandler_t)-1)	/* error return from signal */

/*
 *	struct sigaction: �źŵ����Խṹ��ÿ���źŶ���һ�����������Խṹ�����������źŵ�
 * ������Ϣ��
 *
 *	sa_handler --- �źŶ�Ӧ�Ĵ�������
 *
 *	sa_mask --- ��Щλ�� 1�����źŴ������ִ��ʱ����������Щ�źŵĴ���
 *
 *	sa_flags --- �źŶ�Ӧ��һЩ��־����Щ��־��ı��źŵĴ�����̡�
 *
 *	sa_restorer --- ������Ա�ʾ�ĺ����Ѿ��ı䡣ԭ�����ڱ�ʾ�źŵĻָ��������źŵĻָ�
 * �����ɿ⺯���ṩ���û�����Ҫ���ģ��������û��Զ�����źŴ�����ִ�н���ʱ�ָ������ԭʼ
 * �ֳ���
 *	����ϵͳʹ�����µĻָ�ԭʼ�ֳ��ķ������Ҳ�����Ҫ�⺯���ṩ�ָ�����������������Ծ�
 * �������������źŵĻָ�������
 *	Ŀǰ������Ե��ô���: �û�����ͨ�����ֶ��������źŴ��������ʹ�õ�ջ(�ź�ջ)������
 * ���洢�ź�ջָ�롣
 */
struct sigaction {
	__sighandler_t sa_handler;
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_restorer)(void);
};

#endif
