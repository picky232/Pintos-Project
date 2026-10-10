#include "userprog/tss.h"
#include <debug.h>
#include <stddef.h>
#include "userprog/gdt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "intrinsic.h"

/* 태스크 상태 세그먼트(Task-State Segment, TSS).
 *
 *  x86-64 전용 구조인 TSS의 인스턴스는 "태스크(task)"를 정의하는 데 쓰임.
 *  태스크는 멀티태스킹을 프로세서에 내장해서 지원하는 방식임.
 *  하지만 이식성, 속도, 유연성 등 여러 이유로
 *  대부분의 x86-64 OS는 TSS를 거의 완전히
 *  무시함. 우리도 예외가 아님.
 *
 *  그런데 TSS로만 할 수 있는 일이 하나 있음:
 *  유저 모드에서 발생한 인터럽트를 위한 스택 전환임.
 *  인터럽트가 유저 모드(ring 3)에서 발생하면 프로세서는 현재 TSS의
 *  rsp0 멤버를 참조해서 인터럽트를 처리할
 *  스택을 정함. 그러므로 TSS를 만들고 적어도 이 필드들을
 *  초기화해야 하며, 이 파일이 바로
 *  그 일을 함.
 *
 *  인터럽트 게이트나 트랩 게이트로 인터럽트를 처리할 때
 *  (우리가 처리하는 모든 인터럽트가 해당함) x86-64 프로세서는
 *  다음과 같이 동작함:
 *
 *    - 인터럽트당한 코드가 인터럽트 핸들러와 같은
 *      링(ring)에 있으면 스택 전환이 일어나지
 *      않음. 커널에서 실행 중일 때 발생하는
 *      인터럽트가 이 경우임. 이때 TSS의 내용은
 *      상관없음.
 *
 *    - 인터럽트당한 코드가 핸들러와 다른 링에 있으면
 *      프로세서는 새 링에 대해 TSS에 지정된
 *      스택으로 전환함. 유저 공간에서 실행 중일 때 발생하는
 *      인터럽트가 이 경우임. 손상을 피하려면 이미 쓰고 있지
 *      않은 스택으로 전환하는 것이 중요함. 우리는 유저
 *      공간에서 실행 중이므로 현재 프로세스의 커널
 *      스택은 쓰이고 있지 않음을 알 수 있고, 따라서 항상
 *      그것을 쓸 수 있음. 그래서
 *      스케줄러가 스레드를 전환할 때
 *      TSS의 스택 포인터도 새 스레드의 커널 스택을 가리키도록 바꿈.
 *      (호출은 thread.c의 schedule에 있음.) */

/* 커널 TSS. */
struct task_state *tss;

/* 커널 TSS를 초기화함. */
void
tss_init (void) {
	/* 우리 TSS는 콜 게이트나 태스크 게이트에서 쓰이지 않으므로, 일부
	 * 필드만 참조되며, 우리가 초기화하는 것도
	 * 그 필드들뿐임. */
	tss = palloc_get_page (PAL_ASSERT | PAL_ZERO);
	tss_update (thread_current ());
}

/* 커널 TSS를 반환함. */
struct task_state *
tss_get (void) {
	ASSERT (tss != NULL);
	return tss;
}

/* TSS의 링 0 스택 포인터가 스레드 스택의 끝을
 * 가리키도록 설정함. */
void
tss_update (struct thread *next) {
	ASSERT (tss != NULL);
	tss->rsp0 = (uint64_t) next + PGSIZE;
}
