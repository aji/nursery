	.text

	/* ircspl(char *ln, char **argv) => argc */
	.globl	ircspl
ircspl:
	pushl	%ebp
	movl	%esp, %ebp

	subl	$12, %esp

	movl	%ebx, 0(%esp)                /* save %ebx */
	movl	%esi, 4(%esp)                /* save %esi */
	movl	%edi, 8(%esp)                /* save %edi */
	movl	8(%ebp), %esi                /* %esi = ln */
	movl	12(%ebp), %edi               /* %edi = argv */

	movl	$0, 0(%edi)                  /* argv[0] = NULL */
	movl	$0, 4(%edi)                  /* argv[1] = NULL */
	movl	$2, %ebx                     /* argc = 2 */

	cmpb	$':', (%esi)                 /* if (*ln == ':') */
	jne	1f

	inc	%esi
	movl	%esi, 0(%edi)

	/* TODO: cut at ! */

	pushl	$' '
	pushl	%esi
	call	strcut
	cmp	$0, %eax
	je	9f
	movl	%eax, %esi

1:	cmpb	$0, (%esi)                   /* goto end, if *ln == '\0' */
	je	9f

	addl	$4, %ebx                     /* argc++ */

	cmpb	$':', (%esi)                 /* if (*ln == ':') */
	je	2f                           /*    goto trailing */

	/* space-delimited extraction */
	movl	%esi, -4(%edi, %ebx, 4)      /* argv[argc-1] = ln */

	pushl	$' '
	pushl	%esi
	call	strcut                       /* ln = strcut(ln, ' ') */
	addl	$8, %esp
	movl	%eax, %esi

	jmp	1b

2:	/* trailing argument extraction */
	inc	%esi
	movl	%esi, -4(%edi, %ebx, 4)      /* argv[argc-1] = ++ln */

9:	movl	0(%esp), %ebx                /* restore %ebx */
	movl	4(%esp), %esi                /* restore %esi */
	movl	8(%esp), %edi                /* restore %edi */
	movl	%ebx, %eax                   /* %eax = argc */
	movl	%ebp, %esp
	popl	%ebp
	ret
