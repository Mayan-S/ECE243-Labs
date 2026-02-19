/* Part 4: Audio echo effect
 * Reads audio from the input and outputs it with an echo,
 * simulating a canyon echo effect.
 * Delay is ~0.4 seconds (3200 samples at 8 kHz).
 * The echo feeds back so it repeats and decays naturally.
 */

#define AUDIO_BASE 0xFF203040                     // audio port base address

#define SAMPLE_RATE 8000                          // audio sampling rate in Hz
#define DELAY_SAMPLES 3200                        // 0.4 seconds * 8000 Hz = 3200 samples

int left_buffer[DELAY_SAMPLES];                   // circular buffer for left channel echo history
int right_buffer[DELAY_SAMPLES];                  // circular buffer for right channel echo history

int main(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;  // pointer to audio port base address

    *(audio_ptr) = 0xC;                           // clear both input and output FIFOs
    *(audio_ptr) = 0x0;                           // release the clear

    int buf_index = 0;                            // current position in the circular buffer

    // initialize both buffers to zero (silence)
    for (int i = 0; i < DELAY_SAMPLES; i++) {     // loop through all buffer positions
        left_buffer[i] = 0;                       // set left buffer sample to 0
        right_buffer[i] = 0;                      // set right buffer sample to 0
    }

    while (1) {                                   // infinite loop
        int fifospace = *(audio_ptr + 1);         // read the FIFO space register

        int ravailable = (fifospace >> 8) & 0xFF; // bits 15:8 = read samples available (right)
        int lavailable = fifospace & 0xFF;         // bits 7:0 = read samples available (left)
        int wspace_r = (fifospace >> 24) & 0xFF;  // bits 31:24 = write space available (right)
        int wspace_l = (fifospace >> 16) & 0xFF;  // bits 23:16 = write space available (left)

        if (lavailable > 0 && ravailable > 0 &&   // check that input samples are available
            wspace_l > 0 && wspace_r > 0) {        // and that output has space

            int left_in = *(audio_ptr + 2);       // read one sample from left input FIFO
            int right_in = *(audio_ptr + 3);      // read one sample from right input FIFO

            int left_delayed = left_buffer[buf_index];   // get the delayed sample from the left buffer
            int right_delayed = right_buffer[buf_index]; // get the delayed sample from the right buffer

            // mix current input with damped delayed output (damping = 0.5, using >> 1)
            int left_out = left_in + (left_delayed >> 1);   // current + half of delayed = echo
            int right_out = right_in + (right_delayed >> 1); // same for right channel

            left_buffer[buf_index] = left_out;    // store the mixed output into the buffer for future echo
            right_buffer[buf_index] = right_out;  // this creates a feedback loop so the echo repeats and decays

            *(audio_ptr + 2) = left_out;          // write the mixed sample to left output FIFO
            *(audio_ptr + 3) = right_out;         // write the mixed sample to right output FIFO

            buf_index++;                          // advance the circular buffer index
            if (buf_index >= DELAY_SAMPLES)       // if we've reached the end of the buffer
                buf_index = 0;                    // wrap around to the beginning
        }
    }

    return 0;                                     // never reached
}
