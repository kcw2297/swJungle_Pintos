/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page).
  anon.c : 디스크가 아닌 이미지에 대한 페이지 구현(익명 페이지라고도 함) */

#include "vm/vm.h"
#include "lib/kernel/bitmap.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

// for anon_init
struct bitmap *swap_table;
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;

/* Initialize the data for anonymous pages
익명 페이지에 대한 데이터 초기화 */
void
vm_anon_init (void) {
	swap_disk = disk_get(1, 1);
    size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
    swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping 
파일 매핑 초기화*/
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	
	// struct uninit_page *uninit = &page->uninit;
    // memset(uninit, 0, sizeof(struct uninit_page));
	
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	
	// anon_page->swap_index = -1;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
// swap disk에서 anon page로 스왑

// struct page에 있는 index = 데이터 위치
// swap table 업데이트
	struct anon_page *anon_page = &page->anon;
	size_t page_no = anon_page->swap_index;

	if (bitmap_test(swap_table, page_no) == false) {
        return false;
    }

	for(int i=0 ; i<SECTORS_PER_PAGE ; i++){
		disk_read(swap_disk, page_no * SECTORS_PER_PAGE+i,
		 kva + DISK_SECTOR_SIZE*i);
	}

	bitmap_set(swap_table, page_no, false);
	// ##### 1 고민
	pml4_set_page(thread_current()->pml4, page->va, kva, 0);
	anon_page->swap_index = NULL;
	
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// 스왑 테이블에서 사용 가능한 스왑 슬롯 찾기 (슬롯 == 1 page)
	size_t page_no = bitmap_scan (swap_table, 0, 1, 0);

	if(page_no == BITMAP_ERROR)
		return false;

	for (int i = 0; i < SECTORS_PER_PAGE; i++) {
		disk_write (swap_disk, page_no * SECTORS_PER_PAGE + i, page->va + DISK_SECTOR_SIZE * i);
	}

	bitmap_set(swap_table, page_no, true);
	pml4_clear_page(thread_current()->pml4, page->va);

	anon_page->swap_index = page_no;

	return true; 
	}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
