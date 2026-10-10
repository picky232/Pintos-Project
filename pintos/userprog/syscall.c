#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* 시스템 콜.
 *
 * 예전에는 시스템 콜 서비스를 인터럽트 핸들러가 처리했음
 * (예: 리눅스의 int 0x80). 하지만 x86-64에서는 제조사가 시스템 콜을
 * 요청하는 효율적인 경로인 `syscall` 명령어를 제공함.
 *
 * syscall 명령어는 모델 특정 레지스터(Model Specific Register, MSR)에서
 * 값을 읽어 동작함. 자세한 내용은 매뉴얼을 볼 것. */

#define MSR_STAR 0xc0000081         /* 세그먼트 셀렉터 msr */
#define MSR_LSTAR 0xc0000082        /* 롱 모드 SYSCALL 대상 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크 */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* 인터럽트 서비스 루틴은 syscall_entry가 유저랜드 스택을
	 * 커널 모드 스택으로 바꿀 때까지 어떤 인터럽트도
	 * 처리하면 안 됨. 그래서 FLAG_FL을 마스크함. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* 시스템 콜의 주 인터페이스 */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: 여기에 구현할 것.
	printf ("system call!\n");
	thread_exit ();
}
