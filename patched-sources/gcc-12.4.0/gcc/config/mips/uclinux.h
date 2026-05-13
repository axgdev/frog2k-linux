/* Definitions for MIPS uClinux FLAT targets.
   Copyright (C) 2026 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
      GNU_USER_TARGET_OS_CPP_BUILTINS ();	\
      builtin_define ("__uClinux__");		\
    }						\
  while (0)

/* FLAT executables run at a fixed load address and cannot use the normal
   GNU/Linux MIPS abicalls/GOT model.  Keep the target non-PIC by default. */
#undef TARGET_DEFAULT
#define TARGET_DEFAULT 0

#undef SUBTARGET_ASM_SPEC
#define SUBTARGET_ASM_SPEC ""

#undef STARTFILE_SPEC
#define STARTFILE_SPEC \
  "%{!shared:crt1.o%s} crti.o%s %{shared|pie:crtbeginS.o%s;:crtbegin.o%s}"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC "crtend.o%s crtn.o%s"

#undef LINK_SPEC
#define LINK_SPEC GNU_USER_TARGET_LINK_SPEC " -e __start"

