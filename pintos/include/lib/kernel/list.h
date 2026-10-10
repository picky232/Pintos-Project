#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/* 이중 연결 리스트(doubly linked list).
 *
 * 이 이중 연결 리스트 구현은 동적으로 할당된 메모리를 쓰지
 * 않음. 대신 리스트 원소가 될 수 있는 각 구조체는
 * struct list_elem 멤버를 내장해야 함. 모든 리스트 함수는 이
 * `struct list_elem`들에 대해 동작함. list_entry 매크로로
 * struct list_elem에서 그것을 담고 있는 구조체 객체로
 * 되돌아갈 수 있음.

 * 예를 들어 `struct foo`의 리스트가 필요하다고 하자.
 * `struct foo`는 다음과 같이 `struct list_elem`
 * 멤버를 담고 있어야 함:

 * struct foo {
 *   struct list_elem elem;
 *   int bar;
 *   ...other members...
 * };

 * 그러면 `struct foo`의 리스트를 다음과 같이 선언하고
 * 초기화할 수 있음:

 * struct list foo_list;

 * list_init (&foo_list);

 * 순회(iteration)는 struct list_elem에서 그것을 감싸는
 * 구조체로 되돌아가야 하는 전형적인 상황임.
 * foo_list를 쓰는 예시는 다음과 같음:

 * struct list_elem *e;

 * for (e = list_begin (&foo_list); e != list_end (&foo_list);
 * e = list_next (e)) {
 *   struct foo *f = list_entry (e, struct foo, elem);
 *   ...do something with f...
 * }

 * 리스트를 쓰는 실제 예시는 소스 곳곳에서 찾을 수 있음.
 * 예를 들어 threads 디렉터리의 malloc.c, palloc.c, thread.c는
 * 모두 리스트를 씀.

 * 이 리스트의 인터페이스는 C++ STL의 list<> 템플릿에서
 * 영감을 받았음. list<>에 익숙하다면
 * 쓰기 쉬울 것임. 다만 이 리스트들은 타입 검사를 *전혀*
 * 하지 않고, 그 밖의 정확성 검사도 거의 할 수 없음을
 * 강조해 둠. 실수하면 그 대가를 치르게 됨.

 * 리스트 용어집:

 * - "front": 리스트의 첫 번째 원소. 빈 리스트에서는
 * 정의되지 않음. list_front()가 반환함.

 * - "back": 리스트의 마지막 원소. 빈 리스트에서는
 * 정의되지 않음. list_back()이 반환함.

 * - "tail": 비유적으로 리스트의 마지막 원소 바로 뒤에 있는
 * 원소. 빈 리스트에서도 잘 정의됨.
 * list_end()가 반환함. front에서 back으로의
 * 순회의 끝 표지(sentinel)로 씀.

 * - "beginning": 비어 있지 않은 리스트에서는 front임. 빈
 * 리스트에서는 tail임. list_begin()이 반환함.
 * front에서 back으로의 순회의 시작점으로 씀.

 * - "head": 비유적으로 리스트의 첫 번째 원소 바로 앞에 있는
 * 원소. 빈 리스트에서도 잘 정의됨.
 * list_rend()가 반환함. back에서 front로의
 * 순회의 끝 표지로 씀.

 * - "reverse beginning": 비어 있지 않은 리스트에서는 back임.
 * 빈 리스트에서는 head임. list_rbegin()이 반환함.
 * back에서 front로의 순회의 시작점으로 씀.
 *
 * - "interior element": head도 tail도 아닌 원소, 즉
 * 진짜 리스트 원소임. 빈 리스트에는
 * interior element가 없음.*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 리스트 원소. */
struct list_elem {
	struct list_elem *prev;     /* 이전 리스트 원소. */
	struct list_elem *next;     /* 다음 리스트 원소. */
};

/* 리스트. */
struct list { 
	struct list_elem head;      /* 리스트 head. */
	struct list_elem tail;      /* 리스트 tail. */
};

/* 리스트 원소 LIST_ELEM을 가리키는 포인터를, LIST_ELEM이
   안에 내장된 구조체를 가리키는 포인터로 바꿈. 바깥 구조체의
   이름 STRUCT와 리스트 원소의 멤버 이름 MEMBER를
   넘겨 줘야 함. 예시는 파일 맨 위의 큰
   주석을 볼 것. */
// e를 가지고있는 thread struct의 시작주소 반환
#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
	((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
		- offsetof (STRUCT, MEMBER.next)))

void list_init (struct list *); // 리스트 생성(초기화)

/* 리스트 순회. */
struct list_elem *list_begin (struct list *); // 헤드(센티널)의 다음노드인 리스트의 시작 원소
struct list_elem *list_next (struct list_elem *); // 다음 원소
struct list_elem *list_end (struct list *); // 리스트 끝 표시

struct list_elem *list_rbegin (struct list *); //역방향 시작점 마지막 원소의 prev
struct list_elem *list_prev (struct list_elem *); // 이전 노드
struct list_elem *list_rend (struct list *); // 역방향 끝 표시

struct list_elem *list_head (struct list *); // 리스트 헤드(센티널)
struct list_elem *list_tail (struct list *); // 리스트 테일(센티널)

/* 리스트 삽입. */
void list_insert (struct list_elem *, struct list_elem *); // elem를 before 바로 앞에 끼워넣음
void list_splice (struct list_elem *before, struct list_elem *first, struct list_elem *last); // 다른 리스트에서 first부터 last까지의 구간을 떼어서 before 앞에 붙임
void list_push_front (struct list *, struct list_elem *); // 맨 앞에 삽입
void list_push_back (struct list *, struct list_elem *); // 맨 뒤에 삽입

/* 리스트 제거. */
struct list_elem *list_remove (struct list_elem *); // 특정 원소 리스트에서 뺌
struct list_elem *list_pop_front (struct list *); // 맨 앞에 원소를 뺴냄
struct list_elem *list_pop_back (struct list *); // 맨 뒤에 원소를 빼냄

/* 리스트 원소. */
struct list_elem *list_front (struct list *); // 맨 앞 원소
struct list_elem *list_back (struct list *); // 맨 뒤 원소

/* 리스트 속성. */
size_t list_size (struct list *); // 리스트안 원소 개수 확인
bool list_empty (struct list *); // 리스트가 비었는지 확인

/* 기타. */
void list_reverse (struct list *); // 리스트 뒤집기

/* 보조 데이터 AUX를 받아 두 리스트 원소 A와 B의 값을 비교함.
   A가 B보다 작으면 true를, A가 B보다 크거나 같으면
   false를 반환함. */
typedef bool list_less_func (const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);

/* 정렬된 원소를 가진 리스트에 대한 연산. */
void list_sort (struct list *,
                list_less_func *, void *aux);
void list_insert_ordered (struct list *, struct list_elem *,
                          list_less_func *, void *aux);
void list_unique (struct list *, struct list *duplicates,
                  list_less_func *, void *aux);

/* 최댓값과 최솟값. */
struct list_elem *list_max (struct list *, list_less_func *, void *aux);
struct list_elem *list_min (struct list *, list_less_func *, void *aux);

#endif /* lib/kernel/list.h */
