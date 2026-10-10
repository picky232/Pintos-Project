#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"

static struct file *free_map_file;   /* 빈 공간 맵(free map) 파일. */
static struct bitmap *free_map;      /* 빈 공간 맵. 디스크 섹터당 1비트. */

/* 빈 공간 맵을 초기화함. */
void
free_map_init (void) {
	free_map = bitmap_create (disk_size (filesys_disk));
	if (free_map == NULL)
		PANIC ("bitmap creation failed--disk is too large");
	bitmap_mark (free_map, FREE_MAP_SECTOR);
	bitmap_mark (free_map, ROOT_DIR_SECTOR);
}

/* 빈 공간 맵에서 연속된 CNT개의 섹터를 할당하고 첫 번째 섹터를
 * *SECTORP에 저장함.
 * 성공하면 true를 반환하고,
 * 모든 섹터가 사용 가능했으면 false를 반환함. */
bool
free_map_allocate (size_t cnt, disk_sector_t *sectorp) {
	disk_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
	if (sector != BITMAP_ERROR
			&& free_map_file != NULL
			&& !bitmap_write (free_map, free_map_file)) {
		bitmap_set_multiple (free_map, sector, cnt, false);
		sector = BITMAP_ERROR;
	}
	if (sector != BITMAP_ERROR)
		*sectorp = sector;
	return sector != BITMAP_ERROR;
}

/* SECTOR에서 시작하는 CNT개의 섹터를 사용 가능하게 만듦. */
void
free_map_release (disk_sector_t sector, size_t cnt) {
	ASSERT (bitmap_all (free_map, sector, cnt));
	bitmap_set_multiple (free_map, sector, cnt, false);
	bitmap_write (free_map, free_map_file);
}

/* 빈 공간 맵 파일을 열고 디스크에서 읽어 옴. */
void
free_map_open (void) {
	free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
	if (free_map_file == NULL)
		PANIC ("can't open free map");
	if (!bitmap_read (free_map, free_map_file))
		PANIC ("can't read free map");
}

/* 빈 공간 맵을 디스크에 쓰고 빈 공간 맵 파일을 닫음. */
void
free_map_close (void) {
	file_close (free_map_file);
}

/* 디스크에 새 빈 공간 맵 파일을 만들고 빈 공간 맵을
 * 거기에 씀. */
void
free_map_create (void) {
	/* inode를 만듦. */
	if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map)))
		PANIC ("free map creation failed");

	/* 비트맵을 파일에 씀. */
	free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
	if (free_map_file == NULL)
		PANIC ("can't open free map");
	if (!bitmap_write (free_map, free_map_file))
		PANIC ("can't write free map");
}
