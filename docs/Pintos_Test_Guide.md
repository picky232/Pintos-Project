# Pintos 테스트 가이드 (Project 1: threads)

이 문서 하나로 **기능별 테스트 → 전체 테스트 → 점수 확인 → 만점 판정**까지 합니다.
별도 스크립트 없이 `make`와 Pintos 기본 채점기(`tests/make-grade`)만 씁니다.
아래 화면은 실제 컨테이너에서 실행한 출력입니다. (구현 중인 코드 기준이라 일부는 FAIL로 나옵니다. **만점일 때 화면은 각 장에 따로 표시**했습니다.)

## 목차

0. [30초 요약](#0-30초-요약)
1. [테스트가 동작하는 원리](#1-테스트가-동작하는-원리)
2. [준비: 어디서 실행하나](#2-준비-어디서-실행하나)
3. [기능별 테스트](#3-기능별-테스트-alarm--priority--mlfqs)
4. [전체 테스트](#4-전체-테스트)
5. [점수 확인](#5-점수-확인)
6. [만점 판정](#6-만점-판정)
7. [실패했을 때 원인 보기](#7-실패했을-때-원인-보기)
8. [자주 하는 실수](#8-자주-하는-실수)
9. [디버깅 (GDB)와 메뉴 스크립트](#9-디버깅-gdb와-메뉴-스크립트)

---

## 0. 30초 요약

```bash
# ① 컨테이너 터미널에서 (프롬프트가 jungle@... 로 시작)
cd /workspaces/pintos_22.04_lab_docker/pintos/threads/build
make                                          # 코드 고쳤으면 먼저 빌드

# ② 전체 테스트 실행 (이미 결과가 있는 건 건너뜀) + 통과/실패 목록
make check

# ③ 점수와 만점 여부
../../tests/make-grade ../.. results ../../tests/threads/Grading | grep -E "TOTAL|PERFECT"
```

**만점이면** ③에서 이 두 줄이 나옵니다:

```
TOTAL TESTING SCORE: 100.0%
ALL TESTED PASSED -- PERFECT SCORE
```

| 질문 | 답 |
|---|---|
| 어디서 실행? | **Docker 컨테이너 터미널**의 `pintos/threads/build` |
| 테스트가 몇 개? | **27개** = Alarm 6 + Priority 12 + MLFQS 9 |
| 만점이 몇 점? | **41점** = Alarm 7 + Priority 25 + MLFQS 9 → 최종 100% |
| 배점 비중 | Alarm **20%** / Priority **50%** / MLFQS **30%** |

---

## 1. 테스트가 동작하는 원리

헷갈리는 이유는 파일이 3종류라서입니다. 흐름은 한 방향입니다.

```
 ① 실행                      ② 채점                     ③ 점수
┌──────────────┐          ┌──────────────┐          ┌──────────────────┐
│ QEMU로 커널    │  .output │ 기대 출력과    │  .result │ 모든 .result를 모아  │
│ 부팅해서       │ ───────▶ │ 비교(.ck)     │ ───────▶ │ 배점대로 합산      │
│ 테스트 실행     │          │ → PASS/FAIL  │          │ → 점수, 만점 여부   │
└──────────────┘          └──────────────┘          └──────────────────┘
 make <테스트>.result        make가 자동으로 연결          tests/make-grade
```

| 파일 (위치: `build/tests/threads/`) | 의미 | 언제 보나 |
|---|---|---|
| `<이름>.output` | 커널이 실제로 찍은 전체 출력 | 왜 틀렸는지 볼 때 |
| `<이름>.result` | 채점 결과. 첫 줄이 `PASS` 또는 `FAIL`, FAIL이면 diff가 이어짐 | 통과 여부 확인 |
| `<이름>.errors` | QEMU의 stderr | 실행이 아예 안 됐을 때 |

| 파일 (위치: `build/`) | 의미 |
|---|---|
| `results` | `make check`/`make grade`가 만드는 요약. 한 줄에 테스트 하나 (`pass ...` / `FAIL ...`). **채점기의 입력** |
| `grade` | `make grade`의 출력 저장본 |

중요한 규칙 두 가지:

1. **`make`는 소스가 안 바뀌면 다시 실행하지 않습니다.** `.result`가 있으면 `up to date`만 나옵니다. 다시 돌리려면 `.output`과 `.result`를 지우고 실행합니다.
2. **`make <이름>.result`는 `pass`/`FAIL`만 보여 줍니다.** 점수는 `tests/make-grade`(또는 `make grade`)에서만 나옵니다.

---

## 2. 준비: 어디서 실행하나

### 2-1. 컨테이너 터미널을 쓰세요

테스트는 QEMU가 필요해서 **컨테이너 안에서만** 실행됩니다. VS Code를 DevContainer로 열고 터미널을 열면 됩니다. 프롬프트가 아래처럼 `jungle@`로 시작하면 컨테이너입니다.

```
jungle@44d3205bb33b:/workspaces/pintos_22.04_lab_docker$
```

맥(호스트) 터미널은 프롬프트가 `jiho0215@...`이고, 여기서는 `/workspaces/...` 경로가 없다는 에러가 납니다. (8장 참고)

### 2-2. build 폴더로 이동하고 빌드

```
jungle@44d3205bb33b:/workspaces/pintos_22.04_lab_docker$ cd pintos/threads/build
jungle@44d3205bb33b:/workspaces/pintos_22.04_lab_docker/pintos/threads/build$ make
```

- 코드를 고칠 때마다 `make`로 빌드합니다. 컴파일/링크 에러는 여기서 먼저 확인합니다.
- 이하 모든 명령은 이 `build` 폴더 기준입니다.

---

## 3. 기능별 테스트 (Alarm / Priority / MLFQS)

각 영역은 **① 실행 → ② 점수** 두 단계입니다.

- ① 실행: 이전 결과를 지우고 `make -k`로 그 영역 테스트만 돌립니다. (`-k`는 중간에 실패해도 끝까지 진행)
- ② 점수: 그 영역의 `.result`를 모아 채점기에 넣습니다. 영역 하나만 넣으므로 **그 영역이 100%** 기준입니다.

### 3-1. Alarm Clock (7점, 테스트 6개)

`timer_sleep()`이 busy waiting 없이 정확한 시각에 깨우는지 봅니다.

| 테스트 | 배점 | 무엇을 보나 |
|---|---|---|
| alarm-single | 1 | 스레드 5개가 한 번씩 잔다. 깨어난 순서가 맞는가 |
| alarm-multiple | 1 | 스레드 5개가 7번씩 잔다. 순서가 계속 맞는가 |
| alarm-simultaneous | 1 | 같은 tick에 자는 스레드 3개가 **같은 tick에** 깨어나는가 |
| alarm-priority | **2** | 같은 tick에 깨어난 스레드가 **우선순위 높은 순**으로 실행되는가 |
| alarm-zero | 1 | `timer_sleep(0)`이 바로 return 하는가 |
| alarm-negative | 1 | `timer_sleep(-100)`이 바로 return 하는가 |

> `alarm-priority`는 깨우기만 해서는 안 되고 **ready_list가 우선순위 순서**여야 통과합니다. Priority Scheduling 일부가 필요할 수 있습니다.

**① 실행** (약 20초)

```bash
rm -f tests/threads/alarm-*.output tests/threads/alarm-*.result
make -k tests/threads/alarm-{single,multiple,simultaneous,priority,zero,negative}.result
```

**② 점수**

```bash
for t in single multiple simultaneous priority zero negative; do
  f=tests/threads/alarm-$t
  if [ "$(head -1 $f.result 2>/dev/null)" = PASS ]; then echo "pass $f"; else echo "FAIL $f"; fi
done > /tmp/a.results
printf '100.0%%\ttests/threads/Rubric.alarm\n' > /tmp/a.grading
../../tests/make-grade ../.. /tmp/a.results /tmp/a.grading | sed '/^DETAILS OF/,$d'
```

**만점일 때 화면** (이 코드가 정상 동작하던 시점에 실측):

```
TOTAL TESTING SCORE: 100.0%
ALL TESTED PASSED -- PERFECT SCORE

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

SUMMARY BY TEST SET

Test Set                                      Pts Max  % Ttl  % Max
--------------------------------------------- --- --- ------ ------
tests/threads/Rubric.alarm                      7/  7 100.0%/100.0%
--------------------------------------------- --- --- ------ ------
Total                                                 100.0%/100.0%

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

SUMMARY OF INDIVIDUAL TESTS

Functionality and robustness of alarm clock (tests/threads/Rubric.alarm):
	     1/ 1 tests/threads/alarm-single
	     1/ 1 tests/threads/alarm-multiple
	     1/ 1 tests/threads/alarm-simultaneous
	     2/ 2 tests/threads/alarm-priority
	     1/ 1 tests/threads/alarm-zero
	     1/ 1 tests/threads/alarm-negative

	- Section summary.
	      6/  6 tests passed
	      7/  7 points subtotal
```

### 3-2. Priority Scheduling + Donation (25점, 테스트 12개)

| 구분 | 테스트 | 배점 | 무엇을 보나 |
|---|---|---|---|
| 스케줄링 | priority-change | 1 | 우선순위를 낮추면 더 높은 스레드에게 양보하는가 |
| | priority-preempt | 1 | 높은 우선순위 스레드가 생기면 즉시 선점하는가 |
| | priority-fifo | 1 | 같은 우선순위면 FIFO 순서인가 |
| | priority-sema | 2 | 세마포어 대기자 중 **높은 우선순위가 먼저** 깨어나는가 |
| | priority-condvar | 2 | 조건변수 대기자도 우선순위 순으로 깨어나는가 |
| 기부 | priority-donate-one | 2 | lock 하나에 대한 기부 |
| | priority-donate-multiple | 3 | lock 여러 개에 대한 기부 |
| | priority-donate-multiple2 | 3 | 기부가 여러 단계로 쌓이고 풀리는가 |
| | priority-donate-nest | 3 | 중첩 기부 (A→B→C) |
| | priority-donate-chain | 3 | 긴 체인 기부 |
| | priority-donate-sema | 2 | 세마포어와 lock이 섞인 기부 |
| | priority-donate-lower | 2 | 기부받은 상태에서 우선순위를 낮추려 할 때 |

**① 실행** (약 1분)

```bash
rm -f tests/threads/priority-*.output tests/threads/priority-*.result
make -k tests/threads/priority-{change,preempt,fifo,sema,condvar,donate-one,donate-multiple,donate-multiple2,donate-nest,donate-chain,donate-sema,donate-lower}.result
```

**② 점수**

```bash
for t in change preempt fifo sema condvar donate-one donate-multiple donate-multiple2 donate-nest donate-chain donate-sema donate-lower; do
  f=tests/threads/priority-$t
  if [ "$(head -1 $f.result 2>/dev/null)" = PASS ]; then echo "pass $f"; else echo "FAIL $f"; fi
done > /tmp/p.results
printf '100.0%%\ttests/threads/Rubric.priority\n' > /tmp/p.grading
../../tests/make-grade ../.. /tmp/p.results /tmp/p.grading | sed '/^DETAILS OF/,$d'
```

**구현 전 화면** (전부 FAIL, 0/25, 실측):

```
TOTAL TESTING SCORE: 0.0%

SUMMARY BY TEST SET

Test Set                                      Pts Max  % Ttl  % Max
--------------------------------------------- --- --- ------ ------
tests/threads/Rubric.priority                   0/ 25   0.0%/100.0%
--------------------------------------------- --- --- ------ ------
Total                                                   0.0%/100.0%

Functionality of priority scheduler (tests/threads/Rubric.priority):
	  ** 0/ 1 tests/threads/priority-change
	  ** 0/ 1 tests/threads/priority-preempt

	  ** 0/ 1 tests/threads/priority-fifo
	  ** 0/ 2 tests/threads/priority-sema
	  ** 0/ 2 tests/threads/priority-condvar
	  ... (생략) ...
	      0/ 12 tests passed
	      0/ 25 points subtotal
```

`**`가 붙은 줄이 실패입니다. **만점 화면**은 `**`가 하나도 없고, 위쪽에 `ALL TESTED PASSED -- PERFECT SCORE`가 나오며 `25/ 25`, `12/ 12 tests passed`로 바뀝니다.

### 3-3. MLFQS (9점, 테스트 9개)

| 테스트 | 배점 | 무엇을 보나 |
|---|---|---|
| mlfqs-load-1 | 1 | 스레드 1개가 10초 자고 난 뒤 load_avg 값 |
| mlfqs-load-60 | 1 | 스레드 60개일 때 load_avg 변화 |
| mlfqs-load-avg | 1 | 스레드가 시간차로 일할 때 load_avg 곡선 |
| mlfqs-recent-1 | 1 | recent_cpu 값이 시간에 따라 맞게 변하는가 |
| mlfqs-fair-2 | 1 | nice가 같은 스레드 2개가 CPU를 공평하게 나누는가 |
| mlfqs-fair-20 | 1 | 같은 조건으로 스레드 20개 |
| mlfqs-nice-2 | 1 | nice가 다른 스레드 2개 사이의 CPU 배분 |
| mlfqs-nice-10 | 1 | nice가 다른 스레드 10개 사이의 CPU 배분 |
| mlfqs-block | 1 | 락에 막힌 스레드가 있어도 계산이 맞는가 |

- 폴더가 하나 더 붙습니다: `tests/threads/mlfqs/<이름>.result`
- **오래 걸립니다.** 구현 전 기준으로 9개 실행에 약 **13분**이 걸렸습니다 (타임아웃 60초가 여럿). 구현 후에는 줄어듭니다.
- 직접 `pintos`로 실행할 때는 `-mlfqs` 옵션이 필요합니다: `pintos -- -mlfqs -q run mlfqs-load-1` (`make`로 돌릴 때는 자동으로 붙음)

**① 실행** (구현 전 약 13분)

```bash
rm -f tests/threads/mlfqs/*.output tests/threads/mlfqs/*.result
make -k tests/threads/mlfqs/mlfqs-{load-1,load-60,load-avg,recent-1,fair-2,fair-20,nice-2,nice-10,block}.result
```

**② 점수**

```bash
for t in load-1 load-60 load-avg recent-1 fair-2 fair-20 nice-2 nice-10 block; do
  f=tests/threads/mlfqs/mlfqs-$t
  if [ "$(head -1 $f.result 2>/dev/null)" = PASS ]; then echo "pass $f"; else echo "FAIL $f"; fi
done > /tmp/m.results
printf '100.0%%\ttests/threads/mlfqs/Rubric\n' > /tmp/m.grading
../../tests/make-grade ../.. /tmp/m.results /tmp/m.grading | sed '/^DETAILS OF/,$d'
```

**구현 전 화면** (fair-2, fair-20만 통과 → 2/9, 실측):

```
TOTAL TESTING SCORE: 22.2%

SUMMARY BY TEST SET

Test Set                                      Pts Max  % Ttl  % Max
--------------------------------------------- --- --- ------ ------
tests/threads/mlfqs/Rubric                      2/  9  22.2%/100.0%
--------------------------------------------- --- --- ------ ------
Total                                                  22.2%/100.0%

Functionality of advanced scheduler (tests/threads/mlfqs/Rubric):
	  ** 0/ 1 tests/threads/mlfqs/mlfqs-load-1
	  ** 0/ 1 tests/threads/mlfqs/mlfqs-load-60
	  ** 0/ 1 tests/threads/mlfqs/mlfqs-load-avg

	  ** 0/ 1 tests/threads/mlfqs/mlfqs-recent-1

	     1/ 1 tests/threads/mlfqs/mlfqs-fair-2
	     1/ 1 tests/threads/mlfqs/mlfqs-fair-20

	  ** 0/ 1 tests/threads/mlfqs/mlfqs-nice-2
	  ** 0/ 1 tests/threads/mlfqs/mlfqs-nice-10

	  ** 0/ 1 tests/threads/mlfqs/mlfqs-block
```

(구현 전에도 fair 2개가 통과하는 건 정상입니다. 아무 일도 안 해도 공평하게 나뉘기 때문입니다.)

### 3-4. 테스트 하나만 실행

디버깅할 때는 하나만 돌립니다. 이름은 `tests/threads/` + 테스트이름 + `.result` 입니다 (MLFQS는 `tests/threads/mlfqs/`).

```
jungle@44d3205bb33b:.../threads/build$ make tests/threads/priority-change.result
perl -I../.. ../../tests/threads/priority-change.ck tests/threads/priority-change tests/threads/priority-change.result
FAIL tests/threads/priority-change
Test output failed to match any acceptable form.

Acceptable output:
  (priority-change) begin
  (priority-change) Creating a high-priority thread 2.
  (priority-change) Thread 2 now lowering priority.
  (priority-change) Thread 2 should have just lowered its priority.
  (priority-change) Thread 2 exiting.
  (priority-change) Thread 2 should have just exited.
  (priority-change) end
Differences in `diff -u' format:
  (priority-change) begin
  (priority-change) Creating a high-priority thread 2.
- (priority-change) Thread 2 now lowering priority.
  (priority-change) Thread 2 should have just lowered its priority.
- (priority-change) Thread 2 exiting.
  (priority-change) Thread 2 should have just exited.
  (priority-change) end
```

- `FAIL` 아래 **`-` 줄이 기대했지만 안 나온 출력**, `+` 줄이 나오지 말아야 하는데 나온 출력입니다.
- 통과하면 `pass tests/threads/priority-change` 한 줄만 나옵니다.

**같은 테스트를 다시 돌리려면** (코드를 고쳤다면 `make` 후) 이전 결과를 지워야 합니다:

```bash
make
rm tests/threads/priority-change.output tests/threads/priority-change.result
make tests/threads/priority-change.result
```

여러 개를 한 번에는 공백으로 잇거나 중괄호를 씁니다:

```bash
make -k tests/threads/priority-{change,preempt,fifo}.result
```

---

## 4. 전체 테스트

### 4-1. 가장 간단한 방법: `make check`

27개를 전부 실행하고(이미 최신인 건 건너뜀) 결과를 한 줄씩 보여 줍니다. 끝나면 `build/results` 파일도 갱신됩니다.

```
jungle@44d3205bb33b:.../threads/build$ make check
pass tests/threads/alarm-single
pass tests/threads/alarm-multiple
pass tests/threads/alarm-simultaneous
pass tests/threads/alarm-priority
pass tests/threads/alarm-zero
pass tests/threads/alarm-negative
FAIL tests/threads/priority-change
FAIL tests/threads/priority-donate-one
...
pass tests/threads/mlfqs/mlfqs-fair-2
pass tests/threads/mlfqs/mlfqs-fair-20
FAIL tests/threads/mlfqs/mlfqs-nice-2
...
19 of 27 tests failed.
make: *** [../../tests/Make.tests:29: check] Error 1
```

- 마지막 줄이 `19 of 27 tests failed.`입니다. **전부 통과하면 `All 27 tests passed.`** 가 나옵니다.
- 실패가 있으면 마지막에 `make: *** ... Error 1`이 붙는데, 정상 동작입니다 (실패가 있다는 뜻).
- **점수는 안 나옵니다.** 통과/실패 목록만 나옵니다.

### 4-2. 점수까지: `make grade`

```bash
make grade
```

27개를 실행(없는 것만)하고 채점해서 `TOTAL TESTING SCORE`와 영역별 점수를 보여 줍니다. 실패한 테스트의 기대/실제 출력이 **모두 이어서 나와 출력이 매우 깁니다** (약 7만 자, `build/grade` 파일에도 저장). 요약만 보려면 5장의 방법을 쓰세요.

### 4-3. 시간 정리 (구현 전 기준)

| 범위 | 걸리는 시간 |
|---|---|
| Alarm 6개 | 약 20초 |
| Priority 12개 | 약 1분 |
| MLFQS 9개 | 약 13분 |
| 전체 27개 | 약 15분 |

처음부터 전체를 돌리기보다 **영역별로 나눠서**(3장) 돌리는 편이 빠릅니다. 구현이 끝날수록 MLFQS도 빨라집니다.

---

## 5. 점수 확인

27개 결과가 모두 있는 상태(`make check` 직후)에서, 채점기에 `results`를 넣고 **실패 상세는 잘라서** 요약만 봅니다.

```
jungle@44d3205bb33b:.../threads/build$ make check
jungle@44d3205bb33b:.../threads/build$ ../../tests/make-grade ../.. results ../../tests/threads/Grading | sed '/^DETAILS OF/,$d'
TOTAL TESTING SCORE: 26.7%

SUMMARY BY TEST SET

Test Set                                      Pts Max  % Ttl  % Max
--------------------------------------------- --- --- ------ ------
tests/threads/Rubric.alarm                      7/  7  20.0%/ 20.0%
tests/threads/Rubric.priority                   0/ 25   0.0%/ 50.0%
tests/threads/mlfqs/Rubric                      2/  9   6.7%/ 30.0%
--------------------------------------------- --- --- ------ ------
Total                                                  26.7%/100.0%
...
```

### 표 읽는 법

| 항목 | 의미 |
|---|---|
| `Pts` / `Max` | 얻은 점수 / 만점 (Alarm 7, Priority 25, MLFQS 9) |
| `% Ttl` | 내가 얻은 최종 점수 비율 (영역 점수 ÷ 만점 × 영역 비중) |
| `% Max` | 그 영역의 비중 (20 / 50 / 30) |
| `Total` 줄의 `26.7%/100.0%` | **최종 점수 26.7%, 만점은 100%** |

계산 예: MLFQS 2/9 → 2 ÷ 9 × 30% = 6.7%. Alarm 7/7 → 20.0%. 합계 26.7%.

### 점수 한 줄만 보기

```bash
../../tests/make-grade ../.. results ../../tests/threads/Grading | grep -E "TOTAL|PERFECT"
```

### 영역별 점수

3장의 **② 점수** 블록을 그대로 쓰세요. 영역 하나를 채점할 때는 **그 영역을 100%로 계산**합니다. 최종 점수가 아니라 "이 영역에서 몇 %"입니다.

> `results` 파일은 `make check`/`make grade`를 실행할 때 갱신됩니다. 방금 일부 테스트만 다시 돌렸다면 `make check`를 한 번 실행해서 `results`를 최신으로 만든 뒤 채점하세요.

---

## 6. 만점 판정

아래 4개 중 어느 것이든 하나만 봐도 됩니다.

| 확인 방법 | 만점일 때 화면 |
|---|---|
| ① `make check` | 마지막에 `All 27 tests passed.` |
| ② `make-grade ... results ... \| grep -E "TOTAL\|PERFECT"` | `TOTAL TESTING SCORE: 100.0%` 와 `ALL TESTED PASSED -- PERFECT SCORE` |
| ③ `make grade` | 맨 위에 ②와 같은 두 줄 |
| ④ 영역별 (3장 ② 점수 블록) | Alarm `7/ 7`, Priority `25/ 25`, MLFQS `9/ 9` + `PERFECT SCORE` |

### 만점 최종 화면 (②)

```
TOTAL TESTING SCORE: 100.0%
ALL TESTED PASSED -- PERFECT SCORE

SUMMARY BY TEST SET

Test Set                                      Pts Max  % Ttl  % Max
--------------------------------------------- --- --- ------ ------
tests/threads/Rubric.alarm                      7/  7  20.0%/ 20.0%
tests/threads/Rubric.priority                  25/ 25  50.0%/ 50.0%
tests/threads/mlfqs/Rubric                      9/  9  30.0%/ 30.0%
--------------------------------------------- --- --- ------ ------
Total                                                 100.0%/100.0%
```

> 위 화면 중 Priority/MLFQS 줄은 아직 구현 전이라 **실측하지 못한 예상 화면**입니다. 형식은 `make-grade`가 정하므로 같은 모양으로 `25/ 25`, `9/ 9`가 나옵니다. Alarm 영역의 만점(7/7, 100.0%, `ALL TESTED PASSED -- PERFECT SCORE`)은 실제로 확인했습니다.

### 만점이 아닐 때 보는 곳

- `**` 가 붙은 줄이 실패한 테스트입니다.
- `warning: ... not tested, assuming failure`는 그 테스트의 결과가 없다는 뜻입니다 (실패로 계산). `make check`나 3장 ①로 먼저 실행하세요.
- 실패 원인은 7장에서 봅니다.

### 점수에 대한 주의

- **채점 기준은 배점 합산입니다.** `alarm-priority`처럼 한 테스트가 2점인 것도 있어서 "통과 개수"와 "점수"가 다를 수 있습니다. (예: 6개 중 5개 통과인데 5/7점)
- 한 번 통과했어도 다른 기능을 고치다가 깨질 수 있습니다. **수정 후에는 전체를 다시 실행해서 회귀를 확인**하세요.

---

## 7. 실패했을 때 원인 보기

### 7-1. 순서

```
FAIL 확인 → .result (diff) → .output (커널 전체 출력) → .errors → GDB
```

### 7-2. `.result`: 어디가 틀렸나

```bash
cat tests/threads/priority-change.result
```

`FAIL` 다음에 **기대 출력과 실제 출력의 차이**가 나옵니다 (3-4 참고).

### 7-3. `.output`: 커널이 뭘 했나

```bash
cat tests/threads/priority-change.output
```

끝부분에 통계가 있습니다.

```
Timer: 34 ticks
Thread: 0 idle ticks, 34 kernel ticks, 0 user ticks
```

### 7-4. PANIC (커널이 죽은 경우)

`.output` 마지막 쪽에 아래처럼 찍힙니다. (실제로 겪은 예)

```
Kernel PANIC at ../../lib/kernel/list.c:78 in list_next(): assertion `is_head (elem) || is_interior (elem)' failed.
Call stack: 0x8004213edc 0x800421418e 0x8004206c12 0x800420d7d9 ...
```

`Call stack`의 주소를 함수 이름으로 바꿔 줍니다:

```bash
backtrace kernel.o 0x8004213edc 0x800421418e 0x8004206c12
```

주소마다 `함수 (파일:줄)`이 한 줄씩 나옵니다. 위에서 아래로 읽으면 "누가 누구를 불러서 죽었는지"가 보입니다. (출력 형식은 예시이며 직접 실행해 확인하진 않았습니다.)

> `tests.c:93 in fail(): test failed` 패닉은 테스트가 스스로 "틀렸다"고 선언한 것입니다. 위의 `Differences` 부분을 보세요.

### 7-5. Alarm의 busy waiting 검증 (idle ticks)

**`PASS`만으로는 busy waiting이 없어졌는지 알 수 없습니다.** busy waiting으로 구현해도 시각은 맞게 깨우기 때문입니다. 대신 `.output` 끝의 `idle ticks`를 봅니다.

```bash
grep -H "idle ticks" tests/threads/alarm-*.output
```

| 구현 | idle ticks |
|---|---|
| busy waiting | **0** (Timer와 kernel ticks가 거의 같음) |
| block 방식 (정상) | **크게 증가** (잠든 동안 CPU가 idle로 감) |

정상 구현에서 실측한 값:

```
alarm-single:       Timer: 288 ticks  /  250 idle ticks, 38 kernel ticks
alarm-multiple:     Timer: 594 ticks  /  550 idle ticks, 44 kernel ticks
```

`alarm-zero`, `alarm-negative`는 아예 재우지 않는 테스트라 `idle ticks: 0`이 정상입니다.

---

## 8. 자주 하는 실수

| 증상 | 원인 | 해결 |
|---|---|---|
| `No such file or directory: /workspaces/...` | **맥(호스트) 터미널**에서 컨테이너 경로를 실행함. 프롬프트가 `jiho0215@`면 호스트 | VS Code 터미널(컨테이너, `jungle@...`)에서 실행 |
| `zsh: no matches found: tests/threads/alarm-*.result` | `build` 폴더가 아닌 곳에서 상대경로로 실행 | `cd .../pintos/threads/build` 후 실행 |
| `make: ... is up to date.`만 나오고 아무 일도 없음 | 이미 결과가 있어서 건너뜀 (에러 아님) | `.output`과 `.result`를 지우고 다시 실행 (3장 ①) |
| `.output`이 비어 있고 `Run produced no output at all` | 비대화형 셸이라 `pintos` 명령이 PATH에 없음 (`pintos: not found`) | `source ../../activate` 후 다시 실행. VS Code 터미널은 자동으로 적용됨 |
| `undefined reference to ...`, `error:` 등이 나오며 `make`가 멈춤 | 코드가 컴파일/링크되지 않음. 이 상태에서 `.result`를 지웠다면 점수에 안 잡힘 | 에러를 고치고 `make`가 통과한 뒤 다시 실행 |
| `warning: ... not tested, assuming failure` | 그 테스트의 `.result`가 없음 (안 돌렸거나 빌드 실패로 지워짐) | 3장 ①로 실행 |
| VS Code 탐색기에 `build/`가 안 보임 | `.gitignore`의 `**/build/`를 VS Code가 숨김 | 설정 `explorer.excludeGitIgnore` 해제, 또는 `code <경로>`로 열기 |
| `make check` 끝에 `Error 1` | 실패한 테스트가 있다는 정상 신호 | 실패 목록 확인 |
| 코드를 고쳤는데 결과가 그대로 | `.result`가 있으면 make가 건너뜀 | 지우고 다시 실행 |
| MLFQS가 너무 오래 걸림 | 구현 전에는 타임아웃(60초)이 여럿 | 영역별로 나눠서 실행, 구현 후 다시 확인 |

---

## 9. 디버깅 (GDB)와 메뉴 스크립트

### 커널 출력 직접 보기 (채점 없음)

```bash
pintos -- -q run alarm-multiple
```

테스트 출력과 `Timer`, `idle ticks` 통계가 나옵니다. 채점은 하지 않습니다.

### GDB

```bash
pintos --gdb -- -q run alarm-multiple
```

QEMU가 `localhost:1234`에서 gdb 접속을 기다립니다. 다른 터미널에서 접속해 중단점을 걸고 추적합니다.

### 메뉴 스크립트 (선택)

[picky232/pintos-util](https://github.com/picky232/pintos-util)의 `select_test.sh`는 번호로 테스트를 골라 실행합니다.

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads
curl -fsSLO https://raw.githubusercontent.com/picky232/pintos-util/main/threads/select_test.sh
curl -fsSLO https://raw.githubusercontent.com/picky232/pintos-util/main/threads/.test_config
chmod +x select_test.sh
./select_test.sh -q -r        # -q 결과만, -g GDB 모드, -r clean 후 재빌드
```
