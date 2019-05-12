#ifndef _LINUX_HEAD_H
#define _LINUX_HEAD_H

/*
 *	desc_struct: ���������е��������ṹ��ÿ��������ռ 8 ���ֽڡ�
 *	desc_table: ���������� 256 ����������ɡ�
 */
typedef struct desc_struct {
	unsigned long a,b;
} desc_table[256];

extern unsigned long swapper_pg_dir[1024];	/* �ں˿ռ��ҳĿ¼�� */
extern desc_table idt,gdt;	/* IDT ��� GDT ��λ�� head.S �е� _idt �� _gdt ���� */

#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_TMP 3

#define LDT_NUL 0
#define LDT_CODE 1
#define LDT_DATA 2

#endif
