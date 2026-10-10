#include "filesys/fsutil.h"
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* 루트 디렉터리의 파일 목록을 출력함. */
void
fsutil_ls (char **argv UNUSED) {
	struct dir *dir;
	char name[NAME_MAX + 1];

	printf ("Files in the root directory:\n");
	dir = dir_open_root ();
	if (dir == NULL)
		PANIC ("root dir open failed");
	while (dir_readdir (dir, name))
		printf ("%s\n", name);
	printf ("End of listing.\n");
}

/* 파일 ARGV[1]의 내용을 시스템 콘솔에 16진수와 ASCII로
 * 출력함. */
void
fsutil_cat (char **argv) {
	const char *file_name = argv[1];

	struct file *file;
	char *buffer;

	printf ("Printing '%s' to the console...\n", file_name);
	file = filesys_open (file_name);
	if (file == NULL)
		PANIC ("%s: open failed", file_name);
	buffer = palloc_get_page (PAL_ASSERT);
	for (;;) {
		off_t pos = file_tell (file);
		off_t n = file_read (file, buffer, PGSIZE);
		if (n == 0)
			break;

		hex_dump (pos, buffer, n, true); 
	}
	palloc_free_page (buffer);
	file_close (file);
}

/* 파일 ARGV[1]을 삭제함. */
void
fsutil_rm (char **argv) {
	const char *file_name = argv[1];

	printf ("Deleting '%s'...\n", file_name);
	if (!filesys_remove (file_name))
		PANIC ("%s: delete failed\n", file_name);
}

/* "scratch" 디스크(hdc 또는 hd1:0)에서 파일 시스템의 파일 ARGV[1]로
 * 복사함.
 *
 * scratch 디스크의 현재 섹터는 문자열
 * "PUT\0"으로 시작해야 하고 그 뒤에 32비트 리틀 엔디언 정수가 와서
 * 파일 크기를 바이트 단위로 나타내야 함. 이후 섹터에는
 * 파일 내용이 들어 있음.
 *
 * 이 함수를 처음 호출하면 scratch 디스크의
 * 맨 앞부터 읽음. 이후 호출은 디스크를 따라 앞으로 나아감.
 * 이 디스크 위치는 fsutil_get()이 쓰는 위치와 독립적이므로
 * 모든 `put`은 모든 `get`보다 먼저 해야 함. */
void
fsutil_put (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	struct disk *src;
	struct file *dst;
	off_t size;
	void *buffer;

	printf ("Putting '%s' into the file system...\n", file_name);

	/* 버퍼를 할당함. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("couldn't allocate buffer");

	/* 원본 디스크를 열고 파일 크기를 읽음. */
	src = disk_get (1, 0);
	if (src == NULL)
		PANIC ("couldn't open source disk (hdc or hd1:0)");

	/* 파일 크기를 읽음. */
	disk_read (src, sector++, buffer);
	if (memcmp (buffer, "PUT", 4))
		PANIC ("%s: missing PUT signature on scratch disk", file_name);
	size = ((int32_t *) buffer)[1];
	if (size < 0)
		PANIC ("%s: invalid file size %d", file_name, size);

	/* 대상 파일을 만듦. */
	if (!filesys_create (file_name, size))
		PANIC ("%s: create failed", file_name);
	dst = filesys_open (file_name);
	if (dst == NULL)
		PANIC ("%s: open failed", file_name);

	/* 복사를 수행함. */
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		disk_read (src, sector++, buffer);
		if (file_write (dst, buffer, chunk_size) != chunk_size)
			PANIC ("%s: write failed with %"PROTd" bytes unwritten",
					file_name, size);
		size -= chunk_size;
	}

	/* 마무리. */
	file_close (dst);
	free (buffer);
}

/* 파일 시스템의 파일 FILE_NAME을 scratch 디스크로 복사함.
 *
 * scratch 디스크의 현재 섹터는 "GET\0"을 받고,
 * 그 뒤에 파일 크기를 바이트 단위의 32비트
 * 리틀 엔디언 정수로 받음. 이후 섹터들은 파일의
 * 데이터를 받음.
 *
 * 이 함수를 처음 호출하면 scratch 디스크의 맨 앞부터
 * 씀. 이후 호출은 디스크를 따라 앞으로 나아감.
 * 이 디스크 위치는 fsutil_put()이 쓰는 위치와 독립적이므로
 * 모든 `put`은 모든 `get`보다 먼저 해야 함. */
void
fsutil_get (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	void *buffer;
	struct file *src;
	struct disk *dst;
	off_t size;

	printf ("Getting '%s' from the file system...\n", file_name);

	/* 버퍼를 할당함. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("couldn't allocate buffer");

	/* 원본 파일을 엶. */
	src = filesys_open (file_name);
	if (src == NULL)
		PANIC ("%s: open failed", file_name);
	size = file_length (src);

	/* 대상 디스크를 엶. */
	dst = disk_get (1, 0);
	if (dst == NULL)
		PANIC ("couldn't open target disk (hdc or hd1:0)");

	/* 섹터 0에 크기를 씀. */
	memset (buffer, 0, DISK_SECTOR_SIZE);
	memcpy (buffer, "GET", 4);
	((int32_t *) buffer)[1] = size;
	disk_write (dst, sector++, buffer);

	/* 복사를 수행함. */
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		if (sector >= disk_size (dst))
			PANIC ("%s: out of space on scratch disk", file_name);
		if (file_read (src, buffer, chunk_size) != chunk_size)
			PANIC ("%s: read failed with %"PROTd" bytes unread", file_name, size);
		memset (buffer + chunk_size, 0, DISK_SECTOR_SIZE - chunk_size);
		disk_write (dst, sector++, buffer);
		size -= chunk_size;
	}

	/* 마무리. */
	file_close (src);
	free (buffer);
}
