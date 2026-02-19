/* Part 2: Audio pass-through
 * Reads audio samples from the input (microphone) FIFO
 * and writes them directly to the output (speaker) FIFO.
 * On CPUlator, the input produces a sawtooth wave.
 */

#define AUDIO_BASE 0xFF203040                     // audio port base address

int main(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;  // pointer to audio port base address

    *(audio_ptr) = 0xC;                           // clear both input and output FIFOs (bits 3:2 = CO|CI)
    *(audio_ptr) = 0x0;                           // release the clear by writing 0 to control register

    while (1) {                                   // infinite loop for continuous operation
        int fifospace = *(audio_ptr + 1);         // read the FIFO space register (offset +1 word = +4 bytes)

        int ravilable = (fifospace >> 8) & 0xFF;  // bits 15:8 = number of samples available to read (right)
        int lavilable = fifospace & 0xFF;          // bits 7:0 = number of samples available to read (left)
        int wspace_r = (fifospace >> 24) & 0xFF;  // bits 31:24 = write space available (right)
        int wspace_l = (fifospace >> 16) & 0xFF;  // bits 23:16 = write space available (left)

        if (ravilable > 0 && lavilable > 0 &&     // check that there is data to read
            wspace_r > 0 && wspace_l > 0) {        // and space to write

            int left_sample = *(audio_ptr + 2);   // read one sample from the left input FIFO (offset +2 words)
            int right_sample = *(audio_ptr + 3);  // read one sample from the right input FIFO (offset +3 words)

            *(audio_ptr + 2) = left_sample;       // write the sample to the left output FIFO
            *(audio_ptr + 3) = right_sample;      // write the sample to the right output FIFO
        }
    }

    return 0;                                     // never reached
}
