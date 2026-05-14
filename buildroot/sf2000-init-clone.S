	.set noreorder
	.text
	.globl sf2000_clone_service
	.ent sf2000_clone_service
sf2000_clone_service:
	li	$6, 0x112
	b	sf2000_clone_service_with_flags
	nop
	.end sf2000_clone_service

	.globl sf2000_clone_service_vfork
	.ent sf2000_clone_service_vfork
sf2000_clone_service_vfork:
	li	$6, 0x4112
	b	sf2000_clone_service_with_flags
	nop
	.end sf2000_clone_service_vfork

	.ent sf2000_clone_service_with_flags
sf2000_clone_service_with_flags:
	addiu	$4, $4, -16
	sw	$5, 0($4)
	move	$5, $4
	move	$4, $6
	li	$2, 4120
	addiu	$29, $29, -32
	sw	$0, 16($29)
	sw	$0, 20($29)
	syscall
	bnez	$7, 2f
	nop
	bnez	$2, 1f
	nop
	lw	$4, 0($29)
	la	$25, service_child_exec
	jr	$25
	nop
1:
	addiu	$29, $29, 32
	jr	$31
	nop
2:
	subu	$2, $0, $2
	addiu	$29, $29, 32
	jr	$31
	nop
	.end sf2000_clone_service_with_flags
