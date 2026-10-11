#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* struct thread의 `magic` 멤버에 쓰는 임의의 값.
   스택 오버플로를 감지하는 데 씀. 자세한 내용은 thread.h 맨 위의
   큰 주석을 볼 것. */
#define THREAD_MAGIC 0xcd6abf4b // 스택 오버플로우 감지 Canary

/* 기본 스레드용 임의의 값
   이 값을 수정하지 말 것. */
#define THREAD_BASIC 0xd42df210

/* THREAD_READY 상태의 프로세스 목록, 즉 실행할 준비는 되었지만
   실제로 실행 중은 아닌 프로세스들. */
static struct list ready_list; // 준비 리스트

/* idle 스레드. */
static struct thread *idle_thread;

/* 초기 스레드, init.c:main()을 실행하는 스레드. */
static struct thread *initial_thread;

/* allocate_tid()가 쓰는 락. */
static struct lock tid_lock;

/* 스레드 제거 요청 */
static struct list destruction_req;

/* sleep 스레드 리스트 */
static struct list sleep_list; 

/* 통계. */
static long long idle_ticks;    /* idle 상태로 보낸 타이머 틱 수. */
static long long kernel_ticks;  /* 커널 스레드에서 보낸 타이머 틱 수. */
static long long user_ticks;    /* 유저 프로그램에서 보낸 타이머 틱 수. */

/* 스케줄링. */
#define TIME_SLICE 4            /* 각 스레드에게 줄 타이머 틱 수. */
static unsigned thread_ticks;   /* 마지막 양보 이후의 타이머 틱 수. */

/* false(기본값)이면 라운드 로빈 스케줄러를 씀.
   true이면 멀티레벨 피드백 큐 스케줄러를 씀.
   커널 명령줄 옵션 "-o mlfqs"로 제어함. */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* T가 유효한 스레드를 가리키는 것 같으면 true를 반환함. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* 실행 중인 스레드를 반환함.
 * CPU의 스택 포인터 `rsp`를 읽은 뒤 그 값을
 * 페이지의 시작으로 내림. `struct thread`는 항상 페이지의 맨 앞에
 * 있고 스택 포인터는 그 중간 어딘가에 있으므로,
 * 이렇게 하면 현재 스레드를 찾을 수 있음. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))

// thread_start용 전역 디스크립터 테이블(GDT).
// gdt는 thread_init 이후에 설정되므로,
// 먼저 임시 gdt를 설정해야 함.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };



/* 지금 실행 중인 코드를 스레드로 변환해서 스레딩 시스템을
   초기화함. 이것은 일반적으로는 불가능하고, 이 경우에만
   가능한 이유는 loader.S가 스택의 바닥을
   페이지 경계에 맞추도록 신경 썼기 때문임.

   실행 큐와 tid 락도 함께 초기화함.

   이 함수를 호출한 뒤, thread_create()로 스레드를 만들기 전에
   반드시 페이지 할당기를
   초기화할 것.

   이 함수가 끝나기 전에는 thread_current()를 호출하는 것이
   안전하지 않음. */
void // 스레드 초기화 - 첫스레드 등록
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* 커널용 임시 gdt를 다시 로드함
	 * 이 gdt에는 유저 컨텍스트가 없음.
	 * 커널은 gdt_init ()에서 유저 컨텍스트가 있는 gdt를 다시 만듦. */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* 전역 스레드 컨텍스트를 초기화함 */
	// 전역 쓰레드 자료구조 초기화
	lock_init (&tid_lock);         // tid 할당 보호용 락
	list_init (&ready_list);       // 실행 대기용 스레드 큐
	list_init (&destruction_req);  // 종료됬지만 아직 메모리 해제 안된 스레드 목록
	/* sleep_list 초기화 */
	list_init(&sleep_list);

	/* 실행 중인 스레드용 스레드 구조체를 설정함. */
	initial_thread = running_thread (); // 현재 스레드 struct thread 주소 받아옴
	init_thread (initial_thread, "main", PRI_DEFAULT); 
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid (); // tid 부여(첫스레드라 1)
}

/* 현재 스레드를 sleep_list에 넣고 block상태로 바꿔줌 timer_sleep에서 사용됨 */
void
thread_sleep (int64_t wake_up_ticks)
{
	struct thread *t = thread_current();

	if(t==idle_thread)
		return;

	enum intr_level old = intr_disable();
	t->wake_tick = wake_up_ticks;
	list_insert_ordered(&sleep_list, &(t->elem), wake_tick_less, NULL);
	thread_block();
	intr_set_level(old);
}

/* wake_tick이 더 작은지 큰지 판단하는 함수 sleep_list에 넣을때 정렬해서 넣기 위해 필요함. */
bool 
wake_tick_less (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	struct thread *small_a = list_entry(a, struct thread, elem);
	struct thread *big_b = list_entry(b, struct thread, elem);

	/* wake_tick 비교 */
	if ( small_a->wake_tick < big_b->wake_tick )
		return true;
	else if ( small_a->wake_tick > big_b->wake_tick )
		return false;
	
	/* 우선순위 비교 */
	if ( small_a->priority > big_b->priority )
		return true;
	else if ( small_a->priority < big_b->priority )
		return false;
	/* 전부 같으면 false를 반환해야 순서가 보존되지 않음 */
	return false;
}

/* sleep_list에서 일어날 시간이된 스레드를 깨워주는 함수 - timer_interrupt에서 사용함 */
void
thread_wake_up(int64_t ticks)
{
	/* 맨앞 원소 */
	struct list_elem *e = list_begin(&sleep_list);
	/* sleep_list있는 스레드가 깨어날 시간이 모두 똑같을 경우를 생각해서 순회 */
	while(!list_empty(&sleep_list)){
		struct thread *t = list_entry(e, struct thread, elem);
		/* sleep_lis는 정렬되어있음으로 현재 tick 보다 wake_tick이 더크면 종료 */
		if( t->wake_tick > ticks ){
			break;
		}
		/* sleep_list에서 추출 */
		e = list_remove(e);
		thread_unblock(t);
	}
}

/* 인터럽트를 켜서 선점형 스레드 스케줄링을 시작함.
   idle 스레드도 만듦. */
void
thread_start (void) {
	/* idle 스레드를 만듦. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* 선점형 스레드 스케줄링을 시작함. */
	intr_enable ();

	/* idle 스레드가 idle_thread를 초기화할 때까지 기다림. */
	sema_down (&idle_started);
}

/* 타이머 인터럽트 핸들러가 타이머 틱마다 호출함.
   따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행됨. */
// 이번 tick 누가 썼는지 통계에 기록, 한 스레드가 너무 CPU오래쓰면 양보시킴
void
thread_tick (void) { 
	struct thread *t = thread_current (); // 실행중인 스레드 얻기

	/* 통계를 갱신함. */
	if (t == idle_thread) // idle thread 인지 확인
		idle_ticks++; // idle 틱 ++
#ifdef USERPROG
	else if (t->pml4 != NULL) // 유저프로그램 실행중
		user_ticks++;
#endif
	else
		kernel_ticks++; // 커널 스레드 실행중

	/* 선점을 강제함. */
	if (++thread_ticks >= TIME_SLICE) // thread_ticks는 마지막으로 스레드가 교체된 뒤 지난 tick수
		intr_yield_on_return (); // 핸들러 끝나면 양보
}

/* 스레드 통계를 출력함. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* 주어진 초기 PRIORITY로 NAME이라는 이름의 새 커널 스레드를 만듦.
   이 스레드는 AUX를 인자로 FUNCTION을 실행하고,
   준비 큐(ready queue)에 추가됨. 새 스레드의 스레드 식별자를 반환하고,
   생성에 실패하면 TID_ERROR를 반환함.

   thread_start()가 호출된 뒤라면 thread_create()가 반환되기 전에
   새 스레드가 스케줄될 수도 있음. 심지어 thread_create()가
   반환되기 전에 종료할 수도 있음. 반대로 원래
   스레드가 새 스레드가 스케줄되기 전까지 얼마든지 오래
   실행될 수도 있음. 순서를 보장해야 한다면 세마포어나 다른 형태의
   동기화를 쓸 것.

   제공된 코드는 새 스레드의 `priority` 멤버를
   PRIORITY로 설정하지만, 실제 우선순위 스케줄링은 구현되어 있지 않음.
   우선순위 스케줄링은 Problem 1-3의 목표임. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* 스레드를 할당함. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* 스레드를 초기화함. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* 스케줄되면 kernel_thread를 호출함.
	 * 참고) rdi는 첫 번째 인자이고, rsi는 두 번째 인자임. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* 실행 큐에 추가함. */
	thread_unblock (t);

	return tid;
}

/* 현재 스레드를 재움. thread_unblock()이 깨울 때까지 다시
   스케줄되지 않음.

   이 함수는 인터럽트를 끈 채로 호출해야 함. 보통은 synch.h의
   동기화 도구 중 하나를 쓰는 것이
   더 좋은 방법임. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* 블록된(blocked) 스레드 T를 실행 준비(ready-to-run) 상태로 바꿈.
   T가 블록된 상태가 아니면 오류임. (실행 중인 스레드를 ready로 만들려면
   thread_yield()를 쓸 것.)

   이 함수는 실행 중인 스레드를 선점하지 않음. 이것은
   중요할 수 있음. 호출한 쪽이 직접 인터럽트를 꺼 두었다면,
   스레드를 unblock하고 다른 데이터를 갱신하는 것을
   원자적으로 할 수 있다고 기대할 수 있기 때문임. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_push_back (&ready_list, &t->elem);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* 실행 중인 스레드의 이름을 반환함. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* 실행 중인 스레드를 반환함.
   running_thread()에 몇 가지 안전 검사를 더한 것임.
   자세한 내용은 thread.h 맨 위의 큰 주석을 볼 것. */
// 지금 CPU에서 실행중인 struct thread 포인터를 돌려주는 함수
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really ㅎa thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* 실행 중인 스레드의 tid를 반환함. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* 현재 스레드를 스케줄에서 내리고 제거함. 호출한 쪽으로
   절대 돌아가지 않음. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* 상태를 dying으로 바꾸고 다른 프로세스를 스케줄하기만 함.
	   schedule_tail()을 호출하는 동안 제거될 것임. */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* CPU를 양보함. 현재 스레드는 재워지지 않으며,
   스케줄러의 판단에 따라 곧바로 다시 스케줄될 수도 있음. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();
	if (curr != idle_thread)
		list_push_back (&ready_list, &curr->elem);
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* 현재 스레드의 우선순위를 NEW_PRIORITY로 설정함. */
void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority;
}

/* 현재 스레드의 우선순위를 반환함. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* 현재 스레드의 nice 값을 NICE로 설정함. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: 여기에 구현할 것 */
}

/* 현재 스레드의 nice 값을 반환함. */
int
thread_get_nice (void) {
	/* TODO: 여기에 구현할 것 */
	return 0;
}

/* 시스템 load average의 100배를 반환함. */
int
thread_get_load_avg (void) {
	/* TODO: 여기에 구현할 것 */
	return 0;
}

/* 현재 스레드의 recent_cpu 값의 100배를 반환함. */
int
thread_get_recent_cpu (void) {
	/* TODO: 여기에 구현할 것 */
	return 0;
}

/* idle 스레드. 실행할 준비가 된 다른 스레드가 없을 때 실행됨.

   idle 스레드는 처음에 thread_start()가 ready list에 넣어 둠.
   처음 한 번 스케줄되면 idle_thread를 초기화하고,
   전달받은 세마포어를 "up"해서
   thread_start()가 계속 진행하게 한 뒤 곧바로
   블록됨. 그 후로 idle 스레드는 ready list에 나타나지 않음.
   ready list가 비어 있을 때 특별한 경우로 next_thread_to_run()이
   반환함. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* 다른 스레드가 실행되게 양보함. */
		intr_disable ();
		thread_block ();

		/* 인터럽트를 다시 켜고 다음 인터럽트를 기다림.

		   `sti` 명령은 다음 명령이 끝날 때까지 인터럽트를 끄므로,
		   이 두 명령은
		   원자적으로 실행됨. 이 원자성은
		   중요함. 그렇지 않으면 인터럽트를 다시 켜는 것과 다음 인터럽트를
		   기다리는 것 사이에
		   인터럽트가 처리되어 클록 틱 한 번만큼의
		   시간이 낭비될 수 있음.

		   [IA32-v2a] "HLT", [IA32-v2b] "STI", [IA32-v3a]
		   7.11.1 "HLT Instruction"을 볼 것. */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* 커널 스레드의 기반으로 쓰는 함수. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* 스케줄러는 인터럽트를 끈 채로 실행됨. */
	function (aux);       /* 스레드 함수를 실행함. */
	thread_exit ();       /* function()이 반환하면 스레드를 종료시킴. */
}


/* T를 NAME이라는 이름의 블록된 스레드로
   기본 초기화함. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;
}

/* 다음에 스케줄할 스레드를 골라서 반환함. 실행 큐가 비어
   있지 않으면 실행 큐에서 스레드를 반환해야 함.
   (실행 중인 스레드가 계속 실행할 수 있다면
   그 스레드는 실행 큐에 있을 것임.) 실행 큐가 비어 있으면
   idle_thread를 반환함. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* iretq로 스레드를 시작함 */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* 새 스레드의 페이지 테이블을 활성화해서 스레드를 전환하고,
   이전 스레드가 dying 상태이면 제거함.

   이 함수가 호출된 시점에는 방금 스레드 PREV에서
   전환된 상태이고, 새 스레드가 이미 실행 중이며, 인터럽트는
   아직 꺼져 있음.

   스레드 전환이 끝나기 전에는 printf()를 호출하는 것이 안전하지
   않음. 실제로는 printf()를 쓰려면
   함수의 끝에 추가해야 한다는 뜻임. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* 핵심 전환 로직.
	 * 먼저 실행 컨텍스트 전체를 intr_frame에 복원한 뒤
	 * do_iret을 호출해서 다음 스레드로 전환함.
	 * 주의: 전환이 끝날 때까지 여기서부터는 스택을 쓰면
	 * 안 됨. */
	__asm __volatile (
			/* 사용할 레지스터를 저장함. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* 입력을 한 번에 가져옴 */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // 저장된 rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // 저장된 rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // 저장된 rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // 현재 rip를 읽음.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* 새 프로세스를 스케줄함. 진입할 때 인터럽트가 꺼져 있어야 함.
 * 이 함수는 현재 스레드의 상태를 status로 바꾼 뒤
 * 실행할 다른 스레드를 찾아 전환함.
 * schedule() 안에서는 printf()를 호출하는 것이 안전하지 않음. */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* 실행 중으로 표시함. */
	next->status = THREAD_RUNNING;

	/* 새 타임 슬라이스를 시작함. */
	thread_ticks = 0;

#ifdef USERPROG
	/* 새 주소 공간을 활성화함. */
	process_activate (next);
#endif

	if (curr != next) {
		/* 전환해 나온 스레드가 dying 상태이면 그 struct
		   thread를 제거함. thread_exit()가 자기 발밑을 빼 버리지 않도록
		   이 작업은 늦게 일어나야 함.
		   페이지가 지금 스택에서 쓰이고 있기 때문에
		   여기서는 페이지 해제 요청만 큐에 넣음.
		   실제 제거 로직은 schedule()의
		   시작 부분에서 호출됨. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* 스레드를 전환하기 전에, 먼저 현재 실행 중인 스레드의 정보를
		 * 저장함. */
		thread_launch (next);
	}
}

/* 새 스레드가 쓸 tid를 반환함. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}
