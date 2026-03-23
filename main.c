#define AUDIO_BASE   0xFF203040
#define HEX3_0_BASE  0xFF200020
#define HEX5_4_BASE  0xFF200030
#define DECAY_PERIOD 4000

int seg7_table[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
};

int main(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;
    volatile int *hex3_0    = (int *)HEX3_0_BASE;
    volatile int *hex5_4    = (int *)HEX5_4_BASE;

    int left, right, fifospace;
    int amplitude, peak = 0, decay_counter = 0;

    /* flush stale samples */
    while (1) {
        fifospace = *(audio_ptr + 1);
        if ((fifospace & 0xFF) == 0) break;
        left  = *(audio_ptr + 2);
        right = *(audio_ptr + 3);
    }

    while (1) {
        fifospace = *(audio_ptr + 1);
        if ((fifospace & 0xFF) == 0) continue;

        left  = *(audio_ptr + 2);
        right = *(audio_ptr + 3);

        /* abs of each channel */
        if (left < 0)  left  = -left;
        if (right < 0) right = -right;

        /* max of the two */
        amplitude = (left > right) ? left : right;

        /* update peak with slow decay */
        if (amplitude >= peak) {
            peak = amplitude;
            decay_counter = 0;
        } else {
            decay_counter++;
            if (decay_counter >= DECAY_PERIOD) {
                decay_counter = 0;
                peak -= (peak >> 3) + 1;
                if (peak < 0) peak = 0;
            }
        }

        /* display peak on HEX5..HEX0 */
        *hex3_0 = seg7_table[peak & 0xF]
                | (seg7_table[(peak >> 4)  & 0xF] << 8)
                | (seg7_table[(peak >> 8)  & 0xF] << 16)
                | (seg7_table[(peak >> 12) & 0xF] << 24);

        *hex5_4 = seg7_table[(peak >> 16) & 0xF]
                | (seg7_table[(peak >> 20) & 0xF] << 8);
    }
}