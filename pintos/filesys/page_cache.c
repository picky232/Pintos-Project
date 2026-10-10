/* page_cache.c: 페이지 캐시(버퍼 캐시)의 구현. */

#include "vm/vm.h"
static bool page_cache_readahead (struct page *page, void *kva);
static bool page_cache_writeback (struct page *page);
static void page_cache_destroy (struct page *page);

/* 이 구조체를 수정하지 말 것 */
static const struct page_operations page_cache_op = {
	.swap_in = page_cache_readahead,
	.swap_out = page_cache_writeback,
	.destroy = page_cache_destroy,
	.type = VM_PAGE_CACHE,
};

tid_t page_cache_workerd;

/* 파일 VM의 초기화 함수 */
void
pagecache_init (void) {
	/* TODO: page_cache_kworkerd로 페이지 캐시용 워커 데몬을 만들 것 */
}

/* 페이지 캐시를 초기화함 */
bool
page_cache_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러를 설정함 */
	page->operations = &page_cache_op;

}

/* swap in 메커니즘을 이용해 readahead를 구현함 */
static bool
page_cache_readahead (struct page *page, void *kva) {
}

/* swap out 메커니즘을 이용해 writeback을 구현함 */
static bool
page_cache_writeback (struct page *page) {
}

/* page_cache를 없앰. */
static void
page_cache_destroy (struct page *page) {
}

/* 페이지 캐시용 워커 스레드 */
static void
page_cache_kworkerd (void *aux) {
}
