/* Mic Diagnostic — displays raw average amplitude on HEX.
 * No filtering, no threshold. Just shows what the mic is producing.
 * Use this to find the noise floor, then we'll add filtering.
 */

#define AUDIO_BASE 0xFF203040                     // audio port base address
#define HEX_BASE1  0xFF200020                     // HEX3-0 base address

// 7-segment bit patterns for hex digits 0-F
const char hex_codes[16] = {
    0x3F, 0x06, 0x5B, 0x4F,                      // 0, 1, 2, 3
    0x66, 0x6D, 0x7D, 0x07,                      // 4, 5, 6, 7
    0x7F, 0x67, 0x77, 0x7C,                      // 8, 9, A, b
    0x39, 0x5E, 0x79, 0x71                        // C, d, E, F
};

void display_hex(int value);                      // forward declaration

int main(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;  // pointer to audio port

    *(audio_ptr) = 0x4;                           // clear input FIFO
    *(audio_ptr) = 0x0;                           // release the clear

    display_hex(0);                               // show 0000 initially

    while (1) {                                   // infinite loop
        long long sum = 0;                        // running sum of absolute values
        int count = 0;                            // number of samples read

        // collect 4000 samples (0.5 seconds at 8kHz)
        while (count < 4000) {                    // keep reading until we have enough
            int fifospace = *(audio_ptr + 1);     // read FIFO space register
            int lavailable = fifospace & 0xFF;     // left input samples available

            if (lavailable > 0) {                 // if a sample is ready
                int sample = *(audio_ptr + 2);    // read left channel only
                *(audio_ptr + 3);                 // read and discard right channel (must read both)

                if (sample < 0)                   // take absolute value
                    sample = -sample;             // make positive

                sum = sum + sample;               // add to running total
                count++;                          // one more sample collected
            }
        }

        int average = (int)(sum / 4000);          // compute the average amplitude

        display_hex(average);                     // display it raw on HEX3-0
    }

    return 0;                                     // never reached
}

/* display_hex: shows a value as a hex number on HEX3-0 displays */
void display_hex(int value) {
    volatile int *HEX_ptr = (int *)HEX_BASE1;    // pointer to HEX3-0 register

    int digit0 = value & 0xF;                    // extract bits 3:0
    int digit1 = (value >> 4) & 0xF;             // extract bits 7:4
    int digit2 = (value >> 8) & 0xF;             // extract bits 11:8
    int digit3 = (value >> 12) & 0xF;            // extract bits 15:12

    int hex_display = 0;                          // start with all segments off
    hex_display = hex_display | hex_codes[digit0];             // HEX0
    hex_display = hex_display | (hex_codes[digit1] << 8);      // HEX1
    hex_display = hex_display | (hex_codes[digit2] << 16);     // HEX2
    hex_display = hex_display | (hex_codes[digit3] << 24);     // HEX3

    *HEX_ptr = hex_display;                      // write all four displays
}