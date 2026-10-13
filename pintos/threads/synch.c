/* 이 파일은 Nachos 교육용 운영체제의 소스 코드에서
   가져온 것임. Nachos 저작권 고지는 아래에
   전문을 그대로 실었음. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* 세마포어 SEMA를 VALUE로 초기화함. 세마포어는
   음이 아닌 정수와, 그것을 다루는 두 개의 원자적(atomic)
   연산으로 이루어짐:

   - down 또는 "P": 값이 양수가 될 때까지 기다린 뒤
   값을 1 줄임.

   - up 또는 "V": 값을 1 늘림 (그리고 기다리는 스레드가 있으면
   하나를 깨움). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* 세마포어의 down 또는 "P" 연산. SEMA의 값이 양수가 될 때까지
   기다린 뒤 원자적으로 값을 1 줄임.

   이 함수는 잠들 수 있으므로 인터럽트 핸들러 안에서
   호출하면 안 됨. 이 함수는 인터럽트가 꺼진 채로 호출해도 되지만,
   잠들게 되면 다음에 스케줄되는
   스레드가 아마 인터럽트를 다시 켤 것임. 이것이
   sema_down 함수임. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	/* 세마 value가 0이면 block된 상태여서 멈춰있다가 unblock되면 세마 value가 0이 아니게 되며 조건이 깨짐 */
	while (sema->value == 0) {
		/* sema waiter 스레드 정렬 삽입 */
		list_insert_ordered (&sema->waiters, &thread_current()->elem, thread_priority_greater, NULL);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* 세마포어의 down 또는 "P" 연산이지만, 세마포어가 아직 0이
   아닐 때만 수행함. 세마포어 값을 줄였으면 true를, 아니면
   false를 반환함.

   이 함수는 인터럽트 핸들러에서 호출해도 됨. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* 세마포어의 up 또는 "V" 연산. SEMA의 값을 1 늘리고
   SEMA를 기다리는 스레드가 있으면 그중 하나를 깨움.

   이 함수는 인터럽트 핸들러에서 호출해도 됨. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	sema->value++;
	/* 세마 반환후 스위칭 */
	thread_priority_switch();
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* 한 쌍의 스레드 사이에서 제어가 "핑퐁"하게 만드는
   세마포어 자체 테스트. printf() 호출을 넣으면
   무슨 일이 일어나는지 볼 수 있음. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* sema_self_test()가 쓰는 스레드 함수. */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* LOCK을 초기화함. 락은 어느 시점에든 최대 한 스레드만
   쥘 수 있음. 이 락은 "재귀적(recursive)"이지 않음. 즉
   현재 락을 쥔 스레드가 그 락을 다시
   얻으려 하면 오류임.

   락은 초기값이 1인 세마포어를 특수화한 것임. 락과 그런
   세마포어의 차이는 두 가지임.
   첫째, 세마포어는 값이 1보다 클 수 있지만 락은 한 번에
   한 스레드만 소유할 수 있음.
   둘째, 세마포어는 소유자가 없어서
   한 스레드가 "down"하고 다른 스레드가 "up"할 수 있지만,
   락은 같은 스레드가 얻기(acquire)와 풀기(release)를 모두 해야 함.
   이런 제약이 부담스럽다면
   락 대신 세마포어를 써야 한다는
   좋은 신호임. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* LOCK을 얻음. 필요하면 사용 가능해질 때까지 잠듦.
   락은 현재
   스레드가 이미 쥐고 있으면 안 됨.

   이 함수는 잠들 수 있으므로 인터럽트 핸들러 안에서
   호출하면 안 됨. 인터럽트가 꺼진 채로 호출해도 되지만,
   잠들어야 하면 인터럽트가 다시
   켜짐. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	sema_down (&lock->semaphore);
	lock->holder = thread_current ();
}

/* LOCK을 얻으려고 시도해서 성공하면 true를, 실패하면 false를
   반환함. 락은 현재 스레드가 이미
   쥐고 있으면 안 됨.

   이 함수는 잠들지 않으므로
   인터럽트 핸들러 안에서 호출해도 됨. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* 현재 스레드가 소유한 LOCK을 풂.
   이것이 lock_release 함수임.

   인터럽트 핸들러는 락을 얻을 수 없으므로
   인터럽트 핸들러 안에서 락을 푸는 것은
   의미가 없음. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* 현재 스레드가 LOCK을 쥐고 있으면 true를, 아니면
   false를 반환함. (다른 스레드가 락을 쥐고 있는지
   검사하는 것은 경쟁 상태(racy)가 생길 수 있음에 주의.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* 리스트에 있는 세마포어 하나. */
struct semaphore_elem {
	struct list_elem elem;              /* 리스트 원소. */
	struct semaphore semaphore;         /* 이 세마포어. */
	struct thread *holder;				/* 스레드 필드 추가 (컨디션에서 우선순위 비교를 위해) */
};

/* 컨디션 변수 COND를 초기화함. 컨디션 변수는 한 코드 조각이
   조건을 신호로 알리고, 협력하는 코드가
   그 신호를 받아 대응할 수 있게 해 줌. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* 원자적으로 LOCK을 풀고, 다른 코드가 COND에 신호를 보낼 때까지
   기다림. COND에 신호가 오면 반환하기 전에 LOCK을 다시
   얻음. 이 함수를 호출하기 전에 LOCK을 쥐고 있어야
   함.

   이 함수가 구현하는 모니터는 "Hoare" 방식이 아니라 "Mesa"
   방식임. 즉 신호를 보내는 것과 받는 것이
   원자적 연산이 아님. 그러므로 보통 호출한 쪽은 기다림이
   끝난 뒤 조건을 다시 확인하고, 필요하면 다시
   기다려야 함.

   하나의 컨디션 변수는 하나의 락과만 연결되지만,
   하나의 락은 여러 개의 컨디션 변수와 연결될 수 있음.
   즉 락에서 컨디션 변수로의
   일대다(one-to-many) 대응임.

   이 함수는 잠들 수 있으므로 인터럽트 핸들러 안에서
   호출하면 안 됨. 인터럽트가 꺼진 채로 호출해도 되지만,
   잠들어야 하면 인터럽트가 다시
   켜짐. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));
	/* 아직 신호가 오지 않은 상태(0)로 시작함. cond_signal이 sema_up하기 전까지 sema_down에서 잠듦 */
	sema_init (&waiter.semaphore, 0);
	waiter.holder = thread_current();
	list_insert_ordered (&cond->waiters, &waiter.elem, sema_priority_greater, NULL);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* condition->waiter에 세마포어 원소를 정렬해서 넣음 */
bool
sema_priority_greaterc( const struct list_elem *a, const struct list_elem *b, void *aux)
{
	struct semaphore_elem *sa = list_entry(a, struct semaphore_elem, elem);
	struct semaphore_elem *sb = list_entry(b, struct semaphore_elem, elem);
	
	if (sa->holder->priority > sb->holder->priority)
		return true;
	return false;
}
/* COND를 기다리는 스레드가 있으면(LOCK으로 보호됨),
   그중 하나에게 신호를 보내 기다림에서 깨어나게 함.
   이 함수를 호출하기 전에 LOCK을 쥐고 있어야 함.

   인터럽트 핸들러는 락을 얻을 수 없으므로
   인터럽트 핸들러 안에서 컨디션 변수에 신호를
   보내려는 것은 의미가 없음. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* COND를 기다리는 모든 스레드를 깨움(LOCK으로 보호됨).
   이 함수를 호출하기 전에 LOCK을 쥐고 있어야 함.

   인터럽트 핸들러는 락을 얻을 수 없으므로
   인터럽트 핸들러 안에서 컨디션 변수에 신호를
   보내려는 것은 의미가 없음. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
