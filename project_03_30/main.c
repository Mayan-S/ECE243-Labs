/*
 * wave_sim.c
 * ECE 243 Project - Mayan Saravanabavan, Luca Popescu
 *
 * 2D wave propagation simulator on the DE1-SoC.
 * Microphone input drives wave amplitude at the center of the grid.
 * PS/2 mouse left-click injects waves at the cursor position.
 * Switches control damping. KEY0 resets the simulation.
 * KEY2 toggles rain mode (random droplets + rain audio).
 * HEX1–HEX0 display the current mic amplitude (0x00–0xFF).
 * VGA double-buffered output at 160×120 grid (2×2 pixel blocks).
 *
 * Uses 3 grids (curr, prev, next) to prevent reading partially
 * updated values during physics, which causes filled-polygon artifacts.
 * AMP_SCALE (256) expands the internal amplitude range so the wave
 * equation doesn't lose energy to integer truncation.
 */

#include <stdlib.h>        /* rand() for rain droplet placement */
#include "drop_sample.h"   /* rain audio samples */

/* ── Memory-mapped I/O base addresses ── */
#define VGA_BASE    0xFF203020
#define KEY_BASE    0xFF200050
#define SWITCH_BASE 0xFF200040
#define AUDIO_BASE  0xFF203040
#define HEX3_0_BASE 0xFF200020
#define HEX5_4_BASE 0xFF200030
#define PS2_BASE    0xFF200100    /* PS/2 port (keyboard/mouse) */

/* ── Grid dimensions (half-res: each grid cell = 2×2 VGA pixels) ── */
#define GRID_W  160
#define GRID_H  120

/* ── Amplitude scaling ── */
#define AMP_SCALE 256

/* ── Microphone scaling ── */
#define MIC_PRACTICAL_MAX 0x400000
#define MIC_THRESHOLD     0x20

/* ── Mouse settings ── */
#define MOUSE_DROP_AMP    (-128)

/* ── Rain audio ── */
#define RAIN_N 17035

/* ── Global variables ── */
volatile int pixel_buffer_start;

/* Mouse cursor position (in grid coordinates: 0–159 x, 0–119 y) */
int mouse_x = GRID_W / 2;
int mouse_y = GRID_H / 2;
int mouse_hold_frames = 0;
int mouse_last_buttons = 0;

/* Rain mode state */
int rain_mode = 0;
int rain_pos  = 0;

short int Buffer1[240][512];
short int Buffer2[240][512];

short int wave_colors[256];

short int current_grid_amplitude[GRID_H][GRID_W]  = {0};
short int previous_grid_amplitude[GRID_H][GRID_W] = {0};
short int next_grid_amplitude[GRID_H][GRID_W]     = {0};

/* ── 7-segment lookup 0–F ── */
static const unsigned char seg7[16] = {
    0x3F, /* 0 */  0x06, /* 1 */  0x5B, /* 2 */  0x4F, /* 3 */
    0x66, /* 4 */  0x6D, /* 5 */  0x7D, /* 6 */  0x07, /* 7 */
    0x7F, /* 8 */  0x6F, /* 9 */  0x77, /* A */  0x7C, /* b */
    0x39, /* C */  0x5E, /* d */  0x79, /* E */  0x71  /* F */
};

/* ── Function prototypes ── */
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
void inject_source_sized(short int grid_curr[GRID_H][GRID_W],
                         int cy, int cx, short int amplitude, int size);
void display_hex(unsigned int value);
int  read_mic_amplitude(void);
void mouse_init(void);
int  mouse_read(int *dx, int *dy, int *buttons);
void output_audio(void);

/* ── Helper ── */
static int abs_val(int x) {
    return (x < 0) ? -x : x;
}


/* ══════════════════════════════════════════════════════════════════ */
int main(void) {
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;
    volatile int *key_ptr        = (int *)KEY_BASE;
    volatile int *switch_ptr     = (int *)SWITCH_BASE;

    /* ── VGA double-buffer setup ── */
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
    swap_buffers_on_vsync();
    pixel_buffer_start = *pixel_ctrl_ptr;
    set_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    set_screen();

    /* ── Grid pointers (3 grids: curr, prev, next) ── */
    short int (*grid_curr)[GRID_W] = current_grid_amplitude;
    short int (*grid_prev)[GRID_W] = previous_grid_amplitude;
    short int (*grid_next)[GRID_W] = next_grid_amplitude;

    initialize_LUT();
    clear_grids(grid_curr, grid_prev, grid_next);
    mouse_init();
    display_hex(0);

    /* ── Main loop ── */
    while (1) {

        /* 0. Top off audio FIFO early so rain sound stays smooth */
        output_audio();

        /* 1. Read switches for damping (0–1023)
         *    256 = no damping (all switches off)
         *    ~26 = heavy damping (all switches on) */
        int switch_value = *switch_ptr;
        int damping_factor = 256 - ((switch_value * 230) >> 10);

        /* 2. Read KEYs via edge capture register */
        int pressed = *(key_ptr + 3);
        *(key_ptr + 3) = 0xF;           /* clear edge capture */

        if (pressed & 0x1) {
            /* KEY0: reset */
            clear_grids(grid_curr, grid_prev, grid_next);
            mouse_hold_frames = 0;
        }
        if (pressed & 0x4) {
            /* KEY2: toggle rain mode */
            rain_mode = !rain_mode;
        }

        /* 3. Read microphone — drain FIFO and find peak this frame */
        int mic_amp = read_mic_amplitude();

        /* Show mic amplitude on HEX1–HEX0 */
        display_hex((unsigned int)mic_amp);

        /* 4. If loud enough, inject a wave at the center. */
        if (mic_amp > MIC_THRESHOLD) {
            short int source_amp = (short int)(-(mic_amp >> 1));
            inject_source(grid_curr, GRID_H / 2, GRID_W / 2, source_amp);
        }

        /* 4b. Read mouse — accumulate movement for this frame */
        {
            int dx, dy, buttons;
            int total_dx = 0, total_dy = 0;
            int got_packet = 0;

            while (mouse_read(&dx, &dy, &buttons)) {
                total_dx += dx;
                total_dy += dy;
                mouse_last_buttons = buttons;
                got_packet = 1;
            }

            if (got_packet) {
                int move_x = (total_dx >= 0) ? ((total_dx + 2) >> 2) : -(((-total_dx) + 2) >> 2);
                int move_y = (total_dy >= 0) ? ((total_dy + 2) >> 2) : -(((-total_dy) + 2) >> 2);

                mouse_x += move_x;
                mouse_y -= move_y;

                if (mouse_x < 3)              mouse_x = 3;
                if (mouse_x >= GRID_W - 3)    mouse_x = GRID_W - 4;
                if (mouse_y < 3)              mouse_y = 3;
                if (mouse_y >= GRID_H - 3)    mouse_y = GRID_H - 4;

                int is_moving = (move_x != 0 || move_y != 0);

                if (mouse_last_buttons & 0x1) {
                    if (is_moving) {
                        mouse_hold_frames = 0;
                        inject_source_sized(grid_curr, mouse_y, mouse_x, MOUSE_DROP_AMP, 0);
                    } else {
                        mouse_hold_frames++;
                        int size = mouse_hold_frames / 10;
                        if (size > 2) size = 2;
                        inject_source_sized(grid_curr, mouse_y, mouse_x, MOUSE_DROP_AMP, size);
                    }
                } else {
                    mouse_hold_frames = 0;
                }
            } else {
                if (mouse_last_buttons & 0x1) {
                    mouse_hold_frames++;
                    int size = mouse_hold_frames / 10;
                    if (size > 2) size = 2;
                    inject_source_sized(grid_curr, mouse_y, mouse_x, MOUSE_DROP_AMP, size);
                } else {
                    mouse_hold_frames = 0;
                }
            }
        }

        /* 4c. Rain mode: scatter random droplets across the grid */
        if (rain_mode) {
            for (int d = 0; d < 20; d++) {
                int rx = (rand() % (GRID_W - 6)) + 3;
                int ry = (rand() % (GRID_H - 6)) + 3;
                int amp = ((rand() % 8) == 0) ? -60 : -32;
                inject_source(grid_curr, ry, rx, amp);
            }
        }

        /* 5. Top off audio again before heavy computation */
        output_audio();

        /* 6. Run wave physics — writes next state into grid_next */
        wave_physics(grid_next, grid_curr, grid_prev, damping_factor);

        /* 7. Top off audio after physics */
        output_audio();

        /* 8. Render the new state (grid_next) to back buffer */
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        rendering_vga(grid_next);

        /* 8b. Draw mouse cursor as a small white crosshair */
        {
            int cx = mouse_x * 2;
            int cy = mouse_y * 2;
            short int cursor_color = 0xFFFF;
            for (int i = -2; i <= 2; i++) {
                int px = cx + i;
                if (px >= 0 && px < 320)
                    plot_pixel(px, cy, cursor_color);
            }
            for (int i = -2; i <= 2; i++) {
                int py = cy + i;
                if (py >= 0 && py < 240)
                    plot_pixel(cx, py, cursor_color);
            }
        }

        /* 9. Final audio top-off before vsync wait */
        output_audio();

        /* 10. Swap VGA front/back buffers */
        swap_buffers_on_vsync();

        /* 11. Rotate grid pointers: next → curr → prev → (recycled as next) */
        short int (*temp)[GRID_W] = grid_prev;
        grid_prev = grid_curr;
        grid_curr = grid_next;
        grid_next = temp;
    }
}


/* ══════════════════════════════════════════════════════════════════
 *  Audio output (rain sound)
 *
 *  Fills the audio output FIFO with rain samples when rain_mode is
 *  on, or silence when it is off.  Called multiple times per frame
 *  to keep the FIFO topped off and prevent audio underruns.
 * ══════════════════════════════════════════════════════════════════ */

void output_audio(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;
    int fifospace = *(audio_ptr + 1);

    int wslc = (fifospace >> 24) & 0xFF;   /* write-space left channel */
    int wsrc = (fifospace >> 16) & 0xFF;   /* write-space right channel */

    while (wslc > 0 && wsrc > 0) {
        if (rain_mode) {
            int sample = drop_sample[rain_pos];
            *(audio_ptr + 2) = sample;
            *(audio_ptr + 3) = sample;

            rain_pos++;
            if (rain_pos >= RAIN_N)
                rain_pos = 0;
        } else {
            *(audio_ptr + 2) = 0;
            *(audio_ptr + 3) = 0;
        }
        wslc--;
        wsrc--;
    }
}


/* ══════════════════════════════════════════════════════════════════
 *  Microphone input
 * ══════════════════════════════════════════════════════════════════ */

int read_mic_amplitude(void) {
    volatile int *audio_ptr = (int *)AUDIO_BASE;
    int peak = 0;

    while (1) {
        int fifospace = *(audio_ptr + 1);
        if ((fifospace & 0x000000FF) == 0)
            break;

        int left  = *(audio_ptr + 2);
        int right = *(audio_ptr + 3);

        int mono = ((left + right) / 2) >> 8;
        int amp  = abs_val(mono);

        if (amp > peak)
            peak = amp;
    }

    int wave_amp = (peak * 255) / MIC_PRACTICAL_MAX;
    if (wave_amp > 255) wave_amp = 255;

    return wave_amp;
}


/* ══════════════════════════════════════════════════════════════════
 *  HEX display
 * ══════════════════════════════════════════════════════════════════ */

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


/* ══════════════════════════════════════════════════════════════════
 *  VGA helpers
 * ══════════════════════════════════════════════════════════════════ */

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


/* ══════════════════════════════════════════════════════════════════
 *  Colour LUT
 * ══════════════════════════════════════════════════════════════════ */

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


/* ══════════════════════════════════════════════════════════════════
 *  Wave physics  (3-grid approach)
 * ══════════════════════════════════════════════════════════════════ */

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


/* ══════════════════════════════════════════════════════════════════
 *  VGA rendering
 * ══════════════════════════════════════════════════════════════════ */

void rendering_vga(short int current[GRID_H][GRID_W]) {
    int grid_amplitude;
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            grid_amplitude = current[y][x];

            int color_index = (grid_amplitude / AMP_SCALE) + 128;

            if (color_index < 0)   color_index = 0;
            if (color_index > 255) color_index = 255;

            plot_2X_pixel(x, y, wave_colors[color_index]);
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 *  Grid helpers
 * ══════════════════════════════════════════════════════════════════ */

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

void inject_source(short int grid_curr[GRID_H][GRID_W],
                   int cy, int cx, short int amplitude) {
    if (cy < 1 || cy >= GRID_H - 1 || cx < 1 || cx >= GRID_W - 1) return;

    if (amplitude <= -100 || amplitude >= 100) {
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
        grid_curr[cy][cx]   = amplitude * AMP_SCALE;
        grid_curr[cy-1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy+1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx-1] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx+1] = (amplitude * AMP_SCALE) >> 1;
    }
    else {
        grid_curr[cy][cx] = amplitude * AMP_SCALE;
    }
}

void inject_source_sized(short int grid_curr[GRID_H][GRID_W],
                         int cy, int cx, short int amplitude, int size) {
    if (cy < 2 || cy >= GRID_H - 2 || cx < 2 || cx >= GRID_W - 2) return;

    grid_curr[cy][cx] = amplitude * AMP_SCALE;

    if (size >= 1) {
        grid_curr[cy-1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy+1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx-1] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx+1] = (amplitude * AMP_SCALE) >> 1;
    }

    if (size >= 2) {
        grid_curr[cy-1][cx-1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy-1][cx+1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy+1][cx-1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy+1][cx+1] = (amplitude * AMP_SCALE) >> 2;
    }
}


/* ══════════════════════════════════════════════════════════════════
 *  PS/2 Mouse
 * ══════════════════════════════════════════════════════════════════ */

static int ps2_read_byte(unsigned char *byte) {
    volatile int *ps2_ptr = (int *)PS2_BASE;
    int data = *ps2_ptr;
    if (data & 0x8000) {
        *byte = data & 0xFF;
        return 1;
    }
    return 0;
}

static void ps2_write_byte(unsigned char byte) {
    volatile int *ps2_ptr = (int *)PS2_BASE;
    *ps2_ptr = byte;
}

void mouse_init(void) {
    unsigned char byte;
    int timeout;

    while (ps2_read_byte(&byte));

    ps2_write_byte(0xFF);

    for (int i = 0; i < 3; i++) {
        timeout = 2000000;
        while (timeout > 0) {
            if (ps2_read_byte(&byte)) break;
            timeout--;
        }
    }

    for (volatile int d = 0; d < 500000; d++);

    while (ps2_read_byte(&byte));

    ps2_write_byte(0xF4);

    timeout = 2000000;
    while (timeout > 0) {
        if (ps2_read_byte(&byte)) break;
        timeout--;
    }

    while (ps2_read_byte(&byte));
}

int mouse_read(int *dx, int *dy, int *buttons) {
    static int state = 0;
    static unsigned char pkt[3];
    static int saw_aa = 0;
    unsigned char byte;

    while (ps2_read_byte(&byte)) {

        if (byte == 0xAA) {
            saw_aa = 1;
            state = 0;
            continue;
        }
        if (saw_aa && byte == 0x00) {
            saw_aa = 0;
            state = 0;
            ps2_write_byte(0xF4);
            for (volatile int d = 0; d < 500000; d++);
            while (ps2_read_byte(&byte));
            continue;
        }
        saw_aa = 0;

        if (state == 0) {
            if ((byte & 0x08) && !(byte & 0xC0)) {
                pkt[0] = byte;
                state = 1;
            }
        }
        else if (state == 1) {
            pkt[1] = byte;
            state = 2;
        }
        else if (state == 2) {
            pkt[2] = byte;
            state = 0;

            *buttons = pkt[0] & 0x07;

            *dx = pkt[1];
            if (pkt[0] & 0x10)
                *dx |= 0xFFFFFF00;

            *dy = pkt[2];
            if (pkt[0] & 0x20)
                *dy |= 0xFFFFFF00;

            return 1;
        }
    }

    return 0;
}
