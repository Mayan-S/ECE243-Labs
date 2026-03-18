/* Mic Input — Peak amplitude detection with noise gate.
 * Noise floor measured at ~0xC000, so threshold set above that.
 * Only real sounds (above threshold) show on HEX3-0.
 * Silence shows 0000.
 */

#define AUDIO_BASE 0xFF203040                     // audio port base address
#define HEX_BASE1  0xFF200020                     // HEX3-0 base address

#define NOISE_FLOOR 0xD000                        // anything at or below this is noise

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
        int max_amplitude = 0;                    // peak value for this window

        // collect 4000 samples (0.5 seconds at 8kHz)
        int count = 0;                            // sample counter
        while (count < 4000) {                    // keep reading until we have enough
            int fifospace = *(audio_ptr + 1);     // read FIFO space register
            int lavailable = fifospace & 0xFF;     // left input samples available

            if (lavailable > 0) {                 // if a sample is ready
                int sample = *(audio_ptr + 2);    // read left channel
                *(audio_ptr + 3);                 // read and discard right channel

                if (sample < 0)                   // take absolute value
                    sample = -sample;             // make positive

                if (sample > max_amplitude)       // if this is the loudest so far
                    max_amplitude = sample;       // update the peak

                count++;                          // one more sample collected
            }
        }

        // apply noise gate: only show values above the noise floor
        if (max_amplitude <= NOISE_FLOOR) {       // if peak is within the noise range
            display_hex(0);                       // show 0000 (silence)
        } else {
            // subtract noise floor so the display starts from 0
            int display_value = max_amplitude - NOISE_FLOOR; // remove noise offset
            display_hex(display_value);           // show only the signal above noise
        }
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