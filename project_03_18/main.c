/* Mic Diagnostic — shows raw peak value with NO processing.
 * Run this and report:
 * 1. What HEX shows with mic plugged in, silence
 * 2. What HEX shows with loud noise
 * 3. What HEX shows with mic unplugged
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
        int max_val = 0;                          // highest raw peak this window
        int count = 0;                            // sample counter

        while (count < 4000) {                    // collect 0.5 seconds of samples
            int fifospace = *(audio_ptr + 1);     // read FIFO space register
            int lavailable = fifospace & 0xFF;     // left input samples available

            if (lavailable > 0) {                 // if a sample is ready
                int sample = *(audio_ptr + 2);    // read left channel
                *(audio_ptr + 3);                 // discard right channel

                if (sample < 0)                   // absolute value
                    sample = -sample;

                if (sample > max_val)             // track the peak
                    max_val = sample;

                count++;
            }
        }

        // shift right by 8 to fit the 24-bit value into 16 bits for display
        display_hex(max_val >> 8);                // show upper 16 bits of raw peak
    }

    return 0;
}

/* display_hex: shows a value as a hex number on HEX3-0 displays */
void display_hex(int value) {
    volatile int *HEX_ptr = (int *)HEX_BASE1;

    int digit0 = value & 0xF;
    int digit1 = (value >> 4) & 0xF;
    int digit2 = (value >> 8) & 0xF;
    int digit3 = (value >> 12) & 0xF;

    int hex_display = 0;
    hex_display = hex_display | hex_codes[digit0];
    hex_display = hex_display | (hex_codes[digit1] << 8);
    hex_display = hex_display | (hex_codes[digit2] << 16);
    hex_display = hex_display | (hex_codes[digit3] << 24);

    *HEX_ptr = hex_display;
}