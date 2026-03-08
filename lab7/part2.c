/* A horizontal line moves up and down the screen */

#include <stdlib.h>                                     // needed for abs()

#define PIXEL_BUF_CTRL 0xFF203020                       // pixel buffer controller base address
#define SCREEN_X 320                                    // screen width in pixels
#define SCREEN_Y 240                                    // screen height in pixels

int pixel_buffer_start;                                 // global variable: base address of pixel buffer

void plot_pixel(int x, int y, short int line_color);    // forward declaration
void clear_screen(void);                                // forward declaration
void draw_line(int x0, int y0, int x1, int y1, short int color); // forward declaration
void swap(int *a, int *b);                              // forward declaration
void wait_for_vsync(void);                              // forward declaration

int main(void) {
    volatile int *pixel_ctrl_ptr = (int *)PIXEL_BUF_CTRL; // pointer to pixel buffer controller
    pixel_buffer_start = *pixel_ctrl_ptr;               // read the front buffer address

    clear_screen();                                     // fill entire screen with black

    int y_pos = SCREEN_Y / 2;                           // start the line at the middle row (120)
    int y_dir = 1;                                      // initial direction: moving downward (+1)

    while (1) {                                         // infinite animation loop
        // erase the line at the current position by drawing it in black
        draw_line(0, y_pos, SCREEN_X - 1, y_pos, 0x0000); // draw black line across full width

        y_pos += y_dir;                                // move the line by one row in the current direction

        if (y_pos >= SCREEN_Y - 1)                     // if the line has reached the bottom edge
            y_dir = -1;                                // reverse direction to move upward
        if (y_pos <= 0)                                // if the line has reached the top edge
            y_dir = 1;                                 // reverse direction to move downward

        // draw the line at the new position in white
        draw_line(0, y_pos, SCREEN_X - 1, y_pos, 0xFFFF); // draw white line across full width

        wait_for_vsync();                              // synchronize with VGA vertical sync (1/60 sec)
    }

    return 0;                                          // never reached
}

/* wait_for_vsync: synchronizes with VGA controller by requesting a buffer swap */
void wait_for_vsync(void) {
    volatile int *pixel_ctrl_ptr = (int *)PIXEL_BUF_CTRL; // pointer to pixel buffer controller
    *pixel_ctrl_ptr = 1;                               // write 1 to Buffer register to request swap (sets S=1)

    volatile int *status = (int *)(PIXEL_BUF_CTRL + 0xC); // pointer to status register at 0xFF20302C
    while ((*status & 0x01) != 0)                      // poll the S bit (bit 0) until it becomes 0
        ;                                              // empty loop body — just wait
}

/* clear_screen: sets every pixel on the screen to black */
void clear_screen(void) {
    for (int y = 0; y < SCREEN_Y; y++)                  // loop through all 240 rows
        for (int x = 0; x < SCREEN_X; x++)              // loop through all 320 columns
            plot_pixel(x, y, 0x0000);                   // set pixel to black
}

/* draw_line: Bresenham's line-drawing algorithm */
void draw_line(int x0, int y0, int x1, int y1, short int color) {
    int is_steep = abs(y1 - y0) > abs(x1 - x0);        // check if line is steeper than 45 degrees

    if (is_steep) {                                     // if steep, swap x and y
        swap(&x0, &y0);                                // swap x0 with y0
        swap(&x1, &y1);                                // swap x1 with y1
    }
    if (x0 > x1) {                                     // if line goes right to left
        swap(&x0, &x1);                                // swap x0 with x1
        swap(&y0, &y1);                                // swap y0 with y1
    }

    int deltax = x1 - x0;                              // horizontal distance
    int deltay = abs(y1 - y0);                          // vertical distance (absolute)
    int error = -(deltax / 2);                          // initialize error term
    int y = y0;                                         // start y at first endpoint
    int y_step;                                         // y direction

    if (y0 < y1)                                       // if going downward
        y_step = 1;                                    // step positively
    else                                               // if going upward
        y_step = -1;                                   // step negatively

    for (int x = x0; x <= x1; x++) {                    // loop from x0 to x1
        if (is_steep)                                  // if steep line
            plot_pixel(y, x, color);                   // plot with swapped coordinates
        else                                           // if not steep
            plot_pixel(x, y, color);                   // plot normally

        error = error + deltay;                        // accumulate error
        if (error > 0) {                               // if threshold exceeded
            y = y + y_step;                            // adjust y
            error = error - deltax;                    // reduce error
        }
    }
}

/* swap: exchanges two integer values */
void swap(int *a, int *b) {
    int temp = *a;                                     // store first value
    *a = *b;                                           // overwrite first with second
    *b = temp;                                         // overwrite second with stored value
}

/* plot_pixel: writes a 16-bit color to pixel at (x, y) */
void plot_pixel(int x, int y, short int line_color) {
    volatile short int *one_pixel_address;              // pointer to pixel
    one_pixel_address = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)); // compute address
    *one_pixel_address = line_color;                   // write color
}
