#include <common/util.h>
#include <common/macro.h>
#include <common/kprint.h>

#include "buddy.h"

/*
 * The layout of a phys_mem_pool:
 * | page_metadata are (an array of struct page) | alignment pad | usable memory |
 *
 * The usable memory: [pool_start_addr, pool_start_addr + pool_mem_size).
 */
void init_buddy(struct phys_mem_pool *pool, struct page *start_page,
		vaddr_t start_addr, u64 page_num)
{
	int order;
	int page_idx;
	struct page *page;

	/* Init the physical memory pool. */
	pool->pool_start_addr = start_addr;
	pool->page_metadata = start_page;
	pool->pool_mem_size = page_num * BUDDY_PAGE_SIZE;
	/* This field is for unit test only. */
	pool->pool_phys_page_num = page_num;

	/* Init the free lists */
	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		pool->free_lists[order].nr_free = 0;	                        // 空闲块的个数					
		init_list_head(&(pool->free_lists[order].free_list));
	}

	/* Clear the page_metadata area. */
	memset((char *)start_page, 0, page_num * sizeof(struct page));

	/* Init the page_metadata area. */
	for (page_idx = 0; page_idx < page_num; ++page_idx) {
		page = start_page + page_idx;     //分配页的标号
		page->allocated = 1;
		page->order = 0;                    //页的等级（大小）
	}

	//对每个页做回收操作
	/* Put each physical memory page into the free lists. */
	for (page_idx = 0; page_idx < page_num; ++page_idx) {
		page = start_page + page_idx;
		buddy_free_pages(pool, page);
	}
}
// 获得伙伴块
static struct page *get_buddy_chunk(struct phys_mem_pool *pool,
				    struct page *chunk)
{
	u64 chunk_addr;
	u64 buddy_chunk_addr;
	int order;

	/* Get the address of the chunk. */
	chunk_addr = (u64) page_to_virt(pool, chunk);
	order = chunk->order;
	/* 根据地址计算buddy chunk的地址
	* 好友之间的关系 */
	/*
	 * Calculate the address of the buddy chunk according to the address
	 * relationship between buddies.
	 */
#define BUDDY_PAGE_SIZE_ORDER (12)
	buddy_chunk_addr = chunk_addr ^
	    (1UL << (order + BUDDY_PAGE_SIZE_ORDER));

	/* Check whether the buddy_chunk_addr belongs to pool. */
	if ((buddy_chunk_addr < pool->pool_start_addr) ||
	    (buddy_chunk_addr >= (pool->pool_start_addr +
				  pool->pool_mem_size))) {
		return NULL;
	}

	return virt_to_page(pool, (void *)buddy_chunk_addr);
}

/*
 * split_page: split the memory block into two smaller sub-block, whose order
 * is half of the origin page.
 * pool @ physical memory structure reserved in the kernel
 * order @ order for origin page block
 * page @ splitted page
 * 
 * Hints: don't forget to substract the free page number for the corresponding free_list.
 * you can invoke split_page recursively until the given page can not be splitted into two
 * smaller sub-pages.
 */


/* split_page：将内存块拆分为两个较小的子块，其顺序
 * 是原始页面的一半。 
不要忘记为对应的free_list减去空闲页码。
 * 你可以递归调用 split_page 直到给定的页面不能被分成两个
 * 较小的子页面。
 */
static struct page *split_page(struct phys_mem_pool *pool, u64 order,
			       struct page *page)
{
	// <lab2>
	if(page ->allocated != 0)
	{
		// 在Kprint中定义
		kwarn("this have allocated\n");
		return 0;
	}
	else
	{
		page->allocated = 0;
		struct free_list *fl = &pool->free_lists[page->order];
		list_del(&page->node);
		--fl->nr_free;
		while(page->order > order)
		{
			page->order--;
			// 此时分为两个块，一个为伙伴块
			struct page * tmp = get_buddy_chunk(pool,page);
			if (tmp != NULL)
			{
				tmp->allocated = 0;
				tmp->order = page->order;
				struct free_list *rc  = &pool->free_lists[page->order];
				list_add(&page->node,&rc->free_list);
				++rc->nr_free;
			}

		}
	}

	return page;
	// </lab2>
}

/*
 * buddy_get_pages: get free page from buddy system.
 * pool @ physical memory structure reserved in the kernel
 * order @ get the (1<<order) continous pages from the buddy system
 * 
 * Hints: Find the corresonding free_list which can allocate 1<<order
 * continuous pages and don't forget to split the list node after allocation   
 */
// 从伙伴系统中获得所需要的块，顺序是从高向低找空闲的块来进行分配
struct page *buddy_get_pages(struct phys_mem_pool *pool, u64 order)
{
	// <lab2>
	int cur_order = order;
	while(cur_order < BUDDY_MAX_ORDER && pool->free_lists[cur_order].nr_free <= 0)
	{
		cur_order++;
	}

	if(cur_order >= BUDDY_MAX_ORDER)
	{
		kwarn("not have order chunk\n");
		return NULL;
	}
	// 将free_list 的类型转换为page类型中的free_list->node,目的是向split_page中传参数
	struct page *p = list_entry(pool->free_lists[cur_order].free_list.next,struct page,node);
	if(p == NULL)
	{
		return NULL;
	}
	split_page(pool, order,p);
	p->allocated = 1;
	return p;
	// </lab2>
}

/*
 * merge_page: merge the given page with the buddy page
 * pool @ physical memory structure reserved in the kernel
 * page @ merged page (attempted)
 * 
 * Hints: you can invoke the merge_page recursively until
 * there is not corresponding buddy page. get_buddy_chunk
 * is helpful in this function.
 */
static struct page *merge_page(struct phys_mem_pool *pool, struct page *page)
{
	// <lab2>
	if(page->allocated)
	{
		kwarn("is allocated\n");
		return NULL;
	}
	struct free_list *fl = &pool->free_lists[page->order];
	list_del(&(page->node));
	--fl->nr_free;
	while(page->order < BUDDY_MAX_ORDER -1)
	{
		struct page* b_p = get_buddy_chunk(pool,page);
		if(b_p == NULL || b_p ->allocated || b_p->order != page->order)
		{
			break;
		}
		if(page > b_p)
		{
			struct page * tmp = b_p;
			b_p = page;
			page = tmp; 
		}
		b_p ->allocated = 1; // 没单独拆分出来是不能使用该块的
		struct free_list *fls = &pool->free_lists[page->order];
		list_del(&(page->node));
		--fls->nr_free;
		++page->order;
	}
	struct free_list *fls2 = &pool->free_lists[page->order];
	list_add(&page->node,&fls2->free_list);
	++fls2->nr_free;
	return page; 
	// </lab2>
}

/*
 * buddy_free_pages: give back the pages to buddy system
 * pool @ physical memory structure reserved in the kernel
 * page @ free page structure
 * 
 * Hints: you can invoke merge_page.
 */
void buddy_free_pages(struct phys_mem_pool *pool, struct page *page)
{
	// <lab2>
	if(page->allocated == 0)
	{
		kwarn("not allocated \n");
		return ;
	}
	page->allocated = 0;
	struct free_list *fl = &pool->free_lists[page->order];
	list_add(&page->node,&fl->free_list);
	merge_page(pool,page);
	--fl->nr_free;

	// </lab2>
}

void *page_to_virt(struct phys_mem_pool *pool, struct page *page)
{
	u64 addr;

	/* page_idx * BUDDY_PAGE_SIZE + start_addr */
	addr = (page - pool->page_metadata) * BUDDY_PAGE_SIZE +
	    pool->pool_start_addr;
	return (void *)addr;
}

struct page *virt_to_page(struct phys_mem_pool *pool, void *addr)
{
	struct page *page;

	page = pool->page_metadata +
	    (((u64) addr - pool->pool_start_addr) / BUDDY_PAGE_SIZE);
	return page;
}

u64 get_free_mem_size_from_buddy(struct phys_mem_pool * pool)
{
	int order;
	struct free_list *list;
	u64 current_order_size;
	u64 total_size = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; order++) {
		/* 2^order * 4K */
		current_order_size = BUDDY_PAGE_SIZE * (1 << order);
		list = pool->free_lists + order;
		total_size += list->nr_free * current_order_size;

		/* debug : print info about current order */
		kdebug("buddy memory chunk order: %d, size: 0x%lx, num: %d\n",
		       order, current_order_size, list->nr_free);
	}
	return total_size;
}
