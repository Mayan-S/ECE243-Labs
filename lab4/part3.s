# ECE 243S Lab 4 - Part III
# Binary counter on LEDs using hardware timer
# Counter increments every exactly 0.25 seconds
# Wraps from 255 back to 0
# Stop/Start on any KEY press (using edge capture register)

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
# For 0.25 seconds at 100 MHz: 100,000,000 * 0.25 = 25,000,000 cycles
# 25,000,000 = 0x17D7840
.equ TIMER_PERIOD_LOW, 0x7840       # Low 16 bits of 25,000,000
.equ TIMER_PERIOD_HIGH, 0x017D      # High 16 bits of 25,000,000

# Other constants
.equ MAX_COUNT, 255                 # Maximum counter value before wrap

.text                               # Start of text (code) section
_start:
    li sp, 0x20000                  # Initialize stack pointer to valid memory address
    li s0, 0                        # s0 = counter value (start at 0)
    li s1, LED_BASE                 # s1 = LED base address
    li s2, KEY_EDGE                 # s2 = KEY edge capture register address
    li s3, 1                        # s3 = running flag (1 = running, 0 = stopped)

    jal ra, init_timer              # Initialize the hardware timer
    jal ra, clear_edge_capture      # Clear any pending edge captures
    jal ra, update_leds             # Initialize LED display with counter value

# Main loop - increment counter using timer
main_loop:
    mv a0, s3                       # Pass running flag as argument
    jal ra, check_edge_capture      # Check if any key was pressed
    mv s3, a0                       # Store returned running flag

    beqz s3, main_loop              # If stopped (s3=0), keep checking for key press

    jal ra, wait_timer              # Wait for timer timeout (0.25 seconds)

    mv a0, s3                       # Pass running flag as argument
    jal ra, check_edge_capture      # Check for key press after timer
    mv s3, a0                       # Store returned running flag

    beqz s3, main_loop              # If stopped during wait, don't increment

    addi s0, s0, 1                  # Increment counter by 1
    li t0, MAX_COUNT                # Load max count value
    bgt s0, t0, reset_counter       # If counter > 255, reset to 0
    j update_display                # Otherwise, update display

reset_counter:
    li s0, 0                        # Reset counter to 0

update_display:
    jal ra, update_leds             # Update LED display with new counter value
    j main_loop                     # Continue main loop

# ============================================
# Subroutine: init_timer
# Purpose: Initialize the hardware timer for 0.25 second intervals
# Modifies: t0, t1
# ============================================
init_timer:
    li t0, TIMER_STATUS             # Load timer status register address
    sw zero, 0(t0)                  # Clear the TO (timeout) bit in status register

    li t0, TIMER_PERIOD_LO          # Load timer period low register address
    li t1, TIMER_PERIOD_LOW         # Load low 16 bits of period value
    sw t1, 0(t0)                    # Write low period value to timer

    li t0, TIMER_PERIOD_HI          # Load timer period high register address
    li t1, TIMER_PERIOD_HIGH        # Load high 16 bits of period value
    sw t1, 0(t0)                    # Write high period value to timer

    li t0, TIMER_CONTROL            # Load timer control register address
    li t1, 0x6                      # Set START=1 (bit 2) and CONT=1 (bit 1)
    sw t1, 0(t0)                    # Start the timer in continuous mode

    ret                             # Return to caller

# ============================================
# Subroutine: wait_timer
# Purpose: Poll timer until timeout occurs
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
# Subroutine: clear_edge_capture
# Purpose: Clear all bits in edge capture register
# Modifies: t0
# ============================================
clear_edge_capture:
    li t0, 0xF                      # Load value to clear all 4 edge bits
    sw t0, 0(s2)                    # Write 1s to clear edge capture bits

    ret                             # Return to caller

# ============================================
# Subroutine: check_edge_capture
# Purpose: Check edge capture register and toggle running state
# Input: a0 = current running flag, s2 = edge capture register address
# Output: a0 = updated running flag
# Modifies: t0
# ============================================
check_edge_capture:
    lw t0, 0(s2)                    # Read edge capture register
    beqz t0, edge_check_done        # If no edge detected, return unchanged

    # An edge was detected - toggle running state
    xori a0, a0, 1                  # Toggle running flag (0->1 or 1->0)

    # Clear the edge capture register
    sw t0, 0(s2)                    # Write back value to clear detected edges

edge_check_done:
    ret                             # Return to caller (result in a0)

# ============================================
# Subroutine: update_leds
# Purpose: Write current counter value to LED display
# Input: s0 = value to display, s1 = LED address
# Modifies: none
# ============================================
update_leds:
    sw s0, 0(s1)                    # Write counter value to LED data register
    ret                             # Return to caller

.end                                # End of assembly file