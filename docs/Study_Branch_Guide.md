# 📚 공부 브랜치 가이드 (로컬 전용)

> Pintos 코드를 읽으며 주석을 달 때 팀원끼리 **충돌(conflict)** 이 나지 않게 하는 방법입니다.
> 이 문서는 내 `study-<ID>` 브랜치에만 있는 개인 메모입니다. (push 하지 않음)

---

## 1. 왜 주석에서 충돌이 나나요?

충돌은 두 사람이 **같은 파일의 같은 위치**를 서로 다르게 고쳤을 때 생깁니다.
여러 명이 같은 Pintos 원본 코드를 읽으며 같은 함수 근처에 주석을 달면 거의 무조건 충돌이 납니다.

→ 충돌을 잘 푸는 것보다 **주석을 서로 다른 곳에 두는 것**이 답입니다.

---

## 2. 주석을 두 종류로 나누기

| 종류 | 예 | 어디에 |
|---|---|---|
| **학습 주석** (읽으면서 이해한 것) | "여기서 인터럽트를 끄는 이유는…" | 내 `study-<ID>` 브랜치에만 |
| **구현 주석** (내가 짠 코드 설명) | "donation 체인을 따라 올라감" | 개인 작업 브랜치 → PR. **그 함수 담당자만** 작성 |

- 내가 맡지 않은 함수에는 주석을 달지 않습니다. 할 말은 PR 리뷰 댓글로.
- 팀에 공유하고 싶은 내용은 TIL(`docs/날짜/TIL/<GitHub ID>.md`)에 정리해서 올립니다.

---

## 3. study 브랜치 규칙

| 규칙 | 이유 |
|---|---|
| **로컬 전용** — push 하지 않음 | 원격 브랜치 목록이 깔끔하게 유지됨 |
| **PR 금지** — 어디에도 머지하지 않음 | 학습 주석이 공유 코드에 섞이면 충돌의 원인이 됨 |
| wip 커밋 자유롭게 | 어차피 나만 보는 기록 |

만들기 (처음 1번):
```bash
git fetch origin && git switch -c study-picky232 origin/dev
```

> 백업이 필요하거나 팀원에게 주석을 통째로 보여주고 싶을 때만 push 하세요. (push 해도 PR 은 금지)

---

## 4. 브랜치 만들고 GitHub 에 올리기

패턴은 하나: **`fetch` → `switch -c 이름 출발점` → `push -u`**

**기능 브랜치** (기능당 1번, 한 명만)
```bash
git fetch origin
```
```bash
git switch -c feature-priority origin/dev
```
```bash
git push -u origin HEAD
```

| 명령 | 하는 일 |
|---|---|
| `git fetch origin` | GitHub 최신 상태를 받아옴 (내 코드는 안 바뀜) |
| `git switch -c 이름 출발점` | `출발점`을 복사해 새 브랜치를 **내 컴퓨터에** 만들고 이동. 출발점 생략 시 지금 브랜치에서 땀 |
| `git push -u origin HEAD` | 지금 브랜치(`HEAD`)를 GitHub 에 같은 이름으로 올리고 연결(`-u`). 이때 **GitHub 에도 브랜치가 생김**. 이후엔 `git push` 만 |

**개인 브랜치**
```bash
git switch -c feature-priority-picky232
```
- 바로 위에서 `feature-priority` 를 만들었다면 지금 거기 있으니 출발점 생략
- 다른 팀원이 만든 기능 브랜치에서 딸 때:
  ```bash
  git fetch origin && git switch -c feature-priority-picky232 origin/feature-priority
  ```
- 작업 · 커밋 후 `git push -u origin HEAD` → PR (base = `feature-priority`)

```
GitHub:     dev ─────▶ feature-priority ─────────────────▶ (PR 로 합쳐짐)
                            ▲ push                              ▲ PR
내 컴퓨터:  feature-priority ─▶ feature-priority-picky232 ─ push ┘
```

> 웹으로 만들기: 레포 → 브랜치 드롭다운 → 출발점 `dev` 선택 → 이름 입력 → **Create branch** → 로컬에서 `git fetch origin && git switch feature-priority`

---

## 5. 브랜치를 바꿀 때 — 작업상황 유지하기

커밋하지 않은 변경은 **브랜치를 따라다닙니다.**
그래서 주석을 달다가 그대로 `git switch` 하면 주석이 구현 브랜치로 넘어가거나, Git 이 switch 를 막습니다.

**규칙: 브랜치를 바꾸기 전에 커밋한다.**
```bash
git add -A && git commit -m "wip: 주석"
```
```bash
git switch feature-priority-picky232
```
- 커밋하면 작업상황이 그 브랜치에 저장되고, 다시 돌아오면 그대로 있습니다.
- 구현 브랜치에서도 같습니다. 바꾸기 전에 커밋 (개인 브랜치라 wip 커밋 괜찮음).

---

## 6. dev 최신 코드 받기

이 브랜치는 `origin/dev` 를 따라가도록 만들었으므로:
```bash
git pull
```
- 내 주석과 팀원 코드가 같은 줄이면 충돌이 날 수 있지만, 내 로컬 브랜치라 편하게 정리하면 됩니다.
- 주석 정리가 귀찮으면 dev 를 받기 전에 커밋해 두고, 충돌 난 부분은 팀 코드를 우선으로 고르고 주석만 다시 붙이기.

---

## 7. (선택) 두 브랜치를 동시에 열기 — worktree

공부용 폴더를 하나 더 만들어서 구현 폴더와 나란히 열 수 있습니다.
```bash
git worktree add "../pintos-study" study-picky232
```
- `../pintos-study` 폴더를 **VSCode 로 그냥 열어서 읽기 · 주석 전용**으로 사용
- ⚠️ 개발 컨테이너는 폴더 이름이 `pintos_22.04_lab_docker` 여야 동작하므로, worktree 폴더에서는 **컨테이너로 빌드하지 않기**
- 같은 브랜치는 한 폴더에서만 열 수 있음 (메인 폴더는 다른 브랜치로 switch 해 두기)
- 정리할 때:
  ```bash
  git worktree remove "../pintos-study"
  ```

---

## 8. 하지 말 것 🚫

| 하지 말 것 | 이유 |
|---|---|
| study 브랜치를 PR 로 올리기 | 학습 주석이 팀 코드에 섞임 |
| 남의 담당 함수에 주석 달고 PR | 충돌 원인. 리뷰 댓글로 대신 |
| 커밋 안 하고 브랜치 바꾸기 | 주석이 다른 브랜치로 따라감 |
| `.gitattributes` 의 `merge=union` 사용 | 충돌 시 양쪽 줄을 다 남겨서 코드가 조용히 깨질 수 있음 |
