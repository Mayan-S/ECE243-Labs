/* Part 1: Bresenham's line-drawing algorithm
 * Draws four colored lines on the VGA display:
 * blue diagonal, green diagonal, red horizontal, pink diagonal
 */

#include <stdlib.h>                                     // needed for abs()

#define PIXEL_BUF_CTRL 0xFF203020                       // pixel buffer controller base address
#define SCREEN_X 320                                    // screen width in pixels
#define SCREEN_Y 240                                    // screen height in pixels

int pixel_buffer_start;                                 // global variable: base address of pixel buffer

void plot_pixel(int x, int y, short int line_color);    // forward declaration of plot_pixel
void clear_screen(void);                                // forward declaration of clear_screen
void draw_line(int x0, int y0, int x1, int y1, short int color); // forward declaration of draw_line
void swap(int *a, int *b);                              // forward declaration of swap helper

int main(void) {
    volatile int *pixel_ctrl_ptr = (int *)PIXEL_BUF_CTRL; // pointer to pixel buffer controller
    pixel_buffer_start = *pixel_ctrl_ptr;               // read the front buffer address from the controller

    clear_screen();                                     // fill the entire screen with black pixels
    draw_line(0, 0, 150, 150, 0x001F);                 // draw a blue line from top-left to middle
    draw_line(150, 150, 319, 0, 0x07E0);               // draw a green line from middle to top-right
    draw_line(0, 239, 319, 239, 0xF800);               // draw a red horizontal line along the bottom
    draw_line(319, 0, 0, 239, 0xF81F);                 // draw a pink line from top-right to bottom-left

    return 0;                                           // program done
}

/* clear_screen: sets every pixel on the screen to black (0x0000) */
void clear_screen(void) {
    for (int y = 0; y < SCREEN_Y; y++)                  // loop through all 240 rows
        for (int x = 0; x < SCREEN_X; x++)              // loop through all 320 columns
            plot_pixel(x, y, 0x0000);                   // set pixel to black
}

/* draw_line: Bresenham's line-drawing algorithm
 * Draws a line from (x0,y0) to (x1,y1) in the given color */
void draw_line(int x0, int y0, int x1, int y1, short int color) {
    int is_steep = abs(y1 - y0) > abs(x1 - x0);        // check if line is steeper than 45 degrees

    if (is_steep) {                                     // if steep, swap x and y coordinates
        swap(&x0, &y0);                                // swap x0 with y0
        swap(&x1, &y1);                                // swap x1 with y1
    }
    if (x0 > x1) {                                     // if line goes right to left, swap endpoints
        swap(&x0, &x1);                                // swap x0 with x1
        swap(&y0, &y1);                                // swap y0 with y1
    }

    int deltax = x1 - x0;                              // horizontal distance of the line
    int deltay = abs(y1 - y0);                          // vertical distance of the line (absolute)
    int error = -(deltax / 2);                          // initialize error term to negative half of deltax
    int y = y0;                                         // start y at the first endpoint
    int y_step;                                         // direction to step y (+1 or -1)

    if (y0 < y1)                                       // if line goes downward
        y_step = 1;                                    // step y in the positive direction
    else                                               // if line goes upward
        y_step = -1;                                   // step y in the negative direction

    for (int x = x0; x <= x1; x++) {                    // loop from x0 to x1, one pixel at a time
        if (is_steep)                                  // if we swapped x/y earlier
            plot_pixel(y, x, color);                   // draw with coordinates un-swapped
        else                                           // if not steep
            plot_pixel(x, y, color);                   // draw normally

        error = error + deltay;                        // accumulate error by the vertical distance
        if (error > 0) {                               // if error exceeds threshold
            y = y + y_step;                            // move y one step closer to the endpoint
            error = error - deltax;                    // reduce error by the horizontal distance
        }
    }
}

/* swap: exchanges the values of two integers */
void swap(int *a, int *b) {
    int temp = *a;                                     // store first value temporarily
    *a = *b;                                           // overwrite first with second
    *b = temp;                                         // overwrite second with stored value
}

/* plot_pixel: writes a 16-bit color to the pixel at (x, y) in the frame buffer */
void plot_pixel(int x, int y, short int line_color) {
    volatile short int *one_pixel_address;              // pointer to a single pixel in the frame buffer
    one_pixel_address = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)); // compute address: base + y*1024 + x*2
    *one_pixel_address = line_color;                   // write the color value to that pixel
}
