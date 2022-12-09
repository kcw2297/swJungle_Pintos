/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "threads/mmu.h"


// ##### 1
struct list frame_table;
struct list_elem *start; // 클럭 알고리즘 분침

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes.
 * 각 하위 시스템의 초기화 코드를 호출하여 가상 메모리 하위 시스템을 초기화합니다. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	start = list_begin(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now.
 * 페이지 유형을 가져옵니다. 이 기능은 초기화된 후 페이지 유형을 알고 싶은 경우 유용합니다.
 * 이 기능은 현재 완전히 구현되었습니다.
 *  */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`.
 * Initializer를 사용하여 보류 중인 페이지 개체를 만듭니다.
 * 페이지를 생성하려면 직접 생성하지 말고 이 함수 또는 'vm_alloc_page'를 통해 생성하십시오.
 * load_segment에서 불림, uninit_new 함수 호출*/
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{	
	// printf("va: %p\n",upage);
	// 8번의 enter, 0x400000~0x4747f000, 이후 다시 0x400000로 시작

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;
	
	/* Check wheter the upage is already occupied or not.*/
	if (spt_find_page(spt, upage) == NULL)
	{
		struct page *page = (struct page *)malloc(sizeof(struct page));

		if (VM_TYPE(type) == VM_ANON)
			uninit_new(page, upage, init, type, aux, anon_initializer);
		else if (VM_TYPE(type) == VM_FILE)
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
		else {
			uninit_new(page, upage, init, type, aux, NULL);
		}

		//페이지 구조가 있으면 프로세스의 추가 페이지 테이블에 페이지를 삽입하십시오.
		page->writable = writable;
		/* TODO: Insert the page into the spt.
		 페이지를 SPT에 삽입합니다.*/
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL.
	Returns the page containing the given virtual address, or a null pointer if no such page exists.
	page_lookup
	SPT 및 반환 페이지에서 VA를 찾습니다.
	오류가 발생하면 NULL을 반환합니다.
	지정된 가상 주소가 포함된 페이지를 반환하거나 해당 페이지가 없는 경우 null 포인터를 반환합니다.*/
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{ 
	// 에러전 마지막 va: 0x400000
	struct page *page = NULL;
	page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;

	page->va = pg_round_down(va);
	e = hash_find(&spt->hash_tb, &page->h_elem);

	free(page);

	return e != NULL ? hash_entry(e, struct page, h_elem) : NULL;
}

/* Insert PAGE into spt with validation.
유효성 검사와 함께 페이지를 spt에 삽입합니다. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	// int succ = false;
	/* TODO: Fill this function. */

	// 삽입 성공 시 NULL 반환, 실패 시 elem 포인터 반환
	if (!hash_insert(&spt->hash_tb, &page->h_elem))
		return true;

	return false;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted.
제거될 구조 프레임을 가져옵니다.*/
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	while(start != list_end(&frame_table)) {

		// struct hash_elem *hash_start = list_elem_to_hash_elem (start);
		struct hash_elem *hash_start = list_entry(start, struct hash_elem, list_elem);
		struct page *page = hash_entry(hash_start, struct page, h_elem);
		
		if (pml4_is_accessed(thread_current()->pml4, page->va))
		{
			pml4_set_accessed(thread_current()->pml4, page->va, 0);
		}

		else {
			victim = page->frame;
			return victim;	
		}

		start = list_next(start);
   	}

	start = list_begin(&frame_table);

	while(start != list_end(&frame_table)) {

		struct hash_elem *hash_start = list_elem_to_hash_elem (start);
		struct page *page = hash_entry(hash_start, struct page, h_elem);
		
		if (pml4_is_accessed(thread_current()->pml4, page->va))
		{
			pml4_set_accessed(thread_current()->pml4, page->va, 0);
		}

		else {
			victim = page->frame;
			return victim;	
		}

		start = list_next(start);
   	}

	return victim;

}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.
 한 페이지를 삭제하고 해당 프레임을 반환합니다. 오류가 발생하면 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	// victim->page->operations->swap_out
	
	if (!victim){
		if (swap_out(victim->page))
			return victim;
	}

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.
 * palloc() and get frame 하세요
 * 사용 가능한 페이지가 없는 경우 페이지를 삭제하고 반환합니다.
 * 항상 유효한 주소를 반환합니다.
 * 즉, 사용자 풀 메모리가 가득 차면 이 함수는 사용 가능한 메모리 공간을 얻기 위해 프레임을 제거합니다.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

	// ToDo 1: 프레임 할당
	frame->kva = (struct page *)palloc_get_page(PAL_USER);
	// 유저풀에서 못가져왔을시 페이지가 없는 것 처리를 해야함
	// if frame->kva 가 null일 경우 스왑아웃 처리를
	if (frame->kva == NULL)
	{
		vm_evict_frame();
		//PANIC("에러 !!! 스왑아웃 해야함");

		// frame = vm_evict_frame(); // 다음에 구현
		// frame->page = NULL;
		// return frame;
	}

	list_push_back(&frame_table, &frame->frame_elem);

	frame->page = NULL;

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
    {
        vm_claim_page(addr);
        thread_current()->stack_bottom -= PGSIZE;   // 스택은 위에서부터 쌓기 때문에 주소값 위치를 페이지 사이즈씩 마이너스함
    }
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	// struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (is_kernel_vaddr(addr))
	{
		return false;
	}


	void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
	// printf("=========== vm_try_handle_fault case :1 \n");
	if(!not_present)
		return false;
	// printf("=========== vm_try_handle_fault case :2 \n");
	if(vm_claim_page(addr))
		return true;
	// printf("=========== vm_try_handle_fault case :3 \n");
	if(rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK){
		vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
		return true;
	}
	
	return false;
	// ===> 수정해보자 다음에
	// void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
	// if (not_present)
	// {
	// 	if (!vm_claim_page(addr))
	// 	{
	// 		if (rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK)
	// 		{
	// 			vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
	// 			return true;
	// 		}
	// 		return false;
	// 	}
	// 	else
	// 		return true;
	// }

	// return false;
	// return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	struct thread *cur = thread_current();
	/* TODO: Fill this function */

	page = spt_find_page(&cur->spt, va);
	
	if (page == NULL)
		return false;
	// printf("============ vm_claim_page \n");
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	struct thread *cur = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// 페이지 테이블 entry에 페이지 가상 주소와 프레임 물리주소를 매핑해라
	// bool rw ???
	// pml4_set_page(cur->pml4, page, frame, true); // 수정 필요
	// if (install_page(page, frame, true)) //true???
	// 	return swap_in (page, frame->kva);
	// else
	// 	return false;

	// printf("===> %p \n", page);
	// printf("===> %p \n", frame);
	if (pml4_get_page(cur->pml4, page->va) == NULL && pml4_set_page(cur->pml4, page->va, frame->kva, page->writable)) // install_page()
		return swap_in(page, frame->kva);
	else
		return false;
}

/* Initializes hash table H to compute hash values using HASH and
   compare hash elements using LESS, given auxiliary data AUX.
   주어진 보조 데이터 AUX에서 해시를 사용하여 해시 값을 계산하고
   LESS를 사용하여 해시 요소를 비교하기 위해 해시 테이블 H를 초기화합니다.*/
// bool
// hash_init (struct hash *h,
// 		hash_hash_func *hash, hash_less_func *less, void *aux)

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->hash_tb, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	/*
	 *(최종) src -> dst 로 복사 해야함
	 * uninit 페이지를 할당하고 즉시 요청해야 합니다.
	 * src 의 spt 에 있는 각 페이지를 반복하며 dst의 spt에 정확한 복사본을 만든다
	 */
	struct hash_iterator i;					// 
    hash_first (&i, &src->hash_tb); 		// i가 head를 가리킴
    while (hash_next (&i)) {				// i가 다음 elem이 된다.  시작 src의 각각의 페이지를 반복문을 통해 복사
        struct page *parent_page = hash_entry (hash_cur (&i), struct page, h_elem);   // 현재 해시 테이블의 element 리턴
        enum vm_type type = page_get_type(parent_page);		// 부모 페이지의 type
        void *upage = parent_page->va;						// 부모 페이지의 가상 주소
        bool writable = parent_page->writable;				// 부모 페이지의 쓰기 가능 여부
        vm_initializer *init = parent_page->uninit.init;	// 부모의 초기화되지 않은 페이지들 할당 위해 
        void* aux = parent_page->uninit.aux;

        if (parent_page->uninit.type & VM_MARKER_0) {			// VM_MARKER_0 는 stack을 의미
			// printf("======> VM_MARKER_0 \n");
			// printf("=====> %d \n", parent_page->uninit.type);
            setup_stack(&thread_current()->tf);
        }
        else if(parent_page->operations->type == VM_UNINIT) {	// 부모 타입이 uninit인 경우
			// printf("======> VM_UNINIT \n");
            if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
                return false;
        }
        else {
			// printf("======> else \n");
			// printf("=====> %d \n", parent_page->uninit.type);
            if(!vm_alloc_page(type, upage, writable))
                return false;
            if(!vm_claim_page(upage))
                return false;
        }

        if (parent_page->operations->type != VM_UNINIT) {   // UNIT이 아닌 모든 페이지(stack 포함)는 부모의 것을 memcpy
            struct page* child_page = spt_find_page(dst, upage);
            memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
        }
    }
    return true;

	// // memcpy(&if_, &parent->parent_if, sizeof(struct intr_frame));
	// bool success = false;
	// // success = vm_alloc_page(VM_ANON, &thread_current()->rsp_stack, 1);
	// // if(success)
	// // memcpy(&dst->hash_tb, &src->hash_tb, sizeof(struct hash));
	
	// struct list_elem dstp = list_begin(dst->hash_tb.buckets);
	// for (struct list_elem start = list_begin(src->hash_tb.buckets);
	// 	 start != list_end(src->hash_tb.buckets);
	// 	 start = list_next(start))
	// {
	// 	success = vm_alloc_page(VM_ANON, &thread_current()->rsp_stack, 1);
	// 	if(!success)
	// 		return false;
	// 	// list bucket = 어떤함수(start, hash_elem, elem)
	// 	// for (){}
	// 	struct hash_elem *start_hash = list_elem_to_hash_elem(start);
	// 	struct page * page = hash_entry(start_hash, struct page, hash_elem);
	// 	memcpy(dstp, start, sizeof(struct page));

	// 	dstp = list_next(dstp);
		
	// }
}

void spt_destructor(struct hash_elem *e, void* aux) {
	struct page * page = hash_entry(e, struct page, h_elem);
	// if(page->operations->type == VM_FILE){
	// 	do_munmap(page->va);
	// }
	// vm_dealloc_page(page);
	free(page);
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// munmap ??
	// spt -> hash -> bucket h_elem -> page -> munmap? O , destroy(page) O


	struct hash_iterator i;
    hash_first (&i, &spt->hash_tb); 
    while (hash_next (&i)) {	
		struct page * page = hash_entry(hash_cur(&i), struct page, h_elem);
		if(page->operations->type == VM_FILE){
			do_munmap(page->va);
			// destroy(page);
		}
	}
	hash_destroy(&spt->hash_tb, spt_destructor);
}

// ##### 1

/* Returns a hash value for page p. */
// 해시 테이블 초기화 할 때, 해시 값을 구해주는 함수의 포인터
unsigned
page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, h_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
// 해시 테이블 초기활 할 때, 해시 요소들 비교하는 함수의 포인터
// 가상주소 (해시의 키 값)을 비교
bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, h_elem);
	const struct page *b = hash_entry(b_, struct page, h_elem);

	return a->va < b->va;
}

// bool page_insert();
// bool page_delete();
