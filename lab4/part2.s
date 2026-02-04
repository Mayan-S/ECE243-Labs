.global _start                      

# Memory-mapped I/O addresses
.equ LED_BASE, 0xFF200000           # Base address for red LEDs data register
.equ KEY_BASE, 0xFF200050           # Base address for pushbutton KEYs data register
.equ KEY_EDGE, 0xFF20005C           # Address for KEY edge capture register

# Constants
.equ MAX_COUNT, 255                 # Maximum counter value before wrap
.equ COUNTER_DELAY, 500000          # Delay value for CPUlator (use 10000000 for DE1-SoC)

.text                               
_start:
    li sp, 0x20000                  # Initialize stack pointer
    li s0, 0                        # s0 = counter value (start at 0)
    li s1, LED_BASE                 # s1 = LED base address
    li s2, KEY_BASE                 # s2 = KEY base address
    li s3, KEY_EDGE                 # s3 = KEY edge capture register address
    li s4, 1                        # s4 = running flag (1 = running, 0 = stopped)

    sw zero, 0(s3)                  # Clear edge capture register by writing 0
    li t0, 0xF                      # Load value to clear all 4 edge bits
    sw t0, 0(s3)                    # Clear all edge capture bits (write 1 to reset)

    jal ra, update_leds             # Initialize LED display with counter value

# Main loop (increment counter with delay)
main_loop:
    mv a0, s4                       # Pass running flag as argument
    jal ra, check_edge_capture      # Check if any key was pressed
    mv s4, a0                       # Store returned running flag

    beqz s4, main_loop              # If stopped (s4=0), skip delay and increment

    jal ra, do_delay                # Call delay subroutine (~0.25 seconds)

    mv a0, s4                       # Pass running flag as argument
    jal ra, check_edge_capture      # Check again after delay for responsiveness
    mv s4, a0                       # Store returned running flag

    beqz s4, main_loop              # If stopped during delay, do not increment

    addi s0, s0, 1                  # Increment counter by 1
    li t0, MAX_COUNT                # Load max count value
    bgt s0, t0, reset_counter       # If counter > 255, reset to 0
    j update_display                # Otherwise, update display

reset_counter:
    li s0, 0                        # Reset counter to 0

update_display:
    jal ra, update_leds             # Update LED display with new counter value
    j main_loop                     # Continue main loop

check_edge_capture:
    lw t0, 0(s3)                    # Read edge capture register
    beqz t0, edge_done              # If no edge detected, return unchanged

    xori a0, a0, 1                  # Toggle running flag (0->1 or 1->0)

    sw t0, 0(s3)                    # Write back the value to clear detected edges

edge_done:
    ret                             

do_delay:
    li t0, COUNTER_DELAY            # Load delay counter value

delay_loop:
    addi t0, t0, -1                 # Decrement delay counter by 1
    bnez t0, delay_loop             # If counter not zero, keep looping
    ret                             

update_leds:
    sw s0, 0(s1)                    # Write counter value to LED data register
    ret                             

.end                                