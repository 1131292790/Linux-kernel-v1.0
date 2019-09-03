#ifndef _LINUX_TIME_H
#define _LINUX_TIME_H

/*
 *	struct timeval: ϵͳ��ǰʱ��ṹ�壬��ʾ�� 1970 �� 1 �� 1 �� 0 ʱ������
 * ��������������΢������
 */
struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* microseconds */
};

struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};

#define NFDBITS			__NFDBITS

#define FD_SETSIZE		__FD_SETSIZE
#define FD_SET(fd,fdsetp)	__FD_SET(fd,fdsetp)
#define FD_CLR(fd,fdsetp)	__FD_CLR(fd,fdsetp)
#define FD_ISSET(fd,fdsetp)	__FD_ISSET(fd,fdsetp)
#define FD_ZERO(fdsetp)		__FD_ZERO(fdsetp)

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
/*
 *	�������궨������������ص����ּ����ʱ�������ͣ�ÿ�����͵ļ����ʱ��������������
 * incr �� value������ incr �Ǽ���������ĳ�ʼֵ��value �Ǽ���������ĵ�ǰֵ�����ǵĵ�λ
 * ����ʱ�ӵδ�(tick)����Щ�����Ķ��嶼������� task_struct �ṹ�У���ÿ������֧������
 * �����͵ļ����ʱ����
 *
 *	�����ʱ�����ǵ���ʱ�������󣬼���������ĵ�ǰֵ value �����ϼ�С���� value ����
 * 0 ʱ���ö�ʱ�����ڡ�
 *
 *	1. ITIMER_REAL: ��ʵ�����ʱ������ʵ�����ʱ�������󣬲��������Ƿ����У�ÿ�� tick
 * ������������ĵ�ǰֵ value �� 1���� value ���� 0 ʱ���ں˽��������� SIGALRM �źš�
 *
 *	2. ITIMER_VIRTUAL: ��������ʱ����Ҳ��Ϊ������û�̬�����ʱ������������ʱ��
 * ������ֻ�е��������û�̬������ʱ��ÿ�� tick �Ž���������ĵ�ǰֵ value �� 1���� value
 * ���� 0 ʱ���ں��������� SIGVTALRM �źţ����� value ��ֵ����Ϊ�������ĳ�ʼֵ incr��
 *
 *	3. ITIMER_PROF: PROF �����ʱ����PROF �����ʱ��������ֻҪ�������������У�����
 * �����ں�̬�������û�̬�����У�ÿ�� tick �����������ĵ�ǰֵ value �� 1���� value ���� 0 ʱ��
 * �ں��������� SIGPROF �źţ����� value ��ֵ����Ϊ�������ĳ�ʼֵ incr��
 */
#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2

/*
 *	struct itimerval: ����ṹ�������û���������ļ����ʱ����
 */
struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
};

#endif
