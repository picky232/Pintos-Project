#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* 스레드 생애 주기의 상태들. */
enum thread_status {
	THREAD_RUNNING,     /* 실행 중인 스레드. */
	THREAD_READY,       /* 실행 중은 아니지만 실행할 준비가 된 상태. */
	THREAD_BLOCKED,     /* 어떤 이벤트가 일어나기를 기다리는 상태. */
	THREAD_DYING        /* 곧 제거될 상태. */
};

/* 스레드 식별자 타입.
   원하는 타입으로 다시 정의해도 됨. */
typedef int tid_t; // int에 tid_t라는 별칭(alias) 부여 (thread id)
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. 스레드 생성실패시 반환하는 에러값*/

/* 스레드 우선순위. */
#define PRI_MIN 0                       /* 가장 낮은 우선순위. */
#define PRI_DEFAULT 31                  /* 기본 우선순위. */
#define PRI_MAX 63                      /* 가장 높은 우선순위. */

/* 커널 스레드 또는 유저 프로세스.
 *
 * 각 스레드 구조체는 자기만의 4 kB 페이지에 저장됨.
 * 스레드 구조체 자체는 페이지의 맨 아래(오프셋 0)에 있음.
 * 페이지의 나머지는 스레드의 커널 스택용으로 예약되어 있음.
 * 커널 스택은 페이지 맨 위(오프셋 4 kB)에서 아래로 자람.
 * 그림으로 보면 다음과 같음:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * 이것이 의미하는 바는 두 가지임:
 *
 *    1. 첫째, `struct thread`가 너무 커지면 안 됨.
 *       커지면 커널 스택이 쓸 공간이 부족해짐.
 *       기본 `struct thread`는 크기가 몇 바이트에
 *       불과함. 아마 1 kB보다
 *       훨씬 작게 유지하는 것이 좋음.
 *
 *    2. 둘째, 커널 스택이 너무 커지면 안 됨. 스택이
 *       넘치면 스레드 상태가 망가짐. 그러므로
 *       커널 함수는 큰 구조체나 배열을 static이 아닌
 *       지역 변수로 할당하면 안 됨. 대신
 *       malloc()이나 palloc_get_page()로
 *       동적 할당을 쓸 것.
 *
 * 이 두 문제 중 어느 쪽이든 첫 증상은 아마 thread_current()의
 * assertion 실패로 나타남. 이 함수는 실행 중인 스레드의
 * `struct thread`의 `magic` 멤버가 THREAD_MAGIC으로
 * 설정되어 있는지 검사함. 스택이 넘치면 보통 이 값이
 * 바뀌어서 assertion이 발동함. */
/* `elem` 멤버는 용도가 두 가지임. 실행 큐(run queue, thread.c)의
 * 원소일 수도 있고, 세마포어 대기 목록(synch.c)의 원소일 수도
 * 있음. 이 두 가지로 쓸 수 있는 것은 서로 동시에 일어날 수 없기
 * 때문임. ready 상태의 스레드만 실행 큐에 있고, blocked 상태의
 * 스레드만 세마포어 대기 목록에 있음.
 */
struct thread {
	/* thread.c가 소유함. */
	tid_t tid;                          /* 스레드 식별자. */
	enum thread_status status;          /* 스레드 상태. */
	char name[16];                      /* 이름 (디버깅용). */
	int priority;                       /* 우선순위. */

	int64_t wake_tick;					// 깨워야 할 시간

	/* thread.c와 synch.c가 함께 씀. */
	struct list_elem elem;              /* 리스트 원소. */

#ifdef USERPROG
	/* userprog/process.c가 소유함. */
	uint64_t *pml4;                     /* 페이지 맵 레벨 4 */
#endif
#ifdef VM
	/* 스레드가 소유한 전체 가상 메모리 테이블. */
	struct supplemental_page_table spt;
#endif

	/* thread.c가 소유함. */
	struct intr_frame tf;               /* 문맥 교환(switching)용 정보 */
	unsigned magic;                     /* 스택 오버플로를 감지함. */
};

/* false(기본값)이면 라운드 로빈 스케줄러를 씀.
   true이면 멀티레벨 피드백 큐 스케줄러를 씀.
   커널 명령줄 옵션 "-o mlfqs"로 제어함. */
extern bool thread_mlfqs; // 스케쥴러 종류를 고르는 전역 스위치

void thread_init (void);  // 스레드 초기화 - 첫 스레드 등록
void thread_start (void); // 

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

#endif /* threads/thread.h */
