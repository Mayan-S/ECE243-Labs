# ECE 243S Lab 4 - Part IV
# Real-time binary clock on LEDs
# Seconds displayed on LEDR9:7 (3 bits, 0-7)
# Hundredths displayed on LEDR6:0 (7 bits, 0-99)
# Uses hardware timer for 0.01 second intervals
# Stop/Start on any KEY press
# Wraps at 7 seconds 99 hundredths to 000:0000000

.global _start                      # Declare _start as global entry point

# Memory-mapped I/O addresses
.equ LED_BASE, 0xFF200000           # Base address for red LEDs data register
.equ KEY_BASE, 0xFF200050           # Base address for pushbutton KEYs data register
.equ KEY_EDGE, 0xFF20005C           # Address for KEY edge capture register

# Timer register addresses
.equ TIMER_BASE, 0xFF202000         # Base address for interval timer
.equ TIMER_STATUS, 0xFF202000       # Timer status register (TO and RUN bits)
.equ TIMER_CONTROL, 0xFF202004      # Timer control register (STOP, START, CONT, ITO)
.equ TIMER_PERIOD_LO, 0xFF202008    # Timer counter start value (low 16 bits)
.equ TIMER_PERIOD_HI, 0xFF20200C    # Timer counter start value (high 16 bits)

# Timer constants
# For 0.01 seconds at 100 MHz: 100,000,000 * 0.01 = 1,000,000 cycles
# 1,000,000 = 0x0F4240
.equ TIMER_PERIOD_LOW, 0x4240       # Low 16 bits of 1,000,000
.equ TIMER_PERIOD_HIGH, 0x000F      # High 16 bits of 1,000,000

# Clock constants
.equ MAX_HUNDREDTHS, 99             # Maximum hundredths value (0-99)
.equ MAX_SECONDS, 7                 # Maximum seconds value (0-7)

.text                               # Start of text (code) section
_start:
    li sp, 0x20000                  # Initialize stack pointer to valid memory address
    li s0, 0                        # s0 = hundredths counter (0-99)
    li s1, 0                        # s1 = seconds counter (0-7)
    li s2, LED_BASE                 # s2 = LED base address
    li s3, KEY_EDGE                 # s3 = KEY edge capture register address
    li s4, 1                        # s4 = running flag (1 = running, 0 = stopped)

    jal ra, init_timer              # Initialize the hardware timer for 0.01s
    jal ra, clear_edge_capture      # Clear any pending edge captures
    jal ra, update_display          # Initialize LED display

# Main loop - update clock every 0.01 seconds
main_loop:
    mv a0, s4                       # Pass running flag as argument
    jal ra, check_edge_capture      # Check if any key was pressed
    mv s4, a0                       # Store returned running flag

    beqz s4, main_loop              # If stopped (s4=0), keep checking for key press

    jal ra, wait_timer              # Wait for timer timeout (0.01 seconds)

    mv a0, s4                       # Pass running flag as argument
    jal ra, check_edge_capture      # Check for key press after timer
    mv s4, a0                       # Store returned running flag

    beqz s4, main_loop              # If stopped during wait, don't increment

    mv a0, s0                       # Pass hundredths as first argument
    mv a1, s1                       # Pass seconds as second argument
    jal ra, increment_clock         # Increment the clock values
    mv s0, a0                       # Store returned hundredths
    mv s1, a1                       # Store returned seconds

    jal ra, update_display          # Update LED display with new time

    j main_loop                     # Continue main loop

# ============================================
# Subroutine: init_timer
# Purpose: Initialize the hardware timer for 0.01 second intervals
# Modifies: t0, t1
# ============================================
init_timer:
    li t0, TIMER_STATUS             # Load timer status register address
    sw zero, 0(t0)                  # Clear the TO (timeout) bit in status register

    li t0, TIMER_PERIOD_LO          # Load timer period low register address
    li t1, TIMER_PERIOD_LOW         # Load low 16 bits of period value (0x4240)
    sw t1, 0(t0)                    # Write low period value to timer

    li t0, TIMER_PERIOD_HI          # Load timer period high register address
    li t1, TIMER_PERIOD_HIGH        # Load high 16 bits of period value (0x000F)
    sw t1, 0(t0)                    # Write high period value to timer

    li t0, TIMER_CONTROL            # Load timer control register address
    li t1, 0x6                      # Set START=1 (bit 2) and CONT=1 (bit 1)
    sw t1, 0(t0)                    # Start the timer in continuous mode

    ret                             # Return to caller

# ============================================
# Subroutine: wait_timer
# Purpose: Poll timer until timeout occurs (0.01 seconds)
# Modifies: t0, t1
# ============================================
wait_timer:
    li t0, TIMER_STATUS             # Load timer status register address

timer_poll_loop:
    lw t1, 0(t0)                    # Read timer status register
    andi t1, t1, 0x1                # Mask the TO (timeout) bit (bit 0)
    beqz t1, timer_poll_loop        # If TO=0, keep polling

    # Timeout occurred - clear the TO bit
    sw zero, 0(t0)                  # Write 0 to clear TO bit in status register

    ret                             # Return to caller

# ============================================
# Subroutine: increment_clock
# Purpose: Increment hundredths, and seconds if needed
# Input: a0 = hundredths, a1 = seconds
# Output: a0 = updated hundredths, a1 = updated seconds
# Modifies: t0
# ============================================
increment_clock:
    addi a0, a0, 1                  # Increment hundredths by 1

    li t0, MAX_HUNDREDTHS           # Load max hundredths value (99)
    ble a0, t0, clock_done          # If hundredths <= 99, we're done

    # Hundredths overflow - reset and increment seconds
    li a0, 0                        # Reset hundredths to 0
    addi a1, a1, 1                  # Increment seconds by 1

    li t0, MAX_SECONDS              # Load max seconds value (7)
    ble a1, t0, clock_done          # If seconds <= 7, we're done

    # Seconds overflow - reset to 0
    li a1, 0                        # Reset seconds to 0 (full wrap around)

clock_done:
    ret                             # Return to caller

# ============================================
# Subroutine: update_display
# Purpose: Combine seconds and hundredths and write to LEDs
# Input: s0 = hundredths (bits 6:0), s1 = seconds (bits 9:7)
# Modifies: t0, t1
# ============================================
update_display:
    mv t0, s0                       # Copy hundredths to t0 (bits 6:0)
    slli t1, s1, 7                  # Shift seconds left by 7 bits (to bits 9:7)
    or t0, t0, t1                   # Combine: seconds in bits 9:7, hundredths in bits 6:0

    sw t0, 0(s2)                    # Write combined value to LED data register

    ret                             # Return to caller

# ============================================
# Subroutine: clear_edge_capture
# Purpose: Clear all bits in edge capture register
# Modifies: t0
# ============================================
clear_edge_capture:
    li t0, 0xF                      # Load value to clear all 4 edge bits
    sw t0, 0(s3)                    # Write 1s to clear edge capture bits

    ret                             # Return to caller

# ============================================
# Subroutine: check_edge_capture
# Purpose: Check edge capture register and toggle running state
# Input: a0 = current running flag, s3 = edge capture register address
# Output: a0 = updated running flag
# Modifies: t0
# ============================================
check_edge_capture:
    lw t0, 0(s3)                    # Read edge capture register
    beqz t0, edge_check_done        # If no edge detected, return unchanged

    # An edge was detected - toggle running state
    xori a0, a0, 1                  # Toggle running flag (0->1 or 1->0)

    # Clear the edge capture register
    sw t0, 0(s3)                    # Write back value to clear detected edges

edge_check_done:
    ret                             # Return to caller (result in a0)

.end                                # End of assembly file