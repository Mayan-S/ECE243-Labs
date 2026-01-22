.global _start
_start:

    la s0, result           # s0 = address of result                # The address of the result
    lw t0, 4(s0)            # t0 = count of numbers                 # The number of numbers is in t0
    la t1, numbers          # t1 = address of numbers               # The address of the numbers is in t1     #&numbers[0]
    lw t2, 0(t1)            # t2 = first number (lowest so far)     # Keep the largest number so far in t2

#Loop to search for the biggest number
loop: 
    addi t0, t0, -1         # decrement counter
    ble t0, zero, finished  # if done, exit loop
    addi t1, t1, 4          # move to next word
    lw t3, (t1)             # t3 = next number
    ble t2, t3, loop        # skip if current lowest is smaller
    mv t2, t3               # update lowest
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
