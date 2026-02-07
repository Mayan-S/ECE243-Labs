.global _start
_start:

.equ LEDs,        0xFF200000            # red LED data register address
.equ TIMER,       0xFF202000            # interval timer base address
.equ PUSH_BUTTON, 0xFF200050            # pushbutton base address

    # 1. Turn off global interrupts during setup
    csrci mstatus, 0x8                  # clear MIE bit (bit 3) in mstatus

    # 2. Initialize the stack pointer
    li sp, 0x20000                      # sp = top of stack memory

    # 3. Configure the timer and the pushbutton keys
    jal CONFIG_TIMER                    # set up timer for 0.25 s, continuous, IRQ
    jal CONFIG_KEYS                     # enable pushbutton interrupts

    # 4. Set the interrupt handler address in mtvec
    la t0, interrupt_handler            # t0 = address of interrupt handler
    csrw mtvec, t0                      # write handler address to mtvec

    # 5. Enable IRQ 0 (timer) and IRQ 18 (pushbuttons) in mie
    li t0, 0x50000                      # t0 = (1<<18) | (1<<16) = IRQ18 + IRQ16
    csrw mie, t0                        # enable both interrupt sources

    # 6. Enable global interrupts
    csrsi mstatus, 0x8                  # set MIE bit (bit 3) in mstatus

    # 7. Main loop: continuously copy COUNT to the LEDs
    la s0, LEDs                         # s0 = LED register address (constant)
    la s1, COUNT                        # s1 = address of COUNT variable

LOOP:
    lw s2, 0(s1)                        # s2 = current value of COUNT
    sw s2, 0(s0)                        # write COUNT to the red LEDs
    j LOOP                              # repeat forever

interrupt_handler:
    addi sp, sp, -40                    # allocate stack frame
    sw s0, 0(sp)                        # save s0
    sw s1, 4(sp)                        # save s1
    sw s2, 8(sp)                        # save s2
    sw ra, 12(sp)                       # save return address
    sw a0, 16(sp)                       # save a0 (caller-saved)
    sw a1, 20(sp)                       # save a1 (caller-saved)
    sw t0, 24(sp)                       # save t0 (caller-saved)
    sw t1, 28(sp)                       # save t1 (caller-saved)
    sw t2, 32(sp)                       # save t2 (caller-saved)
    sw t3, 36(sp)                       # save t3 (caller-saved)

    li s0, 0x7FFFFFFF                   # s0 = mask to extract IRQ number
    csrr s1, mcause                     # s1 = mcause register value
    and s1, s1, s0                      # s1 = IRQ number (clear MSB)

    # Check for timer interrupt (IRQ 16)
    li s0, 16                           # s0 = timer IRQ number
    beq s1, s0, call_timer_isr          # if IRQ == 16, handle timer

    # Check for pushbutton interrupt (IRQ 18)
    li s0, 18                           # s0 = pushbutton IRQ number
    bne s1, s0, end_interrupt           # if not pushbuttons, skip
    jal KEY_ISR                         # call pushbutton ISR
    j end_interrupt                     # jump to exit

call_timer_isr:
    jal TIMER_ISR                       # call timer ISR

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
    mret                                # return from interrupt

CONFIG_TIMER:
    addi sp, sp, -4                     # allocate stack space
    sw ra, 0(sp)                        # save return address

    li t0, TIMER                        # t0 = timer base address
    sw zero, 0(t0)                      # clear the TO flag in status register

    li t1, 0x783F                       # t1 = lower 16 bits of period
    sw t1, 8(t0)                        # write period low register

    li t1, 0x017D                       # t1 = upper 16 bits of period
    sw t1, 12(t0)                       # write period high register

    li t1, 0x7                          # t1 = ITO | CONT | START = 0b0111
    sw t1, 4(t0)                        # write control: enable IRQ, continuous, start

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret

CONFIG_KEYS:
    addi sp, sp, -4                     # allocate stack space
    sw ra, 0(sp)                        # save return address

    li t0, PUSH_BUTTON                  # t0 = pushbutton base address
    li t1, 0xF                          # t1 = 0b1111 (all 4 keys)
    sw t1, 0xC(t0)                      # clear edge capture register first
    sw t1, 8(t0)                        # enable interrupts for all 4 keys

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret

TIMER_ISR:
    addi sp, sp, -4                     # allocate stack space
    sw ra, 0(sp)                        # save return address

    li t0, TIMER                        # t0 = timer base address
    sw zero, 0(t0)                      # clear TO flag (acknowledge interrupt)

    la t0, COUNT                        # t0 = address of COUNT variable
    lw t1, 0(t0)                        # t1 = current COUNT value
    la t2, RUN                          # t2 = address of RUN variable
    lw t3, 0(t2)                        # t3 = current RUN value (0 or 1)
    add t1, t1, t3                      # t1 = COUNT + RUN

    li t2, 256                          # t2 = overflow threshold
    blt t1, t2, no_reset                # if COUNT < 256, no overflow
    li t1, 0                            # reset COUNT to 0

no_reset:
    sw t1, 0(t0)                        # store updated COUNT

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret                                 # return to interrupt handler

KEY_ISR:
    addi sp, sp, -4                     # allocate stack space
    sw ra, 0(sp)                        # save return address

    li t0, PUSH_BUTTON                  # t0 = pushbutton base address
    lw t1, 0xC(t0)                      # t1 = edge capture register (which keys)
    sw t1, 0xC(t0)                      # clear edge capture (acknowledge interrupt)

    la t0, RUN                          # t0 = address of RUN variable
    lw t1, 0(t0)                        # t1 = current RUN value
    xori t1, t1, 1                      # toggle RUN: 0->1 or 1->0
    sw t1, 0(t0)                        # store updated RUN value

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret                                 # return to interrupt handler

.data

.global COUNT                           # make COUNT accessible globally
COUNT:
    .word 0x0                           # binary counter, displayed on LEDs

.global RUN                             # make RUN accessible globally
RUN:
    .word 0x1                           # 1 = counting, 0 = paused

.end