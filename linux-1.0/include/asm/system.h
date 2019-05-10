#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <linux/segment.h>

#define move_to_user_mode() \
__asm__ __volatile__ ("movl %%esp,%%eax\n\t" \
	"pushl %0\n\t" \
	"pushl %%eax\n\t" \
	"pushfl\n\t" \
	"pushl %1\n\t" \
	"pushl $1f\n\t" \
	"iret\n" \
	"1:\tmovl %0,%%eax\n\t" \
	"mov %%ax,%%ds\n\t" \
	"mov %%ax,%%es\n\t" \
	"mov %%ax,%%fs\n\t" \
	"mov %%ax,%%gs" \
	: /* no outputs */ :"i" (USER_DS), "i" (USER_CS):"ax")

#define sti() __asm__ __volatile__ ("sti": : :"memory")	/* �����ⲿӲ���ж� */
#define cli() __asm__ __volatile__ ("cli": : :"memory")	/* ��ֹ�ⲿӲ���жϣ������ܽ�ֹʹ�� INT ָ�����������ж� */
#define nop() __asm__ __volatile__ ("nop")		/* �ղ��� */

/*
 * Clear and set 'TS' bit respectively
 */
#define clts() __asm__ __volatile__ ("clts")
#define stts() \
__asm__ __volatile__ ( \
	"movl %%cr0,%%eax\n\t" \
	"orl $8,%%eax\n\t" \
	"movl %%eax,%%cr0" \
	: /* no outputs */ \
	: /* no inputs */ \
	:"ax")


extern inline int tas(char * m)
{
	char res;

	__asm__("xchgb %0,%1":"=q" (res),"=m" (*m):"0" (0x1));
	return res;
}

/* ����־�Ĵ��� EFLAGS �е�ֵ���浽 x �С� */
#define save_flags(x) \
__asm__ __volatile__("pushfl ; popl %0":"=r" (x): /* no input */ :"memory")

/* �� x �е�ֵ�ָ�����־�Ĵ��� EFLAGS �С� */
#define restore_flags(x) \
__asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"r" (x):"memory")

#define iret() __asm__ __volatile__ ("iret": : :"memory")

/*
 *	�ж��������� IDT �п��Դ�� 3 �����͵���������: �ж��š������š������ţ�
 * _set_gate ��ֻ���������ж��ź������š�������ŵĸ�ʽ���ж��ź����������ƣ���
 * �����Ҳ�������õ����ţ����ǵ����Ŵ���Ĳ��� gate_addr ��������
 */
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ __volatile__ ("movw %%dx,%%ax\n\t" \
	"movw %2,%%dx\n\t" \
	"movl %%eax,%0\n\t" \
	"movl %%edx,%1" \
	:"=m" (*((long *) (gate_addr))), \
	 "=m" (*(1+(long *) (gate_addr))) \
	:"i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	 "d" ((char *) (addr)),"a" (KERNEL_CS << 16) \
	:"ax","dx")

/*
 *	set_intr_gate: ���� IDT ���е��ж�������������Ҫ���������жϺ�Ϊ 0x20
 * ���ϵ������жϵĴ�������
 *
 * ���: n --- �жϺš�
 *	 addr --- �жϴ������ĵ�ַ��
 * ����: &idt[n] --- �жϺ� n ��Ӧ���ж��������ĵ�ַ��
 *	 14 --- ���������ͣ���ʾ�ж�����������
 *	 0 --- ����������Ȩ����
 *
 *	���ж������������ʱ���ж��ŵ� DPL �Ǳ����Եģ�������ֱ���л�Ϊ 0 ��Ȩ
 * ������ 0 ��Ȩ����ִ���жϴ��������������Ȩ��Ϊ 3 ���û��ռ�ͨ�� INT ָ��
 * �����������͵��жϣ����ж��ŵ� DPL = 0�������� INT ָ������жϻ�����Ȩ��
 * �Ͷ����ܴ����ж��ű���������
 */
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

/*
 *	set_trap_gate: ���� IDT ���е�����������������Ҫ���������жϺ�Ϊ 0x20
 * ���µ��� CPU �������쳣��
 *
 * ���: n --- �жϺš�
 *	 addr --- ���崦�����ĵ�ַ��
 * ����: &idt[n] --- �жϺ� n ��Ӧ���ж��������ĵ�ַ��
 *	 15 --- ���������ͣ���ʾ��������������
 *	 0 --- ����������Ȩ����
 *
 *	�� CPU �����쳣ʱ�������ŵ� DPL �Ǳ����Եģ�������ֱ���л�Ϊ 0 ��Ȩ����
 * �� 0 ��Ȩ����ִ���쳣���������������Ȩ��Ϊ 3 ���û��ռ�ͨ�� INT ָ���
 * �������͵��쳣���������ŵ� DPL = 0�������� INT ָ������쳣������Ȩ���Ͷ�
 * ���ܴ��������ű���������
 */
#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

/*
 *	set_system_gate: ���� IDT ���е�ϵͳ����������������Ҫ�������ÿ�����
 * ��Ȩ��Ϊ 3 ���û��ռ�ʹ�õ�������������쳣������Ҫ����ϵͳ���� INT 0x80��
 *
 * ���: n --- �жϺš�
 *	 addr --- ���崦�����ĵ�ַ��
 * ����: &idt[n] --- �жϺ� n ��Ӧ���ж��������ĵ�ַ��
 *	 15 --- ���������ͣ���ʾ��������������
 *	 3 --- ����������Ȩ����
 *
 *	��Ϊϵͳ���������û��ռ�ͨ�� INT 0x80 �����ģ�����ֻ�н������ŵ� DPL
 * ����Ϊ 3 ������ϵͳ����˳�����������ţ����������������л�����Ȩ�� 0 ȥִ��
 * ��Ӧ��ϵͳ���ù��̡�
 */
#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

/*
 *	set_call_gate: ����ϵͳ��������������
 *
 * ���: a --- �������������ĵ�ַ��
 *	 addr --- �����Ŵ������ĵ�ַ��
 * ����: 12 --- ���������ͣ���ʾ��������������
 *	 3 --- ����������Ȩ����
 */
#define set_call_gate(a,addr) \
	_set_gate(a,12,3,addr)

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*((gate_addr)+1) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*(gate_addr) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }

#define _set_tssldt_desc(n,addr,limit,type) \
__asm__ __volatile__ ("movw $" #limit ",%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	: /* no output */ \
	:"a" (addr+0xc0000000), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),235,"0x89")
#define set_ldt_desc(n,addr,size) \
	_set_tssldt_desc(((char *) (n)),((int)(addr)),((size << 3) - 1),"0x82")


#endif
