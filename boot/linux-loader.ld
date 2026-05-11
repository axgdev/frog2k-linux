/* SPDX-License-Identifier: MIT */

ENTRY(_start)
OUTPUT_FORMAT("elf32-tradlittlemips")

SECTIONS
{
	. = 0x80001000;

	.text : {
		KEEP(*(.text.start))
		*(.text)
		*(.text.*)
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(16);
	}

	.data : {
		*(.data)
		*(.data.*)
		*(.sdata)
		*(.sdata.*)
		. = ALIGN(16);
	}

	.bss (NOLOAD) : {
		__bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(16);
		__bss_end = .;
	}

	/DISCARD/ : {
		*(.MIPS.abiflags)
		*(.reginfo)
		*(.pdr)
		*(.eh_frame*)
		*(.comment)
		*(.note*)
	}
}
