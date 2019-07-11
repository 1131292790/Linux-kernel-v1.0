#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

/*
 *	linux/include/asm/irq.h
 *
 *	(C) 1992, 1993 Linus Torvalds
 */

#include <linux/segment.h>
#include <linux/linkage.h>

extern void disable_irq(unsigned int);
extern void enable_irq(unsigned int);

/*
 *	#x: �� x ���ַ�����������ʹ����һ���ַ�����
 */
#define __STR(x) #x
#define STR(x) __STR(x)

/*
 *	SAVE_ALL: �� sys_call.S �е� SAVE_ALL ����һ�£�������ͨ�жϣ���Ϊ��ͨ�жϵ�β��Ҫ��ת
 * �� ret_from_sys_call ȥ����ǰ��������������źţ��Ҵ�����Ϻ�Ҫ�� ret_from_sys_call ��ʹ��
 * RESTORE_ALL �˳��жϣ����Դ˴��� SAVE_ALL ��� sys_call.S �е� SAVE_ALL һ�²��С�
 *
 *	1. �ֶ����� GS ---> EBX �Ĵ�����ֵ��
 *	2. ���� DS = ES = KERNEL_DS ���ڷ����ں����ݶΣ����� FS = USER_DS ���ڷ����û����ݶΡ�
 * ����ͨ�жϴ�������У�DS �� ES ���ڷ����ں˿ռ䣬FS ���ڷ����û��ռ䡣
 *	3. ���� %db7 = 0����ֹӲ�����ԡ�
 */
#define SAVE_ALL \
	"cld\n\t" \
	"push %gs\n\t" \
	"push %fs\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %ebp\n\t" \
	"pushl %edi\n\t" \
	"pushl %esi\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"pushl %ebx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t" \
	"movl $" STR(USER_DS) ",%edx\n\t" \
	"mov %dx,%fs\n\t"   \
	"movl $0,%edx\n\t"  \
	"movl %edx,%db7\n\t"

/*
 * SAVE_MOST/RESTORE_MOST is used for the faster version of IRQ handlers,
 * installed by using the SA_INTERRUPT flag. These kinds of IRQ's don't
 * call the routines that do signal handling etc on return, and can have
 * more relaxed register-saving etc. They are also atomic, and are thus
 * suited for small, fast interrupts like the serial lines or the harddisk
 * drivers, which don't actually need signal handling etc.
 *
 * Also note that we actually save only those registers that are used in
 * C subroutines (%eax, %edx and %ecx), so if you do something weird,
 * you're on your own. The only segments that are saved (not counting the
 * automatic stack and code segment handling) are %ds and %es, and they
 * point to kernel space. No messing around with %fs here.
 */
/*
 *	SAVE_MOST/RESTORE_MOST ����ͨ��ʹ�� SA_INTERRUPT ��־��װ�Ŀ����жϴ�����򡣿����ж�
 * ���˳�֮ǰ��������źŴ������̣����ҿ����и����ɵļĴ������淽�������ּĴ����ı���ͻָ�
 * Ҳ��ԭ�ӵģ����������С�͡������жϣ����紮���жϻ�Ӳ���������жϵȣ���Щ�ж�ʵ���ϲ���Ҫ
 * �źŴ���
 *
 *	���˴������ڽ����ж�ʱ�Զ�������ֳ���Ϣ���⣬�˴�����ֻ�������ж� C ���������õ���
 * ��Щ�Ĵ���(%eax, %edx �� %ecx)�������� ES �� DS����������ָ���ں˿ռ䣬������жϲ���Ҫ��
 * �û��ռ佻�����ݣ���˴˴����� FS �����á�
 */

/*
 *	SAVE_MOST: �����ж����ֶ�����ļĴ������������Զ�����ļĴ����� sys_call.S ��һ�£�
 * �˴��ֶ����� ES -> ECX�������� DS = ES = KERNEL_DS ���ڷ����ں˿ռ䡣
 */
#define SAVE_MOST \
	"cld\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t"

/*
 *	RESTORE_MOST: �ֶ��ָ������ж��б���ļĴ��� ECX -> ES����ִ�� iret ָ���˳��жϣ�
 * iret ָ��ִ��ʱ���������Զ��ָ��ж�ǰ���ֳ���
 */
#define RESTORE_MOST \
	"popl %ecx\n\t" \
	"popl %edx\n\t" \
	"popl %eax\n\t" \
	"pop %ds\n\t" \
	"pop %es\n\t" \
	"iret"

/*
 * The "inb" instructions are not needed, but seem to change the timings
 * a bit - without them it seems that the harddisk driver won't work on
 * all hardware. Arghh.
 */
/*
 *	ACK_FIRST(mask): ���� 8259A оƬ��Ӧ��mask ����ȡ 8 ��ֵ����ʾ�� 8259A оƬ�ϵ�
 * 8 ���жϡ�
 *
 *	1. ���� mask ��Ӧ���ж�����ʹ�жϿ��������ܷ������жϵ��ж������źš�
 *	2. ���� 8259A �жϿ��������� EOI ָ�����Ӳ���жϡ���������� EOI�����ʾ��ǰ�ж�
 * �ź�δ�����������ж��źŲ��ܷ��͡�
 */
#define ACK_FIRST(mask) \
	"inb $0x21,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ",_cache_21\n\t" \	/* cache_21 �� mask ��Ӧ��λ�� 1 */
	"movb _cache_21,%al\n\t" \
	"outb %al,$0x21\n\t" \		/* 0x21 �˿�д cache_21������ mask ��Ӧ���ж����� */
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20,%al\n\t" \
	"outb %al,$0x20\n\t"		/* 0x20 �˿�д 0x20������ 8259A �жϿ��������� EOI ָ�� */

/*
 *	ACK_SECOND(mask): ��� 8259A оƬ��Ӧ��mask ����ȡ 8 ��ֵ����ʾ�� 8259A оƬ�ϵ�
 * 8 ���жϡ�
 *
 *	1. ���� mask ��Ӧ���ж�����ʹ�жϿ��������ܷ������жϵ��ж������źš�
 *	2. ��� 8259A �жϿ��������� EOI ָ�
 *	3. ���� 8259A �жϿ��������� EOI ָ���Ϊ�� 8259A �������� 8259A �ϣ��� 8259A ��
 * �ж��źŻ��ȷ��͵��� 8259A����ͨ���� 8259A ���͵���������������Ҫ������оƬ������ EOI��
 */
#define ACK_SECOND(mask) \
	"inb $0xA1,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ",_cache_A1\n\t" \	/* cache_A1 �� mask ��Ӧ��λ�� 1 */
	"movb _cache_A1,%al\n\t" \
	"outb %al,$0xA1\n\t" \		/* 0xA1 �˿�д cache_A1������ mask ��Ӧ���ж����� */
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20,%al\n\t" \
	"outb %al,$0xA0\n\t" \		/* 0xA0 �˿�д 0x20����� 8259A �жϿ��������� EOI ָ�� */
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\toutb %al,$0x20\n\t"	/* 0x20 �˿�д 0x20������ 8259A �жϿ��������� EOI ָ�� */

/*
 *	UNBLK_FIRST(mask): ʹ���� 8259A оƬ�� mask ��Ӧ���ж�����ʹ�жϿ��������Է�����
 * �жϵ��ж������źš�mask ����ȡ 8 ��ֵ����ʾ�� 8259A оƬ�ϵ� 8 ���жϡ�
 */
#define UNBLK_FIRST(mask) \
	"inb $0x21,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~(" #mask "),_cache_21\n\t" \	/* cache_21 �� mask ��Ӧ��λ�� 0 */
	"movb _cache_21,%al\n\t" \
	"outb %al,$0x21\n\t"		/* 0x21 �˿�д cache_21��ʹ�� mask ��Ӧ���ж����� */

/*
 *	UNBLK_SECOND(mask): ʹ�ܴ� 8259A оƬ�� mask ��Ӧ���ж�����ʹ�жϿ��������Է�����
 * �жϵ��ж������źš�mask ����ȡ 8 ��ֵ����ʾ�� 8259A оƬ�ϵ� 8 ���жϡ�
 */
#define UNBLK_SECOND(mask) \
	"inb $0xA1,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~(" #mask "),_cache_A1\n\t" \	/* cache_A1 �� mask ��Ӧ��λ�� 0 */
	"movb _cache_A1,%al\n\t" \
	"outb %al,$0xA1\n\t"		/* 0xA1 �˿�д cache_A1��ʹ�� mask ��Ӧ���ж����� */

/*
 *	���жϱ�� nr = 0 Ϊ��
 */
#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)			/* nr = 0: IRQ0_interrupt(void) */
#define FAST_IRQ_NAME(nr) IRQ_NAME2(fast_IRQ##nr)	/* nr = 0: fast_IRQ0_interrupt(void) */
#define BAD_IRQ_NAME(nr) IRQ_NAME2(bad_IRQ##nr)		/* nr = 0: bad_IRQ0_interrupt(void) */

/*
 *	BUILD_IRQ(chip,nr,mask):
 *
 *	���:	chip: ȡֵΪ FIRST �� SECOND����ʾ���� 8259A �жϿ���оƬ�е���һ����
 *		nr  : ȡֵΪ 0 - 15���� 16 ���ⲿӲ���жϵ��жϱ�š�
 *		mask: �ж��ź�λ����ʾ���ж��ź��������� 8259A �� �� 8259A оƬ���ĸ������ϡ�
 *
 * 	1. �ú�����ʵ���жϺ� nr ��Ӧ�������жϴ��������ֱ�����ͨ�жϡ������жϡ���Ч�жϡ�
 *	2. ÿ���ж��ڳ�ʼ��ʱ��������Ϊ�����ж��е�����һ�����Ҷ�Ӧ���жϴ������ᱻ���õ�
 * �ж϶�Ӧ���ж����������С�
 *	3. ���жϲ���ʱ����ͨ���ж���������ֱ���ҵ���Ӧ���жϴ�������ִ�С�
 *
 *	���� 0 ���жϣ�Ҳ���Ƕ�ʱ���ж�:
 *
 *	1. BUILD_IRQ ���ʵ�� 0 ���ж϶�Ӧ�������жϴ��������ֱ�����ͨ�ж� void IRQ0_interrupt(void)��
 * �����ж� void fast_IRQ0_interrupt(void)����Ч�ж� void bad_IRQ0_interrupt(void)��
 *	2. �� sched_init �� 0 ���ж�ͨ�� request_irq ע��Ϊ��ͨ�жϣ��ж����������� 0 ���ж϶�Ӧ��
 * λ�ô��ᱣ�� IRQ0_interrupt ��������Ϣ��
 *	3. ����ʱ���жϲ���ʱ����������ִ�� 0 ���ж϶�Ӧ����ͨ�жϵ��жϴ����� IRQ0_interrupt()��
 *	4. ͬ��������� 0 ���ж�ע��Ϊ�����жϻ���Ч�жϣ��� 0 ���жϲ���ʱ��ִ�� fast_IRQ0_interrupt
 * �� bad_IRQ0_interrupt��
 */
#define BUILD_IRQ(chip,nr,mask) \
asmlinkage void IRQ_NAME(nr); \			/* nr = 0: void IRQ0_interrupt(void) */	\
asmlinkage void FAST_IRQ_NAME(nr); \		/* nr = 0: void fast_IRQ0_interrupt(void) */	\
asmlinkage void BAD_IRQ_NAME(nr); \		/* nr = 0: void bad_IRQ0_interrupt(void) */	\
__asm__( \
/*
 *	IRQ0_interrupt: 0 ���ж϶�Ӧ����ͨ�жϴ�������[ nr = 0 -> 15 ]
 */
"\n.align 4\n" \
"_IRQ" #nr "_interrupt:\n\t" \		/* void IRQ0_interrupt(void) ����ʵ�� */

	"pushl $-"#nr"-2\n\t" \
	SAVE_ALL \
			/*
			 *	sys_call.S ����ִ�� SAVE_ALL ֮ǰ������һ�� ORIG_EAX������Ϊ��ʹ�� sys_call.S
			 * �е� ret_from_sys_call ������ͨ�� RESTORE_ALL �˳��жϣ�Ҳ��Ҫ�� SAVE_ALL ֮ǰ����
			 * һ��ֵ�����ﱣ�����һ�����жϺ���ص���������ÿ���ж������ֵ���ǲ�һ���ģ�����û
			 * �����ױ����ֵ����˼��ʲô��
			 */
	ACK_##chip(mask) \
			/*
			 *	ACK_FIRST(mask) �� ACK_SECOND(mask): �������θ��жϵ��ж�����ʹ�жϿ�����
			 * ���ٷ������жϵ��ж������źţ��Է��ڴ�����жϵĹ������ٴδ������жϡ�Ȼ�����ж�
			 * ���������� EOI ָ�����Ӳ���жϣ�ʹ�����жϵ������źſ����������͡�
			 */
	"incl _intr_count\n\t"\
	"sti\n\t" \
			/*
			 *	intr_count++;
			 *	sti; ʹ�ܴ�������Ӧ�ⲿӲ���жϣ�����ͨ�жϵ�ִ�й��̿��Ա���ϣ�֧���ж�Ƕ�ס�
			 * ���Ǹ��жϵ��ж������ź��Ѿ������������ˣ�����֧���ж����롣
			 */
	"movl %esp,%ebx\n\t" \
	"pushl %ebx\n\t" \
	"pushl $" #nr "\n\t" \
			/*
			 *	��ǰջָ��(sys_call.S �е� esp0) ��Ϊ do_IRQ �ĵڶ���������ջ��esp0 �� do_IRQ
			 * �б�ת��Ϊ struct pt_regs��ջ�е������� struct pt_regs �еĳ�Աһһ��Ӧ��do_IRQ ��
			 * ͨ�� struct pt_regs ������ջ�е����ݡ�
			 *
			 *	�жϺ� nr ��Ϊ do_IRQ �ĵ�һ��������ջ��
			 */
	"call _do_IRQ\n\t" \
	"addl $8,%esp\n\t" \
			/*
			 *	call do_IRQ(nr, esp0): ����ͨ�õ���ͨ�жϴ����� do_IRQ���� do_IRQ �л����
			 * �жϺ���ִ�о�����жϴ�������
			 *
			 *	�ж�ִ����Ϻ󣬽�ջ�д��ݸ� do_IRQ ����������������ջָ��ص� esp0��
			 */
	"cli\n\t" \
			/*
			 *	cli; ��ֹ��������Ӧ�ⲿӲ���жϣ��Ա�֤����Ĳ�����ԭ�ӵģ��ж���Ӧ����
			 * ret_from_sys_call ���� sti ���������������ִ�� iret ָ��ʱ�� EFLAGS �Ĵ������ָ���
			 */
	UNBLK_##chip(mask) \
			/*
			 *	UNBLK_FIRST(mask) �� UNBLK_SECOND(mask): ʹ�ܸ��жϵ��ж�����ʹ�жϿ�����
			 * ���������������жϵ��ж������źš�
			 */
	"decl _intr_count\n\t" \
	"jmp ret_from_sys_call\n" \
			/*
			 *	intr_count--;
			 *	�����ת�� ret_from_sys_call ��ȥ����ǰ������������ current ���źŲ�����
			 * �˳��жϣ�����ͨ�ж����е�β���ᴦ��ǰ��������������źš�
			 */

/*
 *	fast_IRQ0_interrupt: 0 ���ж϶�Ӧ�Ŀ����жϴ�������[ nr = 0 -> 15 ]
 */
"\n.align 4\n" \
"_fast_IRQ" #nr "_interrupt:\n\t" \	/* void fast_IRQ0_interrupt(void) ����ʵ�� */

	SAVE_MOST \
	ACK_##chip(mask) \
			/*
			 *	�����Ҫ�ļĴ�����
			 *	����ж϶�Ӧ���жϿ���������Ӧ��
			 */
	"incl _intr_count\n\t" \
	"pushl $" #nr "\n\t" \
			/*
			 *	intr_count++;
			 *	�жϺ� nr ��Ϊ do_fast_IRQ ��Ψһ������ջ��
			 */
	"call _do_fast_IRQ\n\t" \
	"addl $4,%esp\n\t" \
			/*
			 *	call do_fast_IRQ(nr): ����ͨ�õĿ����жϴ����� do_fast_IRQ���� do_fast_IRQ
			 * �л�����жϺ���ִ�о�����жϴ�������
			 *
			 *	�����жϴ�����ִ����Ϻ������ݸ� do_fast_IRQ �Ĳ�����ջָ��ص� esp0��
			 */
	"cli\n\t" \
			/*
			 *	cli; ��ֹ�ⲿӲ���жϣ��Ա�֤����Ĳ�����ԭ�ӵģ��ж�״̬�������ִ�� iret
			 * ʱ�� EFLAGS �Ĵ������ָ���
			 */
	UNBLK_##chip(mask) \
	"decl _intr_count\n\t" \
	RESTORE_MOST \
			/*
			 *	ʹ�ܸ��жϵ��ж�����
			 *	intr_count--;
			 *	�����жϵ�β���������źţ�ֱ�ӵ��� RESTORE_MOST �ָ��Ĵ������˳��жϡ�
			 */

/*
 *	bad_IRQ0_interrupt: 0 ���ж϶�Ӧ����Ч�жϴ�������[ nr = 0 -> 15 ]
 */
"\n\n.align 4\n" \
"_bad_IRQ" #nr "_interrupt:\n\t" \	/* void bad_IRQ0_interrupt(void) ����ʵ�� */
	SAVE_MOST \
	ACK_##chip(mask) \
	RESTORE_MOST);
			/*
			 *	���ĳһ���жϱ�ϵͳע��Ϊ��Ч�жϣ������ж϶�Ӧ���жϿ���оƬ��Ӧ���ֱ��
			 * �˳�����Ӧ��ʱ�Ὣ���жϵ��ж������ź����ε���ʹ�жϿ������Ժ��ٷ������жϵ�
			 * �����źš�
			 */

#endif
