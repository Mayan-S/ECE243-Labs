.global _start
_start:

    li s0, 718293           # s0 = student number to find

    la t0, Snumbers         # t0 = address of student numbers
    la t1, Grades           # t1 = address of grades
    li t2, 0                # t2 = index counter

search_loop:
    lw t3, 0(t0)            # t3 = current student number
    beq t3, zero, not_found # if zero, end of list
    beq t3, s0, found       # if match, found it
    addi t0, t0, 4          # move to next student number
    addi t2, t2, 1          # increment index
    j search_loop           # repeat

found:
    slli t4, t2, 2          # t4 = index * 4
    add t4, t1, t4          # t4 = address of grade
    lw s3, 0(t4)            # s3 = grade
    j store_result          # go store it

not_found:
    li s3, -1               # s3 = -1 (not found)

store_result:
    la t5, result           # t5 = address of result
    sw s3, 0(t5)            # store grade in result
    li t6, 0xFF200000       # t6 = LED address
    sw s3, 0(t6)            # write to LEDs

iloop: j iloop              # halt

result: .word 0

Snumbers: .word 10392584, 423195, 644370, 496059, 296800
          .word 265133, 68943, 718293, 315950, 785519
          .word 982966, 345018, 220809, 369328, 935042
          .word 467872, 887795, 681936, 0

Grades: .word 99, 68, 90, 85, 91, 67, 80
        .word 66, 95, 91, 91, 99, 76, 68
        .word 69, 93, 90, 72
