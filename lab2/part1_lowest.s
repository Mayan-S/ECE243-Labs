.global _start

_start:
    la      s0, NUMBERS
    li      s1, 15
    lw      s2, 0(s0)
    addi    s0, s0, 4
    addi    s1, s1, -1

LOOP:
    beq     s1, zero, DONE
    lw      t0, 0(s0)
    ble     s2, t0, NOT_SMALLER
    mv      s2, t0
    
NOT_SMALLER:
    addi    s0, s0, 4
    addi    s1, s1, -1
    j       LOOP

DONE:
    la      t1, RESULT
    sw      s2, 0(t1)
    li      t2, 0xFF200000
    sw      s2, 0(t2)

END:
    j       END

.data
.align 2

NUMBERS:
    .word   150, 823, 47, 512, 999, 256, 678, 91, 1000, 333, 444, 127, 888, 555, 721

RESULT:
    .word   0
