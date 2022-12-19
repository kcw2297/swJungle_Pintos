#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/fat.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format(void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void filesys_init(bool format)
{
	filesys_disk = disk_get(0, 1);
	if (filesys_disk == NULL)
		PANIC("hd0:1 (hdb) not present, file system initialization failed");

	inode_init();

#ifdef EFILESYS
	fat_init();

	if (format)
		do_format();

	fat_open();
	struct thread *initial_thread = thread_current();
	initial_thread->cur_dir = dir_open_root();
#else
	/* Original FS */
	free_map_init();

	if (format)
		do_format();

	free_map_open();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void filesys_done(void)
{
	/* Original FS */
#ifdef EFILESYS
	fat_close();
#else
	free_map_close();
#endif
}

struct dir *parse_path(char *path_name, char *file_name)
{
	struct thread *curr = thread_current();
	struct dir *dir;
	struct inode *inode;

	char *path = (char *)malloc(strlen(path_name) + 1);
	strlcpy(path, path_name, strlen(path_name) + 1);

	if (path == NULL || file_name == NULL)
		return NULL;

	if (strlen(path) == 0)
		return NULL;

	if (path[0] == '/')
		dir = dir_open_root();
	else
		dir = dir_reopen(curr->cur_dir);

	/* PATH_NAME의 절대/상대경로에 따른 디렉터리 정보 저장 (구현)*/
	char *token, *nextToken, *savePtr;
	token = strtok_r(path, "/", &savePtr);
	nextToken = strtok_r(NULL, "/", &savePtr);

	while (token != NULL && nextToken != NULL)
	{
		/* dir에서 token이름의 파일을 검색하여 inode의 정보를 저장*/
		if (!dir_lookup(dir, token, &inode))
			goto fail;

		/* inode가 파일일 경우 NULL 반환 */
		if (!inode_is_dir(inode))
			// return NULL;
			goto fail;

		/* dir의 디렉터리 정보를 메모리에서 해지 */

		dir_close(dir);

		/* inode의 디렉터리 정보를 dir에 저장 */
		dir = dir_open(inode);

		/* token에 검색할 경로 이름 저장 */
		token = nextToken;
		nextToken = strtok_r(NULL, "/", &savePtr);
	}

	if (token == NULL)
		strlcpy(file_name, ".", 2); // '/'인 경우 .이다
	else
		strlcpy(file_name, token, strlen(token) + 1);
	
	free(path);
	/* dir 정보 반환 */
	return dir;

fail:
	dir_close(dir);
	if (inode)
		inode_close(inode);
	free(path);

	return NULL;
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool filesys_create(const char *name, off_t initial_size) // name: file_path
{
	struct thread *curr = thread_current();
	char file_name[NAME_MAX + 1];

	char *copy_name = (char *)malloc(strlen(name) + 1);
	if (copy_name == NULL)
		return false;
	strlcpy(copy_name, name, strlen(name) + 1);

	cluster_t inode_cluster = fat_create_chain(0);
	disk_sector_t inode_sector = cluster_to_sector(inode_cluster);

	// struct dir *dir = dir_open_root();
	struct dir *dir = parse_path(copy_name, file_name);

	bool success = (dir != NULL && inode_create(inode_sector, initial_size, INODE_FILE) && dir_add(dir, file_name, inode_sector));

	if (!success && inode_sector != 0)
		// free_map_release (inode_sector, 1);
		fat_remove_chain(inode_cluster, 0);
	dir_close(dir);
	free(copy_name);
	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
/* 성공하면 새 파일을 반환하고 그렇지 않으면 null 포인터를 반환
   이름이 NAME인 파일이 없거나 내부 메모리 할당이 실패할 경우 실패*/
struct file *
filesys_open(const char *name)
{
	// struct dir *dir = dir_open_root();
	char file_name[NAME_MAX + 1];
	struct dir *dir = parse_path(name, file_name);

	struct inode *inode = NULL;
	// char cp_name[15] = *name;
	// printf("===========> file name :: %s \n", name);
	// printf("===========> dir :: %d \n", dir.inode.sector);
	// parse_path(cp_name)

	if (dir != NULL)
		dir_lookup(dir, file_name, &inode);
	dir_close(dir);
	// printf("===========>  :: %p \n", dir);

	return file_open(inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool filesys_remove(const char *name)
{
	char file_name[NAME_MAX + 1];
	struct dir *dir = parse_path(name, file_name);
	bool success = false;
	struct inode *new_node;

	if( dir_lookup(dir, file_name, &new_node) && inode_is_dir(new_node))
		return false;

	success = dir_remove(dir, file_name);

	dir_close(dir);

	return success;
}

/* Formats the file system. */
static void
do_format(void)
{
	printf("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create();

	/* Root Directory 생성 */
	disk_sector_t root = cluster_to_sector(ROOT_DIR_CLUSTER);
	if (!dir_create(root, 16))
		PANIC("root directory creation failed");

	struct dir *root_dir = dir_open_root();
	dir_add(root_dir, ".", root);
	dir_add(root_dir, "..", root);
	dir_close(root_dir);

	fat_close();
#else
	free_map_create();
	if (!dir_create(ROOT_DIR_SECTOR, 16))
		PANIC("root directory creation failed");
	free_map_close();
#endif

	printf("done.\n");
}

bool filesys_create_dir(char *name){
	bool success;

	char file_name[NAME_MAX + 1];
	struct dir *dir = parse_path(name, file_name);
	if (dir == NULL)
		return false;

	cluster_t inode_cluster = fat_create_chain(0);
	disk_sector_t inode_sector = cluster_to_sector(inode_cluster);

	struct inode *sub_dir_inode;
	struct dir *sub_dir = NULL;

	bool succ_create = dir_create(inode_sector, 16);
	bool succ_add = dir_add(dir, file_name, inode_sector);
	bool succ_lookup = dir_lookup(dir, file_name, &sub_dir_inode);
	bool succ_create_curr_dir = dir_add(sub_dir = dir_open(sub_dir_inode), ".", inode_sector);
	bool succ_create_prev_dir = dir_add(sub_dir, "..", inode_get_inumber(dir_get_inode(dir)));

	success = (succ_create && succ_add && succ_lookup && succ_create_curr_dir && succ_create_prev_dir);
	if (!success && inode_cluster != 0)
		fat_remove_chain(inode_cluster, 0);

	dir_close(sub_dir);
	dir_close(dir);

	return success;
}
