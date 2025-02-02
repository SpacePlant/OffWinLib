.CODE

indirect_syscall PROC
	push rbp					; Save RBP
	mov rbp, rsp				; Create stack frame
	mov rax, rcx				; Prepare the SSN
	mov r11, rdx				; Prepare syscall stub target
	mov rcx, r8					; Move the syscall argument count to RCX
	sub rcx, 4					; The four register arguments are always prepared
	mov r10, r9					; Prepare the first argument
	mov rdx, [rbp + 48]			; Prepare the second argument
	mov r8, [rbp + 56]			; Prepare the third argument
	mov r9, [rbp + 64]			; Prepare the fourth argument
push_loop:
	test rcx, rcx				; Check the amount of arguments that should be pushed to the stack
	jle perform_call			; If the amount is negative or zero, perform the call
	push [rbp + 64 + rcx * 8]	; Push stack argument
	dec rcx						; Decrement remaining argument count
	jmp push_loop				; Continue loop
perform_call:
	sub rsp, 32					; Prepare shadow space
	call r11					; Call syscall stub
	leave						; Restore previous stack frame
	ret							; Return
indirect_syscall ENDP

END
