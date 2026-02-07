.global _start
_start:

.equ LEDs,        0xFF200000            # red LED data register address
.equ TIMER,       0xFF202000            # interval timer base address
.equ PUSH_BUTTON, 0xFF200050            # pushbutton base address

    # 1. Turn off global interrupts during setup
    csrci mstatus, 0x8                  # clear MIE bit in mstatus

    # 2. Initialize the stack pointer
    li sp, 0x20000                      # sp = top of stack memory

    # 3. Configure timer and pushbutton keys
    jal CONFIG_TIMER                    # set up timer for 0.25 s, continuous, IRQ
    jal CONFIG_KEYS                     # enable pushbutton interrupts

    # 4. Set the interrupt handler address in mtvec
    la t0, interrupt_handler            # t0 = address of interrupt handler
    csrw mtvec, t0                      # write handler address to mtvec

    # 5. Enable IRQ 0 (timer) and IRQ 18 (pushbuttons) in mie
    li t0, 0x50000                      # t0 = (1<<18) | (1<<16) = IRQ18 + IRQ16
    csrw mie, t0                        # enable both interrupt sources

    # 6. Enable global interrupts
    csrsi mstatus, 0x8                  # set MIE bit in mstatus

    # 7. Main loop: continuously copy COUNT to the LEDs
    la s0, LEDs                         # s0 = LED register address
    la s1, COUNT                        # s1 = address of COUNT variable

LOOP:
    lw s2, 0(s1)                        # s2 = current COUNT value
    sw s2, 0(s0)                        # write COUNT to the red LEDs
    j LOOP                              # repeat forever

interrupt_handler:
    addi sp, sp, -40                    # allocate stack frame
    sw s0, 0(sp)                        # save s0
    sw s1, 4(sp)                        # save s1
    sw s2, 8(sp)                        # save s2
    sw ra, 12(sp)                       # save return address
    sw a0, 16(sp)                       # save a0
    sw a1, 20(sp)                       # save a1
    sw t0, 24(sp)                       # save t0
    sw t1, 28(sp)                       # save t1
    sw t2, 32(sp)                       # save t2
    sw t3, 36(sp)                       # save t3

    li s0, 0x7FFFFFFF                   # s0 = mask to extract IRQ number
    csrr s1, mcause                     # s1 = mcause value
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

    li t1, 0x783F                       # t1 = lower 16 bits of default period
    sw t1, 8(t0)                        # write period low register

    li t1, 0x017D                       # t1 = upper 16 bits of default period
    sw t1, 12(t0)                       # write period high register

    li t1, 0x7                          # t1 = ITO | CONT | START = 0b0111
    sw t1, 4(t0)                        # enable IRQ, continuous mode, start timer

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret

CONFIG_KEYS:
    addi sp, sp, -4                     # allocate stack space
    sw ra, 0(sp)                        # save return address

    li t0, PUSH_BUTTON                  # t0 = pushbutton base address
    li t1, 0xF                          # t1 = 0b1111 (all 4 keys)
    sw t1, 0xC(t0)                      # clear edge capture register
    sw t1, 8(t0)                        # enable interrupts for all 4 keys

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret

TIMER_ISR:
    addi sp, sp, -4                     # allocate stack space
    sw ra, 0(sp)                        # save return address

    li t0, TIMER                        # t0 = timer base address
    sw zero, 0(t0)                      # clear TO flag (acknowledge interrupt)

    la t0, COUNT                        # t0 = address of COUNT
    lw t1, 0(t0)                        # t1 = current COUNT value
    la t2, RUN                          # t2 = address of RUN
    lw t3, 0(t2)                        # t3 = RUN value (0 or 1)
    add t1, t1, t3                      # t1 = COUNT + RUN

    li t2, 256                          # t2 = overflow threshold
    blt t1, t2, no_reset                # if COUNT < 256, keep value
    li t1, 0                            # reset COUNT to 0

no_reset:
    sw t1, 0(t0)                        # store updated COUNT

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret                                 # return to handler

KEY_ISR:
    addi sp, sp, -8                     # allocate stack space
    sw ra, 0(sp)                        # save return address
    sw s0, 4(sp)                        # save s0

    li t0, PUSH_BUTTON                  # t0 = pushbutton base address
    lw s0, 0xC(t0)                      # s0 = edge capture (which keys pressed)
    sw s0, 0xC(t0)                      # clear edge capture register

    #  Check KEY0: toggle RUN 
    andi t0, s0, 0x1                    # t0 = bit 0 of edge capture (KEY0)
    beq t0, zero, check_key1            # if KEY0 not pressed, check KEY1
    la t0, RUN                          # t0 = address of RUN
    lw t1, 0(t0)                        # t1 = current RUN value
    xori t1, t1, 1                      # toggle RUN: 0->1 or 1->0
    sw t1, 0(t0)                        # store updated RUN

check_key1:
    #  Check KEY1: double speed (halve period) 
    andi t0, s0, 0x2                    # t0 = bit 1 of edge capture (KEY1)
    beq t0, zero, check_key2            # if KEY1 not pressed, check KEY2

    la t0, CURRENT_PERIOD               # t0 = address of CURRENT_PERIOD
    lw t1, 0(t0)                        # t1 = current timer period
    srli t1, t1, 1                      # t1 = period / 2 (double the speed)

    la t2, MIN_PERIOD                   # t2 = address of MIN_PERIOD
    lw t3, 0(t2)                        # t3 = minimum allowed period
    bge t1, t3, store_key1              # if new period >= min, use it
    mv t1, t3                           # clamp to minimum period

store_key1:
    sw t1, 0(t0)                        # store updated period
    mv a0, t1                           # a0 = new period value
    jal SET_TIMER_PERIOD                # stop timer, set new period, restart

check_key2:
    #  Check KEY2: halve speed (double period) 
    andi t0, s0, 0x4                    # t0 = bit 2 of edge capture (KEY2)
    beq t0, zero, key_isr_done          # if KEY2 not pressed, done

    la t0, CURRENT_PERIOD               # t0 = address of CURRENT_PERIOD
    lw t1, 0(t0)                        # t1 = current timer period
    slli t1, t1, 1                      # t1 = period * 2 (halve the speed)

    la t2, MAX_PERIOD                   # t2 = address of MAX_PERIOD
    lw t3, 0(t2)                        # t3 = maximum allowed period
    ble t1, t3, store_key2              # if new period <= max, use it
    mv t1, t3                           # clamp to maximum period

store_key2:
    sw t1, 0(t0)                        # store updated period
    mv a0, t1                           # a0 = new period value
    jal SET_TIMER_PERIOD                # stop timer, set new period, restart

key_isr_done:
    lw ra, 0(sp)                        # restore return address
    lw s0, 4(sp)                        # restore s0
    addi sp, sp, 8                      # deallocate stack space
    ret                                 # return to handler

SET_TIMER_PERIOD:
    addi sp, sp, -4                     # allocate stack space
    sw ra, 0(sp)                        # save return address

    li t0, TIMER                        # t0 = timer base address

    # Stop the timer
    li t1, 0x8                          # t1 = STOP bit (bit 3)
    sw t1, 4(t0)                        # write STOP to control register

    # Clear the timeout flag
    sw zero, 0(t0)                      # clear TO flag in status register

    # Write the new period (a0 holds the period value)
    # Extract lower 16 bits
    li t1, 0xFFFF                       # t1 = 16-bit mask
    and t2, a0, t1                      # t2 = lower 16 bits of period
    sw t2, 8(t0)                        # write period low register

    # Extract upper 16 bits
    srli t2, a0, 16                     # t2 = upper 16 bits of period
    sw t2, 12(t0)                       # write period high register

    # Restart the timer with IRQ and continuous mode
    li t1, 0x7                          # t1 = ITO | CONT | START
    sw t1, 4(t0)                        # write control register

    lw ra, 0(sp)                        # restore return address
    addi sp, sp, 4                      # deallocate stack space
    ret

.data

.global COUNT                           # make COUNT accessible globally
COUNT:
    .word 0x0                           # binary counter displayed on LEDs

.global RUN                             # make RUN accessible globally
RUN:
    .word 0x1                           # 1 = counting, 0 = paused

CURRENT_PERIOD:
    .word 24999999                      # current timer period (0.25 s default)

MIN_PERIOD:
    .word 781249                        # minimum period (~0.0078 s, fastest)

MAX_PERIOD:
    .word 399999999                     # maximum period (4.0 s, slowest)

.end