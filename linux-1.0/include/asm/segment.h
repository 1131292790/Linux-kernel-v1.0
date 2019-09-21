#ifndef _ASM_SEGMENT_H
#define _ASM_SEGMENT_H

/*
 *	�� sys_call.S �ļ��� SAVE_ALL �У�������ͨ��ϵͳ���ô��û�̬���뵽�ں�̬ʱ��
 * �ں˳�������öμĴ��� DS = ES = KERNEL_DS ���ڷ����ں����ݶΣ�FS = USER_DS ����
 * �����û����ݶΡ�
 *
 *	��ˣ���ִ���ں˴���ʱ����Ҫ����������û�̬�ռ��е����ݣ�����Ҫʹ�������
 * ��ʽ�������ں�̬�£���Ҫͨ�� FS �μĴ������û�̬�ռ�������ݽ�����
 */

/*
 *	get_fs_byte:
 *	get_user_byte: �ӵ�ǰ������û�̬�ռ��е� addr ��ַ����ȡһ���ֽڵ����ݡ�
 */
static inline unsigned char get_user_byte(const char * addr)
{
	register unsigned char _v;

	__asm__ ("movb %%fs:%1,%0":"=q" (_v):"m" (*addr));	/* _v = fs:[addr] ��ַ����һ���ֽ�ֵ */
	return _v;
}

#define get_fs_byte(addr) get_user_byte((char *)(addr))

/*
 *	get_fs_word:
 *	get_user_word: �ӵ�ǰ������û�̬�ռ��е� addr ��ַ����ȡһ����(2 �ֽ�)�����ݡ�
 */
static inline unsigned short get_user_word(const short *addr)
{
	unsigned short _v;

	__asm__ ("movw %%fs:%1,%0":"=r" (_v):"m" (*addr));	/* _v = fs:[addr] ��ַ����һ����ֵ */
	return _v;
}

#define get_fs_word(addr) get_user_word((short *)(addr))

/*
 *	get_fs_long:
 *	get_user_long: �ӵ�ǰ������û�̬�ռ��е� addr ��ַ����ȡһ������(4 �ֽ�)�����ݡ�
 */
static inline unsigned long get_user_long(const int *addr)
{
	unsigned long _v;

	__asm__ ("movl %%fs:%1,%0":"=r" (_v):"m" (*addr)); \	/* _v = fs:[addr] ��ַ����һ������ֵ */
	return _v;
}

#define get_fs_long(addr) get_user_long((int *)(addr))

/*
 *	put_fs_byte:
 *	put_user_byte: ��ǰ������û�̬�ռ��е� addr ��ַ��д��һ���ֽ�ֵ val��
 */
static inline void put_user_byte(char val,char *addr)
{
__asm__ ("movb %0,%%fs:%1": /* no outputs */ :"iq" (val),"m" (*addr));	/* fs:[addr] = val(byte) */
}

#define put_fs_byte(x,addr) put_user_byte((x),(char *)(addr))

/*
 *	put_fs_word:
 *	put_user_word: ��ǰ������û�̬�ռ��е� addr ��ַ��д��һ����ֵ(2 �ֽ�) val��
 */
static inline void put_user_word(short val,short * addr)
{
__asm__ ("movw %0,%%fs:%1": /* no outputs */ :"ir" (val),"m" (*addr));	/* fs:[addr] = val(word) */
}

#define put_fs_word(x,addr) put_user_word((x),(short *)(addr))

/*
 *	put_fs_long:
 *	put_user_long: ��ǰ������û�̬�ռ��е� addr ��ַ��д��һ������ֵ(4 �ֽ�) val��
 */
static inline void put_user_long(unsigned long val,int * addr)
{
__asm__ ("movl %0,%%fs:%1": /* no outputs */ :"ir" (val),"m" (*addr));	/* fs:[addr] = val(long) */
}

#define put_fs_long(x,addr) put_user_long((x),(int *)(addr))

static inline void __generic_memcpy_tofs(void * to, const void * from, unsigned long n)
{
__asm__("cld\n\t"
			/* cld: �巽��λ��ʹ��ַ������std: �÷���λ��ʹ��ַ�Լ� */
	"push %%es\n\t"
			/* movsb movsw movsl ָ��Ҫ�õ� es���ʽ�ԭ es ���� */
	"push %%fs\n\t"
	"pop %%es\n\t"
			/* es = fs��es ������ѡ���û����ݶ� */
	"testb $1,%%cl\n\t"
	"je 1f\n\t"
	"movsb\n"
			/*
			 *	���� ecx �е� n �� bit0 λ�Ƿ���� 0��������� 0������ǰ��ת����� 1 ����
			 *
			 *	������� 1����˵�� n ��һ�����������ȴ� ds:esi(from) ������һ���ֽڵ� es:edi(to)
			 * ������ʹ ecx(n) -= 1��ͬʱ esi �� edi ָ������һ���ֽ�ָ����һ��Ҫ���Ƶ�λ�á����ƺ�
			 * ecx �е� n �ͱ����һ��ż����
			 */
	"1:\ttestb $2,%%cl\n\t"
	"je 2f\n\t"
	"movsw\n"
			/*
			 *	���� ecx �е� n �� bit1 λ�Ƿ���� 0��������� 0������ǰ��ת����� 2 ����
			 *
			 *	������� 1����˵�� n �� 2 �ı߽���룬���ȴ� ds:esi(from) �����������ֽڵ�
			 * es:edi(to) ������ʹ ecx �е� n �� 1��ͬʱ from �� to ָ�����������ֽ�ָ����һ��Ҫ
			 * ���Ƶ�λ�á����ƺ�ʣ��Ļ�δ���Ƶ��ֽ������� 4 �������ˣ�����Ϳ���ÿ�θ��� 4 ��
			 * �ֽ��ˡ�
			 *	���θ��Ƶ� ecx -= 1 ��ʾ����һ���֣�ʵ���ϼ� 1 ֮�� ecx �ֻ���һ��������
			 * ������������ŻὫ ecx ���� 4�������������������Ժ���Ľ������Ӱ�졣
			 *
			 *	ǰ�����������Ե���Ҫ������: ������ 4 �ֽڵ��������� movsb �� movsw ���ƹ�ȥ��
			 * ����ʣ�µ����ݾͿ���ѭ���� movsl ÿ�θ��� 4 �ֽ��ˡ�
			 */
	"2:\tshrl $2,%%ecx\n\t"
	"rep ; movsl\n\t"
			/*
			 *	ecx(n) >>= 2����ʱ ecx �е� n �ͱ�ʾÿ�θ��� 4 ���ֽ�ʱ�ܹ���Ҫ���ƵĴ����ˡ�
			 *
			 *	repeat, ds:esi ===> es:edi, esi += 4, edi += 4, ecx -= 1. until ecx == 0.
			 *
			 *	ѭ�����ƣ�ÿ�ν� ds:esi(from) ָ����ں˿ռ��е����ݸ��� 4 �ֽڵ� es:edi(to)
			 * ָ����û��ռ��У�ÿ�θ��ƺ� esi(from) �� edi(to) ������ 4 ���ֽ�ָ����һ��Ҫ����
			 * ��λ�ã����� ecx(n) �� 1��ֱ�� ecx(n) == 0 ʱ��Ҳ�������е����ݶ��������Ժ�ѭ��
			 * ������
			 */
	"pop %%es"
			/* �ָ�ԭ es */
	: /* no outputs */
	:"c" (n),"D" ((long) to),"S" ((long) from)	/* ecx = n, edi = to, esi = from */
	:"cx","di","si");
}

static inline void __constant_memcpy_tofs(void * to, const void * from, unsigned long n)
{
	/* 4 ���ֽ����ڵĸ��Ʒ�ʽ�����ָ��Ʒ�ʽ��ָ�����٣�ִ���ٶȸ��� */
	switch (n) {
		case 0:
			return;
		case 1:
			put_user_byte(*(const char *) from, (char *) to);
			return;
		case 2:
			put_user_word(*(const short *) from, (short *) to);
			return;
		case 3:
			put_user_word(*(const short *) from, (short *) to);
			put_user_byte(*(2+(const char *) from), 2+(char *) to);
			return;
		case 4:
			put_user_long(*(const int *) from, (int *) to);
			return;
	}
#define COMMON(x) \
__asm__("cld\n\t" \
	"push %%es\n\t" \
	"push %%fs\n\t" \
	"pop %%es\n\t" \
	"rep ; movsl\n\t" \
	x \
	"pop %%es" \
	: /* no outputs */ \
	:"c" (n/4),"D" ((long) to),"S" ((long) from) \	/* ecx = n/4, edi = to, esi = from */
	:"cx","di","si")

	/*
	 *	���� 4 ���ֽ�ʱ��ѭ�����ƣ�ÿ�θ��� 4 ���ֽڣ����ƴ���Ϊ n/4������� 4
	 * ���ֽڵĲ����� movsb �� movsw ����������ơ�
	 */
	switch (n % 4) {
		case 0:
			COMMON("");
			return;
		case 1:
			COMMON("movsb\n\t");
			return;
		case 2:
			COMMON("movsw\n\t");
			return;
		case 3:
			COMMON("movsw\n\tmovsb\n\t");
			return;
	}
#undef COMMON
}

static inline void __generic_memcpy_fromfs(void * to, const void * from, unsigned long n)
{
__asm__("cld\n\t"
	"testb $1,%%cl\n\t"
	"je 1f\n\t"
	"fs ; movsb\n"
	"1:\ttestb $2,%%cl\n\t"
	"je 2f\n\t"
	"fs ; movsw\n"
	"2:\tshrl $2,%%ecx\n\t"
	"rep ; fs ; movsl"
	: /* no outputs */
	:"c" (n),"D" ((long) to),"S" ((long) from)
	:"cx","di","si","memory");
}

static inline void __constant_memcpy_fromfs(void * to, const void * from, unsigned long n)
{
	switch (n) {
		case 0:
			return;
		case 1:
			*(char *)to = get_user_byte((const char *) from);
			return;
		case 2:
			*(short *)to = get_user_word((const short *) from);
			return;
		case 3:
			*(short *) to = get_user_word((const short *) from);
			*(char *) to = get_user_byte(2+(const char *) from);
			return;
		case 4:
			*(int *) to = get_user_long((const int *) from);
			return;
	}
#define COMMON(x) \
__asm__("cld\n\t" \
	"rep ; fs ; movsl\n\t" \
	x \
	: /* no outputs */ \
	:"c" (n/4),"D" ((long) to),"S" ((long) from) \
	:"cx","di","si","memory")

	switch (n % 4) {
		case 0:
			COMMON("");
			return;
		case 1:
			COMMON("fs ; movsb");
			return;
		case 2:
			COMMON("fs ; movsw");
			return;
		case 3:
			COMMON("fs ; movsw\n\tfs ; movsb");
			return;
	}
#undef COMMON
}

/*
 *	memcpy_fromfs: �� from ָ����û��ռ��и��� n ���ֽڵ� to ָ����ں˿ռ��С�
 */
#define memcpy_fromfs(to, from, n) \
(__builtin_constant_p(n) ? \
 __constant_memcpy_fromfs((to),(from),(n)) : \
 __generic_memcpy_fromfs((to),(from),(n)))

/*
 *	memcpy_tofs: �� from ָ����ں˿ռ��и��� n ���ֽڵ� to ָ����û��ռ��С�
 */
#define memcpy_tofs(to, from, n) \
(__builtin_constant_p(n) ? \
 __constant_memcpy_tofs((to),(from),(n)) : \
 __generic_memcpy_tofs((to),(from),(n)))

/*
 * Someone who knows GNU asm better than I should double check the followig.
 * It seems to work, but I don't know if I'm doing something subtly wrong.
 * --- TYT, 11/24/91
 * [ nothing wrong here, Linus: I just changed the ax to be any reg ]
 */

/*
 *	get_fs: ��ȡ��ǰ�� FS �μĴ�����ֵ��FS �д�ŵ���ĳһ���εĶ�ѡ�����
 */
static inline unsigned long get_fs(void)
{
	unsigned long _v;
	__asm__("mov %%fs,%w0":"=r" (_v):"0" (0));
	return _v;
}

/*
 *	get_ds: ��ȡ��ǰ�� DS �μĴ�����ֵ��DS �д�ŵ���ĳһ���εĶ�ѡ�����
 */
static inline unsigned long get_ds(void)
{
	unsigned long _v;
	__asm__("mov %%ds,%w0":"=r" (_v):"0" (0));
	return _v;
}

/*
 *	set_fs: ���� FS �μĴ�����ֵ��
 */
static inline void set_fs(unsigned long val)
{
	__asm__ __volatile__("mov %w0,%%fs": /* no output */ :"r" (val));
}

#endif /* _ASM_SEGMENT_H */
