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

	/*
	 * Keep mutable loader state below the embedded image.  If payload comes
	 * first, a sufficiently large kernel puts .data/.bss at 0x80600000 and
	 * the ELF relocation overwrites that state with kernel text while the
	 * loader is still using its SD log buffers.
	 */
	.payload : {
		*(.payload)
		. = ALIGN(16);
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
