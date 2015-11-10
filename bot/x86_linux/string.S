# string.S -- string processing commands


	.globl	strlen
strlen:
	movl	4(%esp), %edx
	movl	%edx, %eax

1:	cmpb	$0, (%eax)
	je	2f
	inc	%eax
	jmp	1b

2:	subl	%edx, %eax
	ret


	.globl	strcmp
strcmp:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi

	movl	8(%ebp), %esi
	movl	12(%ebp), %edi

	xorl	%eax, %eax
	xorl	%edx, %edx

1:	movb	(%esi), %al
	movb	(%edi), %dl
	cmpb	$0, %al
	je	2f
	cmpb	%al, %dl
	jne	2f
	inc	%esi
	inc	%edi
	jmp	1b

2:	subb	%dl, %al
	cbw
	cwde
	popl	%edi
	popl	%esi
	popl	%ebp
	ret

	.globl	strchr
strchr:
	movl	4(%esp), %eax
	movl	8(%esp), %edx

1:	cmpb	$0, (%eax)
	je	2f
	cmpb	%dl, (%eax)
	je	3f
	inc	%eax
	jmp	1b

2:	movl	$0, %eax
3:	ret


	.globl	strcut   # pronounced "haircut"
strcut:
	pushl	%ebp
	movl	%esp, %ebp

	pushl	12(%ebp)
	pushl	8(%ebp)

	call	strchr

	cmpl	$0, %eax
	je	1f

	movb	$0, (%eax)
	inc	%eax

1:	movl	%ebp, %esp
	popl	%ebp
	ret
