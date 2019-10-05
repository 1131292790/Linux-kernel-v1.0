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

/*
 *	wait_queue: ����ṹ���ʾ�Ĳ���һ���ȴ����У����ǵȴ������е�һ��Ԫ�ء�
 * �ȴ������������ɸ�������Ԫ��(struct wait_queue)��ɵ�һ����ѭ������
 *
 *	ϵͳ�еĵȴ����н���һ��ָ�� struct wait_queue ��ָ������ʾ������ task_struct
 * �ṹ�е� struct wait_queue *wait_chldexit �ͱ�ʾ�ȴ��������˳��ĵȴ����С����ָ��
 * ����Զָ���������ȴ������е��Ǹ�Ԫ�أ�����μ� add_wait_queue��
 *
 *	����:	task --- �ȴ������и�Ԫ�ض�Ӧ������
 *		next --- �ȴ�������ָ����һ��Ԫ�ص�ָ�롣
 */
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
