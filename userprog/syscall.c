#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "threads/palloc.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "vm/vm.h"
#include "include/filesys/inode.h"
#include "include/filesys/directory.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
// void check_address(void *addr);
struct page* check_address(void *addr);

void halt (void);
void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int write (int fd, const void *buffer, unsigned size); 
int wait (tid_t pid);
tid_t fork (const char *thread_name, struct intr_frame *f);
int exec (const char *file);
int open (const char *file);
int add_file_to_fdt(struct file *file);
struct file *fd_to_file(int fd);
void remove_fd(int fd); 
void close (int fd);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
bool isdir (int fd);
bool chdir (char *path_name);
bool mkdir (char *dir);
bool readdir (int fd, char *name);
int inumber(int fd);

struct lock filesys_lock;


/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	lock_init(&filesys_lock);
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* 주소 값이 유저 영역에서 사용하는 주소 값인지 확인 하는 함수
Pintos에서는 시스템 콜이 접근할 수 있는 주소를 0x8048000~0xc0000000으로 제한함 
유저 영역을 벗어난 영역일 경우 프로세스 종료(exit(-1)) */
// void 
// check_address(void *addr) {
// 	struct thread *cur = thread_current();
// /* 1. 포인터가 가리키는 주소가 유저영역의 주소인지 확인 */
// /* 2. 포인터가 가리키는 주소가 존재하는지 확인 */
// /* 3. 포인터가 가리키는 주소에 해당하는 실주소가 없는 경우 NULL 반환 */
// // || pml4_get_page(cur->pml4, addr) == NULL
// 	if(!is_user_vaddr(addr) || addr == NULL || pml4_get_page(cur->pml4, addr) == NULL){
// 		exit(-1);
// 	}
// /* 잘못된 접근일 경우 프로세스 종료 */ 
// }

struct page* check_address(void *addr) {
	if(is_kernel_vaddr(addr))
	{
		exit(-1);
	}
	struct page* f_page = spt_find_page(&thread_current()->spt, addr);

	if (!f_page)
		exit(-1);

	return f_page;
}

// SYS_READ -> to_write == 1
// SYS_WRITE -> to_write == 0
void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write) {
    for (int i = 0; i < size; i++) {
        struct page* page = check_address(buffer + i);    // 인자로 받은 buffer부터 buffer + size까지의 크기가 한 페이지의 크기를 넘을수도 있음
        if (page == NULL)
            exit(-1);
        if (to_write == true && page->writable == false)
            exit(-1);
    }
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	#ifdef VM
    thread_current()->rsp_stack = f->rsp;
	#endif
	/* 유저 스택에 저장되어 있는 시스템 콜 넘버를 이용해 시스템 콜 핸들러 구현 */
	int sys_num = f->R.rax;
	// check_address(sys_num);  /* 스택 포인터가 유저 영역인지 확인 */
	// printf("===========syscall_handler 안========%d=======\n", sys_num);

	switch (sys_num){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_WRITE:
			check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 0);
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WAIT:
			f->R.rax = wait(f->R.rdi);
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC:
			if(exec(f->R.rdi) == -1){
				exit(-1);
			}
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 1);
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_MMAP:
			f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
			break;
		case SYS_MUNMAP:
			munmap(f->R.rdi);
			break;
		case SYS_ISDIR:
			f->R.rax = isdir(f->R.rdi);
			break;
		case SYS_CHDIR:
			check_address(f->R.rdi);
			f->R.rax = chdir(f->R.rdi);
			break;
		case SYS_MKDIR:
			check_address(f->R.rdi);
			f->R.rax = mkdir(f->R.rdi);
			break;
		case SYS_READDIR:
			f->R.rax = readdir(f->R.rdi, f->R.rsi);
			break;
		case SYS_INUMBER:
			f->R.rax = inumber(f->R.rdi);
			break;
		default:
			// exit(-1);
			// break;
			thread_exit();
	}
}


void
halt (void) {
	power_off();
}

void
exit (int status) {
	struct thread *cur = thread_current();
	/* 프로세스 디스크립터에 exit status 저장 */ 
	cur->exit_status = status;
	printf("%s: exit(%d)\n" , cur->name , status); 
	thread_exit();
}

int
wait (tid_t pid) {
	/* 자식 프로세스가 종료 될 때까지 대기 */
	return process_wait(pid);
}

bool
create (const char *file, unsigned initial_size) {
	if (file)
        return filesys_create(file,initial_size); // ASSERT, dir_add (name!=NULL)
    else
        exit(-1);
	/*
	lock_acquire(&filesys_lock);
	bool succ = filesys_create(file, initial_size);
	lock_release(&filesys_lock);

	return succ;
	*/
}

bool
remove (const char *file) {
	check_address(file);
	/* 파일 이름에 해당하는 파일을 제거 */
	/* 파일 제거 성공 시 true 반환, 실패 시 false 반환 */
	// return filesys_remove(file);
	bool result = false;
	lock_acquire(&filesys_lock);
	result = filesys_remove(file);
	lock_release(&filesys_lock);

	return result;
}


/* 자식 프로세스를 생성하고 프로그램을 실행시키는 시스템 콜 */
int
exec (const char *file) {
	check_address(file);
	char *fn_copy = palloc_get_page(PAL_ZERO);
	if ((fn_copy) == NULL){
		exit(-1);
	}	
	int size = strlen(file) +1 ; // 마지막 null값이라 +1
	strlcpy(fn_copy, file, size);
	/* process_exec() 함수를 호출하여 자식 프로세스 생성 */ 
	if (process_exec(fn_copy) == -1){			/* 프로그램 적재 실패 시 -1 리턴 */
		return -1;
	}

	/* 프로그램 적재 성공 시 자식 프로세스의 pid 리턴 */
	NOT_REACHED();
	return 0;
}

 /* 파일을 현재 프로세스의 fdt에 추가 */
int 
add_file_to_fdt(struct file *file){
	struct thread *cur = thread_current();
	struct file **cur_fd_table = cur->fd_table;
	// while(cur->fdidx < MAX_FD_NUM && cur_fd_table[cur->fdidx]){
	// 	cur->fdidx++;
	// }
	while (cur->fdidx < MAX_FD_NUM && cur_fd_table[cur->fdidx]){
        cur->fdidx++;
    }

    // error - fd table full
    if (cur->fdidx >= MAX_FD_NUM)
        return -1;

    cur_fd_table[cur->fdidx] = file;
    return cur->fdidx;
	// for (int i = cur->fdidx; i < MAX_FD_NUM; i++){
	// 	if (cur_fd_table[i] == NULL){
	// 		cur_fd_table[i] = file;
	// 		cur->fdidx = i;
	// 		return cur->fdidx;
	// 	}
	// }
	// cur->fdidx = MAX_FD_NUM;
	// return -1;
}

int
open (const char *file) {
/* 성공 시 fd를 생성하고 반환, 실패 시 -1 반환 */
	check_address(file);
	if (file == NULL) {
		return -1;
	}
	lock_acquire(&filesys_lock);

	struct file *open_file = filesys_open (file);
	
	if(open_file == NULL){
		lock_release(&filesys_lock);
		return -1;
	}
	
	int fd = add_file_to_fdt(open_file);
	if (fd == -1){ // fd table 가득 찼다면
		file_close(open_file);
	}
	lock_release(&filesys_lock);
	return fd;
}

int
write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	int write_result;
	struct file *file = fd_to_file(fd);
	if(file == NULL){
		return -1;
	}

	if (fd == 1) {	// stdout(표준 출력) - 모니터
		putbuf(buffer, size);
		write_result = size;
	}else if(fd == 0){ // stdin
		write_result = 0;
	}else{ 
		lock_acquire(&filesys_lock);
		write_result = file_write(file, buffer, size);
		lock_release(&filesys_lock);
	}
	return write_result;
}

/* 현재 프로세스의 복제본으로 자식 프로세스를 생성 */
tid_t
fork (const char *thread_name, struct intr_frame *f){
	return process_fork(thread_name, f);
}

/* 현재 프로세스의 fd에 있는 file 반환 */
struct file *
fd_to_file(int fd){
	struct thread *cur = thread_current();
	if(0 <= fd && fd < MAX_FD_NUM){
		return cur->fd_table[fd];
	}else{
		return NULL;
	}
}

void
remove_fd(int fd){
	struct thread *cur = thread_current();
	if(fd< 0 || fd > MAX_FD_NUM){
		return;
	}
	cur->fd_table[fd] = NULL;
}

void
close (int fd) {
	// fd를 file로 변경해서 file_close()인자로 넣기
	struct file *file = fd_to_file(fd);
	if(file == NULL){
		return;
	}
	// file_close(file);
	// fdt 에서 지워주기
	remove_fd(fd);
}

int
filesize (int fd) {
	struct file *file = fd_to_file(fd);
	if(file == NULL){
		return -1;
	}
	return file_length(file);
}

int
read (int fd, void *buffer, unsigned size) {
	// 버퍼의 처음 시작~ 끝 주소 check
	check_address(buffer);
	// check_address(buffer+size-1); // -1은 null 전까지만 유효하면 되서 
	struct file *file = fd_to_file(fd);
	uint8_t *buf = buffer;
	int read_size;

	if(file == NULL){
		return -1;
	}
	// 정상인데 0 일 때, 키보드면 input_get
	if(fd == 0){
		char keyboard;
		for(read_size =0; read_size < size; read_size ++){
			keyboard = input_getc();
			*buf ++ = keyboard;
			if(keyboard == '\0'){ // null 전까지 저장
				break;
			}
		} 
	}else if(fd == 1){ // stdout
		return -1;
	}else{
	// 정상일 때 file_read
		lock_acquire(&filesys_lock);
		read_size = file_read(file, buffer, size);	// 실제 읽은 사이즈 return
		lock_release(&filesys_lock);
	}
	return read_size;
}

/* 다음 읽거나 쓸 file_pos 옮겨주기 */
void
seek (int fd, unsigned position) {
	struct file *file = fd_to_file(fd);

	if(fd < 2){
		return;
	}
	file_seek(file, position);
}

unsigned
tell (int fd) {
	struct file *file = fd_to_file(fd);
	if(fd < 2){
		return;
	}
	return file_tell(file);
}

struct file *process_get_file(int fd) {
	struct thread *curr = thread_current();
	struct file* fd_file = curr->fd_table[fd];

	if(fd_file)
		return fd_file;
	else
		return	NULL;
}


void 
*mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
	if (offset % PGSIZE != 0){        // offset이 페이지 시작점에 있어야한다
		return NULL;
	} 

	if (pg_round_down(addr) != addr 
		|| is_kernel_vaddr(addr) 
		|| addr == NULL
		|| (long long)length <= 0)
		return NULL;
	
	if (fd < 2)
		exit(-1);

	if (spt_find_page(&thread_current()->spt, addr))
		return NULL;

	struct file * target = process_get_file(fd);

	if (target == NULL)
		return NULL;

	void *ret = do_mmap (addr, length, writable, target, offset);

	return ret;
}

void munmap (void *addr){
	do_munmap(addr);
}

bool isdir (int fd) {
	struct file *file = fd_to_file(fd);
	// struct file *file = (struct file*)malloc(sizeof(struct file));
	// memcpy(file, tfile, sizeof(struct file));
	// tfile.
	if(file){
		return inode_is_dir(file->inode);
	}
	return false;
}

bool chdir (char *path_name) {
	
	struct thread *curr = thread_current();
	struct dir *dir;
	struct inode *inode;

	char *path = (char *)malloc(strlen(path_name) + 1);
	strlcpy(path, path_name, strlen(path_name) + 1);

	if (path == NULL)
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
	// nextToken = strtok_r(NULL, "/", &savePtr);

	while (token != NULL )
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
		token = strtok_r(NULL, "/", &savePtr);
	}
	dir_close(curr->cur_dir);
	curr->cur_dir = dir;
	free(path);

	/* dir 정보 반환 */
	return true;

fail:
	dir_close(dir);
	if (inode)
		inode_close(inode);
	free(path);

	return false;
}


bool mkdir (char *dir){
	return filesys_create_dir(dir);
}

bool readdir (int fd, char *name){
	struct file * file = fd_to_file(fd);
	if(!inode_is_dir(file->inode))
		return false;
	// struct dir *dir = dir_open(file->inode);
	struct dir *dir = file;

	bool result = false;
	result = dir_readdir(dir, name);
		
	// dir_close(dir);
	return result;
}

int inumber(int fd){
	struct file * file = fd_to_file(fd);
	if(file == NULL)
		return false;
	return inode_get_inumber(file->inode);
}