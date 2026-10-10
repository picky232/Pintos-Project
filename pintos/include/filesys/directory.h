#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/disk.h"

/* 파일 이름 구성 요소(component) 하나의 최대 길이.
 * 전통적인 UNIX의 최대 길이임.
 * 디렉터리를 구현한 뒤에도 이 길이를 유지할 수 있지만,
 * 훨씬 긴 전체 경로 이름은 허용해야 함. */
#define NAME_MAX 14

struct inode;

/* 디렉터리 열기와 닫기. */
bool dir_create (disk_sector_t sector, size_t entry_cnt);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);

/* 읽기와 쓰기. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, disk_sector_t);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);

#endif /* filesys/directory.h */
