.global _start


_start:
	b calc_input_len

// Calculate input's length in number of bytes
calc_input_len:
    ldr r0, =input // Get address of input
    ldr r1, =input_length // Get the address of "input_length"
    mov r2, #0 // Initialize the counter to zero

    ldrb  r3, [r0], #1 // Load the first byte, increment r0 by 1
    
    b loop1_start


loop1_start:
    cmp r3, #0 // End loop if r2 equals the null terminator (zero byte)
    beq loop1_end

    add r2, r2, #1 // Increment the counter
    ldrb  r3, [r0], #1 // Load the next byte, increment r0 by 1

    b loop1_start // Repeat until 

loop1_end:
    str r2, [r1] // Update the value of "input_length"
    b check_palindrom

check_palindrom:
    ldr r0, =input           // Load the address of the input string
    ldr r1, =input_length    // Load the address of input_length
    ldr r1, [r1]             // Load the value of input_length

    sub r1, r1, #1           // Decrement r1 to point to the last character

compare_loop:
    ldrb r2, [r0]            // Load the left character
    ldrb r3, [r0, r1]        // Load the right character

    // Check convert to lower case if neccessary

    cmp r2, r3               // Compare characters
    // Check for special character "?"
    bne not_palindrome       // If they don't match, it's not a palindrome

    add r0, r0, #1           // Move left pointer to the right
    sub r1, r1, #1           // Move right pointer to the left

    cmp r0, r1               // Check if pointers have crossed
    bls compare_loop         // If not, continue comparing
	
is_palindrom:
	// Switch on only the 5 rightmost LEDs
    ldr r12, =0xFFFFFFFF

	// Write 'Palindrom detected' to UART
	
	
not_palindrome:
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