/*
 *  linux/boot/head.S
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 *  head.S contains the 32-bit startup code.
 */

/*
 * 	head.S ����λ�� system ģ���ʼ��λ�ã�setup ���Ѿ��� system ģ�����
 * ���� 0x01000 ��ʼ��λ�á�
 * 	���������� IDTR �� GDTR ֮ǰ��IDT �� GDT ��ʹ�� setup �е����á�
 */

.text
.globl _idt,_gdt,
.globl _swapper_pg_dir,_pg0
.globl _empty_bad_page
.globl _empty_bad_page_table
.globl _empty_zero_page
.globl _tmp_floppy_area,_floppy_track_buffer

#include <linux/tasks.h>
#include <linux/segment.h>

#define CL_MAGIC_ADDR	0x90020
#define CL_MAGIC	0xA33F
#define CL_BASE_ADDR	0x90000
#define CL_OFFSET	0x90022

/*
 * swapper_pg_dir is the main page directory, address 0x00001000 (or at
 * address 0x00101000 for a compressed boot).
 */
startup_32:			/* head.S ����ʼִ�еĵط� */
	cld
	movl $(KERNEL_DS),%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
	lss _stack_start,%esp
				/*
				 * 	���ϴ���: �������ݶμĴ�����ջ��λ�á�
				 * 	1. DS = ES = FS = GS = �ں����ݶ�ѡ��� 0x18������ѡ�� GDT ���е�
				 * �ں����ݶ���������
				 * 	2. lss ָ����������ϵͳջָ��: [ SS:ESP = user_stack �����β�� ]
				 */
/*
 * Clear BSS first so that there are no surprises...
 */
	xorl %eax,%eax
	movl $__edata,%edi
	movl $__end,%ecx
	subl %edi,%ecx
	cld
	rep
	stosb
				/*
				 * 	���ϴ���: �� BSS ����0��
				 */
/*
 * start system 32-bit setup. We need to re-do some of the things done
 * in 16-bit mode for the "real" operations.
 */
	call setup_idt
				/*
				 * 	���� IDT ������� IDT ����ϵͳ����ʹ�õ� IDT �����˴���δ���� IDTR��
				 */
	xorl %eax,%eax
1:	incl %eax		# check that A20 really IS enabled
	movl %eax,0x000000	# loop forever if it isn't
	cmpl %eax,0x100000
	je 1b
				/*
				 * 	���ϴ���: ��� A20 ��ַ���Ƿ����ɹ�����δ�ɹ��������������޷�
				 * ���� 1MB ���ϵ��ڴ�ռ䣬�����ڴ˴���ѭ����
				 */
/*
 * Initialize eflags.  Some BIOS's leave bits like NT set.  This would
 * confuse the debugger if this code is traced.
 * XXX - best to initialize before switching to protected mode.
 */
	pushl $0
	popfl
				/*
				 * 	�������д���: ����־�Ĵ��� EFLAGS �еı�־λȫ��Ϊ 0��pushfl ָ��
				 * ���ڽ� EFLAGS ��ջ��popfl ָ�����ڽ�ջ�е�ֵ�����ָ��� EFLAGS �Ĵ����С�
				 * �˴�ʹ�õ�ջ�Ǳ��ļ������ʼ���õ� stack_start��
				 */
/*
 * Copy bootup parameters out of the way. First 2kB of
 * _empty_zero_page is for boot parameters, second 2kB
 * is for the command line.
 */
	movl $0x90000,%esi
	movl $_empty_zero_page,%edi
	movl $512,%ecx
	cld
	rep
	movsl
	xorl %eax,%eax
	movl $512,%ecx
	rep
	stosl
	cmpw $(CL_MAGIC),CL_MAGIC_ADDR
	jne 1f
	movl $_empty_zero_page+2048,%edi
	movzwl CL_OFFSET,%esi	//movzwl: ��0��չ��2�ֽڸ��Ƶ�4�ֽڡ�ESI = CL_OFFSET & 0x0000FFFF = 0x00000022��
	addl $(CL_BASE_ADDR),%esi
	movl $2048,%ecx
	rep
	movsb
				/*
				 * 	���ϴ���: ϵͳΪ����������������Ԥ���� 4kB �Ŀռ䣬����������������
				 * �ֱ�ռ��ǰ�� 2kB��Ŀǰ 4kB �Ŀռ�λ�������ڴ�� 0x5000(20kB) ��ʼ�ĵ�ַ����
				 *
				 * 	1. �� 0x90000 ��ʼ�� 2kB �������������Ƶ� 0x5000 - 0x5800 �С�ʵ���ϣ�
				 * Ŀǰֻ�� 512 �ֽڵ���������������� 1.5kB ʵ������ setup �еĴ��룬����
				 * ��Ч������
				 * 	2. ��������ռ�õ� 2kB(0x5800 - 0x6000) �ռ��� 0��
				 *	3. ȷ�� 0x90020 ��ַ����ֵ�Ƿ���� 0xA33F������������ʾû��������
				 * �����������̽������������˵���������в�������Ҫ���������в�����
				 * 	4. �� CL_BASE_ADDR + (CL_OFFSET & 0x0000FFFF) = 0x90022 ��ʼ�ĵ�ַ��
				 * ���� 2kB �������в����� 0x5800 ��ʼ�Ŀռ��С�
				 *
				 * 	������ !!! ���� ???��
				 */
1:
/* check if it is 486 or 386. */
/*
 * XXX - this does a lot of unnecessary setup.  Alignment checks don't
 * apply at our cpl of 0 and the stack ought to be aligned already, and
 * we don't need to preserve eflags.
 */
	movl %esp,%edi		# save stack pointer
	andl $0xfffffffc,%esp	# align stack to avoid AC fault
	movl $3,_x86
	pushfl			# push EFLAGS
	popl %eax		# get EFLAGS
	movl %eax,%ecx		# save original EFLAGS
	xorl $0x40000,%eax	# flip AC bit in EFLAGS
	pushl %eax		# copy to EFLAGS
	popfl			# set EFLAGS
	pushfl			# get new EFLAGS
	popl %eax		# put it in eax
	xorl %ecx,%eax		# change in flags
	andl $0x40000,%eax	# check if AC bit changed
	je is386
	movl $4,_x86
	movl %ecx,%eax
	xorl $0x200000,%eax	# check ID flag
	pushl %eax
	popfl			# if we are on a straight 486DX, SX, or
	pushfl			# 487SX we can't change it
	popl %eax
	xorl %ecx,%eax
	andl $0x200000,%eax
	je is486
isnew:	pushl %ecx		# restore original EFLAGS
	popfl
	movl $1, %eax		# Use the CPUID instruction to 
	.byte 0x0f, 0xa2	# check the processor type
	andl $0xf00, %eax	# Set _x86 with the family
	shrl $8, %eax		# returned.	
	movl %eax, _x86
	movl %edi,%esp		# restore esp
	movl %cr0,%eax		# 486+
	andl $0x80000011,%eax	# Save PG,PE,ET
	orl $0x50022,%eax	# set AM, WP, NE and MP
	jmp 2f
is486:	pushl %ecx		# restore original EFLAGS
	popfl
	movl %edi,%esp		# restore esp
	movl %cr0,%eax		# 486
	andl $0x80000011,%eax	# Save PG,PE,ET
	orl $0x50022,%eax	# set AM, WP, NE and MP
	jmp 2f
is386:	pushl %ecx		# restore original EFLAGS
	popfl
	movl %edi,%esp		# restore esp
	movl %cr0,%eax		# 386
	andl $0x80000011,%eax	# Save PG,PE,ET
	orl $2,%eax		# set MP
				/*
				 * 	���ϴ���: ��� CPU �ͺţ������ͺű����� schedule.c �ļ��� x86 �����У�
				 * x86 ������ȡֵ������ 3��4��5 �ȵȣ��ֱ���� 386��486��586 �ȡ�
				 *
				 * 	�����ͺź󽫸��ͺ�����Ҫ���ø� CR0 ��ֵ��ŵ� eax �Ĵ����С�CR0 - CR3
				 * �ǿ��ƼĴ����飬���� CR0 �к��д���������ģʽ��״̬��ϵͳ���Ʊ�־����ͬ CPU
				 * �ͺŵ� CR0 �еı�־�ǲ�һ���ģ���˴˴�����Ҫ��� x86 �ľ����ͺš�
				 *
				 * 	�������������͵� CPU�������� eax �е�ֵ���� 0(PE�����Ʒֶ�) λ�͵�
				 * 31(PG�����Ʒ�ҳ) λ��ֵ������ԭ����ֵ��PE λ�Ѿ��� setup ������λ�������ˣ�
				 * �ֶλ����Ѿ������ˣ�PG λ֮ǰû�б����ù���������Ȼ�� 0����ҳ������δ������
				 */
2:	movl %eax,%cr0
				/*
				 * 	���д����� eax �е�ֵ���� CR0 �Ĵ��������ú������Թ�����ֻ�����˷ֶ�
				 * ���Ƶı���ģʽ�¡�
				 */
	call check_x87
				/*
				 * 	check_x87: �����ѧЭ������ 80287/80387 �Ƿ���ڲ�������ر�־��
				 */
	call setup_paging
				/*
				 * 	setup_paging: ����ҳĿ¼����ҳ����������ҳ���ƣ��Ӵ˴������㹤����
				 * ������ҳ���Ƶı���ģʽ�¡��Ժ��������������Ե�ַ�������� MMU �ĵ�ַת��
				 * ���ܷ��������ڴ��ַ��
				 */
	lgdt gdt_descr
	lidt idt_descr
				/*
				 * 	��ҳ�����Ѿ�������������ҳ��ӳ���Ѿ���ɣ������¼��� GDTR �� IDTR �ˡ�
				 */
	ljmp $(KERNEL_CS),$1f
				/*
				 * 	ͨ����תָ����ں˴���ε�ѡ�������ѡ���ں˴���ε�����������Ϊ
				 * �������е����Ի���ַ�Ѿ��ĳ��� 0xC0000000�����ԴӴ�֮�󣬴������������ں�
				 * �����Ի���ַ���ǻ��� 0xC0000000 �ġ��ڴ�֮ǰ�������Ի���ַ���ǻ��� 0 �ġ�
				 */
1:	movl $(KERNEL_DS),%eax	# reload all the segment registers
	mov %ax,%ds		# after changing gdt.
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
				/*
				 * 	�޸��� GDT����Ҫ����װ�����еĶμĴ�����DS = ES = FS = GS = 0x18��
				 */
	lss _stack_start,%esp
				/*
				 * 	�������� SS:ESP = user_stack �����β����ջָ��ص�ջ�ײ���ջΪ�ա�
				 */
	xorl %eax,%eax
	lldt %ax
				/*
				 * 	���þֲ����������Ĵ��� LDTR = 0��LDTR Ŀǰ������Ч״̬��
				 */
	pushl %eax		# These are the parameters to main :-)
	pushl %eax
	pushl %eax
				/*
				 * 	��ջ��ѹ�� 3 ��ֵ������ 0��������ֵ����Ϊ�������� start_kernel �Ĳ�����
				 * start_kernel ������δ�õ����������������Դ˴�ֱ���� 0�������������ϵ�������
				 * ��ʾ  envp  argv  argc ��
				 */
	cld			# gcc2 wants the direction flag cleared at all times
	call _start_kernel
				/*
				 *	���� head.S ������λ�ã���ת�� main.c �ļ��е� start_kernel ����������
				 * ִ�к������롣ȫ���жϻ����ڽ�ֹ״̬��
				 */
L6:
	jmp L6			# main should never return here, but
				# just in case, we know what happens.

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
	movl $0,_hard_math
	clts
	fninit
	fstsw %ax
	cmpb $0,%al
	je 1f
	movl %cr0,%eax		/* no coprocessor: have to set bits */
	xorl $4,%eax		/* set EM */
	movl %eax,%cr0
	ret
.align 2
1:	movl $1,_hard_math
	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */
	ret
				/*
				 * 	���ϴ���: �����ѧЭ������ 80287/80387 �Ƿ���ڣ������ڣ������ö�����
				 * sched.c �еı��� hard_math = 1 ��ʾ��ѧЭ���������ڡ��������������� CR0 ��
				 * �� EM = 1����ʾ������û���ڲ����ⲿЭ��������
				 */


				/*
				 * �ж��������� IDT �е��ж�����������ʽ:
				 *
				 * | 31                   16 | 15 | 14 13 | 12      8 | 7 6 5 | 4     0 |
				 * ----------------------------------------------------------------------
				 * | ������ڵ�ƫ��(31 - 16) | P  |  DPL  | 0 1 1 1 0 | 0 0 0 | 0 0 0 0 |
				 * ----------------------------------------------------------------------
				 * |       ��ѡ���          |           ������ڵ�ƫ��(15 - 0)         |
				 * ----------------------------------------------------------------------
				 */
/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It doesn't actually load
 *  idt - that can be done only after paging has been enabled
 *  and the kernel moved to 0xC0000000. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok.
 */
setup_idt:
	lea ignore_int,%edx
	movl $(KERNEL_CS << 16),%eax
	movw %dx,%ax		/* selector = 0x0010 = cs */
	movw $0x8E00,%dx	/* interrupt gate - dpl=0, present */

	lea _idt,%edi
	mov $256,%ecx
rp_sidt:
	movl %eax,(%edi)
	movl %edx,4(%edi)
	addl $8,%edi
	dec %ecx
	jne rp_sidt
	ret
				/*
				 * 	���ϴ���: �����ж��������� IDT��IDT �� 256 ���ж�����������ɣ�ÿ��
				 * ����ռ 8 ���ֽڣ�IDT ����ռ�� 2kB ���ڴ�ռ䣬Ĭ�ϵ� 256 ��������ͬ��ָ��
				 * ͬһ���ж��� ignore_int���������ж����ں���Ĵ����а�װ��IDT ����λ���ڱ�
				 * �ļ��� _idt ��������ʾ��λ�ô���
				 *
				 * 	���ú���ж�����������:
				 * | 31                   16 | 15 | 14 13 | 12      8 | 7 6 5 | 4     0 |
				 * ----------------------------------------------------------------------
				 * |  ignore_int �����ĵ�ַ  | 1  |  0 0  | 0 1 1 1 0 | 0 0 0 | 0 0 0 0 |
				 * ----------------------------------------------------------------------
				 * |   0x10 ѡ���ں˴����   |                   0x0000                 |
				 * ----------------------------------------------------------------------
				 *
				 * 	����: P = 1 ��ʾ�ж��Ŵ��ڡ�DPL = 0 ��ʾ�ж�Ҫִ�еĴ�������λ�� 0
				 * ��Ȩ���Ĵ����С���ˣ�Ĭ������£����жϲ���ʱ������ִ���ں˴�����е�
				 * ignore_int �������ں��氲װ�������ж���ʱֻ�轫 ignore_int �滻��������
				 * �жϴ����������ɡ���װ�����ţ�����������ʱ����Ҫ���������������͡�
				 */
				/*
				 * 	Ŀǰֻ�������˷ֶλ��ƣ���δ���÷�ҳ���ƣ�����Ŀǰ�ں˴�������Ե�ַ
				 * �ռ���������ַ�ռ�һ�¡������÷�ҳ���ƺ��ں˻ᱻ�ƶ��� 4GB ���Ե�ַ�ռ�
				 * �� 0xC0000000(3GB) ��ַ������Ϊ�ж���������ǰ�����Ե�ַ�����÷�ҳ����֮��
				 * ֮������Ե�ַ��һ�£����Դ˴��������� IDTR�������ȿ�����ҳ�����ں��ƶ���
				 * 0xC0000000 ��ַ������ȷ���� IDT �����Ե�ַ�ռ�󣬲����� IDTR��
				 */


				/*
				 * 	ҳĿ¼���е�ҳĿ¼���ҳ���е�ҳ���������ͬ�ĸ�ʽ:
				 *
				 * | 31                    12 | 11 10 9 | 8 7 | 6 | 5 | 4 3 |  2  |  1  | 0 |
				 * --------------------------------------------------------------------------
				 * |    �����ڴ�ҳ�����ַ    |   AVL   | 0 0 | D | A | 0 0 | U/S | R/W | P |
				 * --------------------------------------------------------------------------
				 *
				 *	�������ڴ�ҳ�����ַ�� 20 λ���������е��ڴ�ҳ�涼������ 4kB �ı߽���롣
				 *
				 * 	P: ���ڱ�־�����ڱ���ҳĿ¼��ָ���ҳ����ҳ����ָ��������ڴ�ҳ���Ƿ�
				 * ���ڡ�P = 1 ��ʾҳ����ڡ����Ե�ַ��������ַ��ת�������У�ҳ�治���ڽ���
				 * ����ȱҳ�쳣��
				 * 	R/W: ��д��־������ 1 ��ʾҳ��ɱ�����д��ִ�У����� 0 ��ʾҳ��ֻ����
				 * ��ִ�У���һ������д��ҳ��ִ��д����ʱ�����ҳд�����쳣��
				 *	U/S: ���� 1 ��ʾ��ҳ�����û���ҳ�棬����0 ��ʾ��ҳ���ǳ����û���ҳ�档
				 *	A: �ѷ��ʱ�־�����������ʸ�ҳ��ʱ���ñ�־�ᱻ�� 1��
				 *	D: ҳ�����޸�(Dirty)��־����һ��ҳ��ִ��д����ʱ�����ø�λ����������
				 * �����޸�ҳĿ¼���е� D ��־��
				 */
				/*
				 * 	������ҳ����֮��ĵ�ַ�任����:
				 *
				 * 	������������ʼ�������Ե�ַ��δ������ҳʱ�����Ե�ַ��������ַ��ͬ������
				 * ת����������ҳ�����Ե�ַҪ���� MMU ��ת������֪�����������յ�������ַ��MMU
				 * ʹ�ò���ϵͳ�������ڴ��е�ҳĿ¼����ҳ��������Ե�ַ��������ַ֮���ת����
				 *
				 * 	1. 32 λ�����Ե�ַ���ָ�Ϊ 3 ���֣������ 10bit���м� 10bit�������
				 * 12bit �ָҳĿ¼����������ַ������ CR3 �С�
				 *
				 * 	2. ��� 10bit ����ѡ��ҳĿ¼�� 1024 ��ҳĿ¼���е�����һ��ҳĿ¼���
				 * ���ҳĿ¼������ȡ��ҳĿ¼����ָ���ҳ���������ڴ����ַ��
				 *
				 * 	3. ��ȡ��ҳ�������м� 10bit ѡ��ҳ�� 1024 ��ҳ�����е�����һ��ҳ���
				 * �����ҳ��������ȡ��ҳ������ָ������������ڴ�ҳ��Ļ���ַ��
				 *
				 * 	4. ��ȡ�������ڴ�ҳ��֮������� 12bit ��Ϊҳ��ƫ�ƣ��ӵ������ڴ�ҳ��
				 * ����ַ���γ����յ������ڴ��ַ��
				 *
				 *	��ִ���� setup_paging ֮���״̬Ϊ׼���Դ������������Ե�ַ 0xC03FF408
				 * Ϊ����������ַת���Ĺ���:
				 *
				 * 	1. MMU��ҳĿ¼������ַ�Ĵ��� CR3 ��ȡ��ҳĿ¼������ַ��
				 * CR3 = 0x00001000(swapper_pg_dir)�����Ե�ַ���ָ�Ϊ 3 ���֣��ֱ������ 10bit
				 * Ϊ 0x300(0b1100000000)���м� 10bit Ϊ 0x3FF(0b1111111111)����� 12bit Ϊ 0x408��
				 *
				 * 	2. ��� 10bit 0x300ѡ��ҳĿ¼���еĵ� 768 ��ҳĿ¼������ҳĿ¼���ȡ
				 * ��ҳĿ¼����ָ���ҳ���������ڴ�ҳ�����ַΪ 0x00002000(pg0)��
				 *
				 * 	3. ��ȡ��ҳ�������м� 10bit 0x3FF ѡ��ҳ���еĵ� 1023 ��ҳ��������
				 * ҳ�����ȡ��ҳ������ָ������������ڴ�ҳ��Ļ���ַ 0x003ff000��
				 *
				 * 	4. �����ڴ�ҳ�����ַ + ���Ե�ַ��� 12bit ��ҳ��ƫ�ƣ��õ����յ������ڴ�
				 * ��ַ�����Ե�ַ 0xC03FF408 ===> ������ַ 0x003FF408��
				 */
/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 4MB.  The rest are initialized later.
 *
 * (ref: added support for up to 32mb, 17Apr92)  -- Rik Faith
 * (ref: update, 25Sept92)  -- croutons@crunchy.uucp 
 * (ref: 92.10.11 - Linus Torvalds. Corrected 16M limit - no upper memory limit)
 */
.align 2
setup_paging:
	movl $1024*2,%ecx		/* 2 pages - swapper_pg_dir+1 page table */
	xorl %eax,%eax
	movl $_swapper_pg_dir,%edi	/* swapper_pg_dir is at 0x1000 */
	cld;rep;stosl
				/*
				 * 	����ҳ�ڴ�ռ�(swapper_pg_dir �� pg0���� 8kB)�� 0��
				 */
/* Identity-map the kernel in low 4MB memory for ease of transition */
	movl $_pg0+7,_swapper_pg_dir		/* set present bit/user r/w */
/* But the real place is at 0xC0000000 */
	movl $_pg0+7,_swapper_pg_dir+3072	/* set present bit/user r/w */
				/*
				 * 	1. ����ҳĿ¼���еĵ� 0 ��ҳĿ¼���ҳĿ¼����ָ���ҳ��Ϊ pg0��
				 * ��ҳĿ¼��ӳ������Ե�ַ�ռ�Ϊ 0 - 4MB��
				 *
				 * 	2. ����ҳĿ¼���еĵ� 768 ��ҳĿ¼���ҳĿ¼��ָ���ҳ��Ϊ pg0��
				 * ��ҳĿ¼��ӳ������Ե�ַ�ռ�Ϊ 0xC0000000(3GB) ��ʼ�� 4MB �ռ䡣
				 *
				 * 	���Ե�ַ�ռ��е� 0 - 4MB �� 0xC0000000 ��ʼ�� 4MB ����Ӧ������
				 * ҳĿ¼������ݶ�Ϊ 0x00002007����ָ��ͬһ��ҳ�� pg0��Ҳ����ζ��������
				 * 4MB �����Ե�ַ�ռ佫ӳ�䵽��ͬ��������ַ�ռ��ϡ���������� pg0 �е�
				 * ҳ������ս����� 4MB �����Ե�ַ�ռ�ӳ�䵽������ַ�ռ�� 0 - 4MB �ϡ�
				 *
				 * 	��������ԭ��:
				 *
				 * 	1. ������ҳĿ¼����ҳ��֮��Ὺ����ҳ����ҳ����֮������������
				 * ���е����Ե�ַ����Ҫ���� MMU ��ת�����ܷ��������ڴ档
				 *
				 * 	2. ������ҳʱ��GDTR �е�ֵ��δ�������ã����� setup �����õ��Ǹ�ֵ��
				 * ����ں˴���ε����Ի���ַ���� 0�����������������Ե�ַ���ǻ��� 0 ������
				 * ��ַ��������Ҫ�Ƚ������Ե�ַ 0 ��ʼ��һ�οռ��ӳ�䣬�˴�ӳ�� 4MB ��
				 * �ռ���ѣ���ȻҲ����ӳ�����Ŀռ䣬��ûʲô�ô���
				 *
				 * 	3. ��������¼��� GDTR�����غ��ں˴���ε����Ի���ַ��Ϊ 0xC0000000��
				 * ��ʱ���������������Ե�ַ�ǻ��� 0xC0000000 ��ʼ�����Ե�ַ��������Ҫ��
				 * �������Ե�ַ 0xC0000000 ��ʼ��һ�οռ��ӳ�䡣ͬ���� 4MB��
				 *
				 * 	4. ���ܴ��������������Ե�ַ�ǻ��� 0 �Ļ��ǻ��� 0xC0000000 �ģ�������
				 * �Ĵ�������ݶ���һ���ģ������Ҫ������ӳ�䵽��ͬ�������ڴ�ռ��ϡ�����Щ
				 * ����������Ѿ������������ڴ�� 0 - 4MB �Ŀռ��ϣ����Խ����� 4MB �����Ե�ַ
				 * ֱ��ӳ�䵽�˴���
				 *
				 * 	�˴�ֻ�������� 4MB ��ӳ�䣬��������Ե�ַ�ռ��ں���ӳ�䣬������ط�
				 * ��ʼ����������ӳ�����������Ե�ַ�ռ䣬4MB �Ŀռ��㹻�ˣ��˴�û�б�Ҫӳ��
				 * ��������Ե�ַ�ռ䡣
				 */
	movl $_pg0+4092,%edi
	movl $0x03ff007,%eax		/*  4Mb - 4096 + 7 (r/w user,p) */
	std
1:	stosl			/* fill the page backwards - more efficient :-) */
	subl $0x1000,%eax
	jge 1b
				/*
				 * 	����ҳ�� pg0 �е� 1024 ��ҳ����: �����һ��ҳ���ʼ��ǰ���ã�
				 * ÿ��ҳ�����е������ǵ�ǰ����ӳ�������ҳ�����ַ + ��ҳ��ı�־ 7��
				 *
				 * 	ҳ�� pg0 ӳ�������ڴ�� 0 - 4MB�����һ��ҳ�����Ӧ�������ڴ�ҳ��
				 * ����ַ�� 0x3ff000����ҳ���־�����һ����� 0x3ff007����һ��ҳ�����Ӧ
				 * �������ڴ�ҳ�����ַ�� 0x000000����ҳ���־�����һ����� 0x000007��
				 */
	cld
	movl $_swapper_pg_dir,%eax
	movl %eax,%cr3			/* cr3 - page directory start */
				/*
				 * 	CR3 ��ҳĿ¼����ַ�Ĵ��� PDBR�����б���ҳĿ¼�������ڴ����ַ��
				 * �� swapper_pg_dir �������ʾ�ĵ�ַ�ĸ� 20 λװ�� CR3 �У�CR3 �� 12
				 * λ������
				 *
				 * 	���غ� CR3 = 0x00001000��
				 */
	movl %cr0,%eax
	orl $0x80000000,%eax
	movl %eax,%cr0		/* set paging (PG) bit */
	ret			/* this also flushes the prefetch-queue */
				/*
				 * 	���� CR0 �� PG λ��������ҳ���ƣ��� PE һ�������� PG �������һ��
				 * ��תָ����ˢ���Ѿ�Ԥȡ��ָ�����У�����������ָ�� ret ͬ������ˢ�µ�ǰ
				 * ָ�����У���˲�������תָ��� ret ָ�������ˢ�¹�����
				 */

/*
 * page 0 is made non-existent, so that kernel NULL pointer references get
 * caught. Thus the swapper page directory has been moved to 0x1000
 *
 * XXX Actually, the swapper page directory is at 0x1000 plus 1 megabyte,
 * with the introduction of the compressed boot code.  Theoretically,
 * the original design of overlaying the startup code with the swapper
 * page directory is still possible --- it would reduce the size of the kernel
 * by 2-3k.  This would be a good thing to do at some point.....
 */
/*
 * 	�������ע�Ϳ�֪:
 *
 * 	1. ҳ 0 �����ڣ�Ҳ���������ڴ� 0 ��ʼ�� 4kB �ռ�δʹ�ã���Ҫ��Ϊ�˲����ں�
 * ��ָ�����ã���� setup �ڰ��� system ��ʱ�򣬲�δ�� system ���Ƶ� 0 ��ַ��������
 * ���Ƶ��� 0x1000 ��ʼ�ĵط���
 *
 * 	2. ҳ����Ŀ¼�������� 0x1000 �������λ��Ҳ�� head ���뿪ʼ��λ�ã�������
 * ����ҳĿ¼����ʱ��Ḳ�ǵ��ⲿ�ִ��롣��Ϊ��ʼ��ʱֻ������ҳĿ¼���ĵ� 0 ��͵�
 * 768 ����Զ� head �Ĵ���ִ�в��������ʲôӰ�졣
 *
 * 	3. ���������ѹ���������뼼��������ҳĿ¼����������� 0x00101000 ����
 */
.org 0x1000
_swapper_pg_dir:
				/*
				 * 	swapper_pg_dir: �����ڴ��ַ 0x1000����ҳĿ¼����һ��ҳĿ¼���� 1024 ��
				 * ҳĿ¼�ÿ��ҳĿ¼���ӳ�� 4MB �����Ե�ַ�ռ䣬��һ��ҳĿ¼����ӳ�� 4GB ��
				 * ���Ե�ַ�ռ䡣��ʼ��ʱֻӳ�����Ե�ַ�ռ�� 0 - 4MB �� 0xC0000000 ��ʼ�� 4MB��
				 *
				 * 	��ʼ����ҳĿ¼���е�����:
				 *
				 * 	���	ҳĿ¼������	ָ���ҳ����ַ	  ҳĿ¼��ָ���ҳ������ҳ�������
				 *	 0	0x00002007	0x00002000 (pg0)	7 (�û� | �ɶ�д | ����)
				 *	 1	0x00000000				0 (������)
				 *	 2	0x00000000				0 (������)
				 *	 .	.		.
				 *	768	0x00002007	0x00002000 (pg0)	7 (�û� | �ɶ�д | ����)
				 *	 .	.		.
				 *	1023	0x00000000				0 (������)
				 */

/*
 * The page tables are initialized to only 4MB here - the final page
 * tables are set up later depending on memory size.
 */
.org 0x2000
_pg0:
				/*
				 * 	pg0: �����ڴ��ַ 0x2000����ҳĿ¼���ĵ�һ��ҳ�������ڽ����Ե�ַ�ռ�
				 * �� 0 - 4MB �� 0xC0000000(3GB) ��ʼ�� 4MB ӳ�䵽������ַ�ռ�� 0 - 4MB��
				 *
				 * 	��ʼ����ҳ���е�����:
				 *
				 * 	���	ҳ��������	ָ��������ڴ��ַ	��ӳ��������ڴ�ҳ�������
				 * 	 0	0x00000007	0x00000000 (0)		7 (�û� | �ɶ�д | ����)
				 * 	 1	0x00001007	0x00001000 (4kB)	7
				 *	 2	0x00002007	0x00002000 (8kB)	7
				 * 	 .	    .			.		.
				 * 	 .	    .			.		.
				 * 	 .	    .			.		.
				 *	1023	0x003ff007	0x003ff000 (4MB - 4kB)	7
				 */

.org 0x3000
_empty_bad_page:
				/*
				 * 	_empty_bad_page: 4kB����δ֪��
				 */

.org 0x4000
_empty_bad_page_table:
				/*
				 * 	_empty_bad_page_table: 4kB����δ֪��
				 */

.org 0x5000
_empty_zero_page:
				/*
				 * 	_empty_zero_page: ������������������в������ֱ�ռǰ�� 2kB �ռ䣬�ⲿ
				 * ͨ������ empty_zero_page �������ռ䡣
				 */

.org 0x6000
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
_tmp_floppy_area:
	.fill 1024,1,0
				/*
				 * 	0x6000 ��ʼ�� 1kB �ڴ�Ԥ����������������ʹ�á��ⲿͨ������
				 * tmp_floppy_area ���������ռ䡣
				 */
/*
 * floppy_track_buffer is used to buffer one track of floppy data: it
 * has to be separate from the tmp_floppy area, as otherwise a single-
 * sector read/write can mess it up. It can contain one full track of
 * data (18*2*512 bytes).
 */
_floppy_track_buffer:
	.fill 512*2*18,1,0
				/*
				 * 	������ tmp_floppy_area ֮��� 18kB �ڴ�Ԥ������������ʹ�ã�����
				 * �洢һ�������ŵ������ݡ��ⲿͨ������ floppy_track_buffer ���������ռ䡣
				 */

/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown interrupt\n"
.align 2
ignore_int:
	cld
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $(KERNEL_DS),%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	pushl $int_msg
	call _printk
	popl %eax
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret
				/*
				 * 	ignore_int: ϵͳĬ�ϵ��жϴ���������ֻ��� "Unknown interrupt\n"
				 * ֮����˳�������ʲôҲ������
				 */

/*
 * The interrupt descriptor table has room for 256 idt's
 */
.align 4
.word 0
idt_descr:
	.word 256*8-1		# idt contains 256 entries
	.long 0xc0000000+_idt
				/*
				 * 	idt_descr: �˴��� 6 ���ֽڽ��� lidt ָ����ص��ж����������Ĵ���
				 * IDTR �У�ǰ�����ֽڱ�ʾ IDT ���ĳ��ȣ����ĸ��ֽڱ�ʾ IDT �������Ե�ַ
				 * �ռ��е����Ի���ַ��λ�� 0xC0000000(3GB) ��ʼ���ں˿ռ��С�
				 */

.align 4
_idt:
	.fill 256,8,0		# idt is uninitialized
				/*
				 * 	_idt: �ж�����������λ�ã���ռ�� 2kB ���ڴ�ռ䡣
				 */

.align 4
.word 0
gdt_descr:
	.word (8+2*NR_TASKS)*8-1
	.long 0xc0000000+_gdt
				/*
				 * 	gdt_descr: �˴��� 6 ���ֽڽ��� lgdt ָ����ص�ȫ�����������Ĵ���
				 * GDTR �У�ǰ�����ֽڱ�ʾ GDT ���ĳ��ȣ����ĸ��ֽڱ�ʾ GDT �������Ե�ַ
				 * �ռ��е����Ի���ַ��λ�� 0xC0000000(3GB) ��ʼ���ں˿ռ��С�
				 */

/*
 * This gdt setup gives the kernel a 1GB address space at virtual
 * address 0xC0000000 - space enough for expansion, I hope.
 */
.align 4
_gdt:
	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x0000000000000000	/* not used */
	.quad 0xc0c39a000000ffff	/* 0x10 kernel 1GB code at 0xC0000000 */
	.quad 0xc0c392000000ffff	/* 0x18 kernel 1GB data at 0xC0000000 */
	.quad 0x00cbfa000000ffff	/* 0x23 user   3GB code at 0x00000000 */
	.quad 0x00cbf2000000ffff	/* 0x2b user   3GB data at 0x00000000 */
	.quad 0x0000000000000000	/* not used */
	.quad 0x0000000000000000	/* not used */
	.fill 2*NR_TASKS,8,0		/* space for LDT's and TSS's etc */
				/*
				 * 	_gdt: GDT ����λ�ã��ɶ���������ɣ�ÿ����������ռ 8 �ֽڣ�������
				 * �����Ի���ַ����С����Ϣ���� 0 �� �� 1 ��������δʹ�á�
				 *
				 * 	�ڶ������������ں˴���ε����������ɶ�ѡ��� 0x10 ѡ�����������
				 * ����������Ϣָ��: �ں˴���ε����Ի���ַΪ 0xC0000000�����޳�Ϊ 1GB��
				 * ��һ����ִ�д���Σ��ö��Ѵ������ڴ��У��ε���Ȩ�ȼ�Ϊ 0���ɶ���ִ�С�
				 *
				 * 	���������������ں����ݶε����������ɶ�ѡ��� 0x18 ѡ�����������
				 * ����������Ϣָ��: �ں����ݶε����Ի���ַΪ 0xC0000000�����޳�Ϊ 1GB��
				 * ��һ�����ݶΣ��ö��Ѵ������ڴ��У��ε���Ȩ�ȼ�Ϊ 0���ɶ�д��
				 *
				 * 	���������������û�����ε����������ɶ�ѡ��� 0x23 ѡ�����������
				 * ����������Ϣָ��: �û����ݶε����Ի���ַΪ 0x00000000�����޳�Ϊ 3GB��
				 * ��һ����ִ�д���Σ��ö��Ѵ������ڴ��У��ε���Ȩ�ȼ�Ϊ 3���ɶ���ִ�С�
				 *
				 * 	���ĸ����������û����ݶε����������ɶ�ѡ��� 0x2B ѡ�����������
				 * ����������Ϣָ��: �û����ݶε����Ի���ַΪ 0x00000000�����޳�Ϊ 3GB��
				 * ��һ�����ݶΣ��ö��Ѵ������ڴ��У��ε���Ȩ�ȼ�Ϊ 3���ɶ�д��
				 *
				 * 	������������������Ҳδʹ�á�
				 *
				 * 	�ӵڰ˸���������ʼ������������ص���������������������� LDT �κ�
				 * TSS �Σ�ÿ������ռ��������������Ŀǰ��Щ���������� 0��
				 */