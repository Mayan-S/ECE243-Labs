.global _start
_start:

.equ HEX_BASE1, 0xff200020         # memory-mapped address for HEX3-HEX0
.equ HEX_BASE2, 0xff200030         # memory-mapped address for HEX5-HEX4

    li sp, 0x20000                  # initialize the stack pointer

    #  Display digit 0 on HEX0 
    li a0, 0x0                      # a0 = digit value 0
    li a1, 0                        # a1 = target display HEX0
    jal HEX_DISP                    # call HEX_DISP subroutine

    #  Display digit 3 on HEX1 
    li a0, 0x3                      # a0 = digit value 3
    li a1, 1                        # a1 = target display HEX1
    jal HEX_DISP                    # call HEX_DISP subroutine

    #  Display digit 7 on HEX2 
    li a0, 0x7                      # a0 = digit value 7
    li a1, 2                        # a1 = target display HEX2
    jal HEX_DISP                    # call HEX_DISP subroutine

    #  Display digit A (10) on HEX3 
    li a0, 0xA                      # a0 = digit value A (hex)
    li a1, 3                        # a1 = target display HEX3
    jal HEX_DISP                    # call HEX_DISP subroutine

    #  Display digit F (15) on HEX4 
    li a0, 0xF                      # a0 = digit value F (hex)
    li a1, 4                        # a1 = target display HEX4
    jal HEX_DISP                    # call HEX_DISP subroutine

    #  Display digit 5 on HEX5 
    li a0, 0x5                      # a0 = digit value 5
    li a1, 5                        # a1 = target display HEX5
    jal HEX_DISP                    # call HEX_DISP subroutine

    #  Blank HEX3 to demonstrate blanking 
    li a0, 0x10                     # a0 bit 4 = 1 means blank the display
    li a1, 3                        # a1 = target display HEX3
    jal HEX_DISP                    # call HEX_DISP subroutine

iloop:
    j iloop                         # idle loop — program done

HEX_DISP:
    addi sp, sp, -16               # allocate 16 bytes on the stack
    sw s0, 0(sp)                   # save s0 on the stack
    sw s1, 4(sp)                   # save s1 on the stack
    sw s2, 8(sp)                   # save s2 on the stack
    sw s3, 12(sp)                  # save s3 on the stack

    la   s0, BIT_CODES             # s0 = base address of 7-seg bit patterns
    andi s1, a0, 0x10              # s1 = bit 4 of a0 (blank flag)
    beq  s1, zero, not_blank       # if blank flag is 0, go display the digit
    mv   s2, zero                  # s2 = 0x00 (all segments off = blank)
    j    DO_DISP                   # jump to display logic

not_blank:
    andi a0, a0, 0x0f              # mask a0 to keep only lower 4 bits
    add  a0, a0, s0               # a0 = address of the bit code for this digit
    lb   s2, 0(a0)                # s2 = 7-segment bit pattern for the digit

DO_DISP:
    la   s0, HEX_BASE1            # s0 = base address of HEX3-0 register
    li   s1, 4                    # s1 = 4 (boundary between first/second set)
    blt  a1, s1, FIRST_SET        # if display < 4, use HEX_BASE1
    sub  a1, a1, s1               # adjust index for second register (4->0, 5->1)
    addi s0, s0, 0x0010           # s0 = HEX_BASE2 (HEX5-4 register)

FIRST_SET:
    slli a1, a1, 3                # a1 = display_index * 8 (bit shift amount)
    addi s3, zero, 0xff           # s3 = 0xFF (single-byte mask)
    sll  s3, s3, a1               # shift mask to target byte position
    li   a0, -1                   # a0 = 0xFFFFFFFF (all ones)
    xor  s3, s3, a0               # invert mask to clear target byte
    sll  a0, s2, a1               # shift bit pattern to target byte position
    lw   a1, 0(s0)                # a1 = current value of HEX register
    and  a1, a1, s3               # clear the target byte in current value
    or   a1, a1, a0               # insert new bit pattern into target byte
    sw   a1, 0(s0)                # write updated value back to HEX register

END:
    mv   a0, s2                   # return the 7-segment bit pattern in a0

    lw s0, 0(sp)                  # restore s0 from the stack
    lw s1, 4(sp)                  # restore s1 from the stack
    lw s2, 8(sp)                  # restore s2 from the stack
    lw s3, 12(sp)                 # restore s3 from the stack
    addi sp, sp, 16               # deallocate stack space
    ret

.data
BIT_CODES:
    .byte 0b00111111               # 0: segments a,b,c,d,e,f
    .byte 0b00000110               # 1: segments b,c
    .byte 0b01011011               # 2: segments a,b,d,e,g
    .byte 0b01001111               # 3: segments a,b,c,d,g
    .byte 0b01100110               # 4: segments b,c,f,g
    .byte 0b01101101               # 5: segments a,c,d,f,g
    .byte 0b01111101               # 6: segments a,c,d,e,f,g
    .byte 0b00000111               # 7: segments a,b,c
    .byte 0b01111111               # 8: all segments
    .byte 0b01100111               # 9: segments a,b,c,f,g
    .byte 0b01110111               # A: segments a,b,c,e,f,g
    .byte 0b01111100               # b: segments c,d,e,f,g
    .byte 0b00111001               # C: segments a,d,e,f
    .byte 0b01011110               # d: segments b,c,d,e,g
    .byte 0b01111001               # E: segments a,d,e,f,g
    .byte 0b01110001               # F: segments a,e,f,g

.end
