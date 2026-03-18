/* Mic Input — Read audio samples and track the max amplitude value.
 * Displays smoothed peak amplitude on HEX3-0 displays.
 * Includes noise threshold and smoothing to prevent erratic display.
 */

#define AUDIO_BASE 0xFF203040                     // audio port base address
#define HEX_BASE1  0xFF200020                     // HEX3-0 base address

#define NOISE_THRESHOLD 0x2000                    // amplitudes below this are treated as silence
#define SMOOTHING 4                               // smoothing factor (higher = smoother but slower)

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

    *(audio_ptr) = 0x4;                           // clear input FIFO (bit 2 = CI)
    *(audio_ptr) = 0x0;                           // release the clear

    int max_amplitude = 0;                        // tracks the highest sample value in current window
    int sample_count = 0;                         // counts samples in the current window
    int window_size = 2400;                       // samples per window (0.3s at 8kHz, slower updates)
    int smoothed = 0;                             // smoothed display value (persists across windows)

    display_hex(0);                               // clear HEX displays to show 0000 initially

    while (1) {                                   // infinite loop
        int fifospace = *(audio_ptr + 1);         // read FIFO space register

        int ravailable = (fifospace >> 8) & 0xFF; // samples available in right input FIFO
        int lavailable = fifospace & 0xFF;         // samples available in left input FIFO

        if (lavailable > 0 && ravailable > 0) {   // if input samples are ready
            int left_sample = *(audio_ptr + 2);   // read left input sample
            int right_sample = *(audio_ptr + 3);  // read right input sample

            // compute absolute value of left sample
            int abs_left = left_sample;            // start with the raw value
            if (abs_left < 0)                     // if negative
                abs_left = -abs_left;             // make it positive

            // compute absolute value of right sample
            int abs_right = right_sample;          // start with the raw value
            if (abs_right < 0)                    // if negative
                abs_right = -abs_right;           // make it positive

            // pick the larger of the two channels
            int sample_amp = abs_left;            // assume left is larger
            if (abs_right > abs_left)             // if right is actually larger
                sample_amp = abs_right;           // use right instead

            // update max if this sample is louder
            if (sample_amp > max_amplitude)       // compare with current max
                max_amplitude = sample_amp;       // update max

            sample_count++;                       // increment sample counter

            // after one window of samples, output the result and reset
            if (sample_count >= window_size) {    // if we've collected enough samples

                // apply noise threshold
                if (max_amplitude < NOISE_THRESHOLD) // if below noise floor
                    max_amplitude = 0;            // treat as silence

                // apply smoothing: blend new peak with previous displayed value
                // smoothed = ((SMOOTHING-1) * smoothed + max_amplitude) / SMOOTHING
                // this prevents sudden jumps in the display
                smoothed = ((SMOOTHING - 1) * smoothed + max_amplitude) / SMOOTHING;

                display_hex(smoothed);            // display smoothed amplitude on HEX3-0

                max_amplitude = 0;                // reset max for next window
                sample_count = 0;                 // reset sample counter
            }
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

    int hex_display = 0;                          // build the full 32-bit register value
    hex_display = hex_display | hex_codes[digit0];             // HEX0 occupies bits 7:0
    hex_display = hex_display | (hex_codes[digit1] << 8);      // HEX1 occupies bits 15:8
    hex_display = hex_display | (hex_codes[digit2] << 16);     // HEX2 occupies bits 23:16
    hex_display = hex_display | (hex_codes[digit3] << 24);     // HEX3 occupies bits 31:24

    *HEX_ptr = hex_display;                      // write all four displays at once
}