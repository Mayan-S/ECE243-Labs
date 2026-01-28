/* Program to Count the number of 1's and Zeroes in a sequence of 32-bit words,
and determines the largest of each */

.global _start
_start:

/* Your code here  */

    li sp, 0x800000         # initialize stack pointer

    la s0, TEST_NUM         # s0 = address of array
    li s1, 0                # s1 = largest ones
    li s2, 0                # s2 = largest zeroes

main_loop:
    lw s3, 0(s0)            # s3 = current word
    beq s3, zero, end_main  # if zero, end of list

    mv a0, s3               # a0 = current word
    jal ra, ONES            # count ones
    bge s1, a0, skip_ones   # skip if not larger
    mv s1, a0               # update largest ones

skip_ones:
    xori a0, s3, -1         # a0 = inverted word
    jal ra, ONES            # count ones (= zeroes)
    bge s2, a0, skip_zeroes # skip if not larger
    mv s2, a0               # update largest zeroes

skip_zeroes:
    addi s0, s0, 4          # next word
    j main_loop             # repeat

end_main:
    la t0, LargestOnes      # t0 = address
    sw s1, 0(t0)            # store largest ones
    la t1, LargestZeroes    # t1 = address
    sw s2, 0(t1)            # store largest zeroes

stop: j stop

/* ONES: counts 1's in a word. Input: a0 = word. Output: a0 = count of 1's */
ONES:
    addi sp, sp, -12        # allocate stack
    sw t2, 0(sp)            # save t2
    sw t3, 4(sp)            # save t3
    sw t4, 8(sp)            # save t4

    li t2, 0                # t2 = counter
    li t3, 32               # t3 = bit counter

ones_loop:
    beq t3, zero, ones_done # if done, exit
    andi t4, a0, 1          # t4 = lowest bit
    add t2, t2, t4          # add to counter
    srli a0, a0, 1          # shift right
    addi t3, t3, -1         # decrement
    j ones_loop             # repeat

ones_done:
    mv a0, t2               # a0 = result
    lw t2, 0(sp)            # restore t2
    lw t3, 4(sp)            # restore t3
    lw t4, 8(sp)            # restore t4
    addi sp, sp, 12         # deallocate stack
    ret                     # return

.data
TEST_NUM:  .word 0x4a01fead, 0xF677D671,0xDC9758D5,0xEBBD45D2,0x8059519D
            .word 0x76D8F0D2, 0xB98C9BB5, 0xD7EC3A9E, 0xD9BADC01, 0x89B377CD
            .word 0  # end of list 

LargestOnes: .word 0
LargestZeroes: .word 0