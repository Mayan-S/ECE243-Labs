# ECE 243S Lab 4 - Part I
# LED Display controlled by pushbutton KEYs using polling
# KEY0: Set display to 1
# KEY1: Increment (max 15)
# KEY2: Decrement (min 1)
# KEY3: Blank display (0)

.global _start                      # Declare _start as global entry point

# Memory-mapped I/O addresses
.equ LED_BASE, 0xFF200000           # Base address for red LEDs data register
.equ KEY_BASE, 0xFF200050           # Base address for pushbutton KEYs data register

# Constants
.equ MIN_VALUE, 1                   # Minimum allowed display value
.equ MAX_VALUE, 15                  # Maximum allowed display value
.equ BLANK_VALUE, 0                 # Value for blanked display

.text                               # Start of text (code) section
_start:
    li s0, 0                        # s0 = current display value (start at 0/blank)
    li s1, LED_BASE                 # s1 = LED base address
    li s2, KEY_BASE                 # s2 = KEY base address

    jal ra, update_leds             # Call subroutine to update LED display

# Main polling loop - continuously check for button presses
main_loop:
    lw t0, 0(s2)                    # Read KEY data register into t0
    beqz t0, main_loop              # If no key pressed, keep polling

    # A key is pressed - check if display is blank first
    bnez s0, check_key0             # If display not blank, check which key
    andi t1, t0, 0x8                # Check if KEY3 is pressed
    bnez t1, wait_release           # If KEY3, stay blank (do nothing)
    li s0, 1                        # Any other key when blank: set to 1
    j wait_release                  # Go wait for release

    # A key is pressed - determine which one
check_key0:
    andi t1, t0, 0x1                # Mask bit 0 (KEY0)
    beqz t1, check_key1             # If KEY0 not pressed, check KEY1
    li s0, 1                        # KEY0 pressed: set display value to 1
    j wait_release                  # Jump to wait for key release

check_key1:
    andi t1, t0, 0x2                # Mask bit 1 (KEY1)
    beqz t1, check_key2             # If KEY1 not pressed, check KEY2
    li t2, MAX_VALUE                # Load maximum value into t2
    bge s0, t2, wait_release        # If current >= max, don't increment
    addi s0, s0, 1                  # KEY1 pressed: increment display value
    j wait_release                  # Jump to wait for key release

check_key2:
    andi t1, t0, 0x4                # Mask bit 2 (KEY2)
    beqz t1, check_key3             # If KEY2 not pressed, check KEY3
    li t2, MIN_VALUE                # Load minimum value into t2
    ble s0, t2, wait_release        # If current <= min, don't decrement
    addi s0, s0, -1                 # KEY2 pressed: decrement display value
    j wait_release                  # Jump to wait for key release

check_key3:
    andi t1, t0, 0x8                # Mask bit 3 (KEY3)
    beqz t1, wait_release           # If KEY3 not pressed, just wait
    li s0, BLANK_VALUE              # KEY3 pressed: set display to blank (0)

# Wait for the pressed key to be released before continuing
wait_release:
    jal ra, update_leds             # Update LEDs with new value

release_loop:
    lw t0, 0(s2)                    # Read KEY data register
    bnez t0, release_loop           # If any key still pressed, keep waiting
    j main_loop                     # Key released, return to main polling loop

# ============================================
# Subroutine: update_leds
# Purpose: Write current value to LED display
# Input: s0 = value to display
# Modifies: t0
# ============================================
update_leds:
    sw s0, 0(s1)                    # Write current value to LED data register
    ret                             # Return to caller

.end                                # End of assembly file