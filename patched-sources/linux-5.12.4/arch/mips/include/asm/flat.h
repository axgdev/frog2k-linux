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
#define FLAT_MIPS_HI16_SLOTS	32768
#define FLAT_MIPS_HI16_SCAN	4096

static u32 __user *flat_mips_hi16_rp;
static u32 flat_mips_hi16_insn;
static u32 __user *flat_mips_hi16_rps[FLAT_MIPS_HI16_SLOTS];
static u32 flat_mips_hi16_insns[FLAT_MIPS_HI16_SLOTS];
static u32 flat_mips_image_limit;

static inline u32 flat_mips_reloc_type(u32 rel)
{
	return rel & FLAT_MIPS_R_MASK;
}

static inline u32 flat_mips_lui_rt(u32 insn)
{
	return (insn >> 16) & 0x1f;
}

static inline u32 flat_mips_lo16_rs(u32 insn)
{
	return (insn >> 21) & 0x1f;
}

static inline void mips_flat_reloc_begin(u32 limit)
{
	int i;

	flat_mips_hi16_rp = NULL;
	flat_mips_hi16_insn = 0;
	for (i = 0; i < FLAT_MIPS_HI16_SLOTS; i++) {
		flat_mips_hi16_rps[i] = NULL;
		flat_mips_hi16_insns[i] = 0;
	}
	flat_mips_image_limit = limit;
}

static inline int flat_mips_addr_plausible(u32 addr)
{
	if (!flat_mips_image_limit)
		return 0;
	if (addr <= flat_mips_image_limit)
		return 1;
	if (addr >= 0x10000 && addr - 0x10000 <= flat_mips_image_limit)
		return 1;

	return 0;
}

static inline void flat_mips_store_hi16(u32 __user *rp, u32 insn, int latest)
{
	unsigned long slot = ((unsigned long)rp >> 2) &
			     (FLAT_MIPS_HI16_SLOTS - 1);
	int i;

	if (latest) {
		flat_mips_hi16_rp = rp;
		flat_mips_hi16_insn = insn;
	}

	for (i = 0; i < 64; i++) {
		unsigned long idx = (slot + i) & (FLAT_MIPS_HI16_SLOTS - 1);

		if (!flat_mips_hi16_rps[idx] || flat_mips_hi16_rps[idx] == rp) {
			flat_mips_hi16_rps[idx] = rp;
			flat_mips_hi16_insns[idx] = insn;
			return;
		}
	}

	flat_mips_hi16_rps[slot] = rp;
	flat_mips_hi16_insns[slot] = insn;
}

static inline void flat_mips_remember_hi16(u32 __user *rp, u32 insn)
{
	flat_mips_store_hi16(rp, insn, 1);
}

static inline int flat_mips_lookup_hi16(u32 __user *rp, u32 *insn)
{
	unsigned long slot = ((unsigned long)rp >> 2) &
			     (FLAT_MIPS_HI16_SLOTS - 1);
	int i;

	for (i = 0; i < 64; i++) {
		unsigned long idx = (slot + i) & (FLAT_MIPS_HI16_SLOTS - 1);

		if (flat_mips_hi16_rps[idx] == rp) {
			*insn = flat_mips_hi16_insns[idx];
			return 1;
		}
		if (!flat_mips_hi16_rps[idx])
			return 0;
	}

	return 0;
}

static inline int flat_mips_find_hi16_for_lo16(u32 __user *rp, u32 lo,
					       u32 __user **hi_rp,
					       u32 *hi_insn)
{
	u32 *p = (__force u32 *)rp;
	u32 rs = flat_mips_lo16_rs(lo);
	int i, best = -1;
	u32 *best_p = NULL;

	if (flat_mips_hi16_rp && flat_mips_lui_rt(flat_mips_hi16_insn) == rs) {
		u32 candidate = ((flat_mips_hi16_insn & 0xffff) << 16) +
				(s16)(lo & 0xffff);

		if ((__force u32 *)flat_mips_hi16_rp < p &&
		    flat_mips_addr_plausible(candidate)) {
			*hi_rp = flat_mips_hi16_rp;
			*hi_insn = flat_mips_hi16_insn;
			return 1;
		}
	}

	for (i = 0; i < FLAT_MIPS_HI16_SLOTS; i++) {
		u32 *scan = (__force u32 *)flat_mips_hi16_rps[i];
		u32 candidate;

		if (!scan || scan >= p)
			continue;
		if (flat_mips_lui_rt(flat_mips_hi16_insns[i]) != rs)
			continue;

		candidate = ((flat_mips_hi16_insns[i] & 0xffff) << 16) +
			    (s16)(lo & 0xffff);
		if (!flat_mips_addr_plausible(candidate))
			continue;
		if (!best_p || scan > best_p) {
			best = i;
			best_p = scan;
		}
	}
	if (best >= 0) {
		*hi_rp = flat_mips_hi16_rps[best];
		*hi_insn = flat_mips_hi16_insns[best];
		return 1;
	}

	for (i = 1; i <= FLAT_MIPS_HI16_SCAN; i++) {
		u32 *scan = p - i;
		u32 insn = get_unaligned(scan);

		if ((insn & 0xffe00000) != 0x3c000000 ||
		    flat_mips_lui_rt(insn) != rs)
			continue;
		if (!flat_mips_lookup_hi16((__force u32 __user *)scan, hi_insn)) {
			u32 candidate = ((insn & 0xffff) << 16) +
					(s16)(lo & 0xffff);

			if (!flat_mips_addr_plausible(candidate))
				continue;
			*hi_insn = insn;
			flat_mips_remember_hi16((__force u32 __user *)scan, insn);
		}

		*hi_rp = (__force u32 __user *)scan;
		return 1;
	}

	*hi_rp = NULL;
	*hi_insn = 0;
	return 0;
}

static inline int flat_get_addr_from_rp(u32 __user *rp, u32 relval, u32 flags,
					u32 *addr)
{
	u32 *p = (__force u32 *)rp;
	u32 insn, hi, lo;
	u32 __user *hi_rp;

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
		if (flat_mips_find_hi16_for_lo16(rp, lo, &hi_rp, &hi)) {
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

static inline void mips_flat_reloc_prescan(u32 relval, u32 __user *rp)
{
	if (flat_mips_reloc_type(relval) == FLAT_MIPS_R_HI16)
		flat_mips_store_hi16(rp, get_unaligned((__force u32 *)rp), 0);
}

static inline int flat_put_addr_at_rp(u32 __user *rp, u32 addr, u32 relval)
{
	u32 *p = (__force u32 *)rp;
	u32 insn, hi, lo;
	u32 __user *hi_rp;

	switch (flat_mips_reloc_type(relval)) {
	case FLAT_MIPS_R_26:
		insn = get_unaligned(p);
		insn = (insn & 0xfc000000) | ((addr >> 2) & 0x03ffffff);
		put_unaligned(insn, p);
		return 0;
	case FLAT_MIPS_R_HI16:
		if (flat_mips_lookup_hi16(rp, &insn)) {
			flat_mips_hi16_rp = rp;
			flat_mips_hi16_insn = insn;
		} else {
			flat_mips_remember_hi16(rp, get_unaligned(p));
		}
		return 0;
	case FLAT_MIPS_R_LO16:
		lo = get_unaligned(p);
		if (flat_mips_find_hi16_for_lo16(rp, lo, &hi_rp, &hi)) {
			u32 *hi_p = (__force u32 *)hi_rp;

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

static inline u32 mips_flat_reloc_addr_fixup(u32 relval, u32 addr, u32 limit)
{
	if (flat_mips_reloc_type(relval) == FLAT_MIPS_R_LO16 &&
	    addr > limit && addr >= 0x10000 && addr - 0x10000 <= limit)
		return addr - 0x10000;

	return addr;
}

#define flat_reloc_addr_fixup(rel, addr, limit) \
	mips_flat_reloc_addr_fixup(rel, addr, limit)
#define flat_reloc_begin(limit) \
	mips_flat_reloc_begin(limit)
#define flat_reloc_prescan(rel, rp) \
	mips_flat_reloc_prescan(rel, rp)

/*
 * elf2flt stores the MIPS relocation kind in the top two bits. Keep the
 * remaining 30 bits for the relocation target offset.
 */
#define flat_get_relocate_addr(rel)	((rel) & FLAT_MIPS_R_ADDR)

#endif
