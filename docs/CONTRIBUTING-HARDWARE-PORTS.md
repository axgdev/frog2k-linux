# Contributing Loader and Hardware Ports

This is the process for changes that cross the kernel, QEMU, userspace, or a
vendor-library boundary. It is deliberately evidence-driven: an output file
existing is not proof that it is valid, and a mocked register test is not proof
of vendor compatibility.

## 1. Preserve a Reproducible Baseline

Record the known-good commit, artifact SHA-256, QEMU smoke result, physical log,
and tool versions before editing. Rebuild that commit from an empty output
directory. If a clean parallel build fails, fix the dependency graph first.

Keep format migrations separate from driver additions. A review must be able to
answer independently:

- Does the new portable driver implement the intended contract?
- Does the OS adapter own and map the hardware safely?
- Does the executable loader start ordinary programs correctly?
- Does the same artifact pass QEMU and physical-device tests?

## 2. Reverse Engineer a Vendor Archive

For each archive, commit an evidence table containing:

1. archive hash, object list, exported symbols, and ABI signatures;
2. disassembly/decompiler locations supporting each register and bit meaning;
3. observed register traces from the vendor implementation;
4. unresolved symbols and explicitly unsupported behavior;
5. differential tests which feed identical calls to vendor and source builds.

Separate the portable state machine from adapters. The portable layer receives
`read32`, `write32`, cache-maintenance, delay, and locking callbacks. Linux,
HCRTOS, and QEMU adapters implement those callbacks. Do not put `/dev/mem`,
FreeRTOS queues, Linux file descriptors, or fixed virtual aliases in the
portable layer.

A fake-MMIO unit test proves ordering and error handling only. It must not be
described as vendor parity. Parity requires symbol coverage plus differential
behavior or a documented physical trace.

Destructive operations need a separate opt-in target. eFuse programming,
unbounded DMA, clock/reset writes, and display-controller ownership must never
run from the default image or a broad smoke suite.

## 3. Migrate to ELF Without Breaking the Working Path

The SF2000 has no MMU, so executable layout is a resource-ownership problem:

- fixed static `ET_EXEC` optionally uses the physical window at `0x05000000`
  (kernel alias `0x85000000`); it is disabled and unreserved by default;
- static-PIE `ET_DYN` receives a distinct contiguous allocation per process;
- dynamically interpreted ELF is unsupported.

Implement and verify in this order:

1. Add the kernel loader behind its own Kconfig symbol.
2. Build tiny fixed and PIE fixtures and inspect them with `readelf`.
3. Reject `PT_INTERP`, unsupported relocations, malformed extents, overflows,
   and segments outside their allocation.
4. Allocate only after `begin_new_exec()` installs the new memory map. Invoke
   `SET_PERSONALITY2` with the validated architecture ELF state so MIPS signal
   delivery, floating-point ABI selection, and thread ABI state are initialized.
5. Apply symbol-free `R_MIPS_REL32` relocations, then relocate the MIPS local
   GOT described by `DT_PLTGOT` and `DT_MIPS_LOCAL_GOTNO`. Reject a binary
   unless `DT_MIPS_SYMTABNO == DT_MIPS_GOTSYM`; that equality proves there are
   no global GOT entries which would require a runtime dynamic linker. Flush
   the instruction cache after relocation.
6. Verify `argc`, `argv`, environment, auxv, `brk`, clone/exec, exit, and
   repeated launches in QEMU.
7. Convert one service, then the remaining services, then BusyBox and the
   frontend. Remove the old format only after the rootfs audit proves that no
   old executable remains.

Never suppress linker diagnostics with `--noinhibit-exec`. Normal applications,
the first-stage init, and services are fully static PIE: they use ELF type
`ET_DYN`, have no
`PT_INTERP`, and contain only relocations supported by the kernel loader. Fixed
static `ET_EXEC` is compatibility-only and requires `FIXED_ET_EXEC=1`, which
also enables its device-tree reservation.

MIPS dynarecs need one additional ABI audit. Generated code may use `$gp` as a
guest register, but every transition from generated code to a PIC C function
must put the callee address in `$t9`; the callee derives its ELF GOT pointer
from `$t9`. Assembly feature tests must accept GCC's `__PIC__` definition, not
only a project-local `PIC` macro. Calls through a register-state function table
must restore the application `$gp` before entering C. Verify the final
instructions and exercise at least one generated memory/I/O helper under QEMU;
merely reaching a dynarec entry point does not cover this boundary.

On NOMMU SF2000, valid executable VMAs use cached KSEG0 addresses. Generic
MIPS `access_ok()` rejects those values, so the `cacheflush` syscall must
validate the requested range against a VMA owned by `current->mm`. Never accept
KSEG0 broadly. QEMU TCG can execute stale-cache bugs successfully, therefore a
dynarec smoke must also assert a nonzero cache-sync call count and zero syscall
failures.

## 4. Integrate Hardware Safely

One owner controls each register block. A userspace probe must not write a
block while a kernel driver, display service, decoder, or audio service owns
it. Device-tree nodes describe hardware; they do not prove that a compatible
Linux driver exists.

For DMA, document CPU address, physical address, alignment, cache direction,
maximum length, completion condition, timeout, and reset recovery. Test invalid
sizes and timeouts before a real transfer. Never report PASS merely because a
register was readable or a command was issued.

## 5. Verification Ladder

Run the cheapest useful gate first:

1. `make check`
2. vendor/source differential tests for the changed library
3. `make ROOTFS=full elf-audit`
4. the subsystem QEMU smoke
5. lifecycle and repeated-exec smoke
6. full ASD boot smoke
7. physical device, retained log, and artifact hash confirmation

The final commit body lists every command and result. A physical test artifact
must be copied to both `build/sf2000-linux-full.asd` and
`build/sdcard/bios/bisrv.asd`, with matching hashes.

## 6. Review Checklist

- [ ] Clean `make -j` build works without stale files.
- [ ] No ignored patch, compiler, linker, or test failure.
- [ ] No bFLT executable and no ELF interpreter in the rootfs.
- [ ] Fixed and PIE fixtures both execute repeatedly.
- [ ] New register facts have vendor evidence.
- [ ] Portable core and OS adapters are separate.
- [ ] Destructive probes are opt-in.
- [ ] QEMU models errors, completion, reset, and timing used by the driver.
- [ ] Documentation distinguishes implemented, tested, and inferred behavior.
- [ ] Artifact paths and SHA-256 values are reported.
