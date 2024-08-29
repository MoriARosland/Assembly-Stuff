.global _start


_start:
	b calc_input_len

// Calculate input's length in number of bytes
calc_input_len:
    ldr r0, =input // Get address of input
    ldr r1, =input_length // Get the address of "input_length"
    mov r2, #0 // Initialize the counter to zero

    ldrb  r3, [r0], #1 // Load the first byte, increment r0 by 1


loop1_start:
    cmp r3, #0 // End loop if r2 equals the null terminator (zero byte)
    beq loop1_end

    add r2, r2, #1 // Increment the counter
    ldrb  r3, [r0], #1 // Load the next byte, increment r0 by 1

    b loop1_start // Repeat until 

loop1_end:
    str r2, [r1]
    b _exit

check_palindrom:
	// Here you could check whether input is a palindrom or not
	
	
is_palindrom:
	// Switch on only the 5 rightmost LEDs
	// Write 'Palindrom detected' to UART
	
	
is_no_palindrom:
	// Switch on only the 5 leftmost LEDs
	// Write 'Not a palindrom' to UART
	
	
_exit:
	// Branch here for exit
	b .
	
.data
.align
	input: .asciz "Grav ned den varg"
.align
    input_length: .word 2
.align
    isPalinFlag: .word 0 // 0 = False, 1 = True
.end