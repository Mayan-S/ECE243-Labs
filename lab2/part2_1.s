.global _start

_start:
    li      s0, 718293
    la      s1, Snumbers
    li      s2, 0
    li      s3, -1

SEARCH_LOOP:
    lw      t0, 0(s1)
    beq     t0, zero, NOT_FOUND
    beq     t0, s0, FOUND
    addi    s1, s1, 4
    addi    s2, s2, 1
    j       SEARCH_LOOP

FOUND:
    la      t1, Grades
    slli    t2, s2, 2
    add     t1, t1, t2
    lw      s3, 0(t1)

NOT_FOUND:
    la      t3, result
    sw      s3, 0(t3)
    li      t4, 0xFF200000
    sw      s3, 0(t4)

iloop: 
    j       iloop

result: 
    .word   0

Snumbers: 
    .word   10392584, 423195, 644370, 496059, 296800
    .word   265133, 68943, 718293, 315950, 785519
    .word   982966, 345018, 220809, 369328, 935042
    .word   467872, 887795, 681936, 0

Grades: 
    .word   99, 68, 90, 85, 91, 67, 80
    .word   66, 95, 91, 91, 99, 76, 68
    .word   69, 93, 90, 72
