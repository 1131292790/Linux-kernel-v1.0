#ifndef _LINUX_UNISTD_H
#define _LINUX_UNISTD_H

/*
 * This file contains the system call numbers and the syscallX
 * macros
 */
/*
 *	ϵͳ���úţ�����ϵͳ���ú����� sys_call_table[] �е�����ֵ��
 */
#define __NR_setup		  0	/* used only by init, to get system going */
#define __NR_exit		  1
#define __NR_fork		  2
#define __NR_read		  3
#define __NR_write		  4
#define __NR_open		  5
#define __NR_close		  6
#define __NR_waitpid		  7
#define __NR_creat		  8
#define __NR_link		  9
#define __NR_unlink		 10
#define __NR_execve		 11
#define __NR_chdir		 12
#define __NR_time		 13
#define __NR_mknod		 14
#define __NR_chmod		 15
#define __NR_chown		 16
#define __NR_break		 17
#define __NR_oldstat		 18
#define __NR_lseek		 19
#define __NR_getpid		 20
#define __NR_mount		 21
#define __NR_umount		 22
#define __NR_setuid		 23
#define __NR_getuid		 24
#define __NR_stime		 25
#define __NR_ptrace		 26
#define __NR_alarm		 27
#define __NR_oldfstat		 28
#define __NR_pause		 29
#define __NR_utime		 30
#define __NR_stty		 31
#define __NR_gtty		 32
#define __NR_access		 33
#define __NR_nice		 34
#define __NR_ftime		 35
#define __NR_sync		 36
#define __NR_kill		 37
#define __NR_rename		 38
#define __NR_mkdir		 39
#define __NR_rmdir		 40
#define __NR_dup		 41
#define __NR_pipe		 42
#define __NR_times		 43
#define __NR_prof		 44
#define __NR_brk		 45
#define __NR_setgid		 46
#define __NR_getgid		 47
#define __NR_signal		 48
#define __NR_geteuid		 49
#define __NR_getegid		 50
#define __NR_acct		 51
#define __NR_phys		 52
#define __NR_lock		 53
#define __NR_ioctl		 54
#define __NR_fcntl		 55
#define __NR_mpx		 56
#define __NR_setpgid		 57
#define __NR_ulimit		 58
#define __NR_oldolduname	 59
#define __NR_umask		 60
#define __NR_chroot		 61
#define __NR_ustat		 62
#define __NR_dup2		 63
#define __NR_getppid		 64
#define __NR_getpgrp		 65
#define __NR_setsid		 66
#define __NR_sigaction		 67
#define __NR_sgetmask		 68
#define __NR_ssetmask		 69
#define __NR_setreuid		 70
#define __NR_setregid		 71
#define __NR_sigsuspend		 72
#define __NR_sigpending		 73
#define __NR_sethostname	 74
#define __NR_setrlimit		 75
#define __NR_getrlimit		 76
#define __NR_getrusage		 77
#define __NR_gettimeofday	 78
#define __NR_settimeofday	 79
#define __NR_getgroups		 80
#define __NR_setgroups		 81
#define __NR_select		 82
#define __NR_symlink		 83
#define __NR_oldlstat		 84
#define __NR_readlink		 85
#define __NR_uselib		 86
#define __NR_swapon		 87
#define __NR_reboot		 88
#define __NR_readdir		 89
#define __NR_mmap		 90
#define __NR_munmap		 91
#define __NR_truncate		 92
#define __NR_ftruncate		 93
#define __NR_fchmod		 94
#define __NR_fchown		 95
#define __NR_getpriority	 96
#define __NR_setpriority	 97
#define __NR_profil		 98
#define __NR_statfs		 99
#define __NR_fstatfs		100
#define __NR_ioperm		101
#define __NR_socketcall		102
#define __NR_syslog		103
#define __NR_setitimer		104
#define __NR_getitimer		105
#define __NR_stat		106
#define __NR_lstat		107
#define __NR_fstat		108
#define __NR_olduname		109
#define __NR_iopl		110
#define __NR_vhangup		111
#define __NR_idle		112
#define __NR_vm86		113
#define __NR_wait4		114
#define __NR_swapoff		115
#define __NR_sysinfo		116
#define __NR_ipc		117
#define __NR_fsync		118
#define __NR_sigreturn		119
#define __NR_clone		120
#define __NR_setdomainname	121
#define __NR_uname		122
#define __NR_modify_ldt		123
#define __NR_adjtimex		124
#define __NR_mprotect		125
#define __NR_sigprocmask	126
#define __NR_create_module	127
#define __NR_init_module	128
#define __NR_delete_module	129
#define __NR_get_kernel_syms	130
#define __NR_quotactl		131
#define __NR_getpgid		132
#define __NR_fchdir		133
#define __NR_bdflush		134

extern int errno;

/* XXX - _foo needs to be __foo, while __NR_bar could be _NR_bar. */

/*
 *	����ϵͳ����Ƕ��ʽ���꺯����
 *
 *	1. ##�����ַ������ڽ�ǰ�������ַ���������һ����һ���ַ�����__NR_##name ��Ԥ����׶εĺ궨��
 * �滻ʱ��ת��Ϊ __NR_name������ name ��ϵͳ��������__NR_name �ٱ���Ӧ�ĺ궨���滻�����ϵͳ���úš�
 *	���� fork ϵͳ���� ===> __NR_##fork ===> __NR_fork ===> 2����ʾ fork ��ϵͳ���ú��� 2��
 *
 *	2. "int $0x80" ָ���ϵͳ���ã��漴������ִ��ϵͳ�����쳣(�ж�)����ǰ�����̱���ͣ�� "int $0x80"
 * ָ��ĺ�һ����� [ if (__res >= 0) ] ����ϵͳ���÷��غ�Ӵ˴���������ִ�С�
 *
 *	3. ϵͳ����ָ�� "int $0x80" ʹ�ô���������Ȩ���� 3 �л�Ϊ 0��ʹ�õ�ջ����Ȩ�� 3 ��Ӧ��ջ�л�Ϊ
 * ��Ȩ�� 0 ��Ӧ��ջ��
 *	�����ڵ�ǰ�����������������û�̬�����ں�̬��������ʹ�õ�ջ�ɽ��̵��û�̬ջ���Ϊ���̵��ں�̬ջ��
 *
 *	4. �����������͵�ϵͳ���ã�����ϵͳ���õĵ�һ����������ϵͳ���ú� __NR_##name��ͨ�� eax �Ĵ���
 * �������ϵͳ���úţ�����������������ϵͳ���ú�ִ�ж�Ӧ��ϵͳ���ô�������
 *	ϵͳ����ֻ��һ������ֵ��ͨ�� eax �Ĵ����������ֵ��eax >= 0 ��ʾϵͳ����ִ�гɹ���eax < 0 ��ʾ
 * ϵͳ����ִ��ʧ�ܣ��������ֵ���ջᱻ������� __res �С�
 *
 *	5. ��ϵͳ����ִ��ʧ�ܣ����Ӧ�Ĵ���Żᱻ������ errno [ errno = -__res ] �У�ϵͳ����Ƕ��ʽ���
 * �꺯�����᷵�� -1�����������֪����Ĵ���ԭ����Ҫͨ�� errno ȷ����
 *	��ϵͳ����ִ�гɹ����꺯��ֱ�ӽ�ϵͳ���õķ���ֵ���ء�
 *
 *	6. 0x80 ���ж϶�Ӧ���жϴ�����Ϊ system_call����ʵ��λ�� sys_call.S �е� _system_call ����
 */


/*
 *	_syscall0: ����������ϵͳ���ú꺯�� type name(void)��
 */
#define _syscall0(type,name) \
type name(void) \
{ \
	long __res; \
	__asm__ volatile ("int $0x80" \		/* ����ִ��ϵͳ���� system_call����ǰ���̱��жϴ�� */
			: "=a" (__res) \
			: "0" (__NR_##name)); \
					/*
					 *	output: ϵͳ����ֻ��һ������ֵ���ᱻ���� eax �Ĵ����У�
					 * ���մ��뷵��ʱ eax �е�ֵ�ᱻ������� __res �С�
					 *
					 *	input: ����ϵͳ���õĵ�һ��������ϵͳ���ú� __NR_##name��
					 * ��ֵ�������� eax �Ĵ����д��ݡ�
					 */
	if (__res >= 0) \
		return (type) __res; \		/* ϵͳ����ִ�гɹ� */
	errno = -__res; \
	return -1; \				/* ϵͳ����ִ��ʧ�� */
}

/*
 *	_syscall1: ��һ��������ϵͳ���ú꺯�� type name(atype a)��
 */
#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
	long __res; \
	__asm__ volatile ("int $0x80" \
			: "=a" (__res) \
			: "0" (__NR_##name),"b" ((long)(a))); \
					/*
					 *	input: �꺯���Ĳ��� a ��ͨ�� ebx �Ĵ������ݡ�
					 */
	if (__res >= 0) \
		return (type) __res; \
	errno = -__res; \
	return -1; \
}

/*
 *	_syscall1: ������������ϵͳ���ú꺯�� type name(atype a, btype b)��
 */
#define _syscall2(type,name,atype,a,btype,b) \
type name(atype a,btype b) \
{ \
	long __res; \
	__asm__ volatile ("int $0x80" \
			: "=a" (__res) \
			: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b))); \
					/*
					 *	input: �꺯���Ĳ��� a��b ������ͨ�� ebx��ecx �Ĵ������ݡ�
					 */
	if (__res >= 0) \
		return (type) __res; \
	errno = -__res; \
	return -1; \
}

/*
 *	_syscall1: ������������ϵͳ���ú꺯�� type name(atype a, btype b, ctype c)��
 */
#define _syscall3(type,name,atype,a,btype,b,ctype,c) \
type name(atype a,btype b,ctype c) \
{ \
	long __res; \
	__asm__ volatile ("int $0x80" \
			: "=a" (__res) \
			: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b)),"d" ((long)(c))); \
					/*
					 *	input: �꺯���Ĳ��� a��b��c ������ͨ�� ebx��ecx��edx
					 * �Ĵ������ݡ�
					 */
	if (__res>=0) \
		return (type) __res; \
	errno=-__res; \
	return -1; \
}

/*
 *	_syscall1: ���ĸ�������ϵͳ���ú꺯�� type name(atype a, btype b, ctype c, dtype d)��
 */
#define _syscall4(type,name,atype,a,btype,b,ctype,c,dtype,d) \
type name (atype a, btype b, ctype c, dtype d) \
{ \
	long __res; \
	__asm__ volatile ("int $0x80" \
			: "=a" (__res) \
			: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b)), \
			  "d" ((long)(c)),"S" ((long)(d))); \
					/*
					 *	input: �꺯���Ĳ��� a��b��c��d��������ͨ��
					 * ebx��ecx��edx��esi �Ĵ������ݡ�
					 */
	if (__res>=0) \
		return (type) __res; \
	errno=-__res; \
	return -1; \
}

/*
 *	_syscall1: �����������ϵͳ���ú꺯�� type name(atype a, btype b, ctype c, dtype d, etype e)��
 */
#define _syscall5(type,name,atype,a,btype,b,ctype,c,dtype,d,etype,e) \
type name (atype a,btype b,ctype c,dtype d,etype e) \
{ \
	long __res; \
	__asm__ volatile ("int $0x80" \
			: "=a" (__res) \
			: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b)), \
			  "d" ((long)(c)),"S" ((long)(d)),"D" ((long)(e))); \
					/*
					 *	input: �꺯���Ĳ��� a��b��c��d��e ������ͨ��
					 * ebx��ecx��edx��esi��edi �Ĵ������ݡ�
					 */
	if (__res>=0) \
		return (type) __res; \
	errno=-__res; \
	return -1; \
}

#endif /* _LINUX_UNISTD_H */
