.global _start
_start:

    la s0, result           # s0 = address of result                # The address of the result
    lw t0, 4(s0)            # t0 = count of numbers                 # The number of numbers is in t0
    la t1, numbers          # t1 = address of numbers               # The address of the numbers is in t1     #&numbers[0]
    lw t2, 0(t1)            # t2 = first number (largest so far)    # Keep the largest number so far in t2

#Loop to search for the biggest number
loop: 
    addi t0, t0, -1         # decrement counter
    ble t0, zero, finished  # if done, exit loop                    # branch if less or equal
    addi t1, t1, 4          # move to next word                     # move one number to the right
    lw t3, (t1)             # t3 = next number                      # load the next number into t3
    bge t2, t3, loop        # skip if current largest is bigger     # branch if greater or equal
    mv t2, t3               # update largest                        # if not, save the new largest number on t2
    j loop                  # repeat

finished: 
    sw t2, (s0)             # store result
    li t4, 0xFF200000       # t4 = LED address
    sw t2, 0(t4)            # write to LEDs

iloop: j iloop              # halt


.data

result: .word 0
n:      .word 15
numbers: .word 42, 517, 893, 156, 724
         .word 301, 987, 445, 612, 78
         .word 234, 999, 567, 189, 845
