#ifndef _LINUX_PAGE_H
#define _LINUX_PAGE_H

			/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT			12
#define PAGE_SIZE			((unsigned long)1<<PAGE_SHIFT)	/* ҳ���СΪ 4kB */

#ifdef __KERNEL__

			/* number of bits that fit into a memory pointer */
#define BITS_PER_PTR			(8*sizeof(unsigned long))
			/* to mask away the intra-page address bits */
#define PAGE_MASK			(~(PAGE_SIZE-1))
			/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)		(((addr)+PAGE_SIZE-1)&PAGE_MASK)
			/* to align the pointer to a pointer address */
#define PTR_MASK			(~(sizeof(void*)-1))

					/* sizeof(void*)==1<<SIZEOF_PTR_LOG2 */
					/* 64-bit machines, beware!  SRB. */
#define SIZEOF_PTR_LOG2			2

			/* to find an entry in a page-table-directory */
/*
 *	PAGE_DIR_OFFSET(base, address): base ��ҳĿ¼��������ڴ����ַ��address �Ǹ�ҳĿ¼����
 * ӳ���ĳһ�����Ե�ַ��
 *
 *	��������ڻ�ȡҳĿ¼���е�ĳһ��ҳĿ¼����ڴ��ַ�����ҳĿ¼���ӳ�����Ե�ַ address
 * ���ڵ�����ҳ�档
 *
 *	����һ�����������Ե�ַ����ҳĿ¼�������ҽ���һ��ҳĿ¼��(���ҽ���һ��ҳ��)����ӳ������Ե�ַ��
 */
#define PAGE_DIR_OFFSET(base,address)	((unsigned long*)((base)+\
  ((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)*2&PTR_MASK&~PAGE_MASK)))

			/* to find an entry in a page-table */
/*
 *	PAGE_PTR(address): address ��ĳһ�����Ե�ַ��
 *
 *	��������ڻ�ȡҳ���е�ĳһ��ҳ���������ҳ�����ַ��ƫ�Ƶ�ַ�����ƫ�Ƶ�ַ + ҳ�����ַ����
 * ���ҳ�����������ڴ��е�������ַ�����ҳ�����ӳ�����Ե�ַ address ���ڵ�����ҳ�档
 *
 *	����һ�����������Ե�ַ����Ψһ��ҳ�������ҽ���һ��ҳ����𽫸����Ե�ַӳ�䵽Ψһ��ҳ���С�
 */
#define PAGE_PTR(address)		\
  ((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)&PTR_MASK&~PAGE_MASK)

			/* the no. of pointers that fit on a page */
/*
 *	PTRS_PER_PAGE: һҳ�ڴ��п��Դ�ŵ�ҳ����ĸ�����
 */
#define PTRS_PER_PAGE			(PAGE_SIZE/sizeof(void*))

#endif /* __KERNEL__ */

#endif /* _LINUX_PAGE_H */
