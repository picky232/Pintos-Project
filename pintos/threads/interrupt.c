#include "threads/interrupt.h"
#include <debug.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "threads/flags.h"
#include "threads/intr-stubs.h"
#include "threads/io.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/gdt.h"
#endif

/* x86_64 인터럽트 개수. */
#define INTR_CNT 256

/* FUNCTION을 호출하는 게이트(gate)를 만듦.

   게이트는 디스크립터 특권 레벨(DPL)을 가지는데, 프로세서가 DPL
   이하의 낮은 번호 링(ring)에 있을 때 의도적으로 호출할 수 있다는
   뜻임. 실제로는 DPL==3이면 유저 모드가 게이트를 호출할 수 있고,
   DPL==0이면 그런 호출을 막음. 유저 모드에서 발생하는 폴트와
   예외는 DPL==0인 게이트도 호출하게 만듦.


   TYPE은 14(인터럽트 게이트)나 15(트랩 게이트) 중 하나여야 함.
   차이는 인터럽트 게이트에 진입하면 인터럽트가 꺼지고, 트랩
   게이트에 진입하면 꺼지지 않는다는 점임. 자세한 내용은
   [IA32-v3a] 5.12.1.2절 "Flag Usage By Exception- or
   Interrupt-Handler Procedure"를 볼 것. */

struct gate {
	unsigned off_15_0 : 16;   // 세그먼트 안 오프셋의 하위 16비트
	unsigned ss : 16;         // 세그먼트 셀렉터
	unsigned ist : 3;        // 인자 개수, 인터럽트/트랩 게이트는 0
	unsigned rsv1 : 5;        // 예약됨 (0이어야 하는 듯함)
	unsigned type : 4;        // 타입 (STS_{TG,IG32,TG32})
	unsigned s : 1;           // 반드시 0 (시스템)
	unsigned dpl : 2;         // 디스크립터(새) 특권 레벨
	unsigned p : 1;           // Present(존재)
	unsigned off_31_16 : 16;  // 세그먼트 안 오프셋의 상위 비트
	uint32_t off_32_63;
	uint32_t rsv2;
};

/* 인터럽트 디스크립터 테이블(IDT). 형식은 CPU가 정해 둠.
   [IA32-v3a] 5.10절 "Interrupt Descriptor
   Table (IDT)", 5.11절 "IDT Descriptors", 5.12.1.2절 "Flag Usage By
   Exception- or Interrupt-Handler Procedure"를 볼 것. */
static struct gate idt[INTR_CNT];

static struct desc_ptr idt_desc = {
	.size = sizeof(idt) - 1,
	.address = (uint64_t) idt
};


#define make_gate(g, function, d, t) \
{ \
	ASSERT ((function) != NULL); \
	ASSERT ((d) >= 0 && (d) <= 3); \
	ASSERT ((t) >= 0 && (t) <= 15); \
	*(g) = (struct gate) { \
		.off_15_0 = (uint64_t) (function) & 0xffff, \
		.ss = SEL_KCSEG, \
		.ist = 0, \
		.rsv1 = 0, \
		.type = (t), \
		.s = 0, \
		.dpl = (d), \
		.p = 1, \
		.off_31_16 = ((uint64_t) (function) >> 16) & 0xffff, \
		.off_32_63 = ((uint64_t) (function) >> 32) & 0xffffffff, \
		.rsv2 = 0, \
	}; \
}

/* 주어진 DPL로 FUNCTION을 호출하는 인터럽트 게이트를 만듦. */
#define make_intr_gate(g, function, dpl) make_gate((g), (function), (dpl), 14)

/* 주어진 DPL로 FUNCTION을 호출하는 트랩 게이트를 만듦. */
#define make_trap_gate(g, function, dpl) make_gate((g), (function), (dpl), 15)



/* 각 인터럽트의 핸들러 함수. */
static intr_handler_func *intr_handlers[INTR_CNT];

/* 디버깅용 각 인터럽트의 이름. */
static const char *intr_names[INTR_CNT];

/* 외부 인터럽트는 타이머처럼 CPU 바깥의 장치가 만드는
   인터럽트임. 외부 인터럽트는 인터럽트가 꺼진 채로 실행되므로
   중첩되지 않고 선점되지도 않음. 외부 인터럽트 핸들러는
   잠들 수도 없지만, intr_yield_on_return()을 불러서
   인터럽트가 반환되기 직전에 새 프로세스를
   스케줄하도록 요청할 수는
   있음. */
static bool in_external_intr;   /* 지금 외부 인터럽트를 처리 중인가? */
static bool yield_on_return;    /* 인터럽트에서 돌아갈 때 양보해야 하는가? */

/* 프로그래머블 인터럽트 컨트롤러(PIC) 도우미 함수들. */
static void pic_init (void);
static void pic_end_of_interrupt (int irq);

/* 인터럽트 핸들러들. */
void intr_handler (struct intr_frame *args);

/* 현재 인터럽트 상태를 반환함. */
// CPU가 인터럽트를 받는 상태인지 확인
enum intr_level
intr_get_level (void) {
	uint64_t flags;

	/* 플래그 레지스터를 프로세서 스택에 push한 뒤,
	   그 값을 스택에서 pop해서 `flags`에 넣음. [IA32-v2b] "PUSHF"
	   와 "POP", [IA32-v3a] 5.8.1 "Masking Maskable Hardware
	   Interrupts"를 볼 것. */
	asm volatile ("pushfq; popq %0" : "=g" (flags));

	return flags & FLAG_IF ? INTR_ON : INTR_OFF;
}

/* LEVEL로 지정한 대로 인터럽트를 켜거나 끄고,
   이전 인터럽트 상태를 반환함. */
// 인터럽트 상태에 따라서 키거나 끔
enum intr_level
intr_set_level (enum intr_level level) {
	return level == INTR_ON ? intr_enable () : intr_disable (); 
}

/* 인터럽트를 켜고 이전 인터럽트 상태를 반환함. */
enum intr_level
intr_enable (void) {
	enum intr_level old_level = intr_get_level ();
	ASSERT (!intr_context ());

	/* 인터럽트 플래그를 설정해서 인터럽트를 켬.

	   [IA32-v2b] "STI"와 [IA32-v3a] 5.8.1 "Masking Maskable
	   Hardware Interrupts"를 볼 것. */
	asm volatile ("sti"); // 여기서 킴

	return old_level;
}

/* 인터럽트를 끄고 이전 인터럽트 상태를 반환함. */
enum intr_level
intr_disable (void) { // 인터럽트를 끄고, 끄기 직전의 상태를 반환함
	enum intr_level old_level = intr_get_level (); // 현재 상태 저장

	/* 인터럽트 플래그를 지워서 인터럽트를 끔.
	   [IA32-v2b] "CLI"와 [IA32-v3a] 5.8.1 "Masking Maskable
	   Hardware Interrupts"를 볼 것. */
	asm volatile ("cli" : : : "memory"); // 인터럽트 끄기
 
	return old_level; // 이전 상태 반환
}

/* 인터럽트 시스템을 초기화함. */
void
intr_init (void) {
	int i;

	/* 인터럽트 컨트롤러를 초기화함. */
	pic_init ();

	/* IDT를 초기화함. */
	for (i = 0; i < INTR_CNT; i++) {
		make_intr_gate(&idt[i], intr_stubs[i], 0);
		intr_names[i] = "unknown";
	}

#ifdef USERPROG
	/* TSS를 로드함. */
	ltr (SEL_TSS);
#endif

	/* IDT 레지스터를 로드함. */
	lidt(&idt_desc);

	/* intr_names를 초기화함. */
	intr_names[0] = "#DE Divide Error";
	intr_names[1] = "#DB Debug Exception";
	intr_names[2] = "NMI Interrupt";
	intr_names[3] = "#BP Breakpoint Exception";
	intr_names[4] = "#OF Overflow Exception";
	intr_names[5] = "#BR BOUND Range Exceeded Exception";
	intr_names[6] = "#UD Invalid Opcode Exception";
	intr_names[7] = "#NM Device Not Available Exception";
	intr_names[8] = "#DF Double Fault Exception";
	intr_names[9] = "Coprocessor Segment Overrun";
	intr_names[10] = "#TS Invalid TSS Exception";
	intr_names[11] = "#NP Segment Not Present";
	intr_names[12] = "#SS Stack Fault Exception";
	intr_names[13] = "#GP General Protection Exception";
	intr_names[14] = "#PF Page-Fault Exception";
	intr_names[16] = "#MF x87 FPU Floating-Point Error";
	intr_names[17] = "#AC Alignment Check Exception";
	intr_names[18] = "#MC Machine-Check Exception";
	intr_names[19] = "#XF SIMD Floating-Point Exception";
}

/* 인터럽트 VEC_NO가 디스크립터 특권 레벨 DPL로 HANDLER를
   호출하도록 등록함. 디버깅용으로 인터럽트 이름을 NAME으로
   정함. 인터럽트 핸들러는 인터럽트 상태가 LEVEL로
   설정된 채 호출됨. */
static void
register_handler (uint8_t vec_no, int dpl, enum intr_level level,
		intr_handler_func *handler, const char *name) {
	ASSERT (intr_handlers[vec_no] == NULL);
	if (level == INTR_ON) {
		make_trap_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
	}
	else {
		make_intr_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
	}
	intr_handlers[vec_no] = handler;
	intr_names[vec_no] = name;
}

/* 외부 인터럽트 VEC_NO가 HANDLER를 호출하도록 등록함.
   HANDLER의 이름은 디버깅용으로 NAME임. 핸들러는
   인터럽트가 꺼진 채 실행됨. */
void
intr_register_ext (uint8_t vec_no, intr_handler_func *handler,
		const char *name) {
	ASSERT (vec_no >= 0x20 && vec_no <= 0x2f);
	register_handler (vec_no, 0, INTR_OFF, handler, name);
}

/* 내부 인터럽트 VEC_NO가 HANDLER를 호출하도록 등록함.
   HANDLER의 이름은 디버깅용으로 NAME임. 인터럽트 핸들러는
   인터럽트 상태 LEVEL로 호출됨.

   핸들러는 디스크립터 특권 레벨 DPL을 가지는데, 프로세서가
   DPL 이하의 낮은 번호 링에 있을 때 의도적으로 호출할 수
   있다는 뜻임. 실제로는 DPL==3이면 유저 모드가 인터럽트를
   호출할 수 있고, DPL==0이면 그런 호출을 막음. 유저 모드에서
   발생하는 폴트와 예외는 DPL==0인 인터럽트도 호출하게
   만듦. 더 자세한 내용은
   [IA32-v3a] 4.5절 "Privilege Levels"와
   4.8.1.1절 "Accessing Nonconforming Code Segments"를
   볼 것. */
void
intr_register_int (uint8_t vec_no, int dpl, enum intr_level level,
		intr_handler_func *handler, const char *name)
{
	ASSERT (vec_no < 0x20 || vec_no > 0x2f);
	register_handler (vec_no, dpl, level, handler, name);
}

/* 외부 인터럽트를 처리하는 동안에는 true를,
   그 밖의 모든 때에는 false를 반환함. */
bool
intr_context (void) {
	return in_external_intr;
}

/* 외부 인터럽트를 처리하는 동안, 인터럽트 핸들러가 인터럽트에서
   돌아가기 직전에 새 프로세스에게 양보하도록 지시함.
   그 밖의 어떤 때에도
   호출하면 안 됨. */
void
intr_yield_on_return (void) {
	ASSERT (intr_context ());
	yield_on_return = true;
}

/* 8259A 프로그래머블 인터럽트 컨트롤러(PIC). */

/* 모든 PC에는 8259A 프로그래머블 인터럽트 컨트롤러(PIC) 칩이 두 개 있음.
   하나는 포트 0x20과 0x21로 접근하는 "마스터(master)"임.
   다른 하나는 마스터의 IRQ 2 라인에 연결(cascade)된 "슬레이브(slave)"로
   포트 0xa0과 0xa1로 접근함. 포트 0x20에 접근하면 A0 라인이 0이 되고,
   0x21에 접근하면 A1 라인이 1이 됨.
   슬레이브 PIC도 마찬가지임.

   기본적으로 PIC가 전달하는 인터럽트 0...15는 인터럽트 벡터
   0...15로 감. 그런데 그 벡터들은 CPU 트랩과 예외에도
   쓰임. 그래서 PIC를 다시 프로그래밍해서 인터럽트 0...15가
   대신 인터럽트 벡터 32...47(0x20...0x2f)로
   전달되게 함. */

/* PIC를 초기화함. 자세한 내용은 [8259A]를 볼 것. */
static void
pic_init (void) {
	/* 두 PIC의 모든 인터럽트를 마스크(차단)함. */
	outb (0x21, 0xff);
	outb (0xa1, 0xff);

	/* 마스터를 초기화함. */
	outb (0x20, 0x11); /* ICW1: 단일 모드, 에지 트리거, ICW4를 기대함. */
	outb (0x21, 0x20); /* ICW2: 라인 IR0...7 -> irq 0x20...0x27. */
	outb (0x21, 0x04); /* ICW3: 슬레이브 PIC가 IR2 라인에 있음. */
	outb (0x21, 0x01); /* ICW4: 8086 모드, 일반 EOI, 버퍼 없음. */

	/* 슬레이브를 초기화함. */
	outb (0xa0, 0x11); /* ICW1: 단일 모드, 에지 트리거, ICW4를 기대함. */
	outb (0xa1, 0x28); /* ICW2: 라인 IR0...7 -> irq 0x28...0x2f. */
	outb (0xa1, 0x02); /* ICW3: 슬레이브 ID는 2. */
	outb (0xa1, 0x01); /* ICW4: 8086 모드, 일반 EOI, 버퍼 없음. */

	/* 모든 인터럽트의 마스크를 해제함. */
	outb (0x21, 0x00);
	outb (0xa1, 0x00);
}

/* 주어진 IRQ에 대해 PIC에 EOI(end-of-interrupt) 신호를 보냄.
   IRQ를 확인(acknowledge)해 주지 않으면 다시는 전달되지 않으므로
   이것은 중요함.  */
static void
pic_end_of_interrupt (int irq) {
	ASSERT (irq >= 0x20 && irq < 0x30);

	/* 마스터 PIC에 확인(acknowledge)을 보냄. */
	outb (0x20, 0x20);

	/* 슬레이브 인터럽트이면 슬레이브 PIC에도 확인을 보냄. */
	if (irq >= 0x28)
		outb (0xa0, 0x20);
}
/* 인터럽트 핸들러들. */

/* 모든 인터럽트, 폴트, 예외를 처리하는 핸들러. 이 함수는
   intr-stubs.S의 어셈블리 인터럽트 스텁이 호출함. FRAME은
   인터럽트와 인터럽트당한 스레드의 레지스터를
   설명함. */
void
intr_handler (struct intr_frame *frame) {
	bool external;
	intr_handler_func *handler;

	/* 외부 인터럽트는 특별함.
	   한 번에 하나만 처리하고(그래서 인터럽트가 꺼져 있어야 함)
	   PIC에 확인을 보내야 함(아래 참고).
	   외부 인터럽트 핸들러는 잠들 수 없음. */
	external = frame->vec_no >= 0x20 && frame->vec_no < 0x30;
	if (external) {
		ASSERT (intr_get_level () == INTR_OFF);
		ASSERT (!intr_context ());

		in_external_intr = true;
		yield_on_return = false;
	}

	/* 인터럽트의 핸들러를 호출함. */
	handler = intr_handlers[frame->vec_no];
	if (handler != NULL)
		handler (frame);
	else if (frame->vec_no == 0x27 || frame->vec_no == 0x2f) {
		/* 핸들러는 없지만, 이 인터럽트는 하드웨어 결함이나
		   하드웨어 경쟁 상태(race condition) 때문에 가짜로(spurious)
		   발생할 수 있음. 무시함. */
	} else {
		/* 핸들러도 없고 가짜 인터럽트도 아님. 예상치 못한
		   인터럽트 핸들러를 호출함. */
		intr_dump_frame (frame);
		PANIC ("Unexpected interrupt");
	}

	/* 외부 인터럽트 처리를 마무리함. */
	if (external) {
		ASSERT (intr_get_level () == INTR_OFF);
		ASSERT (intr_context ());

		in_external_intr = false;
		pic_end_of_interrupt (frame->vec_no);

		if (yield_on_return)
			thread_yield ();
	}
}

/* 디버깅용으로 인터럽트 프레임 F를 콘솔에 덤프함. */
void
intr_dump_frame (const struct intr_frame *f) {
	/* CR2는 마지막 페이지 폴트의 선형 주소임.
	   [IA32-v2a] "MOV--Move to/from Control Registers"와
	   [IA32-v3a] 5.14 "Interrupt 14--Page Fault Exception
	   (#PF)"를 볼 것. */
	uint64_t cr2 = rcr2();
	printf ("Interrupt %#04llx (%s) at rip=%llx\n",
			f->vec_no, intr_names[f->vec_no], f->rip);
	printf (" cr2=%016llx error=%16llx\n", cr2, f->error_code);
	printf ("rax %016llx rbx %016llx rcx %016llx rdx %016llx\n",
			f->R.rax, f->R.rbx, f->R.rcx, f->R.rdx);
	printf ("rsp %016llx rbp %016llx rsi %016llx rdi %016llx\n",
			f->rsp, f->R.rbp, f->R.rsi, f->R.rdi);
	printf ("rip %016llx r8 %016llx  r9 %016llx r10 %016llx\n",
			f->rip, f->R.r8, f->R.r9, f->R.r10);
	printf ("r11 %016llx r12 %016llx r13 %016llx r14 %016llx\n",
			f->R.r11, f->R.r12, f->R.r13, f->R.r14);
	printf ("r15 %016llx rflags %08llx\n", f->R.r15, f->eflags);
	printf ("es: %04x ds: %04x cs: %04x ss: %04x\n",
			f->es, f->ds, f->cs, f->ss);
}

/* 인터럽트 VEC의 이름을 반환함. */
const char *
intr_name (uint8_t vec) {
	return intr_names[vec];
}
