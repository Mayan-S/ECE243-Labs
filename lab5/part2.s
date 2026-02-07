.global _start
_start:

.equ HEX_BASE1, 0xff200020             # HEX3-0 data register address
.equ HEX_BASE2, 0xff200030             # HEX5-4 data register address
.equ PUSH_BUTTON, 0xFF200050            # pushbutton base address

    # 1. Turn off global interrupts during setup
    csrci mstatus, 0x8                  # clear MIE bit (bit 3) in mstatus

    # 2. Initialize the stack pointer
    li sp, 0x20000                      # sp = top of stack memory

    # 3. Set the interrupt handler address in mtvec
    la t0, interrupt_handler            # t0 = address of interrupt handler
    csrw mtvec, t0                      # write handler address to mtvec CSR

    # 4. Enable IRQ 18 (pushbuttons) in the mie register
    li t0, 0x40000                      # t0 = 1 << 18 (bit 18 for IRQ 18)
    csrw mie, t0                        # enable external interrupt for IRQ 18

    # 5. Configure pushbutton interrupt mask and clear edge capture
    li t0, PUSH_BUTTON                  # t0 = pushbutton base address
    li t1, 0xF                          # t1 = 0b1111 (enable all 4 keys)
    sw t1, 8(t0)                        # write to interrupt mask register (+0x8)
    sw t1, 0xC(t0)                      # clear edge capture register (+0xC)

    # 6. Blank all HEX displays initially
    li t0, HEX_BASE1                    # t0 = HEX3-0 register address
    sw zero, 0(t0)                      # clear HEX3-0 (all blank)
    li t0, HEX_BASE2                    # t0 = HEX5-4 register address
    sw zero, 0(t0)                      # clear HEX5-4 (all blank)

    # 7. Enable global interrupts
    csrsi mstatus, 0x8                  # set MIE bit (bit 3) in mstatus

    # 8. Idle loop — all work is done in interrupt handler
IDLE:
    j IDLE                              # spin forever, waiting for interrupts

interrupt_handler:
    addi sp, sp, -40                    # allocate stack frame (10 registers)
    sw s0, 0(sp)                        # save s0
    sw s1, 4(sp)                        # save s1
    sw s2, 8(sp)                        # save s2
    sw ra, 12(sp)                       # save return address
    sw a0, 16(sp)                       # save a0 (caller-saved, may be in use)
    sw a1, 20(sp)                       # save a1 (caller-saved)
    sw t0, 24(sp)                       # save t0 (caller-saved)
    sw t1, 28(sp)                       # save t1 (caller-saved)
    sw t2, 32(sp)                       # save t2 (caller-saved)
    sw t3, 36(sp)                       # save t3 (caller-saved)

    li s0, 0x7FFFFFFF                   # s0 = mask to clear MSB of mcause
    csrr s1, mcause                     # s1 = mcause value
    and s1, s1, s0                      # s1 = interrupt ID (clear interrupt bit)
    li s0, 18                           # s0 = IRQ 18 (pushbutton IRQ number)
    bne s1, s0, end_interrupt           # if not pushbuttons, skip ISR

    jal KEY_ISR                         # call pushbutton interrupt service routine

end_interrupt:
    lw s0, 0(sp)                        # restore s0
    lw s1, 4(sp)                        # restore s1
    lw s2, 8(sp)                        # restore s2
    lw ra, 12(sp)                       # restore return address
    lw a0, 16(sp)                       # restore a0
    lw a1, 20(sp)                       # restore a1
    lw t0, 24(sp)                       # restore t0
    lw t1, 28(sp)                       # restore t1
    lw t2, 32(sp)                       # restore t2
    lw t3, 36(sp)                       # restore t3
    addi sp, sp, 40                     # deallocate stack frame

    mret                                # return from interrupt (restores mepc)

KEY_ISR:
    addi sp, sp, -20                    # allocate stack frame (5 registers)
    sw ra, 0(sp)                        # save ra (we call HEX_DISP)
    sw s0, 4(sp)                        # save s0
    sw s1, 8(sp)                        # save s1
    sw s2, 12(sp)                       # save s2
    sw s3, 16(sp)                       # save s3

    li s0, PUSH_BUTTON                  # s0 = pushbutton base address
    lw s1, 0xC(s0)                      # s1 = edge capture register value
    sw s1, 0xC(s0)                      # clear edge capture by writing back

    la s2, HEX_STATE                    # s2 = base address of toggle state array
    li s3, 0                            # s3 = loop counter i = 0

key_loop:
    li t0, 4                            # t0 = 4 (number of keys)
    beq s3, t0, key_done                # if i == 4, all keys checked, exit loop

    li t0, 1                            # t0 = 1
    sll t0, t0, s3                      # t0 = bitmask for KEYi (1 << i)
    and t0, t0, s1                      # t0 = edge capture bit for KEYi
    beq t0, zero, next_key              # if KEYi was not pressed, skip

    slli t1, s3, 2                      # t1 = byte offset i * 4
    add t1, s2, t1                      # t1 = address of HEX_STATE[i]
    lw t2, 0(t1)                        # t2 = current toggle state for HEXi
    xori t2, t2, 1                      # toggle the state (0->1 or 1->0)
    sw t2, 0(t1)                        # store updated toggle state

    beq t2, zero, do_blank              # if state == 0, blank the display
    mv a0, s3                           # a0 = digit value i (same as key number)
    j do_display                        # jump to display call

do_blank:
    li a0, 0x10                         # a0 bit 4 = 1, blank the display

do_display:
    mv a1, s3                           # a1 = HEX display number i
    jal HEX_DISP                        # call HEX_DISP to update the display

next_key:
    addi s3, s3, 1                      # i++
    j key_loop                          # check next key

key_done:
    lw ra, 0(sp)                        # restore ra
    lw s0, 4(sp)                        # restore s0
    lw s1, 8(sp)                        # restore s1
    lw s2, 12(sp)                       # restore s2
    lw s3, 16(sp)                       # restore s3
    addi sp, sp, 20                     # deallocate stack frame
    ret                                 # return to interrupt handler

HEX_DISP:
    addi sp, sp, -16                    # allocate 16 bytes on the stack
    sw s0, 0(sp)                        # save s0
    sw s1, 4(sp)                        # save s1
    sw s2, 8(sp)                        # save s2
    sw s3, 12(sp)                       # save s3

    la   s0, BIT_CODES                  # s0 = base address of 7-seg bit patterns
    andi s1, a0, 0x10                   # s1 = bit 4 of a0 (blank flag)
    beq  s1, zero, not_blank            # if not blanking, go look up the digit
    mv   s2, zero                       # s2 = 0x00 (all segments off)
    j    DO_DISP                        # jump to display write logic

not_blank:
    andi a0, a0, 0x0f                   # keep only lower 4 bits of a0
    add  a0, a0, s0                     # a0 = address of BIT_CODES[digit]
    lb   s2, 0(a0)                      # s2 = 7-segment pattern for the digit

DO_DISP:
    la   s0, HEX_BASE1                  # s0 = HEX3-0 register base
    li   s1, 4                          # s1 = 4 (boundary value)
    blt  a1, s1, FIRST_SET              # if display < 4, use HEX_BASE1
    sub  a1, a1, s1                     # adjust index for HEX5-4 (subtract 4)
    addi s0, s0, 0x0010                 # point s0 to HEX_BASE2

FIRST_SET:
    slli a1, a1, 3                      # a1 = display_index * 8 (shift amount)
    addi s3, zero, 0xff                 # s3 = 0xFF (byte mask)
    sll  s3, s3, a1                     # shift mask to target byte position
    li   a0, -1                         # a0 = 0xFFFFFFFF
    xor  s3, s3, a0                     # invert mask to clear target byte only
    sll  a0, s2, a1                     # shift pattern to target byte position
    lw   a1, 0(s0)                      # a1 = current HEX register value
    and  a1, a1, s3                     # clear target byte
    or   a1, a1, a0                     # insert new pattern
    sw   a1, 0(s0)                      # write back to HEX register

    mv   a0, s2                         # return bit pattern in a0

    lw s0, 0(sp)                        # restore s0
    lw s1, 4(sp)                        # restore s1
    lw s2, 8(sp)                        # restore s2
    lw s3, 12(sp)                       # restore s3
    addi sp, sp, 16                     # deallocate stack space
    ret

.data

HEX_STATE:
    .word 0                             # HEX0 toggle state (initially blank)
    .word 0                             # HEX1 toggle state (initially blank)
    .word 0                             # HEX2 toggle state (initially blank)
    .word 0                             # HEX3 toggle state (initially blank)

BIT_CODES:
    .byte 0b00111111                    # 0
    .byte 0b00000110                    # 1
    .byte 0b01011011                    # 2
    .byte 0b01001111                    # 3
    .byte 0b01100110                    # 4
    .byte 0b01101101                    # 5
    .byte 0b01111101                    # 6
    .byte 0b00000111                    # 7
    .byte 0b01111111                    # 8
    .byte 0b01100111                    # 9
    .byte 0b01110111                    # A
    .byte 0b01111100                    # b
    .byte 0b00111001                    # C
    .byte 0b01011110                    # d
    .byte 0b01111001                    # E
    .byte 0b01110001                    # F

.end
