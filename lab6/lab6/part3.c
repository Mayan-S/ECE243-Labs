/* Part 3: Square wave generator
 * Outputs a square wave to the audio device.
 * The 10 switches on the DE1-SoC control the frequency
 * across the audible range from ~100 Hz to ~2000 Hz.
 * Sampling rate is 8000 Hz.
 */

#define AUDIO_BASE 0xFF203040                     // audio port base address
#define SW_BASE    0xFF200040                     // switch data register address

#define SAMPLE_RATE 8000                          // audio sampling rate in Hz
#define MIN_FREQ    100                           // minimum frequency in Hz
#define MAX_FREQ    2000                          // maximum frequency in Hz
#define AMPLITUDE   0x3FFFFF                      // amplitude of the square wave (24-bit audio)

int main(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;  // pointer to audio port base address
    volatile int *sw_ptr = (int *)SW_BASE;        // pointer to switch data register

    *(audio_ptr) = 0xC;                           // clear both input and output FIFOs
    *(audio_ptr) = 0x0;                           // release the clear

    int sample_count = 0;                         // counts samples within one full wave period
    int half_period = SAMPLE_RATE / (2 * MIN_FREQ); // initial half-period in samples (default 100 Hz)

    while (1) {                                   // infinite loop
        int sw_value = *sw_ptr & 0x3FF;           // read switches, mask to 10 bits (0-1023)

        // map switch value to frequency: 100 Hz (sw=0) to 2000 Hz (sw=1023)
        int freq = MIN_FREQ + (sw_value * (MAX_FREQ - MIN_FREQ)) / 1023;

        half_period = SAMPLE_RATE / (2 * freq);   // compute half-period in samples
        if (half_period < 1) half_period = 1;     // clamp to minimum of 1 sample

        int fifospace = *(audio_ptr + 1);         // read the FIFO space register
        int wspace_l = (fifospace >> 16) & 0xFF;  // bits 23:16 = write space available (left)
        int wspace_r = (fifospace >> 24) & 0xFF;  // bits 31:24 = write space available (right)

        if (wspace_l > 0 && wspace_r > 0) {       // check that there is space to write
            int sample;                           // the sample value to output
            if (sample_count < half_period)       // first half of the period
                sample = AMPLITUDE;               // positive amplitude
            else                                  // second half of the period
                sample = -AMPLITUDE;              // negative amplitude

            *(audio_ptr + 2) = sample;            // write sample to left output FIFO
            *(audio_ptr + 3) = sample;            // write sample to right output FIFO

            sample_count++;                       // advance sample counter
            if (sample_count >= 2 * half_period)  // if we've completed one full period
                sample_count = 0;                 // reset counter for the next period
        }
    }

    return 0;                                     // never reached
}
