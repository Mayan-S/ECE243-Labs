/*  ECE 243 Project: Luca Popescu, Mayan Saravanabavan. 
    Description: 2D wave propagation simulator on the DE1-SoC. 
*/

// Memory-mapped I/O base addresses
#define VGA_BASE    0xFF203020
#define KEY_BASE    0xFF200050
#define SWITCH_BASE 0xFF200040
#define AUDIO_BASE  0xFF203040
#define HEX3_0_BASE 0xFF200020
#define HEX5_4_BASE 0xFF200030

// Grid dimensions (half-res: each grid cell = 2×2 VGA pixels)
#define GRID_W  160
#define GRID_H  120

/*  Amplitude scaling:
    Multiplies injected amplitude so the grid stores values in the full
    short int range [-32768, 32767], preventing the wave from dying out
    due to integer truncation in the physics calculations.
*/
#define AMP_SCALE 256

// Microphone scaling
#define MIC_PRACTICAL_MAX 0x400000  // observed loud-as-possible ceiling
#define MIC_THRESHOLD     8         // ignore mic amplitudes below this

// Global variables
volatile int pixel_buffer_start;

short int Buffer1[240][512];
short int Buffer2[240][512];

short int wave_colors[256];

short int current_grid_amplitude[GRID_H][GRID_W]  = {0};
short int previous_grid_amplitude[GRID_H][GRID_W] = {0};
short int next_grid_amplitude[GRID_H][GRID_W]     = {0};

// 7-segment lookup 0–F
static const unsigned char seg7[16] = {
    0x3F,  0x06,  0x5B,  0x4F,
    0x66,  0x6D,  0x7D,  0x07,
    0x7F,  0x6F,  0x77,  0x7C,
    0x39,  0x5E,  0x79,  0x71
};

// Function headers
void plot_pixel(int x, int y, short int colour);
void plot_2X_pixel(int x, int y, short int colour);
void set_screen(void);
void swap_buffers_on_vsync(void);
void initialize_LUT(void);
void wave_physics(short int grid_next[GRID_H][GRID_W],
                  short int grid_curr[GRID_H][GRID_W],
                  short int grid_prev[GRID_H][GRID_W], int damp);
void rendering_vga(short int current[GRID_H][GRID_W]);
void clear_grids(short int grid_curr[GRID_H][GRID_W],
                 short int grid_prev[GRID_H][GRID_W],
                 short int grid_next[GRID_H][GRID_W]);
void inject_source(short int grid_curr[GRID_H][GRID_W],
                   int cy, int cx, short int amplitude);
void display_hex(unsigned int value);
int  read_mic_amplitude(void);

// Helper Function
static int abs_val(int x) {
    return (x < 0) ? -x : x;
}

// Main Function
int main(void) {
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;
    volatile int *key_ptr        = (int *)KEY_BASE;
    volatile int *switch_ptr     = (int *)SWITCH_BASE;

    // VGA double-buffer setup
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
    swap_buffers_on_vsync();
    pixel_buffer_start = *pixel_ctrl_ptr;
    set_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    set_screen();

    // Grid pointers (3 grids: curr, prev, next)
    short int (*grid_curr)[GRID_W] = current_grid_amplitude;
    short int (*grid_prev)[GRID_W] = previous_grid_amplitude;
    short int (*grid_next)[GRID_W] = next_grid_amplitude;

    initialize_LUT();
    clear_grids(grid_curr, grid_prev, grid_next);
    display_hex(0);

    // Main loop
    while (1) {

        // 1. Read switches for damping (0–1023); 256 = no damping (all switches off); ~26 = heavy damping (all switches on)
        int switch_value = *switch_ptr;
        int damping_factor = 256 - ((switch_value * 230) >> 10);

        // 2. Read KEY0 for reset via edge capture register
        int pressed = *(key_ptr + 3);
        *(key_ptr + 3) = 0xF;           /* clear edge capture */

        if (pressed & 0x1) {
            clear_grids(grid_curr, grid_prev, grid_next);
        }

        // 3. Read microphone — drain FIFO and find peak this frame
        int mic_amp = read_mic_amplitude();

        // Show mic amplitude on HEX1–HEX0
        display_hex((unsigned int)mic_amp);

        /* 4.   If loud enough, inject a wave at the center.
                Map mic 0–255 --> amplitude 0 to –128.
                inject_source will multiply by AMP_SCALE internally,
                so center value reaches up to –128 * 256 = –32768. */
        if (mic_amp > MIC_THRESHOLD) {
            short int source_amp = (short int)(-(mic_amp >> 1));
            inject_source(grid_curr, GRID_H / 2, GRID_W / 2, source_amp);
        }

        // 5. Run wave physics, writes next state into grid_next
        wave_physics(grid_next, grid_curr, grid_prev, damping_factor);

        // 6. Render the new state (grid_next) to back buffer
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        rendering_vga(grid_next);

        // 7. Swap VGA front/back buffers
        swap_buffers_on_vsync();

        // 8. Rotate grid pointers: next --> curr --> prev --> (recycled as next)
        short int (*temp)[GRID_W] = grid_prev;
        grid_prev = grid_curr;
        grid_curr = grid_next;
        grid_next = temp;
    }
}

/*  Microphone input
    Drains all available samples from the audio FIFO (non-blocking),
    tracks the peak amplitude, and returns it scaled to 0-255.
    At 60 fps / 8 kHz, there are ~133 samples per frame.
*/

int read_mic_amplitude(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;
    int peak = 0;

    // Drain every available sample from the input FIFO
    while (1) {
        int fifospace = *(audio_ptr + 1);
        if ((fifospace & 0x000000FF) == 0)
            break;   // no more samples available

        int left  = *(audio_ptr + 2);
        int right = *(audio_ptr + 3);

        // Codec data is in bits [23:8]; shift down to get useful range
        int mono = ((left + right) / 2) >> 8;
        int amp  = abs_val(mono);

        if (amp > peak)
            peak = amp;
    }

    // Scale peak to 0–255
    int wave_amp = (peak * 255) / MIC_PRACTICAL_MAX;
    if (wave_amp > 255) wave_amp = 255;

    return wave_amp;
}


// HEX display
void display_hex(unsigned int value) {
    volatile int *hex3_0 = (int *)HEX3_0_BASE;
    volatile int *hex5_4 = (int *)HEX5_4_BASE;

    value &= 0x00FFFFFF;

    unsigned char nibbles[6];
    for (int i = 0; i < 6; i++) {
        nibbles[i] = seg7[value & 0xF];
        value >>= 4;
    }

    *hex3_0 = (nibbles[3] << 24) | (nibbles[2] << 16) |
              (nibbles[1] << 8)  |  nibbles[0];
    *hex5_4 = (nibbles[5] << 8)  |  nibbles[4];
}


// VGA helpers
void swap_buffers_on_vsync(void) {
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;
    *pixel_ctrl_ptr = 1;
    while (*(pixel_ctrl_ptr + 3) & 0x1);
}

void plot_pixel(int x, int y, short int colour) {
    volatile short int *one_pixel_address;
    one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);
    *one_pixel_address = colour;
}

void plot_2X_pixel(int x, int y, short int colour) {
    x = x * 2;
    y = y * 2;
    int byte_address = pixel_buffer_start + (y << 10) + (x << 1);
    volatile short int *pixel_ptr = (short int *)byte_address;

    *pixel_ptr       = colour;
    *(pixel_ptr + 1) = colour;
    *(pixel_ptr + 512) = colour;
    *(pixel_ptr + 513) = colour;
}

void set_screen(void) {
    int y, x;
    for (x = 0; x < 320; x++)
        for (y = 0; y < 240; y++)
            plot_pixel(x, y, 0x551D);
}


// Colour LUT
void initialize_LUT(void) {
    int r, g, b, step;

    for (int i = 0; i < 128; i++) {
        r = 4  + ((i * 6)  >> 7);
        g = 12 + ((i * 28) >> 7);
        b = 11 + ((i * 18) >> 7);
        wave_colors[i] = (r << 11) | (g << 5) | b;
    }

    for (int i = 128; i < 256; i++) {
        step = i - 127;
        r = 10 + ((step * 21) >> 7);
        g = 40 + ((step * 23) >> 7);
        b = 29 + ((step * 2)  >> 7);
        wave_colors[i] = (r << 11) | (g << 5) | b;
    }
}


/*  Wave physics  (3-grid approach)
 
    Writes the next state into grid_next (a dedicated scratch grid)
    so that grid_curr is never partially updated during the same pass.
    This prevents the filled-polygon artifact.
*/

void wave_physics(short int grid_next[GRID_H][GRID_W],
                  short int grid_curr[GRID_H][GRID_W],
                  short int grid_prev[GRID_H][GRID_W], int damp) {
    for (int y = 1; y < GRID_H - 1; y++) {
        for (int x = 1; x < GRID_W - 1; x++) {
            int neighbour_sum = grid_curr[y-1][x] + grid_curr[y+1][x]
                              + grid_curr[y][x-1] + grid_curr[y][x+1];

            int next_amplitude = (neighbour_sum >> 1) - grid_prev[y][x];
            next_amplitude = (next_amplitude * damp) >> 8;

            grid_next[y][x] = (short int)next_amplitude;
        }
    }
}


/*  VGA rendering
    Divides internal amplitude by AMP_SCALE to map back to the
    0-255 colour index range.
*/

void rendering_vga(short int current[GRID_H][GRID_W]) {
    int grid_amplitude;
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            grid_amplitude = current[y][x];

            // Undo AMP_SCALE to get back to color range
            int color_index = (grid_amplitude / AMP_SCALE) + 128;

            if (color_index < 0)   color_index = 0;
            if (color_index > 255) color_index = 255;

            plot_2X_pixel(x, y, wave_colors[color_index]);
        }
    }
}


// Grid helpers
void clear_grids(short int grid_curr[GRID_H][GRID_W],
                 short int grid_prev[GRID_H][GRID_W],
                 short int grid_next[GRID_H][GRID_W]) {
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) {
            grid_curr[y][x] = 0;
            grid_prev[y][x] = 0;
            grid_next[y][x] = 0;
        }
}

/*  Inject a source at (cy, cx).
    Amplitude is multiplied by AMP_SCALE so the grid stores values
    in the full short int range, preventing truncation death.
 
    The amplitude thresholds determine droplet size:
    |amp| >= 100 → 3×3 with diagonals (large)
    |amp| >= 50  → cross pattern (medium)
    else         → single pixel (tiny)
 */
void inject_source(short int grid_curr[GRID_H][GRID_W],
                   int cy, int cx, short int amplitude) {
    if (cy < 1 || cy >= GRID_H - 1 || cx < 1 || cx >= GRID_W - 1) return;

    if (amplitude <= -100 || amplitude >= 100) {
        // Large droplet: center + cross + diagonals
        grid_curr[cy][cx]     = amplitude * AMP_SCALE;

        grid_curr[cy-1][cx]   = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy+1][cx]   = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx-1]   = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx+1]   = (amplitude * AMP_SCALE) >> 1;

        grid_curr[cy-1][cx-1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy-1][cx+1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy+1][cx-1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy+1][cx+1] = (amplitude * AMP_SCALE) >> 2;
    }
    else if (amplitude <= -50 || amplitude >= 50) {
        // Medium droplet: center + cross
        grid_curr[cy][cx]   = amplitude * AMP_SCALE;
        grid_curr[cy-1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy+1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx-1] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx+1] = (amplitude * AMP_SCALE) >> 1;
    }
    else {
        // Tiny droplet: single pixel
        grid_curr[cy][cx] = amplitude * AMP_SCALE;
    }
}