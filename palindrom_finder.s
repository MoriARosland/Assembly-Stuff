.global _start

_start:
    // Initialize all leds to OFF
    ldr r0, =led_base_addr
    ldr r0, [r0]

    mov r1, #0
    str r1, [r0]

    b calc_input_len

// ------------------------------------------------------------- //
// Calculate input's length in number of bytes
calc_input_len:
    ldr r0, =input           // Get address of "input"
    ldr r1, =input_length    // Get the address of "input_length"
    mov r2, #0               // Initialize the counter to zero

count_chars:
    ldrb r3, [r0], #1        // Load a byte and increment r0
    cmp r3, #0               // Check if the byte is the null terminator
    beq store_string_len     // If yes, branch to store_string_len

    add r2, r2, #1           // Increment the counter
    b count_chars            // Repeat until end of string

store_string_len:
    str r2, [r1]             // Save the counter value in "input_length"
    b check_palindrom        

// ------------------------------------------------------------- //
check_palindrom:
    ldr r0, =input           // Load the address of the input string
    ldr r1, =input_length    // Load the address of input_length
    ldr r1, [r1]             // Load the value of input_length

    sub r1, r1, #1           // Decrement r1 to point to the terminate the null byte.
    add r1, r0, r1           // Make the right pointer point to the address of the last byte in the input
    b compare_loop

compare_loop:
    ldrb r2, [r0]            // Load the left character
    ldrb r3, [r1]        // Load the right character

    cmp r2, #97              // Check if r2 is a lowercase letter
    bhs convert_r2_to_upper 

check_r3:
    cmp r3, #97              // Check if r3 is a lowercase letter
    bhs convert_r3_to_upper 

compare_continue:
    cmp r2, r3               // Compare characters
    bne check_special_char   // If they don't match, check for "?"

increment_pointers:
    add r0, r0, #1           // Increment left pointer
    sub r1, r1, #1           // Decrement right pointer

    cmp r0, r1               // Check if pointers have crossed
    bls compare_loop         // Continue if they haven't crossed

    b is_palindrom           // If loop is finished, exit

convert_r2_to_upper:
    sub r2, r2, #32          // Convert r2 to uppercase
    b check_r3               // Check and potentially convert r3

convert_r3_to_upper:
    sub r3, r3, #32          // Convert r3 to uppercase
    b compare_continue       // Continue with comparison
	
is_palindrom:
    ldr r0, =led_base_addr
    ldr r0, [r0]

    mov r1, #0x1f           
    str r1, [r0]  // Switch on the 5 rightmost LEDs. Setting a bit high (0x1) corresponds to switching it on.

    // Write 'Palindrome detected' to UART (implementation needed)
    b _exit

check_special_char:
    cmp r2, #63                 // 63 = "?" in ASCII
    beq increment_pointers

    cmp r3, #63
    beq increment_pointers

    b not_palindrome           // If neither is "?", it's not a palindrome

not_palindrome:
    ldr r0, =led_base_addr     // Switch on the 5 leftmost LEDs
    ldr r0, [r0]

    movw r1, #0x3e0
    str r1, [r0]
    // Write 'Not a palindrome' to UART (implementation needed)
    b _exit
	
_exit:
    // Halt execution
    b .

.data
.align
    input: .asciz "Grav ned den varg"
.align
    input_length: .word 0    // Initialize to 0
.align
    led_base_addr: .word 0xff200000
.end
