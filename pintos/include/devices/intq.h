#ifndef DEVICES_INTQ_H
#define DEVICES_INTQ_H

#include "threads/interrupt.h"
#include "threads/synch.h"

/* "인터럽트 큐(interrupt queue)". 커널 스레드와 외부 인터럽트
   핸들러가 함께 쓰는 원형 버퍼.

   인터럽트 큐 함수는 커널 스레드에서도, 외부 인터럽트 핸들러
   에서도 호출할 수 있음. intq_init()을 빼면 어느 경우든
   인터럽트가 꺼져 있어야 함.

   인터럽트 큐는 "모니터" 구조임. threads/synch.h의 락과
   컨디션 변수는 평소처럼 쓸 수 없음. 그것들은
   커널 스레드끼리만 보호할 수 있고,
   인터럽트 핸들러로부터는 보호하지 못하기
   때문임. */

/* 큐 버퍼 크기 (바이트 단위). */
#define INTQ_BUFSIZE 64

/* 바이트를 담는 원형 큐. */
struct intq {
	/* 기다리는 스레드. */
	struct lock lock;           /* 한 번에 한 스레드만 기다릴 수 있음. */
	struct thread *not_full;    /* "큐가 가득 차지 않음" 조건을 기다리는 스레드. */
	struct thread *not_empty;   /* "큐가 비어 있지 않음" 조건을 기다리는 스레드. */

	/* 큐. */
	uint8_t buf[INTQ_BUFSIZE];  /* 버퍼. */
	int head;                   /* 새 데이터를 여기에 씀. */
	int tail;                   /* 오래된 데이터를 여기서 읽음. */
};

void intq_init (struct intq *);
bool intq_empty (const struct intq *);
bool intq_full (const struct intq *);
uint8_t intq_getc (struct intq *);
void intq_putc (struct intq *, uint8_t);

#endif /* devices/intq.h */
