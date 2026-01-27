/* Program to Count the number of 1's in a 32-bit word,
located at InputWord */

.global _start
_start:
	
	/* Put your code here */

    la t0, InputWord        # t0 = address of InputWord
    lw t1, 0(t0)            # t1 = the input word
    li t2, 0                # t2 = counter for 1's
    li t3, 32               # t3 = bit counter

count_loop:
    beq t3, zero, done      # if 32 bits checked, done
    andi t4, t1, 1          # t4 = lowest bit
    add t2, t2, t4          # add to counter
    srli t1, t1, 1          # shift right by 1
    addi t3, t3, -1         # decrement bit counter
    j count_loop            # repeat

done:
    la t5, Answer           # t5 = address of Answer
    sw t2, 0(t5)            # store result

    stop: j stop

.data
InputWord: .word 0x4a01fead

Answer: .word 0
