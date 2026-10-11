#include "list.h"
#include "../debug.h"

/* 우리의 이중 연결 리스트에는 두 개의 헤더 원소가 있음.
   첫 번째 원소 바로 앞의 "head"와 마지막 원소 바로 뒤의 "tail"임.
   front 헤더의 `prev` 링크는 널이고, back 헤더의
   `next` 링크도 널임. 나머지 두 링크는 리스트의
   내부 원소들을 거쳐 서로를 향함.

   빈 리스트는 이렇게 생겼음:

   +------+     +------+
   <---| head |<--->| tail |--->
   +------+     +------+

   원소가 두 개인 리스트는 이렇게 생겼음:

   +------+     +-------+     +-------+     +------+
   <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
   +------+     +-------+     +-------+     +------+

   이 배열의 대칭성 덕분에 리스트 처리에서 수많은 특수 경우가
   사라짐. 예를 들어 list_remove()를 보면:
   포인터 대입이 두 번뿐이고 조건문이 하나도 없음.
   헤더 원소가 없었다면 코드가 훨씬
   복잡했을 것임.

   (각 헤더 원소에서 포인터가 하나씩만 쓰이므로,
   사실 이 단순함을 해치지 않고 하나의 헤더 원소로
   합칠 수도 있음. 하지만 두 개를 따로 두면
   일부 연산에서 약간의 검사를 할 수 있어서
   가치가 있음.) */

static bool is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) UNUSED;

/* ELEM이 head이면 true를, 아니면 false를 반환함. */
static inline bool
is_head (struct list_elem *elem) {
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* ELEM이 interior 원소이면 true를,
   아니면 false를 반환함. */
static inline bool
is_interior (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* ELEM이 tail이면 true를, 아니면 false를 반환함. */
static inline bool
is_tail (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* LIST를 빈 리스트로 초기화함. */
void
list_init (struct list *list) {
	ASSERT (list != NULL);
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* LIST의 beginning을 반환함.  */
struct list_elem *
list_begin (struct list *list) {
	ASSERT (list != NULL);
	return list->head.next;
}

/* ELEM의 리스트에서 ELEM 다음 원소를 반환함. ELEM이 리스트의
   마지막 원소이면 리스트 tail을 반환함. ELEM 자신이 리스트 tail이면
   결과는 정의되지 않음. */
struct list_elem *
list_next (struct list_elem *elem) {
	ASSERT (is_head (elem) || is_interior (elem));
	return elem->next;
}

/* LIST의 tail을 반환함.

   list_end()는 리스트를 front에서 back으로 순회할 때 자주 쓰임.
   예시는 list.h 맨 위의 큰 주석을
   볼 것. */
struct list_elem *
list_end (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* LIST의 reverse beginning을 반환함. LIST를 back에서 front로
   거꾸로 순회할 때 씀. */
struct list_elem *
list_rbegin (struct list *list) {
	ASSERT (list != NULL);
	return list->tail.prev;
}

/* ELEM의 리스트에서 ELEM 앞의 원소를 반환함. ELEM이 리스트의
   첫 번째 원소이면 리스트 head를 반환함. ELEM 자신이 리스트 head이면
   결과는 정의되지 않음. */
struct list_elem *
list_prev (struct list_elem *elem) {
	ASSERT (is_interior (elem) || is_tail (elem));
	return elem->prev;
}

/* LIST의 head를 반환함.

   list_rend()는 리스트를 back에서 front로 거꾸로 순회할 때
   자주 쓰임. list.h 맨 위의 예시에 이어지는 전형적인
   사용법은 다음과 같음:

   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list);
   e = list_prev (e))
   {
   struct foo *f = list_entry (e, struct foo, elem);
   ...do something with f...
   }
   */
struct list_elem *
list_rend (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* LIST의 head를 반환함.

   list_head()는 리스트를 순회하는 다른 방식에
   쓸 수 있음. 예:

   e = list_head (&list);
   while ((e = list_next (e)) != list_end (&list))
   {
   ...
   }
   */
struct list_elem *
list_head (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* LIST의 tail을 반환함. */
struct list_elem *
list_tail (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* ELEM을 BEFORE 바로 앞에 삽입함. BEFORE는 interior 원소일 수도
   tail일 수도 있음. tail인 경우는 list_push_back()과
   같음. */
void
list_insert (struct list_elem *before, struct list_elem *elem) {
	ASSERT (is_interior (before) || is_tail (before));
	ASSERT (elem != NULL);

	elem->prev = before->prev;
	elem->next = before;
	before->prev->next = elem;
	before->prev = elem;
}

/* FIRST부터 LAST(LAST 제외)까지의 원소를 현재 리스트에서 제거한 뒤
   BEFORE 바로 앞에 삽입함. BEFORE는
   interior 원소일 수도 tail일 수도 있음. */
void
list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last) {
	ASSERT (is_interior (before) || is_tail (before));
	if (first == last)
		return;
	last = list_prev (last);

	ASSERT (is_interior (first));
	ASSERT (is_interior (last));

	/* FIRST...LAST를 현재 리스트에서 깔끔하게 제거함. */
	first->prev->next = last->next;
	last->next->prev = first->prev;

	/* FIRST...LAST를 새 리스트에 끼워 넣음(splice). */
	first->prev = before->prev;
	last->next = before;
	before->prev->next = first;
	before->prev = last;
}

/* ELEM을 LIST의 맨 앞에 삽입해서 LIST의 front가
   되게 함. */
void
list_push_front (struct list *list, struct list_elem *elem) {
	list_insert (list_begin (list), elem);
}

/* ELEM을 LIST의 맨 뒤에 삽입해서 LIST의 back이
   되게 함. */
void
list_push_back (struct list *list, struct list_elem *elem) {
	list_insert (list_end (list), elem);
}

/* ELEM을 리스트에서 제거하고 그 뒤에 있던 원소를 반환함.
   ELEM이 리스트에 없으면 동작이 정의되지 않음.

   제거한 뒤에는 ELEM을 리스트의 원소로 다루는 것이 안전하지 않음.
   특히 제거 후 ELEM에 list_next()나 list_prev()를 쓰면
   동작이 정의되지 않음. 이는 리스트의 원소를 제거하는
   단순한 루프가 실패한다는 뜻임:

 ** DON'T DO THIS **
 for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
 {
 ...do something with e...
 list_remove (e);
 }
 ** DON'T DO THIS **

  리스트의 원소를 순회하면서 제거하는 올바른 방법 하나는
 다음과 같음:

for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
{
...do something with e...
}

list의 원소를 free()해야 한다면
더 보수적으로 해야 함. 그 경우에도 통하는
다른 전략은 다음과 같음:

while (!list_empty (&list))
{
struct list_elem *e = list_pop_front (&list);
...do something with e...
}
*/
struct list_elem *
list_remove (struct list_elem *elem) {
	ASSERT (is_interior (elem));
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	return elem->next;
}

/* LIST의 front 원소를 제거하고 반환함.
   제거 전에 LIST가 비어 있으면 동작이 정의되지 않음. */
struct list_elem *
list_pop_front (struct list *list) {
	struct list_elem *front = list_front (list);
	list_remove (front);
	return front;
}

/* LIST의 back 원소를 제거하고 반환함.
   제거 전에 LIST가 비어 있으면 동작이 정의되지 않음. */
struct list_elem *
list_pop_back (struct list *list) {
	struct list_elem *back = list_back (list);
	list_remove (back);
	return back;
}

/* LIST의 front 원소를 반환함.
   LIST가 비어 있으면 동작이 정의되지 않음. */
struct list_elem *
list_front (struct list *list) {
	ASSERT (!list_empty (list));
	return list->head.next;
}

/* LIST의 back 원소를 반환함.
   LIST가 비어 있으면 동작이 정의되지 않음. */
struct list_elem *
list_back (struct list *list) {
	ASSERT (!list_empty (list));
	return list->tail.prev;
}

/* LIST의 원소 개수를 반환함.
   원소 수에 대해 O(n)으로 실행됨. */
size_t
list_size (struct list *list) {
	struct list_elem *e;
	size_t cnt = 0;

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		cnt++;
	return cnt;
}

/* LIST가 비어 있으면 true를, 아니면 false를 반환함. */
bool
list_empty (struct list *list) {
	return list_begin (list) == list_end (list);
}

/* A와 B가 가리키는 `struct list_elem *`를 서로 바꿈. */
static void
swap (struct list_elem **a, struct list_elem **b) {
	struct list_elem *t = *a;
	*a = *b;
	*b = t;
}

/* LIST의 순서를 뒤집음. */
void
list_reverse (struct list *list) {
	if (!list_empty (list)) {
		struct list_elem *e;

		for (e = list_begin (list); e != list_end (list); e = e->prev)
			swap (&e->prev, &e->next);
		swap (&list->head.next, &list->tail.prev);
		swap (&list->head.next->prev, &list->tail.prev->next);
	}
}

/* 리스트 원소 A부터 B(B 제외)까지가 보조 데이터 AUX를 받은 LESS 기준으로
   순서대로 정렬되어 있을 때만 true를 반환함. */
static bool
is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	if (a != b)
		while ((a = list_next (a)) != b)
			if (less (a, list_prev (a), aux))
				return false;
	return true;
}

/* A에서 시작해 B를 넘지 않는 곳에서 끝나는, 리스트 원소들의 run을 찾음.
   run은 LESS와 보조 데이터 AUX 기준으로 줄어들지 않는(nondecreasing)
   순서인 원소들임. run의 (제외되는) 끝을
   반환함.
   A부터 B(B 제외)까지는 비어 있지 않은 범위여야 함. */
static struct list_elem *
find_end_of_run (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	ASSERT (a != NULL);
	ASSERT (b != NULL);
	ASSERT (less != NULL);
	ASSERT (a != b);

	do {
		a = list_next (a);
	} while (a != b && !less (a, list_prev (a), aux));
	return a;
}

/* A0부터 A1B0(A1B0 제외)까지와 A1B0부터 B1(B1 제외)까지를 합쳐
   마찬가지로 B1(B1 제외)에서 끝나는 하나의 범위를 만듦. 두
   입력 범위는 모두 비어 있지 않아야 하고, LESS와 보조 데이터
   AUX 기준으로 줄어들지 않는 순서로 정렬되어 있어야 함.
   출력 범위도 같은 방식으로 정렬됨. */
// 정렬된 두 리스트 합쳐주는 함수
static void
inplace_merge (struct list_elem *a0, struct list_elem *a1b0,
		struct list_elem *b1,
		list_less_func *less, void *aux) {
	ASSERT (a0 != NULL);
	ASSERT (a1b0 != NULL);
	ASSERT (b1 != NULL);
	ASSERT (less != NULL);
	ASSERT (is_sorted (a0, a1b0, less, aux));
	ASSERT (is_sorted (a1b0, b1, less, aux));

	while (a0 != a1b0 && a1b0 != b1)
		if (!less (a1b0, a0, aux))
			a0 = list_next (a0);
		else {
			a1b0 = list_next (a1b0);
			list_splice (a0, list_prev (a1b0), a1b0);
		}
}

/* LESS와 보조 데이터 AUX 기준으로 LIST를 정렬함. 자연(natural)
   반복식 병합 정렬을 쓰며 LIST의 원소 수에 대해 O(n lg n) 시간과
   O(1) 공간으로 실행됨. */
void
list_sort (struct list *list, list_less_func *less, void *aux) {
	size_t output_run_cnt;        /* 현재 패스에서 출력한 run의 수. */

	ASSERT (list != NULL);
	ASSERT (less != NULL);

	/* 줄어들지 않는 원소들의 인접한 run을 합치면서 리스트를
	   반복해서 훑되, run이 하나만 남을 때까지 함. */
	do {
		struct list_elem *a0;     /* 첫 번째 run의 시작. */
		struct list_elem *a1b0;   /* 첫 번째 run의 끝이자 두 번째 run의 시작. */
		struct list_elem *b1;     /* 두 번째 run의 끝. */

		output_run_cnt = 0;
		for (a0 = list_begin (list); a0 != list_end (list); a0 = b1) {
			/* 반복마다 출력 run 하나를 만듦. */
			output_run_cnt++;

			/* 줄어들지 않는 원소들의 인접한 두 run인
			   A0...A1B0과 A1B0...B1을 찾음. */
			a1b0 = find_end_of_run (a0, list_end (list), less, aux);// 정렬이 유지되는 구간의 끝을 찾음
			if (a1b0 == list_end (list)) // 리스트의 미지막과 같으면 깨짐
				break;
			b1 = find_end_of_run (a1b0, list_end (list), less, aux); // 나눈 두번째 배열에서 정렬이 유지되는 구간의 끝을 찾음

			/* run들을 합침. */
			inplace_merge (a0, a1b0, b1, less, aux); // 
		}
	}
	while (output_run_cnt > 1);

	ASSERT (is_sorted (list_begin (list), list_end (list), less, aux));
}

/* ELEM을 LIST의 알맞은 위치에 삽입함. LIST는 LESS와 보조 데이터 AUX 기준으로
   정렬되어 있어야 함.
   LIST의 원소 수에 대해 평균적으로 O(n)에 실행됨. */
// 정렬 순서에 맞는 자리에 넣는 함수
void 
list_insert_ordered (struct list *list, struct list_elem *elem,
		list_less_func *less, void *aux) {
	struct list_elem *e;

	ASSERT (list != NULL);
	ASSERT (elem != NULL);
	ASSERT (less != NULL);

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		if (less (elem, e, aux))
			break;
	return list_insert (e, elem);
}

/* LIST를 훑으면서, LESS와 보조 데이터 AUX 기준으로 같은 인접한
   원소들의 묶음마다 첫 번째만 남기고 모두 제거함. DUPLICATES가
   널이 아니면 LIST에서 제거한 원소들을 DUPLICATES에
   덧붙임. */
void
list_unique (struct list *list, struct list *duplicates,
		list_less_func *less, void *aux) {
	struct list_elem *elem, *next;

	ASSERT (list != NULL);
	ASSERT (less != NULL);
	if (list_empty (list))
		return;

	elem = list_begin (list);
	while ((next = list_next (elem)) != list_end (list))
		if (!less (elem, next, aux) && !less (next, elem, aux)) {
			list_remove (next);
			if (duplicates != NULL)
				list_push_back (duplicates, next);
		} else
			elem = next;
}

/* LESS와 보조 데이터 AUX 기준으로 LIST에서 가장 큰 값을 가진
   원소를 반환함. 최댓값이 여러 개이면 리스트에서 더 앞에 나오는
   것을 반환함. 리스트가 비어 있으면
   tail을 반환함. */
struct list_elem *
list_max (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *max = list_begin (list);
	if (max != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (max); e != list_end (list); e = list_next (e))
			if (less (max, e, aux))
				max = e;
	}
	return max;
}

/* LESS와 보조 데이터 AUX 기준으로 LIST에서 가장 작은 값을 가진
   원소를 반환함. 최솟값이 여러 개이면 리스트에서 더 앞에 나오는
   것을 반환함. 리스트가 비어 있으면
   tail을 반환함. */
struct list_elem *
list_min (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *min = list_begin (list);
	if (min != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (min); e != list_end (list); e = list_next (e))
			if (less (e, min, aux))
				min = e;
	}
	return min;
}
