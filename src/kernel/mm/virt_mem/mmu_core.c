#include <hal/mm/mmu_core.h>
#include <kernel/mm/virt_mem/table_alloc.h>
#include <kernel/mm/virt_mem/mmu_core.h>
#include <kernel/mm/opallocator.h>


static unsigned long *context[] = {NULL, NULL, NULL, NULL, NULL };


int mmu_map_region(mmu_ctx_t ctx, paddr_t phy_addr, vaddr_t virt_addr,
		size_t reg_size, mmu_page_flags_t flags) {
	uint8_t cur_level;
	uint32_t cur_offset;
	signed long treg_size;
	pte_t pte;
	paddr_t paddr;
	vaddr_t vaddr = 0;
	context[1] = (unsigned long *) mmu_get_root(ctx);
	/* assuming addresses aligned to page size */
	phy_addr &= ~MMU_PAGE_MASK;
	virt_addr &= ~MMU_PAGE_MASK;
	reg_size &= ~MMU_PAGE_MASK;
	treg_size = reg_size & (~(MMU_PAGE_MASK ));
	/* translate general flags to arch-specified */
	flags = mmu_flags_translate(flags);
	/* will map the best fitting area on each step
	 * will choose smaller area, while not found the best fitting */
	while ( treg_size > 0) {
		for (cur_level = 1; cur_level < 4; cur_level++) {
			cur_offset = ((virt_addr & mmu_table_masks[cur_level]) >> mmu_table_offsets[cur_level]);
#ifdef DEBUG
			printf("level %d; vaddr 0x%8x; paddr 0x%8x; context 0x%8x\n",
				cur_level, (uint32_t)virt_addr, (uint32_t)phy_addr, context[cur_level]);
#endif
			vaddr += mmu_level_capacity[cur_level] * cur_offset;
			/* if mapping vaddr is aligned and if required size is pretty fit */
			if ((virt_addr % mmu_level_capacity[cur_level] == 0) &&
			       (mmu_level_capacity[cur_level] == treg_size)) {
#ifdef DEBUG
				printf("exiting\n");
#endif
			    break;
			}
			/* if there is mapping allready */
			//TODO untested!
			if (mmu_is_pte(*(context[cur_level] + cur_offset))) {
#ifdef DEBUG
				printf("already mapped\n");
#endif
				paddr = mmu_pte_extract(*(context[cur_level] + cur_offset));
				*(context[cur_level] + cur_offset) = 0;
				/* need to divide -- old part rests from left */
				if (vaddr < virt_addr && (virt_addr - vaddr > 0)) {
					mmu_map_region(ctx, paddr, vaddr, virt_addr - vaddr, mmu_flags_extract(*(context[cur_level] + cur_offset)));
				}
				/* old part remains from right */
				else {
					if (mmu_level_capacity[cur_level] - treg_size > 0) {
						mmu_map_region(ctx, paddr + treg_size, vaddr + treg_size, mmu_level_capacity[cur_level] - treg_size,
								mmu_flags_extract(*(context[cur_level] + cur_offset)));
					}
				}
			}
			/* if there is no next-level page table */
			if (*(context[cur_level] + cur_offset) == 0) {
#ifdef DEBUG
				printf("no mapping\n");
#endif
				/* there is no middle page - creating */
				pmark_t *table = (pmark_t *) mmu_table_alloc(mmu_page_table_sizes[cur_level + 1]);
				if (table == NULL) {
					return -1;
				}
				/* setting it's pointer to a prev level page */
				(*mmu_page_table_sets[cur_level])(context[cur_level] + cur_offset,
					(void *) table);
			}
			/* going to the next level map */
			pte = (pte_t)(((unsigned long *) context[cur_level]) + cur_offset);
			context[cur_level + 1] = (*mmu_page_table_gets[cur_level])(pte);
		}
		/* we are on the best fitting level - creating mapping */
		mmu_set_pte(context[cur_level] + cur_offset, mmu_pte_format(phy_addr, flags));
#ifdef DEBUG
		printf("%x\n", mmu_pte_format(phy_addr, flags));
#endif
		/* reducing mapping area */
		treg_size -= mmu_level_capacity[cur_level];
		phy_addr += mmu_level_capacity[cur_level];
		virt_addr += mmu_level_capacity[cur_level];
	}
	return 0;
}
