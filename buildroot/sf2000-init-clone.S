	.set noreorder
	.text
	.globl sf2000_clone_service
	.ent sf2000_clone_service
sf2000_clone_service:
	addiu	$4, $4, -16
	sw	$5, 0($4)
	move	$5, $4
	li	$4, 0x112
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
	.end sf2000_clone_service
