#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

/*
 *	wait4 ϵͳ���û��õ��ı�־:
 *
 *	WNOHANG: ��ʾ���ָ����������û���˳���Ҳ����û�д��� TASK_ZOMBIE ״̬
 * ʱ��ϵͳ���ô����������Ϸ��أ�����Ҫ����ǰ����������ȴ���
 *
 *	WUNTRACED: ��ʾ���ָ�����������Ѿ����� TASK_STOPPED ״̬����ϵͳ����
 * �����������Ϸ��ء�
 *
 *	__WCLONE: ��ʾֻ�ȴ�ͨ�� clone �ķ�ʽ������������Ҳ���ǵȴ����߳��˳���
 * �����߳������Ե�ַ�ռ��ϵ������ԣ����߳��˳�ʱ����ʹ�� SIGCHLD ������ź���
 * ֪ͨ�丸���񣬼����߳��˳�ʱ������������ SIGCHLD �źš�
 */
#define WNOHANG		0x00000001
#define WUNTRACED	0x00000002

#define __WCLONE	0x80000000

struct wait_queue {
	struct task_struct * task;
	struct wait_queue * next;
};

struct semaphore {
	int count;
	struct wait_queue * wait;
};

#define MUTEX ((struct semaphore) { 1, NULL })

struct select_table_entry {
	struct wait_queue wait;
	struct wait_queue ** wait_address;
};

typedef struct select_table_struct {
	int nr;
	struct select_table_entry * entry;
} select_table;

#define __MAX_SELECT_TABLE_ENTRIES (4096 / sizeof (struct select_table_entry))

#endif
