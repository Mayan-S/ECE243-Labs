#include "drop_sample.h"

// Global variables
volatile int pixel_buffer_start;
short int Buffer1[240][512];
short int Buffer2[240][512];
short int wave_colors[256];  
short int current_grid_amplitude[GRID_H][GRID_W] = {0};
short int previous_grid_amplitude[GRID_H][GRID_W] = {0};
short int next_grid_amplitude[GRID_H][GRID_W] = {0};


// State globals
bool rain_mode = false;
int rain_pos = 0;


// Function prototypes
void plot_pixel(int x, int y, short int colour);
void plot_2X_pixel(int x, int y, short int colour);
void set_screen();
void swap_buffers_on_vsync ();
void initialize_LUT();
void wave_physics(short int grid_next[GRID_H][GRID_W], short int grid_curr[GRID_H][GRID_W], short int grid_prev[GRID_H][GRID_W], int damp);
void rendering_vga(short int current[GRID_H][GRID_W]);
void clear_grids(short int grid_curr[GRID_H][GRID_W], short int grid_prev[GRID_H][GRID_W], short int grid_next[GRID_H][GRID_W]);
void inject_source(short int grid_curr[GRID_H][GRID_W], int cy, int cx, short int amplitude);
void output_audio();


int main(void){
    volatile int * pixel_ctrl_ptr = (int *)VGA_BASE;
    volatile int * key_ptr = (int *)KEY_BASE;
    volatile int * switch_ptr = (int *)SWITCH_BASE;
   
    // Initialize double buffering
    *(pixel_ctrl_ptr+1) = (int) &Buffer1;
    swap_buffers_on_vsync();
    pixel_buffer_start = *pixel_ctrl_ptr;
    set_screen();
    *(pixel_ctrl_ptr + 1) = (int) &Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    set_screen();


    // Grid pointers
    short int (*grid_curr)[GRID_W] = current_grid_amplitude;
    short int (*grid_prev)[GRID_W] = previous_grid_amplitude;
    short int (*grid_next)[GRID_W] = next_grid_amplitude;


    initialize_LUT();
    clear_grids(grid_curr, grid_prev, grid_next);
   
    while (1){
        // 1. Top off audio
        output_audio();


        int switch_value = *switch_ptr;
        int damping_factor = 256 - ((switch_value * 230) >> 10);


        int pressed = *(key_ptr + 3);  
        *(key_ptr + 3) = 0xf; // Clear edge capture
       
        if (pressed & 0x1) {        
            clear_grids(grid_curr, grid_prev, grid_next);
        }
        else if (pressed & 0x2) {    
            inject_source(grid_curr, GRID_H/2, GRID_W/2, -128);
        }
        else if (pressed & 0x4) {    
            rain_mode = !rain_mode;
        }


        if (rain_mode) {
            for (int d = 0; d < 20; d++) {
                int rx = (rand() % (GRID_W - 6)) + 3;
                int ry = (rand() % (GRID_H - 6)) + 3;
                int amp = ((rand() % 8) == 0) ? -60 : -32;
                inject_source(grid_curr, ry, rx, amp);
            }
        }


        // 2. Top off audio
        output_audio();


        wave_physics(grid_next, grid_curr, grid_prev, damping_factor);


        // 3. Top off audio
        output_audio();


        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        rendering_vga(grid_next);


        // 4. Final top off before vsync wait
        output_audio();


        swap_buffers_on_vsync();


        short int (*temp)[GRID_W] = grid_prev;  
        grid_prev = grid_curr;                  
        grid_curr = grid_next;                  
        grid_next = temp;
    }
}


// Corrected audio output logic
void output_audio() {
    volatile int * audio_ptr = (int *) AUDIO_BASE;
    int fifospace = *(audio_ptr + 1);
   
    // Corrected bits and safe masking
    int wslc = (fifospace >> 24) & 0xFF; // Bits 31-24
    int wsrc = (fifospace >> 16) & 0xFF; // Bits 23-16
   
    while (wslc > 0 && wsrc > 0) {
        if (rain_mode) {
            // Play rain sound
            int sample = drop_sample[rain_pos];
            *(audio_ptr + 2) = sample;
            *(audio_ptr + 3) = sample;
           
            rain_pos++;
            if (rain_pos >= RAIN_N) {
                rain_pos = 0;
            }
        } else {
            // Write absolute silence when rain mode is off to prevent buzzing
            *(audio_ptr + 2) = 0;
            *(audio_ptr + 3) = 0;
        }
       
        wslc--;
        wsrc--;
    }
}


void swap_buffers_on_vsync () {
    volatile int * pixel_ctrl_ptr = (int *) VGA_BASE;
    *pixel_ctrl_ptr = 1;  
    while (*(pixel_ctrl_ptr + 3) & 0x1);  
}


void plot_pixel(int x, int y, short int colour){
    volatile short int *one_pixel_address;
    one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);
    *one_pixel_address = colour;
}


void plot_2X_pixel(int x, int y, short int colour) {
    x = x * 2;
    y = y * 2;
   
    int byte_address = pixel_buffer_start + (y << 10) + (x << 1);
    volatile short int *pixel_ptr = (short int *) byte_address;
   
    *pixel_ptr = colour;                
    *(pixel_ptr + 1) = colour;          
    *(pixel_ptr + 512) = colour;        
    *(pixel_ptr + 513) = colour;        
}


void set_screen(){
    int y, x;
    for (x = 0; x < 320; x++)
        for (y = 0; y < 240; y++)
            plot_pixel (x, y, 0x551D);
}


void initialize_LUT(){
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


void wave_physics(short int grid_next[GRID_H][GRID_W], short int grid_curr[GRID_H][GRID_W], short int grid_prev[GRID_H][GRID_W], int damp){
    for(int y = 1; y<GRID_H-1; y++){
        for(int x = 1; x<GRID_W-1 ; x++){
            int neighbour_sum = grid_curr[y-1][x] + grid_curr[y+1][x] + grid_curr[y][x-1] + grid_curr[y][x+1];
            int next_amplitude;


            next_amplitude = (neighbour_sum >> 1) - grid_prev[y][x];
            next_amplitude = (next_amplitude * damp) >> 8;


            grid_next[y][x] = (short int)next_amplitude;
         }
    }
}


void rendering_vga(short int current[120][160]){
    int grid_amplitude;
    for(int y = 0; y < 120; y++){
        for(int x = 0; x < 160; x++){
            grid_amplitude = current[y][x];
            int color_index = (grid_amplitude / AMP_SCALE) + 128;
           
            if (color_index < 0) color_index = 0;
            else if (color_index > 255) color_index = 255;


            plot_2X_pixel(x, y, wave_colors[color_index]);
        }
    }
}


void clear_grids(short int grid_curr[GRID_H][GRID_W], short int grid_prev[GRID_H][GRID_W], short int grid_next[GRID_H][GRID_W]) {
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) {
            grid_curr[y][x] = 0;
            grid_prev[y][x] = 0;
            grid_next[y][x] = 0;
        }
}


void inject_source(short int grid_curr[GRID_H][GRID_W], int cy, int cx, short int amplitude) {
    if (cy < 1 || cy >= GRID_H - 1 || cx < 1 || cx >= GRID_W - 1) return;
       
    if (amplitude <= -100 || amplitude >= 100) {
        grid_curr[cy][cx] = amplitude * AMP_SCALE;                          
        grid_curr[cy-1][cx] = (amplitude * AMP_SCALE) >> 1;                  
        grid_curr[cy+1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx-1] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx+1] = (amplitude * AMP_SCALE) >> 1;
       
        grid_curr[cy-1][cx-1] = (amplitude * AMP_SCALE) >> 2;            
        grid_curr[cy-1][cx+1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy+1][cx-1] = (amplitude * AMP_SCALE) >> 2;
        grid_curr[cy+1][cx+1] = (amplitude * AMP_SCALE) >> 2;
    }
    else if (amplitude <= -50 || amplitude >= 50) {
        grid_curr[cy][cx] = amplitude * AMP_SCALE;                          
        grid_curr[cy-1][cx] = (amplitude * AMP_SCALE) >> 1;                  
        grid_curr[cy+1][cx] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx-1] = (amplitude * AMP_SCALE) >> 1;
        grid_curr[cy][cx+1] = (amplitude * AMP_SCALE) >> 1;
    }
    else {
        grid_curr[cy][cx] = amplitude * AMP_SCALE;                            
    }
}

