#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* 8254 타이머 칩의 하드웨어 세부 사항은 [8254]를 볼 것. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* OS가 부팅한 뒤의 타이머 틱 수. */
static int64_t ticks; // 부팅후 지난 틱 수

/* 타이머 틱당 루프 횟수.
   timer_calibrate()가 초기화함. */
static unsigned loops_per_tick; 

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* 8254 프로그래머블 인터벌 타이머(PIT)가 초당 PIT_FREQ번
   인터럽트를 내도록 설정하고, 그에 해당하는
   인터럽트를 등록함. */
void
timer_init (void) {
	/* 8254 입력 주파수를 TIMER_FREQ로 나누고 가장 가까운
	   값으로 반올림함. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ; // 인터럽트 1번당 입력 클럭 수

	outb (0x43, 0x34);    /* CW: 카운터 0, LSB 다음 MSB, 모드 2, 이진수. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer"); // 인터럽트 핸들러 등록
}

/* 짧은 지연을 구현하는 데 쓰는 loops_per_tick을 보정함. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* loops_per_tick을 한 타이머 틱보다 작은 가장 큰
	   2의 거듭제곱으로 근사함. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* loops_per_tick의 다음 8비트를 다듬음. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* OS가 부팅한 뒤의 타이머 틱 수를 반환함. */
int64_t // 부팅 이후 지난 틱
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level); 
	barrier (); 
	return t;
}

/* THEN 이후 경과한 타이머 틱 수를 반환함. THEN은
   한때 timer_ticks()가 반환했던 값이어야 함. */
// 경과시간 계산
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* 약 TICKS 타이머 틱 동안 실행을 멈춤. */
void // 몇 틱 잘껀지
timer_sleep (int64_t sleep_ticks) {
	int64_t start = timer_ticks ();
	
	ASSERT (intr_get_level () == INTR_ON);
	
	if(sleep_ticks<=0) return;
	thread_sleep(start+sleep_ticks);
}

/* 약 MS 밀리초 동안 실행을 멈춤. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* 약 US 마이크로초 동안 실행을 멈춤. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* 약 NS 나노초 동안 실행을 멈춤. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* 타이머 통계를 출력함. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* 타이머 인터럽트 핸들러. */
// 10ms 마다 하드웨어가 자동호출
// 여기서 깨워야함
static void 
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	thread_wake_up(ticks);
	thread_tick (); 
}

/* LOOPS번 반복하는 데 한 타이머 틱보다 오래 걸리면 true를,
   아니면 false를 반환함. */
static bool
too_many_loops (unsigned loops) {
	/* 타이머 틱을 기다림. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* LOOPS번 루프를 돎. */
	start = ticks;
	busy_wait (loops);

	/* 틱 수가 바뀌었으면 너무 오래 반복한 것임. */
	barrier ();
	return start != ticks;
}

/* 짧은 지연을 구현하려고 간단한 루프를
   LOOPS번 반복함.

   NO_INLINE으로 표시한 이유는 코드 정렬이 타이밍에 크게
   영향을 줄 수 있어서, 이 함수가 곳곳에서 서로 다르게
   인라인되면 결과를 예측하기
   어려워지기 때문임. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* 약 NUM/DENOM초 동안 잠듦. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* NUM/DENOM초를 타이머 틱으로 환산하며, 내림함.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* 최소 한 번의 온전한 타이머 틱을 기다려야 함. timer_sleep()을
		   쓰는 이유는 CPU를 다른 프로세스에게
		   양보하기 때문임. */
		timer_sleep (ticks);
	} else {
		/* 그렇지 않으면 더 정확한 틱 미만 타이밍을 위해 바쁜 대기(busy-wait)
		   루프를 씀. 오버플로가 일어나지 않도록 분자와 분모를
		   1000으로 나눠 줄임. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
