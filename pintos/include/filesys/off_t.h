#ifndef FILESYS_OFF_T_H
#define FILESYS_OFF_T_H

#include <stdint.h>

/* 파일 안에서의 오프셋(offset).
 * 여러 헤더가 이 정의만 필요로 하고 다른 것은 필요로 하지 않아서
 * 별도의 헤더로 분리했음. */
typedef int32_t off_t;

/* printf()용 서식 지정자. 예:
 * printf ("offset=%"PROTd"\n", offset); */
#define PROTd PRId32

#endif /* filesys/off_t.h */
