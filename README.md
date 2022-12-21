
# Pintos 운영체제 과제
SW 정글 사관학교 운영체제 과정

## About
- [Threads](#Threads)
    - CPU의 가상화로 multi-tasking이 가능한 운영체제 구현
    - 스레드 동기화 문제 해결
- [User Program](#User_Program)
    - 유저와의 interaction을 위해 system call로 의해 OS와 소통
    - 유저의 요청사항 확인 및 보호
- [Vitural Memory](#Virtual_Memory)
    - 물리 메모리의 크기 공간을 극복하기 위해, 프로그램 별 가상 메모리 제공
    - page 기법을 활용해 메모리 관리
- [File System](#File_System)
    - 스토리지의 가상화로 FAT file system 구현
    - 디렉터리 계층화 및 Soft Link 구현

### Assignment [[Goto]](https://casys-kaist.github.io/pintos-kaist/)
- 과제 해석 후 skeleton code위해 필요한 기능을 추가

### Stack
<img src="https://img.shields.io/badge/Language-A8B9CC?style=flat-square&logo=C&logoColor=white"/>


### Development Period
- 2022.11.10 - 2022.12.20

## Implement

## Threads

## User Program

## Virtual Memory

**Memory Management**

- [x] Page Structure and Operations
    - page_operations - unint, anon, file_backed
    - page type에 따른 다른 기능 추가
- [x] Implement Supplemental Page Table
    - supplemental_page_table_init, spt_find_page, spt_insert_page
    - pml4 생성 및 spt에 idx 및 value 삽입
- [x] Frame Management
    - vm_get_frame, vm_do_claim_page, vm_claim_page
    - frame 생성 및 page type에 따른 operation 수행
    - page과 frame 매핑
- 

## File System
- [x] Indexing large files with FAT(File Allocation Table)
    - fat_init, fat_open, fat_close, fat_create, fat_boot_create
    - 부팅 시에 FAT을 메인 메모리에 생성 및 변경된 내용을 디스크에 업데이트
- [x] File Growth
    - inode_write_at, inode_read_at
    - file의 크기가 initial size보다 클 경우, sectors를 추가
- [x] Subdirectories
    - chdir, mkdir, readdir, isdir, inumber
    - 기존의 single directory에서 계층적
- [ ] Soft Link
    - symlink




### PARTICIPANT
길인식, 이송희, 김채욱
* 과제 구현을 위해 팀 단위로 협업