/* Part 1: Polling-based LED control using KEY pushbuttons
 * KEY0 pressed and released -> all 10 red LEDs turn on
 * KEY1 pressed and released -> all 10 red LEDs turn off
 * Uses edge capture register (polling, no interrupts)
 */

#define LEDR_BASE 0xFF200000                      // red LED data register address
#define KEY_BASE  0xFF200050                      // pushbutton base address

int main(void) {
    volatile int *LED_ptr = (int *)LEDR_BASE;     // pointer to red LED data register
    volatile int *KEY_ptr = (int *)KEY_BASE;       // pointer to pushbutton base address

    *(KEY_ptr + 3) = 0xF;                          // clear the edge capture register by writing 1s to it

    while (1) {                                    // infinite loop for continuous operation
        int edge = *(KEY_ptr + 3);                 // read the edge capture register (offset +3 words = +0xC bytes)

        if (edge & 0x1) {                          // check if KEY0 was pressed (bit 0)
            *LED_ptr = 0x3FF;                      // turn on all 10 LEDs (bits 9:0 all set to 1)
            *(KEY_ptr + 3) = 0xF;                  // clear edge capture register to acknowledge
        }

        if (edge & 0x2) {                          // check if KEY1 was pressed (bit 1)
            *LED_ptr = 0x0;                        // turn off all LEDs
            *(KEY_ptr + 3) = 0xF;                  // clear edge capture register to acknowledge
        }
    }

    return 0;                                      // never reached, but good practice
}
