; microvm64 function to calculate the nth fibonacci number
; arguments - n, unsigned int, nth number to calculate - on stack

fib:
	pop a
	mov r, a
	xor r, 1
	jzr @one
	mov r, a
	xor r, 2
	jzr @one
	
main:
	sub a, 2 ; start at 2nd fibonacci number
	mov r, a ; move counter to r for easy loop control
	
	; b register is n-2th fibonacci number
	; c register is n-1th fibonacci number
	mov b, 1
	mov c, 1

do:
	mov d, c
	add c, b
	mov b, d
	sub r, 1 ; decrement counter
	jzr @done ; finish loop if counter reaches 0
	jmp @do

done:
	mov r, c ; return value in c
	ret
	
one:
	mov r, 1
	ret