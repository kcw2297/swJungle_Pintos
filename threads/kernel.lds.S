#include "threads/loader.h"

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)			/* Kernel starts at "start" symbol. */

SECTIONS
{
  /* Specifies the virtual address for the kernel base. 
  커널 기반의 가상 주소를 지정합니다.
  . = 0x8004000000 + 0x200000*/
	. = LOADER_KERN_BASE + LOADER_PHYS_BASE;

	PROVIDE(start = .);
  /* Kernel starts with code, followed by read-only data and writable data. 
  커널은 코드로 시작하고 읽기 전용 데이터와 쓰기 가능한 데이터가 뒤를 잇는다.*/
	.text : AT(LOADER_PHYS_BASE) {
		*(.entry)
		*(.text .text.* .stub .gnu.linkonce.t.*)
	} = 0x90
	.rodata         : { *(.rodata .rodata.* .gnu.linkonce.r.*) }

	. = ALIGN(0x1000);
	PROVIDE(_end_kernel_text = .);

  .data : { *(.data) *(.data.*)}

  /* BSS (zero-initialized data) is after everything else. 
  BSS(초기화되지 않은 데이터)는 무엇보다도 중요합니다.*/
  PROVIDE(_start_bss = .);
  .bss : { *(.bss) }
  PROVIDE(_end_bss = .);

  PROVIDE(_end = .);

	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack .stab)
	}
}
