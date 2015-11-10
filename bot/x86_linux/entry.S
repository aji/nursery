# entry.S -- where the bot starts

	.data
msg_hello: .asciz "FOOBOT starting\n"
net_register: .asciz "NICK FOOBOT\r\nUSER foo * 0 :i386\r\n"
net_join: .asciz "JOIN #autistic-faggots\r\n"

msg_test:
	.asciz ":aji!alex@ponychat.net PRIVMSG #geek :hello there, geeks"

	.text
outs:
	pushl	%edx
	call	strlen
	popl	%edx
	pushl	%eax
	pushl	%edx
	pushl	$3
	call	write
	addl	$12, %esp

	ret

puts_all:
	pushl	%ebp
	movl	%esp, %ebp

	movl	8(%ebp), %edi
	movl	12(%ebp), %ebx

2:	cmpl	$0, %ebx
	je	1f

	dec	%ebx
	addl	$4, %edi

	cmp	$0, (%edi)
	je	2b

	push	(%edi)
	call	puts
	movl	$10, (%esp)
	call	putc

	jmp	2b

1:	xorl	%eax, %eax
	movl	%ebp, %esp
	popl	%ebp
	ret

main:
	pushl	%ebp
	movl	%esp, %ebp

	subl	$64, %esp
	movl	%esp, %edi

	pushl	%edi
	pushl	$msg_test
	call	ircspl
	addl	$8, %esp

	pushl	%eax
	pushl	%edi
	call	puts_all

	movl	%ebp, %esp
	popl	%ebp
	ret

	pushl	$msg_hello
	call	puts
	addl	$4, %esp

	movl	$net_register, %edx
	call	outs
	movl	$net_join, %edx
	call	outs

	xorl	%eax, %eax
	movl	%ebp, %esp
	popl	%ebp
	ret

	.globl	_start
_start:
	movl	%esp, %ebp

	call	main

	movl	%eax, %ebx
	movl	$1, %eax
	int	$0x80
