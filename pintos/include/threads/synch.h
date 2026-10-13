#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* 카운팅 세마포어. */
struct semaphore {
	unsigned value;             /* 현재 값. */
	struct list waiters;        /* 기다리는 스레드 목록. */
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* 락(Lock). */
struct lock {
	struct thread *holder;      /* 락을 쥔 스레드 (디버깅용). */
	struct semaphore semaphore; /* 접근을 제어하는 이진 세마포어. */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* 컨디션 변수. */
struct condition {
	struct list waiters;        /* 기다리는 스레드 목록. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* 
	cond -> waiter -> semaphore_elem의 elem(list_elem) -> semaphore->waiter의 elem(list_elem)으로 스레드에 접근해서
	스레드의 우선순위를 확인하고 이에 맞춰서 semaphore_elem를 정렬하기 위해 a, b를 비교하는 함수
*/
bool sema_priority_greater(struct list_elem *a, struct list_elem *b, void *aux);
/* 최적화 배리어(Optimization barrier).
 *
 * 컴파일러는 최적화 배리어를 넘어서 연산의 순서를 바꾸지
 * 않음. 자세한 내용은 레퍼런스 가이드의 "Optimization
 * Barriers"를 볼 것.*/
#define barrier() asm volatile ("" : : : "memory") // 메모리에서 값을 다시 읽어오라는 명령, 이줄 넘어서 메모리 읽기/쓰기 순서 바꾸기 X

#endif /* threads/synch.h */
