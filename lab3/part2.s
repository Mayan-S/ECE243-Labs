/* Program to Count the number of 1's in a 32-bit word,
located at InputWord, using a subroutine called ONES */

.global _start
_start:
	
	/* Main program */

    la t0, InputWord        # t0 = address of InputWord
    lw a0, 0(t0)            # a0 = input word (parameter to ONES)
    jal ra, ONES            # call ONES subroutine
    la t1, Answer           # t1 = address of Answer
    sw a0, 0(t1)            # store result in Answer

    stop: j stop

/* ONES subroutine: counts number of 1's in a word
   Input: a0 = word to count 1's in
   Output: a0 = number of 1's */

ONES:
    li t2, 0                # t2 = counter for number of 1's
    li t3, 32               # t3 = bit counter (32 bits)

ones_loop:
    beq t3, zero, ones_done # if checked all 32 bits, done
    andi t4, a0, 1          # t4 = lowest bit of a0
    add t2, t2, t4          # add bit value to counter
    srli a0, a0, 1          # shift right to check next bit
    addi t3, t3, -1         # decrement bit counter
    j ones_loop             # repeat

ones_done:
    mv a0, t2               # a0 = result (number of 1's)
    ret                     # return to caller

.data
InputWord: .word 0x4a01fead

Answer: .word 0
