/*
 * mic_amplitude.c
 * ECE 243 Project - Mayan Saravanabavan, Luca Popescu
 *
 * Reads audio samples from the DE1-SoC microphone, finds the peak
 * amplitude over a rolling window of 800 samples (~0.1 s at 8 kHz),
 * and displays the value across all six HEX displays (HEX5..HEX0).
 *
 * Audio samples are 24-bit signed. The absolute peak value is masked
 * to 24 bits and shown in hex: 6 nibbles across 6 displays (HEX5..HEX0).
 */

#define AUDIO_BASE   0xFF203040
#define HEX3_0_BASE  0xFF200020   /* HEX3 … HEX0 (four 7-seg, packed) */
#define HEX5_4_BASE  0xFF200030   /* HEX5 … HEX4 (two 7-seg, packed)  */

#define WINDOW_SIZE  800           /* samples in one measurement window */

/* 7-segment lookup 0–F: segment bit layout 0gfedcba */
static const unsigned char seg7[16] = {
    0x3F, /* 0 */  0x06, /* 1 */  0x5B, /* 2 */  0x4F, /* 3 */
    0x66, /* 4 */  0x6D, /* 5 */  0x7D, /* 6 */  0x07, /* 7 */
    0x7F, /* 8 */  0x6F, /* 9 */  0x77, /* A */  0x7C, /* b */
    0x39, /* C */  0x5E, /* d */  0x79, /* E */  0x71  /* F */
};

/* Absolute value helper (avoid stdlib dependency) */
static int abs_val(int x) {
    return (x < 0) ? -x : x;
}

/*
 * Write a 24-bit value to HEX5..HEX0 in hexadecimal.
 *   nibble[0] = bits 3..0   -> HEX0
 *   nibble[5] = bits 23..20 -> HEX5
 */
static void display_hex(unsigned int value) {
    volatile int *hex3_0 = (int *) HEX3_0_BASE;
    volatile int *hex5_4 = (int *) HEX5_4_BASE;

    /* Mask to 24 bits */
    value &= 0x00FFFFFF;

    unsigned char nibbles[6];
    for (int i = 0; i < 6; i++) {
        nibbles[i] = seg7[value & 0xF];
        value >>= 4;
    }

    /* HEX3-0: pack four 7-seg patterns into one 32-bit word */
    *hex3_0 = (nibbles[3] << 24) | (nibbles[2] << 16) |
              (nibbles[1] << 8)  |  nibbles[0];

    /* HEX5-4: pack two 7-seg patterns into one 32-bit word */
    *hex5_4 = (nibbles[5] << 8) | nibbles[4];
}

int main(void) {
    volatile int *audio_ptr = (int *) AUDIO_BASE;

    int fifospace;
    int left, right;
    int peak = 0;          /* running peak amplitude in current window */
    int sample_count = 0;  /* how many samples collected so far        */

    /* Show 0 on startup */
    display_hex(0);

    while (1) {
        fifospace = *(audio_ptr + 1);

        /* Check RARC: at least 1 sample available? */
        if ((fifospace & 0x000000FF) > 0) {
            left  = *(audio_ptr + 2);
            right = *(audio_ptr + 3);

            /* Codec gives 16-bit data in bits [23:8]; shift to get useful bits */
            int mono = ((left + right) / 2) >> 8;
            int amp  = abs_val(mono);

            if (amp > peak)
                peak = amp;

            sample_count++;

            /* End of measurement window — update display */
            if (sample_count >= WINDOW_SIZE) {
                /* Display raw 24-bit peak amplitude in hex */
                display_hex((unsigned int) peak);
                peak = 0;
                sample_count = 0;
            }
        }
    }

    return 0;   /* never reached */
}
