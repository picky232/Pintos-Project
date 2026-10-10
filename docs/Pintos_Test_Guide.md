# Pintos 테스트 가이드

> 모든 명령은 **Docker 컨테이너 안**의 `build` 폴더에서 실행합니다.
> ```bash
> cd /workspaces/pintos_22.04_lab_docker/pintos/threads/build
> ```

## 목차

1. [명령 한눈에 보기](#1-명령-한눈에-보기)
2. [테스트 실행하기](#2-테스트-실행하기)
3. [결과 확인하기](#3-결과-확인하기)
4. [점수 보기](#4-점수-보기)
5. [Alarm Clock 구현 검증 (idle ticks)](#5-alarm-clock-구현-검증-idle-ticks)
6. [테스트 목록과 배점](#6-테스트-목록과-배점)
7. [디버깅](#7-디버깅)
8. [메뉴 스크립트 (선택)](#8-메뉴-스크립트-선택)

---

## 1. 명령 한눈에 보기

| 하고 싶은 것 | 명령 | 채점 |
|---|---|---|
| 테스트 하나 실행 | `make tests/threads/<이름>.result` | O |
| 전체 pass/FAIL 확인 | `make check` | O |
| 전체 점수 | `make grade` | O |
| **Alarm만 점수** | [4-2](#4-2-alarm만-점수-보기) | O |
| **Alarm 전부 + idle ticks** | [5장](#5-alarm-clock-구현-검증-idle-ticks) | X |
| 커널 출력을 눈으로 확인 | `pintos -- -q run <이름>` | X |
| GDB 디버깅 | `pintos --gdb -- -q run <이름>` | X |

- `make ...result`, `check`, `grade`는 같은 채점기(`.ck`)를 씁니다. 판정은 같고 **실행 범위와 출력 형태**만 다릅니다.
- `pintos -- -q run`은 채점 없이 커널 출력만 보여 줍니다.

---

## 2. 테스트 실행하기

`.result`는 미리 있는 파일이 아니라 **테스트를 돌려서 만들어지는 결과 파일**입니다. 이 이름을 `make`의 목표로 주면 테스트가 실행됩니다.

```bash
make tests/threads/alarm-single.result
```

```
pass tests/threads/alarm-single        ← 통과
FAIL tests/threads/alarm-single        ← 실패 (아래에 이유가 같이 출력)
```

### 이름 규칙
`tests/threads/` + `테스트이름` + `.result`

mlfqs 테스트만 폴더가 하나 더 붙습니다: `tests/threads/mlfqs/<이름>.result`

### 여러 개 한 번에
공백으로 이어 씁니다. `-k`를 붙이면 중간에 에러가 나도 끝까지 실행합니다.

```bash
make -k tests/threads/alarm-single.result tests/threads/alarm-multiple.result
```

Alarm 6개 전부:

```bash
make -k tests/threads/alarm-{single,multiple,simultaneous,priority,zero,negative}.result
```

### 코드를 고치고 다시 실행하기
같은 이름의 `.result`가 있으면 `up to date`만 나오고 **다시 실행되지 않습니다.** 빌드한 뒤 이전 결과를 지우고 실행하세요.

```bash
make
rm tests/threads/alarm-single.output tests/threads/alarm-single.result
make tests/threads/alarm-single.result
```

전부 초기화: `make clean && make`

---

## 3. 결과 확인하기

`make`를 실행하면 화면에 `pass`/`FAIL`이 바로 나옵니다. 파일은 필요할 때만 봅니다.

| 파일 | 내용 | 언제 보나 |
|---|---|---|
| `tests/threads/<이름>.result` | `PASS` 또는 `FAIL` + diff | `up to date`로 건너뛰어 화면에 안 나왔을 때 |
| `tests/threads/<이름>.output` | 커널이 실제로 찍은 전체 출력 | FAIL 원인을 자세히 볼 때 |
| `tests/threads/<이름>.errors` | panic, assertion 메시지 | 실행 도중 죽었을 때 |

```bash
cat tests/threads/alarm-single.result
cat tests/threads/alarm-single.output
```

---

## 4. 점수 보기

### 4-1. 전체 점수

```bash
make check     # 테스트별 pass/FAIL 목록 + 요약 (예: "1 of 27 tests failed.")
make grade     # 영역별 점수 + TOTAL TESTING SCORE (build/grade 파일에도 저장)
```

전체를 돌리므로 오래 걸립니다.

### 4-2. Alarm만 점수 보기

`make-grade`는 결과 파일과 루브릭 파일만 읽는 스크립트라서, Alarm 결과만 모아 넘기면 Alarm 점수만 계산됩니다.

**① Alarm 6개 실행**

```bash
make -k tests/threads/alarm-{single,multiple,simultaneous,priority,zero,negative}.result
```

**② Alarm만 채점** (실패 상세는 잘라서 요약만 출력)

```bash
for t in single multiple simultaneous priority zero negative; do if echo PASS | cmp -s tests/threads/alarm-$t.result -; then echo "pass tests/threads/alarm-$t"; else echo "FAIL tests/threads/alarm-$t"; fi; done > alarm.results && printf '20.0%%\ttests/threads/Rubric.alarm\n' > alarm.grading && ../../tests/make-grade ../.. alarm.results alarm.grading | sed '/^DETAILS OF/,$d'
```

출력 예 (현재 상태):

```
SUMMARY BY TEST SET

Test Set                                      Pts Max  % Ttl  % Max
--------------------------------------------- --- --- ------ ------
tests/threads/Rubric.alarm                      5/  7  14.3%/ 20.0%

Functionality and robustness of alarm clock (tests/threads/Rubric.alarm):
	     1/ 1 tests/threads/alarm-single
	     1/ 1 tests/threads/alarm-multiple
	     1/ 1 tests/threads/alarm-simultaneous
	  ** 0/ 2 tests/threads/alarm-priority      ← ** = 실패
	     1/ 1 tests/threads/alarm-zero
	     1/ 1 tests/threads/alarm-negative

	      5/  6 tests passed
	      5/  7 points subtotal
```

| 항목 | 의미 |
|---|---|
| `5/ 7` | 얻은 점수 / Alarm 만점 |
| `14.3%/ 20.0%` | 전체 점수에서 차지하는 몫 / Alarm 비중(20%) |
| `**` | 실패한 테스트 |

- 실패 원인(기대 출력과 실제 출력의 diff)을 보려면 명령 끝의 `| sed '/^DETAILS OF/,$d'`를 빼세요. 단, 커널 부팅 로그까지 길게 나옵니다.
- `alarm.results`, `alarm.grading`은 `build/` 안의 임시 파일이라 지워도 됩니다.

---

## 5. Alarm Clock 구현 검증 (idle ticks)

**`pass`만으로는 busy waiting이 제거됐는지 알 수 없습니다.** busy waiting 버전도 깨어나는 시각은 맞게 내서 테스트를 통과하기 때문입니다. 대신 커널이 끝날 때 찍는 `idle ticks`로 확인합니다.

```bash
for t in single multiple simultaneous priority zero negative; do echo "== alarm-$t"; pintos -- -q run alarm-$t < /dev/null 2>&1 | grep -E "Timer:|idle ticks"; done
```

이미 만들어진 출력 파일에서 바로 보려면 (실행 없이):

```bash
grep -H "idle ticks" tests/threads/alarm-*.output
```

### 읽는 법

```
Timer: 278 ticks                                          ← 테스트 전체 소요 시간
Thread: 0 idle ticks, 278 kernel ticks, 0 user ticks      ← idle / 그 외 스레드가 쓴 틱
```

| 구분 | idle ticks | kernel ticks |
|---|---|---|
| 구현 전 (busy waiting) | **0** | Timer와 거의 같음 |
| 구현 후 (block 방식) | **크게 증가** | 크게 감소 |

- 모든 스레드가 자는 동안 CPU가 idle 스레드로 가기 때문에, 제대로 구현하면 `idle ticks`가 늘어납니다.
- `alarm-zero`, `alarm-negative`는 애초에 재우지 않는 테스트라 전후 차이가 없습니다. 비교는 `single`, `multiple`, `simultaneous`, `priority`에서 합니다.

### 구현 전 기준값 (busy waiting)

| 테스트 | Timer | idle ticks | kernel ticks |
|---|---|---|---|
| alarm-single | 278 | 0 | 278 |
| alarm-multiple | 590 | 0 | 590 |
| alarm-simultaneous | 283 | 0 | 283 |
| alarm-priority | 526 | 0 | 526 |
| alarm-zero | 23 | 0 | 23 |
| alarm-negative | 24 | 0 | 24 |

---

## 6. 테스트 목록과 배점

### 전체 비중 (`tests/threads/Grading`)

| 영역 | 비중 | 루브릭 |
|---|---|---|
| Alarm Clock | 20% | `tests/threads/Rubric.alarm` |
| Priority Scheduling | 50% | `tests/threads/Rubric.priority` |
| MLFQS | 30% | `tests/threads/mlfqs/Rubric` |

### Alarm (7점)

| 테스트 | 배점 | 현재 |
|---|---|---|
| alarm-single | 1 | pass |
| alarm-multiple | 1 | pass |
| alarm-simultaneous | 1 | pass |
| alarm-priority | **2** | FAIL |
| alarm-zero | 1 | pass |
| alarm-negative | 1 | pass |

> **`alarm-priority`는 Alarm Clock만으로는 통과하지 못할 수 있습니다.** 깨어난 스레드가 우선순위 순서로 실행되어야 하므로 Priority Scheduling(ready_list 우선순위 반영)이 필요합니다. Alarm 구현 직후에는 이것만 FAIL이어도 정상일 수 있습니다.

### Priority

priority-change, priority-preempt, priority-fifo, priority-sema, priority-condvar,
priority-donate-one, priority-donate-multiple, priority-donate-multiple2, priority-donate-nest,
priority-donate-chain, priority-donate-sema, priority-donate-lower

### MLFQS (`tests/threads/mlfqs/` 아래)

mlfqs-load-1, mlfqs-load-60, mlfqs-load-avg, mlfqs-recent-1, mlfqs-fair-2, mlfqs-fair-20, mlfqs-nice-2, mlfqs-nice-10, mlfqs-block

> `pintos`로 mlfqs 테스트를 직접 실행할 때는 `-mlfqs` 옵션이 필요합니다: `pintos -- -mlfqs -q run mlfqs-load-1`
> `make`로 실행할 때는 자동으로 붙습니다.

### 구현 전 베이스라인 (통과 7개)

alarm 5개(single, multiple, simultaneous, zero, negative) + mlfqs-fair-2, mlfqs-fair-20

구현하면서 이 7개가 계속 통과하는지(회귀 없음) 확인하세요.

---

## 7. 디버깅

### 커널 출력 직접 보기 (채점 없음)

```bash
pintos -- -q run alarm-multiple
```

테스트 출력과 함께 마지막에 `Timer`, `idle ticks` 통계가 나옵니다.

### GDB

```bash
pintos --gdb -- -q run alarm-multiple
```

qemu가 gdb 접속을 기다립니다 (`localhost:1234`). 중단점을 걸어 추적할 때 씁니다.

---

## 8. 메뉴 스크립트 (선택)

[picky232/pintos-util](https://github.com/picky232/pintos-util)의 `select_test.sh`는 번호로 테스트를 골라 실행합니다.

설치 (`pintos/threads`에서):

```bash
curl -fsSLO https://raw.githubusercontent.com/picky232/pintos-util/main/threads/select_test.sh
curl -fsSLO https://raw.githubusercontent.com/picky232/pintos-util/main/threads/.test_config
chmod +x select_test.sh
```

실행 (`pintos/threads`에서, Bash 4 이상 필요):

```bash
./select_test.sh -q -r
```

| 옵션 | 의미 |
|---|---|
| `-q` | 결과만 빠르게 확인 |
| `-g` | GDB 모드 (VS Code 디버깅 연결) |
| `-r` | clean 후 재빌드 |

메뉴에서 번호를 입력합니다: `1-6`(Alarm 전부), `1 3 5`, `2-4`
