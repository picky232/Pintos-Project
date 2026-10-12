# 2026-10-10 · Jongeume

> 팀 리포 경로 : `docs/2026-10-10/TIL/Jongeume.md`

## 한 줄 요약

> **Alarm Clock = 잠들 스레드를 BLOCKED 로 빼 두고, 타이머 인터럽트가 시간이 된 스레드를 READY 로 돌려놓는 것.** 기다리는 동안 CPU 를 쓰지 않는다.

## 오늘 목표 · 관련 이슈

- 목표 : Alarm Clock 을 구현하기 전에 잠들기 · 깨우기의 동작 원리를 이해한다 (busy waiting 이 왜 문제인지, 누가 깨우는지, 왜 인터럽트를 잠깐 끄는지)
- 이슈 : #9 ([논의] alarm - 추상화 함수 파악) · #10 ([논의] alarm - 문제와 함수 엮기) · #6 (TIL 공유)
- 브랜치 : `feature-alarm-clock-jongeume` (오늘 만듦)

## 배운 것 — 잠들기와 깨우기

### 맨 앞 요약

> [!NOTE]
> **30초 요약**
> **Alarm Clock은 스레드를 원하는 시간 동안 쉬게 하고, 시간이 되면 다시 실행할 수 있게 만드는 기능이다.**
> 잠든 스레드는 실행 준비 줄에서 빠지고, 타이머 인터럽트가 시간이 된 스레드를 다시 줄에 넣는다.
> **RUNNING → BLOCKED → READY → RUNNING** (실행 중 → 잠듦 → 실행 준비 → 실제 실행)

- **무엇을 고치나?** `devices/timer.c`의 `timer_sleep()`
- **왜 고치나?** 기본 코드는 시간을 반복 확인하는 busy waiting을 사용한다
- **어떻게 바꾸나?** 깨울 시각을 기록하고 스레드를 재운 뒤, 타이머 인터럽트에서 깨운다
- **꼭 구분할 것** : CPU를 양보하는 `yield`와, 기다리는 조건이 풀릴 때까지 실행 대상에서 빠지는 `block`
- **깨어나기 ≠ 즉시 실행** : READY가 된 뒤 스케줄러에게 선택되어야 실행한다

- 공식 요구사항 : 일정한 tick 수가 지난 스레드를 ready queue에 넣는 것 [공식 Alarm Clock 설명](https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html)
	- busy waiting을 없애는 것이 이번 과제의 핵심이다

```text
tick 100      A 실행 중
   │ ① A 가 timer_sleep(5) → wakeup_tick 105 기록 → sleep_list 에 걸고 thread_block()
   ▼
tick 101~104  A : BLOCKED (sleep_list)   CPU : B 실행
   │          매 tick 처리기가 B 를 잠깐 멈추고 확인 → 105 ≤ 101 ? 아니오 …
   ▼
tick 105      ② 처리기 : 105 ≤ 105 → sleep_list 에서 빼 thread_unblock(A)
   │          A : READY (ready_list)   CPU : 아직 B
   ▼
나중          ③ schedule() 이 A 를 고름 → 문맥 복원
              A : RUNNING → timer_sleep() 반환, 다음 문장으로
```

- 이 글의 흐름
	1. 교실 비유와 tick 단위
	2. 왜 필요한가 (busy waiting)
	3. 잠드는 절반
	4. 깨우는 절반
	5. 여러 스레드, 인터럽트 끄기
	6. 추가할 것, 코드 순서

- 알람 대기 목록(`sleep_list`)과 깨울 시각(`wakeup_tick`)은 **우리가 추가할 설계 예시**다

---

### 1. 교실의 공용 계산기로 생각해 보자

> [!NOTE]
> **요약**
> CPU 하나에서 여러 스레드가 번갈아 실행되는 상황을 교실 하나로 그려 본다.
> **준비 줄은 지금 실행해도 되는 작업의 줄**, 쉬는 의자는 아직 실행하면 안 되는 작업을 두는 곳이다.

> [!TIP]
> **비유**
> 계산기가 하나뿐인 교실에서 아이들이 돌아가며 계산한다.
> 시간이 될 때까지 쉬어야 하는 아이는 줄이 아니라 의자에 앉는다.
> 종이 울릴 때마다 선생님이 의자의 아이 중 때가 된 아이를 줄로 보낸다.

| 교실 비유 | 컴퓨터에서의 의미 |
|---|---|
| 계산기 | CPU |
| 계산기를 사용하는 아이 | 실행 중인 스레드 |
| 계산기 앞의 줄 | ready queue: 실행 준비 줄 |
| 쉬는 의자 | 시간이 되기를 기다리는 스레드 목록 |
| 정기적으로 울리는 종 | 타이머 인터럽트 |
| 다음 아이를 고르는 규칙 | 스케줄러 |

```text
[계산기 = CPU]  A 사용 중 (RUNNING)
   │ ① 잠들기 : A 는 줄이 아니라 의자로 (BLOCKED)
   ▼
[쉬는 의자 = sleep_list]  A·105, D·110
   │ ② 종(타이머 인터럽트)이 울릴 때마다 의자를 본다
   │ ③ 때가 된 아이만 줄 끝으로 (READY)  — D·110 은 그대로
   ▼
[계산기 앞의 줄 = ready_list]  B, C, A
   │ ④ 선생님(스케줄러)이 줄 맨 앞을 부른다 (RUNNING)
   ▼
[계산기 = CPU]

선생님은 줄에서만 고른다 → 의자의 아이는 종이 줄로 보내 줘야 차례가 온다
```

---

### 2. Alarm Clock은 무엇인가?

> [!NOTE]
> **요약**
> `timer_sleep(5)` = "타이머 눈금이 5칸 지나기 전까지 내 작업을 멈춰 줘". **5초가 아니다.**
> 기본 설정에서 1초 = 100 ticks, 1 tick ≈ 10ms다.

- tick : 운영체제의 시간 눈금 [기본 timer.h](https://github.com/casys-kaist/pintos-kaist/blob/master/include/devices/timer.h)
	- KAIST 기본 설정은 `TIMER_FREQ = 100`
	- 1초에 100 ticks → 한 tick 간격은 1000ms ÷ 100 = 약 10ms
	- 100 ticks는 대략 1초에 해당한다
	- 호출 순간은 눈금 사이일 수도 있으므로, 정확한 현실 시간과 완전히 같은 것으로 생각하지는 말자

```text
tick 100 : A 가 timer_sleep(5) → wakeup_tick = 100 + 5 = 105 → BLOCKED
tick 101 : 타이머 인터럽트 → ticks++ → 105 ≤ 101 ? 아니오
tick 102 : 타이머 인터럽트 → ticks++ → 105 ≤ 102 ? 아니오
tick 103 : 타이머 인터럽트 → ticks++ → 105 ≤ 103 ? 아니오
tick 104 : 타이머 인터럽트 → ticks++ → 105 ≤ 104 ? 아니오
tick 105 : 타이머 인터럽트 → ticks++ → 105 ≤ 105 ? 예 → A : READY

5 = 기다릴 눈금의 개수 (5 초가 아니다)
TIMER_FREQ 100 → 1 tick = 1000ms ÷ 100 ≈ 10ms → 5 tick ≈ 50ms
```

- 쓰임 : 일정한 간격으로 일을 해야 할 때 유용하다
	- 예 : 커서를 주기적으로 깜빡이게 하는 작업
	- 새 작업을 만드는 게 아니라, 기존 스레드의 작업을 잠깐 멈췄다가 이어 가는 기능이다

> [!IMPORTANT]
> **105에 바로 실행한다는 뜻은 아니다**
> 105가 되면 A를 실행 준비 줄에 넣는다.
> 실제 실행 시점은 스케줄러가 결정하므로 더 늦게 실행될 수도 있다. [공식 요구사항](https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html)

- tick 한 칸, 현재 누적 ticks, 기다릴 tick 수는 서로 다른 값이다 (예 : 1 tick · 100 · `timer_sleep(5)` 의 5)

---

### 3. 이제 Alarm Clock이 필요한 이유

> [!NOTE]
> **요약**
> 기본 `timer_sleep()`은 시간을 확인하고 `thread_yield()`를 부르는 일을 반복한다. 이것이 **busy waiting**이다.
> 목표는 **A를 BLOCKED로 빼 두고, 시간이 되었을 때 타이머 처리 쪽에서 READY로 만드는 것**이다.

> [!TIP]
> **비유**
> 지금 = 계산할 일이 없는데도 줄에 다시 서서 "이제 시간이 됐나?"를 묻는 아이.
> 바꾼 뒤 = 알람을 맞추고 쉬는 의자에 앉는다. 시계(타이머)가 때가 된 아이만 깨운다.

아래는 흐름 설명용 코드다.

```c
void task_A (void *aux) {
    /* 계산 작업 */
    timer_sleep (5);
    /* 기다린 뒤 이어서 할 작업 */
}
```

- Project 1에서 `timer_sleep()`은 커널 함수의 **일반 함수 호출**이다
	- 현재 커널 스레드에서 실행한다
	- 이 호출 자체로 별도의 알람 스레드가 생기지 않는다
	- 사용자 프로그램의 시스템 콜도 아니다

- 기본 구현의 반복 [기본 timer.c](https://github.com/casys-kaist/pintos-kaist/blob/master/devices/timer.c)
	1. 시작 tick을 기억한다
	2. 기다릴 시간이 남았는지 확인한다
	3. 남았으면 `thread_yield()`
	4. 다시 선택되면 2로 돌아간다

```text
기본 timer_sleep() — 시간이 될 때까지 되풀이
  ① 시간 확인 : timer_elapsed(start) < 5 ?
     ├─ 지났다 → 반환, 다음 문장으로
     └─ 아직  → ② thread_yield() : RUNNING → READY
                ③ ready_list 뒤에 선다, 다른 스레드가 실행
                ④ 다시 선택된다 → ① 로

CPU 를 어디에 쓰나 (가로 = 시간)
  기본 : [A?][   B   ][A?][   C   ][A?][   B   ][A?][ A ]
  개선 : [A ][      B      ][      C      ][      B      ][ A ]

  A? = 차례를 받아 시간만 보고 다시 양보한 칸 → 이 CPU 시간이 낭비 (busy waiting)
  개선 = A 는 처음에 BLOCKED, 105 에 타이머가 READY 로 → 그 사이를 B · C 가 쓴다
```

- 왜 낭비인가
	- A는 준비 줄에 계속 다시 들어가므로, 자기 차례를 얻을 때마다 시간을 확인한다
	- 다른 스레드가 실행될 수는 있지만, A의 불필요한 확인에도 CPU 시간이 쓰인다

- "불필요"의 뜻 : **"내 차례니까 시간을 볼 필요가 없다"가 아니다**
	- 내 차례를 얻는 사건과 요청한 5 ticks가 지나는 사건은 서로 다르다
	- 지금 방식에서는 필요한 확인이다
	- A를 BLOCKED로 두고 타이머가 깨워 주게 바꾸면, A가 반복 확인할 필요가 없어진다

- **타이머 ≠ Alarm Clock**
	- 실행 몫을 확인하는 타이머 기능 : 기본 코드에도 있다
	- 잠든 스레드를 시간이 되면 깨우는 Alarm Clock : 여기에 추가할 기능이다

- 구현 목표 [Alarm Clock 공식 요구사항](https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html)
	- A가 시간을 기다리는 동안 BLOCKED가 되어 CPU 실행 대상에서 빠진다
	- 타이머 처리 쪽이 시간이 된 A를 READY로 만든다
	- `timer_sleep()`의 busy waiting을 없앤다
	- 기다릴 단위는 초가 아니라 **tick 수**다

---

### 4. CPU에서 무슨 일이 벌어지나? — 잠드는 절반

> [!NOTE]
> **요약**
> A는 깨울 시각을 적고 대기 목록에 매달린 뒤 BLOCKED가 된다.
> CPU는 B로 넘어간다. A는 `timer_sleep()`을 끝내지 못한 채 **CPU를 쓰지 않고** 기다린다.

```text
현재 tick 100, A 가 CPU 에서 실행 중

[CPU] A 실행
   │ ② A 가 timer_sleep(5) 호출
   ▼
[struct thread A]
   ③ wakeup_tick = 100 + 5 = 105
   ④ elem 을 sleep_list 에 등록 ──▶ sleep_list : [A·105]
   ⑤ thread_block() : RUNNING → BLOCKED
   │
   │ ready_list : [B] [C]   ← A 는 여기 없다 → 스케줄러가 A 를 고를 수 없다
   │ ⑥ schedule() 이 B 를 고르고 문맥 교환
   ▼
[CPU] ⑦ B 실행 중

A 의 커널 스택 — 그대로 남는다
   task_A
   timer_sleep(5)
   thread_block()
   schedule()        ← 여기서 멈춤 (나중에 여기서 이어 간다)
```

- 가정 : 현재 tick이 100이고, A가 CPU에서 실행 중
	- 아래 순서는 **개선할 설계의 예시**다

1. **CPU는 A의 코드를 실행한다.** A의 계산 값과 스택 위치를 사용하고 있다.
2. **A가 `timer_sleep(5)`를 호출한다.** CPU는 같은 A의 실행 흐름에서 그 함수 안의 명령을 실행한다.
3. **커널 코드가 깨울 시각 105를 기록한다.** 현재 100 + 기다릴 5이다.
4. **A를 잠든 스레드 목록에 등록한다.** 등록과 block 사이에 타이머가 끼어들지 않도록 짧은 구간을 보호한다.
5. **`thread_block()`으로 A를 BLOCKED로 만들고 스케줄링한다.** A는 ready_list에서 차례를 기다리는 상태가 아니다.
6. **B가 선택되면 문맥을 바꾼다.** A의 실행 기록은 보관하고 B의 실행 기록을 CPU에 복원한다.
7. **CPU는 B의 작업을 실행한다.** A는 `timer_sleep()`을 아직 끝내지 못한 채 기다린다.

> [!IMPORTANT]
> **잠든 A가 잠들기 함수를 계속 실행하는 것이 아니다**
> A는 호출을 끝내지 못했지만 CPU를 사용하지 않는다.
> A의 함수 호출 기록은 자기 스택에 남는다.
> CPU는 지금 B의 실행 흐름을 진행한다.

- 보호 구간 [코드의 호출 조건](https://github.com/casys-kaist/pintos-kaist/blob/master/threads/thread.c), [공식 동기화 설명](https://casys-kaist.github.io/pintos-kaist/project1/introduction.html)
	- 기본 `thread_block()`은 인터럽트가 꺼져 있어야 호출할 수 있다
	- 외부 인터럽트 처리기 안에서는 사용할 수 없다
	- 인터럽트와 공유하는 목록은 짧은 구간만 보호한다
	- **A가 자는 전체 시간 동안 인터럽트를 끄는 것이 아니다**
	- 왜 끄나 : 아래 7절

---

### 5. CPU에서 무슨 일이 벌어지나? — 깨우는 절반

> [!NOTE]
> **요약**
> 타이머 인터럽트마다 처리 코드가 대기 목록을 보고, **깨울 시각 ≤ 현재 tick**인 스레드를 READY로 만든다.
> **시간이 되었다, READY가 되었다, 선택되어 RUNNING이 되었다**는 서로 다른 세 사건이다.

```text
[타이머 장치] ─① 인터럽트─▶ [CPU : B 실행 중 → 잠깐 멈춤] ─진입─▶ timer_interrupt()
   ② ticks++ → 105
   ③ sleep_list 를 훑는다       : [A·105] [D·110]
   ④ wakeup_tick ≤ 105 인가?    : A 예 · D 아니오 (그대로)
   ⑤ A 를 빼고 thread_unblock(A) : ready_list [C] [A]  ← A 는 줄 끝에 선다
   ⑥ 처리 끝 → B 로 복귀 (처리기는 스레드가 아니다)

A 에게 일어나는 일 — 셋은 서로 다른 사건이다
   tick 105 (시간이 됐다)
     ▼
   READY (⑤ ready_list 줄 끝)
     ▼
   RUNNING (⑦ 스케줄러가 고른 뒤 — 바로가 아닐 수 있다)
     ▼
   timer_sleep() 반환 → 다음 문장
```

- B가 실행 중이어도 타이머 장치는 시간의 흐름을 알릴 수 있다

1. **타이머 장치가 인터럽트를 요청한다.** CPU가 이를 받아들이면 B의 흐름을 잠시 멈추고 처리 코드로 들어간다.
2. **타이머 처리 코드가 tick을 증가시킨다.** 기본 코드가 이미 하는 일이다.
3. **우리가 추가할 알람 관리 코드가 목표 시각을 확인한다.** 아직 101~104면 A는 계속 기다린다.
4. **현재 tick이 105가 되면 A를 깨울 대상에 넣는다.** `깨울 시각 <= 현재 tick`인 스레드를 모두 찾는다. 같은 시각에 여러 스레드가 있으면 모두 확인한다.
5. **A를 대기 목록에서 제거하고 `thread_unblock(A)`를 호출한다.** A는 READY가 되어 실행 준비 줄에 들어간다.
6. **인터럽트 처리가 끝난다.** B로 돌아갈 수도 있고, 실행 몫이나 스케줄링 정책에 따라 차례를 바꿀 수도 있다.
7. **A가 선택되면 A의 문맥을 복원한다.** 잠들기 과정의 함수 호출에서 돌아와 이전 인터럽트 상태를 복원하고, `timer_sleep()` 뒤의 작업을 이어 간다.

> [!TIP]
> **누가 시계를 보나?**
> 잠든 A가 아니라 **타이머 인터럽트에서 실행되는 커널 코드**가 확인한다.
> 인터럽트 처리기는 별도의 "알람 스레드"가 아니다. CPU가 잠시 하던 일을 멈추고 처리하는 코드다.
> 타이머 인터럽트에서 잠든 스레드를 깨우라는 방향은 [Project 1 소개](https://casys-kaist.github.io/pintos-kaist/project1/introduction.html)에 있다.

- 기본 `thread_unblock()`은 준비 줄에 넣는 동작이다
	- 그 자체가 즉시 실행을 강제하지 않는다

- 공식 Alarm Clock 요구도 일정한 tick 수를 기다린 후 준비 줄에 넣는 것이다 [공식 요구사항](https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html)
	- 정확히 그 tick에 다음 문장을 실행할 필요는 없다

- 깨울 조건을 `현재 tick == 깨울 시각`으로만 외우지 말자
	- 이미 기한이 지난 대상도 깨울 수 있게 "현재 시각이 목표 이상인가?"로 이해하는 것이 정확하다

- 여러 스레드가 잠든 경우 : 아래 6절

#### A는 어디서 다시 시작하나?

- 작업 함수의 맨 처음부터 다시 시작하지 않는다

- 저장된 문맥을 복원하면 멈춰 있던 커널 함수 호출들이 이어진다
	- 결국 `timer_sleep()`이 반환한다

- **새 스레드를 만들어 처음부터 재실행하는 것이 아니다**

---

### 6. 여러 스레드가 잠들면? — 알람 예약표

> [!NOTE]
> **요약**
> 깨울 시각을 관리하는 목록과, 실제 실행 차례를 관리하는 ready queue는 **다른 것**이다.
> **깨울 시각 ≤ 현재 tick**인 스레드는 빠짐없이 모두 깨운다.

- 가정 : A·B·C가 모두 tick 100에서 아래 요청을 했다

| 스레드 | 기다릴 tick | 깨울 목표 tick |
|---|---:|---:|
| A | 5 | 105 |
| B | 2 | 102 |
| C | 5 | 105 |

```text
tick 100 : B 는 2 tick, A · C 는 5 tick 요청
           sleep_list (깨울 시각 순) : [B·102] [A·105] [C·105]

tick 102 : B·102 ≤ 102 → 깨움 (ready_list 로)
           A·105 > 102 → 여기서 멈춤 (뒤는 전부 더 늦다)
           sleep_list : [A·105] [C·105]     ready_list : [B]

tick 105 : A·105 ≤ 105 → 깨움
           C·105 ≤ 105 → 깨움 (같은 시각이면 모두, A 하나로 끝내면 안 된다)
           sleep_list : []                  ready_list : [B] [A] [C]

READY 가 된 순서 ≠ 실행 순서 — 언제 CPU 를 받을지는 스케줄러가 정한다
```

- 실행 순서는 스케줄러와 다른 준비된 스레드에 따라 달라진다

- 목록을 확인하는 방법
	- 시각순으로 정렬하면 앞쪽부터 확인할 수 있다
	- 정렬하지 않으면 전체를 확인할 수 있다
	- 어느 방식이든 시간이 된 스레드를 빠뜨리지 않아야 한다

---

### 7. 구현을 읽을 때: 왜 인터럽트를 잠깐 끄나?

> [!NOTE]
> **요약**
> 잠드는 코드와 깨우는 코드(타이머 처리기)가 **같은 대기 목록**을 만진다.
> 등록과 block 사이에 타이머가 끼어들지 않도록, 그 **짧은 구간만** 인터럽트를 끈다.

```text
보호 없이 — 등록과 block 사이에 타이머가 끼어들면
  ① A : sleep_list 에 등록 (아직 RUNNING)
        ⚡ 타이머 인터럽트가 끼어든다
  ② 처리기 : 105 ≤ 105 → thread_unblock(A)
  ③ A 는 아직 BLOCKED 가 아니다
     → unblock 의 상태 검사(ASSERT) 실패, 또는 A 가 뒤늦게 block 돼 깨워 줄 쪽이 없다

보호 구간 — 이 셋을 인터럽트가 꺼진 채 한 번에
  old = intr_disable()
  │ ① wakeup_tick 기록
  │ ② sleep_list 등록
  │ ③ thread_block() → schedule() → B 로 넘어감
  │   (⚡ 이 사이에 온 타이머는 끝날 때까지 기다린다)
  ▼
  B 실행 중 : 인터럽트가 다시 켜져 있다 → 기다리던 타이머를 여기서 처리
  A 가 깨어나 돌아오면 intr_set_level(old) 로 원래대로

꺼 두는 건 ① ~ ③ 짧은 구간뿐 — A 가 자는 내내 끄는 게 아니다
```

- 끼어들면 생기는 일
	1. A가 대기 목록에 자신을 등록했다
	2. 아직 BLOCKED가 되기 전에 타이머 인터럽트가 끼어든다
	3. 처리기가 잘못된 상태의 A를 깨우려 한다

- 그래서 함께 보호할 구간
	1. 현재 시각 확인
	2. 대기 등록
	3. block

- 기본 `thread_block()`의 조건 [thread.c의 조건](https://github.com/casys-kaist/pintos-kaist/blob/master/threads/thread.c)
	- 인터럽트가 꺼져 있어야 호출할 수 있다
	- 인터럽트 처리기 안에서 호출할 수 없다

- **A가 자는 내내 시스템 전체의 인터럽트를 꺼 두는 것은 아니다** [공식 동기화 주의사항](https://casys-kaist.github.io/pintos-kaist/project1/introduction.html)
	- 다른 스레드가 실행되는 동안에도 타이머가 동작해야 A를 깨울 수 있다
	- 보호 구간은 짧게 유지한다
	- 이 설명은 해당 과제의 단일 CPU 환경을 기준으로 한다

- 처리기가 잠들 수 없는 이유 : 처리기는 스레드가 아니라서 block 하면 깨워 줄 쪽이 없다 → 잠들 수 있는 락 대신 스레드 쪽에서 인터럽트를 잠깐 끈다

---

### 8. 우리가 추가할 것과 이미 있는 것을 구분하자

> [!NOTE]
> **요약**
> **우리가 CPU나 문맥 교환기를 새로 만드는 과제가 아니다.**
> 이미 있는 스레드 기능을 이용해 "시간이 될 때까지 실행 대상에서 빼 두기"를 구현한다.

| 이미 있는 기본 기능 | 우리가 연결하거나 추가할 부분 |
|---|---|
| 타이머 장치 설정과 인터럽트 등록 | 기존 타이머 흐름에 알람 검사를 연결 |
| ticks 증가 | 각 스레드의 깨울 목표 시각 관리 |
| thread_block / thread_unblock | 잠들기 등록과 깨우기 호출 연결 |
| ready_list와 스케줄러 | 잠든 스레드를 별도 관리 |
| 문맥 저장과 복원 | 잠들기와 깨우기가 안전한 순서로 이루어지게 함 |

- sleep list나 wakeup_tick 같은 이름은 설계 예시다
	- 실제 이름, 정렬 여부와 자료구조는 다르게 정할 수 있다
	- 이 글은 완성 구현 코드보다 **각 기능이 왜 필요한지**를 설명한다

---

### 9. 실제 코드는 이 순서로 찾아보자

> [!NOTE]
> **요약**
> 스레드 쪽(`thread.h`, `thread.c`)을 먼저 읽고, 타이머 쪽(`timer.c`), 마지막에 인터럽트(`interrupt.c`)를 본다.

- 기준 : 권영진 교수님의 **Pintos KAIST x86-64 버전** [공식 소개](https://casys-kaist.github.io/pintos-kaist/)
	- 파일 경로는 Pintos 저장소 루트 기준이다
	- 내 코드가 이미 수정되어 있다면 공개 기본 코드와 비교하며 읽자

| 순서 | 찾아볼 곳 | 확인할 목적 | 답할 질문 |
| --- | --- | --- | --- |
| 1 | include/threads/thread.h → struct thread, thread_status | 스레드가 기억하는 정보 | 상태 이름은 무엇인가? |
| 2 | threads/thread.c → thread_create | 새 실행 흐름의 준비 | |
| 3 | threads/thread.c → thread_yield | 양보한 뒤 준비 줄로 가는 과정 | 양보하면 어느 줄로 가나? |
| 4 | threads/thread.c → thread_block | 조건 대기로 실행 대상에서 빠짐 | 상태를 바꾸고 무엇을 호출하나? |
| 5 | threads/thread.c → schedule / next_thread_to_run | 다음 스레드 선택 | 준비된 작업이 없으면 누가 실행되나? |
| 6 | threads/thread.c → thread_launch / do_iret | 실행 상태 전환의 목적만 파악 | |
| 7 | devices/timer.c → timer_sleep | 기본 busy waiting 확인 | 시간을 어디서 반복 확인하나? |
| 8 | devices/timer.c → timer_interrupt | tick 증가와 추가할 알람 검사 위치 | tick은 어디서 증가하나? |
| 9 | threads/thread.c → thread_unblock | BLOCKED에서 READY로 | 어떻게 READY로 만드나? |
| 10 | threads/interrupt.c → intr_handler | 인터럽트 처리 후 필요할 때 양보 | |

```text
[재우기]
timer.c   timer_sleep(ticks)
            [지울 것] while … thread_yield() 반복
            [추가] wakeup_tick = 지금 + ticks
            [추가] sleep_list 에 넣고 thread_block() 을 부른다
               │
               ▼
thread.c  thread_block()        : status = BLOCKED
            → schedule()
            → next_thread_to_run() : ready_list 맨 앞
            → thread_launch()      : 문맥 교환

[깨우기]
timer.c   timer_interrupt() — 매 tick
            ticks++
            thread_tick() ──────────▶ thread.c thread_tick()     : 실행 몫 관리
            [추가] wakeup_tick ≤ ticks 인 스레드 찾기
               │
               ▼
thread.c  thread_unblock(t)     : BLOCKED → READY
            → ready_list 끝에 push_back

[추가] · [지울 것] = Alarm Clock 에서 바꿀 곳, 나머지는 이미 있는 기본 코드
```

- 기본 타이머 처리기는 tick을 증가시키고 `thread_tick()`을 호출한다
	- **잠든 스레드의 알람 검사는 과제에서 추가할 동작**이다

- 기본 구현에서 준비된 작업이 없으면 idle 스레드를 실행한다 [timer.c](https://github.com/casys-kaist/pintos-kaist/blob/master/devices/timer.c), [thread.c](https://github.com/casys-kaist/pintos-kaist/blob/master/threads/thread.c)

## 세션별 기록

### 세션 1 — 팀 일정 · 과제 분석 정리

- 도구 : Claude
- 물어본 것
	- 개인 브랜치 만들기, 팀코어 메모 정리, 논의 이슈 형식
	- Priority Scheduling · Advanced Scheduler 를 테스트 → 헤더 → 관련 소스 순서로 정리
- AI 답 요약
	- 공통 일정은 새 이슈 템플릿 없이 💬 논의 이슈 하나에 셋을 할당하고, 각자 댓글로 답한다
	- 함수를 미리 정해 두고 답할 질문 · 댓글 형식까지 정해야 셋의 답을 비교할 수 있다
- 내가 직접 확인한 것
	- 팀 프로젝트에 「기능」 칸 (alarm 등) 이 이미 있다
- 틀렸거나 의심스러운 답
	- 

### 세션 2 — alarm 테스트 · 인터럽트 보호 책임

- 도구 : Claude
- 물어본 것
	- alarm 만 따로 테스트하는 법
	- 인터럽트 보호 책임이 무엇인가
- AI 답 요약
	- alarm 테스트 6개만 따로 돌릴 수 있다
	- alarm-priority 는 priority 구현 전이라 실패하는 게 정상
	- 인터럽트 보호 책임 = sleep_list 를 만지는 동안 인터럽트를 끄고 되돌리는 일을 **어느 함수가 맡나**
- 내가 직접 확인한 것
	- 
- 틀렸거나 의심스러운 답
	- 

### 세션 3 — 비교 함수 · list_entry

- 도구 : Claude
- 물어본 것
	- 비교 함수에서 두 `list_elem` 으로 `struct thread` 를 어떻게 꺼내나
	- 재우는 함수가 이상한지 (비교 함수 뒤에 `()` 를 붙였던 것)
	- 비교 함수 안 변수 이름
- AI 답 요약
	- `list_entry` 로 꺼낸다 (`thread_wakeup` 에서 이미 쓴 방법과 같다)
	- 비교 함수는 넘기는 자리라 `()` 를 뺀다 — 호출이 아니라 함수 자체를 넘긴다
	- 변수 이름은 인자 `a` · `b` 와 짝이 보이게 `thread_a` · `thread_b`
- 내가 직접 확인한 것
	- 
- 틀렸거나 의심스러운 답
	- 

## 코드 변경

- 구현 정리 · 테스트 결과는 10/11 TIL (`docs/2026-10-11/TIL/Jongeume.md`) 에 (10/11 에 `thread_sleep` · `thread_wakeup` 으로 책임을 나누며 마무리)

## 막힌 것 · 열린 질문

- sleep_list 를 만지는 동안 인터럽트를 **어느 함수에서** 끄고 켜는 게 맞나 (`timer_sleep` vs 재우는 함수) → 10/11 에 정리

## 팀에 공유할 것

> 팀원이 이것만 읽어도 되게

- 기본 `timer_sleep` 은 `thread_yield` 를 되풀이하는 busy waiting — 기다리는 스레드도 차례마다 CPU 를 쓴다
- 바꿀 방향 : 깨울 절대 tick 을 적고 sleep_list 에 넣어 **BLOCKED** → 타이머 인터럽트가 **깨울 시각 ≤ 현재 tick** 인 스레드를 **READY** 로
- 시간이 됨 ≠ READY ≠ RUNNING — 깨워도 바로 실행되지 않고 스케줄러가 고른 뒤 실행된다
- 처리기는 스레드가 아니다 — B 를 실행하던 CPU 가 잠깐 들어가는 커널 코드
- 등록과 block 사이만 인터럽트를 잠깐 끈다 — 자는 내내 끄는 게 아니다
- CPU · 문맥 교환을 새로 만드는 과제가 아니라 `thread_block` · `thread_unblock` 을 시간 대기에 잇는 과제

## 참고

- [Pintos KAIST Alarm Clock 요구사항](https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html)
- [Project 1 Introduction](https://casys-kaist.github.io/pintos-kaist/project1/introduction.html)
- [pintos-kaist threads/thread.c](https://github.com/casys-kaist/pintos-kaist/blob/master/threads/thread.c) · [devices/timer.c](https://github.com/casys-kaist/pintos-kaist/blob/master/devices/timer.c)
