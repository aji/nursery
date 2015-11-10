# io.S -- input and output

	# TODO: verify register clobbering!

	.globl	puts  # no trailing newline added
puts:
	pushl	%ebp
	movl	%esp, %ebp

	pushl	8(%ebp)
	call	strlen

	movl	%ebx, (%esp)

	movl	%eax, %edx
	movl	$4, %eax
	movl	$1, %ebx
	movl	8(%ebp), %ecx
	int	$0x80

	popl	%ebx

	movl	%ebp, %esp
	popl	%ebp
	ret


	.globl	putc
putc:
	pushl	%ebx

	movl	$4, %eax
	movl	$1, %ebx
	movl	%esp, %ecx
	addl	$4, %ecx
	movl	$1, %edx
	int	$0x80

	popl	%ebx

	ret


	.globl	putd
putd:
	pushl	%ebp
	movl	%esp, %ebp

	subl	$64, %esp
	movl	%esp, %ecx
	addl	$63, %ecx
	movb	$0, (%ecx)

	movl	$10, %ebx
	movl	8(%ebp), %eax

1:	movl	$0, %edx
	div	%ebx

	dec	%ecx
	addl	$0x30, %edx
	movb	%dl, (%ecx)

	cmpl	$0, %eax
	jne	1b

	pushl	%ecx
	call	puts

	movl	%ebp, %esp
	popl	%ebp
	ret


	.globl	write
write:
	movl	$4, %eax
	movl	4(%esp), %ebx
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	int	$0x80

	ret


	.globl	read
read:
	movl	$3, %eax
	movl	4(%esp), %ebx
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	int	$0x80

	ret

