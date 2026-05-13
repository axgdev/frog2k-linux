/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_MIPS_FLAT_H
#define _ASM_MIPS_FLAT_H

#include <asm/byteorder.h>
#include <asm/unaligned.h>

#define FLAT_MIPS_R_32		0x00000000
#define FLAT_MIPS_R_26		0x40000000
#define FLAT_MIPS_R_HI16	0x80000000
#define FLAT_MIPS_R_LO16	0xc0000000
#define FLAT_MIPS_R_MASK	0xc0000000
#define FLAT_MIPS_R_ADDR	0x3fffffff

static u32 __user *flat_mips_hi16_rp;
static u32 flat_mips_hi16_insn;

static inline u32 flat_mips_reloc_type(u32 rel)
{
	return rel & FLAT_MIPS_R_MASK;
}

static inline int flat_get_addr_from_rp(u32 __user *rp, u32 relval, u32 flags,
					u32 *addr)
{
	u32 *p = (__force u32 *)rp;
	u32 insn, hi, lo;

	switch (flat_mips_reloc_type(relval)) {
	case FLAT_MIPS_R_26:
		insn = get_unaligned(p);
		*addr = htonl((insn & 0x03ffffff) << 2);
		return 0;
	case FLAT_MIPS_R_HI16:
		/*
		 * The low half carries the complete signed address when the
		 * matching LO16 relocation is processed. Return a harmless
		 * non-zero value so binfmt_flat calls flat_put_addr_at_rp(),
		 * where we remember this high-half instruction.
		 */
		*addr = htonl(1);
		return 0;
	case FLAT_MIPS_R_LO16:
		lo = get_unaligned(p);
		if (flat_mips_hi16_rp) {
			hi = flat_mips_hi16_insn;
			*addr = htonl(((hi & 0xffff) << 16) +
				      (s16)(lo & 0xffff));
		} else {
			*addr = htonl((s16)(lo & 0xffff));
		}
		return 0;
	default:
		*addr = get_unaligned(p);
		return 0;
	}
}

static inline int flat_put_addr_at_rp(u32 __user *rp, u32 addr, u32 relval)
{
	u32 *p = (__force u32 *)rp;
	u32 insn, hi, lo;

	switch (flat_mips_reloc_type(relval)) {
	case FLAT_MIPS_R_26:
		insn = get_unaligned(p);
		insn = (insn & 0xfc000000) | ((addr >> 2) & 0x03ffffff);
		put_unaligned(insn, p);
		return 0;
	case FLAT_MIPS_R_HI16:
		flat_mips_hi16_rp = rp;
		flat_mips_hi16_insn = get_unaligned(p);
		return 0;
	case FLAT_MIPS_R_LO16:
		lo = get_unaligned(p);
		if (flat_mips_hi16_rp) {
			u32 *hi_p = (__force u32 *)flat_mips_hi16_rp;

			hi = get_unaligned(hi_p);
			hi = (hi & 0xffff0000) |
			     (((addr + 0x8000) >> 16) & 0xffff);
			put_unaligned(hi, hi_p);
		}
		lo = (lo & 0xffff0000) | (addr & 0xffff);
		put_unaligned(lo, p);
		return 0;
	default:
		put_unaligned(addr, p);
		return 0;
	}
}

/*
 * elf2flt stores the MIPS relocation kind in the top two bits. Keep the
 * remaining 30 bits for the relocation target offset.
 */
#define flat_get_relocate_addr(rel)	((rel) & FLAT_MIPS_R_ADDR)

#endif
