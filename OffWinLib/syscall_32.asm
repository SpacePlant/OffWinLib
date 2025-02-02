.686
.MODEL FLAT, C

.CODE

indirect_syscall PROC
	push ebp					; Save EBP
	mov ebp, esp				; Create stack frame
	mov eax, [ebp + 8]			; Prepare the SSN
	mov edx, [ebp + 12]			; Prepare syscall stub target
	mov ecx, [ebp + 16]			; Move the syscall argument count to ECX
push_loop:
	test ecx, ecx				; Check the amount of arguments that should be pushed to the stack
	jz perform_call				; If the amount is zero, perform the call
	push [ebp + 16 + ecx * 4]	; Push argument
	dec ecx						; Decrement remaining argument count
	jmp push_loop				; Continue loop
perform_call:
	call edx					; Call syscall stub
	leave						; Restore previous stack frame
	ret							; Return
indirect_syscall ENDP

END
