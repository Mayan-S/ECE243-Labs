/* Program to Count the number of 1's and Zeroes in a sequence of 32-bit words,
and determines the largest of each, then displays them on LEDs alternately */

/* Q: Which is faster, CPUlator or the real DE1-SoC processor?
   A: The real DE1-SoC processor is MUCH faster than CPUlator.
      CPUlator is a software simulator running in a browser, which adds
      significant overhead. The real hardware executes instructions directly
      on the FPGA at full speed. You may need to increase the delay loop
      iterations significantly when running on real hardware. */

.global _start
_start:

/* Your code here  */

    la s0, TEST_NUM         # s0 = address of TEST_NUM array
    li s1, 0                # s1 = largest ones count so far
    li s2, 0                # s2 = largest zeroes count so far

main_loop:
    lw s3, 0(s0)            # s3 = current word from array
    beq s3, zero, end_main  # if word is 0, end of list

    /* Count ones in current word */
    mv a0, s3               # a0 = current word (parameter)
    jal ra, ONES            # call ONES subroutine
    bge s1, a0, skip_ones   # if largest >= current, skip update
    mv s1, a0               # update largest ones

skip_ones:
    /* Count zeroes by inverting the word first */
    xori a0, s3, -1         # a0 = NOT(current word), flips all bits
    jal ra, ONES            # count 1's in inverted word = count 0's
    bge s2, a0, skip_zeroes # if largest >= current, skip update
    mv s2, a0               # update largest zeroes

skip_zeroes:
    addi s0, s0, 4          # move to next word in array
    j main_loop             # repeat

end_main:
    la t0, LargestOnes      # t0 = address of LargestOnes
    sw s1, 0(t0)            # store largest ones count
    la t1, LargestZeroes    # t1 = address of LargestZeroes
    sw s2, 0(t1)            # store largest zeroes count

    li s4, 0xFF200000       # s4 = LED base address

/* Infinite loop to alternate displaying LargestOnes and LargestZeroes */
display_loop:
    sw s1, 0(s4)            # display LargestOnes on LEDs
    jal ra, DELAY           # call delay subroutine
    sw s2, 0(s4)            # display LargestZeroes on LEDs
    jal ra, DELAY           # call delay subroutine
    j display_loop          # repeat forever

/* ONES subroutine: counts number of 1's in a word
   Input: a0 = word to count 1's in
   Output: a0 = number of 1's */

ONES:
    addi sp, sp, -12        # allocate stack space
    sw t2, 0(sp)            # save t2
    sw t3, 4(sp)            # save t3
    sw t4, 8(sp)            # save t4

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
    lw t2, 0(sp)            # restore t2
    lw t3, 4(sp)            # restore t3
    lw t4, 8(sp)            # restore t4
    addi sp, sp, 12         # deallocate stack space
    ret                     # return to caller

/* DELAY subroutine: software delay loop
   Adjust the delay count for CPUlator vs real hardware
   CPUlator: use smaller value (~500000)
   Real DE1-SoC: use larger value (~5000000) */

DELAY:
    addi sp, sp, -4         # allocate stack space
    sw t0, 0(sp)            # save t0

    li t0, 500000           # t0 = delay count (adjust as needed)

delay_loop:
    addi t0, t0, -1         # decrement counter
    bne t0, zero, delay_loop # if not zero, keep looping

    lw t0, 0(sp)            # restore t0
    addi sp, sp, 4          # deallocate stack space
    ret                     # return to caller

.data
TEST_NUM:  .word 0x4a01fead, 0xF677D671,0xDC9758D5,0xEBBD45D2,0x8059519D
            .word 0x76D8F0D2, 0xB98C9BB5, 0xD7EC3A9E, 0xD9BADC01, 0x89B377CD
            .word 0  # end of list 

LargestOnes: .word 0
LargestZeroes: .word 0
