#ifndef THREADS_INTR_STUBS_H
#define THREADS_INTR_STUBS_H

/* 인터럽트 스텁(stub).
 *
 * intr-stubs.S에 있는 작은 코드 조각들로, x86이 낼 수 있는 256개
 * 인터럽트마다 하나씩 있음. 각 스텁은 스택을 조금 조작한 뒤
 * intr_entry()로 점프함.
 * 자세한 내용은 intr-stubs.S를 볼 것.
 *
 * 이 배열은 각 인터럽트 스텁의 진입점을 가리키며,
 * intr_init()이 쉽게 찾을 수 있게 하기 위한 것임. */
typedef void intr_stub_func (void);
extern intr_stub_func *intr_stubs[256];

#endif /* threads/intr-stubs.h */
