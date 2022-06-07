/*
 * Copyright (c) 2020 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * OS-Lab-2020 (i.e., ChCore) is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *   http://license.coscl.org.cn/MulanPSL
 *   THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 *   PURPOSE.
 *   See the Mulan PSL v1 for more details.
 */

#include <common/types.h>
#include <process/thread.h>
#include <mm/vmspace.h>
#include <common/types.h>
#include <common/errno.h>

#include <common/kprint.h>
#include <common/util.h>
#include <common/macro.h>
#include <common/mm.h>
#include <common/kmalloc.h>

#include "esr.h"

static inline vaddr_t get_fault_addr()
{
	vaddr_t addr;
	asm volatile ("mrs %0, far_el1\n\t":"=r" (addr));
	return addr;
}

int handle_trans_fault(struct vmspace *vmspace, vaddr_t fault_addr);


// 预处理跳转
void do_page_fault(u64 esr, u64 fault_ins_addr)
{
	vaddr_t fault_addr;
	int fsc;		// fault status code

	fault_addr = get_fault_addr();
	fsc = GET_ESR_EL1_FSC(esr);
	switch (fsc) {
	case DFSC_TRANS_FAULT_L0:
	case DFSC_TRANS_FAULT_L1:
	case DFSC_TRANS_FAULT_L2:
	case DFSC_TRANS_FAULT_L3:{
			int ret;

			ret =
			    handle_trans_fault(current_thread->vmspace,
					       fault_addr);
			if (ret != 0) {
				kinfo("pgfault at 0x%p failed\n", fault_addr);
				sys_exit(ret);
			}
			break;
		}
	default:
		kinfo("do_page_fault: fsc is unsupported (0x%b) now\n", fsc);
		BUG_ON(1);
		break;
	}
}


// 检查PMO的合法性
// 分配一个页
// 把页映射到发生缺页异常的地址处
int handle_trans_fault(struct vmspace *vmspace, vaddr_t fault_addr)
{
	struct vmregion *vmr;
	struct pmobject *pmo;
	paddr_t pa;
	u64 offset;

	/*
	 * Lab3: your code here
	 * In this function, you should:
	 * 1. Get the vmregion of the fault_addr using find_vmr_for_va
	 * 2. If the pmo is not of type PMO_ANONYM, return -ENOMAPPING
	 * 3. Allocate one physical memory page for the PMO
	 * 4. Map the allocated address back to the page table
	 *
	 * NOTE: when any problem happened in this function, return
	 * -ENOMAPPING
	 * 
	 * NOTE: the real physical address of the PMO may not be
	 * continuous. In real chcore, all the physical pages of a PMO
	 * are recorded in a radix tree for easy management. Such code
	 * has been omitted in our lab for simplification.
	 */

/* * Lab3：你的代码在这里
* 在这个函数中，你应该：
* 1. 使用find_vmr_for_va获取fault_addr的vmregion
* 2.如果pmo不是PMO_ANONYM类型，返回-ENOMAPPING
* 3.为PMO分配一个物理内存页
* 4. 将分配的地址映射回页表
*
* 注意：当这个函数发生任何问题时，返回
* - 映射
*
* 注意：PMO 的真实物理地址可能不是
* 连续的。在真正的 chcore 中，一个 PMO 的所有物理页面
* 记录在基数树中，便于管理。这样的代码
* 为简化起见，在我们的实验室中已省略。
 */
	vmr = find_vmr_for_va(vmspace,fault_addr);
	if(vmr == NULL)
	{
		kdebug("Could not found vmr for va\n");
		return -ENOMAPPING;
	}
	if(vmr->pmo->type != PMO_ANONYM)
	{
		kdebug("PMO type is not PMO_ANONYM!\n");
	} 
	void *page = get_pages(0);
	if(page == NULL)
	{
		kdebug("Could not get a new page\n");
		return -ENOMAPPING; 
	}
	// 取内核的虚拟地址来做物理地址的映射
	pa = (paddr_t)virt_to_phys(page);
	// 缺页异常代表fault_addr 所在的那个整页都不存在，所以向下取整得到页的起始地址
	offset = ROUND_DOWN(fault_addr,PAGE_SIZE);
	int ret = map_range_in_pgtbl(vmspace->pgtbl,offset,pa,PAGE_SIZE,vmr->perm);
	if(ret < 0)
	{
		free_pages(page);
		kdebug("Map range in pgtbl fault\n");
		return -ENOMAPPING;
	}
	kdebug("page fault success\n");
	return 0;
}
