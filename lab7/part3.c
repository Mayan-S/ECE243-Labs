/* Part 3: Bouncing boxes animation with double buffering
 * Eight small filled boxes move diagonally and bounce off screen edges.
 * Boxes are connected by lines forming a chain.
 * Uses double buffering for flicker-free animation.
 */

#include <stdlib.h>                                     // needed for abs() and rand()

#define PIXEL_BUF_CTRL 0xFF203020                       // pixel buffer controller base address
#define SCREEN_X 320                                    // screen width in pixels
#define SCREEN_Y 240                                    // screen height in pixels
#define NUM_BOXES 8                                     // number of bouncing boxes
#define BOX_SIZE 5                                      // width and height of each box in pixels

volatile int pixel_buffer_start;                        // global variable: base address of current draw buffer
short int Buffer1[240][512];                            // front frame buffer (240 rows, 512 columns with padding)
short int Buffer2[240][512];                            // back frame buffer

// arrays to store box positions, directions, and colors
int box_x[NUM_BOXES];                                   // x position of each box
int box_y[NUM_BOXES];                                   // y position of each box
int box_dx[NUM_BOXES];                                  // x direction of each box (+1 or -1)
int box_dy[NUM_BOXES];                                  // y direction of each box (+1 or -1)
short int box_color[NUM_BOXES];                         // color of each box

// previous positions for erasing (need two frames back for double buffering)
int prev_x[NUM_BOXES][2];                               // previous x positions for both buffers
int prev_y[NUM_BOXES][2];                               // previous y positions for both buffers
int buf_idx = 0;                                        // tracks which buffer we are drawing to (0 or 1)

void plot_pixel(int x, int y, short int line_color);    // forward declaration
void clear_screen(void);                                // forward declaration
void draw_line(int x0, int y0, int x1, int y1, short int color); // forward declaration
void swap(int *a, int *b);                              // forward declaration
void wait_for_vsync(void);                              // forward declaration
void draw_box(int x, int y, short int color);           // forward declaration

int main(void) {
    volatile int *pixel_ctrl_ptr = (int *)PIXEL_BUF_CTRL; // pointer to pixel buffer controller

    // initialize boxes with random positions, directions, and colors
    for (int i = 0; i < NUM_BOXES; i++) {               // loop through all 8 boxes
        box_x[i] = (rand() % (SCREEN_X - BOX_SIZE));   // random x position within screen bounds
        box_y[i] = (rand() % (SCREEN_Y - BOX_SIZE));   // random y position within screen bounds
        box_dx[i] = ((rand() % 2) * 2) - 1;            // random direction: -1 or +1
        box_dy[i] = ((rand() % 2) * 2) - 1;            // random direction: -1 or +1
        // generate a random bright color in RGB565 format
        box_color[i] = ((rand() % 32) << 11) | ((rand() % 64) << 5) | (rand() % 32);
        if (box_color[i] == 0) box_color[i] = 0xFFFF;  // avoid black color (would be invisible)
        prev_x[i][0] = box_x[i];                       // initialize previous position buffer 0
        prev_y[i][0] = box_y[i];                       // initialize previous position buffer 0
        prev_x[i][1] = box_x[i];                       // initialize previous position buffer 1
        prev_y[i][1] = box_y[i];                       // initialize previous position buffer 1
    }

    /* set front pixel buffer to Buffer 1 */
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;             // write Buffer1 address to backbuffer register
    wait_for_vsync();                                  // swap to make Buffer1 the front buffer
    pixel_buffer_start = *pixel_ctrl_ptr;              // read front buffer address
    clear_screen();                                    // clear the front buffer (Buffer1)

    /* set back pixel buffer to Buffer 2 */
    *(pixel_ctrl_ptr + 1) = (int)&Buffer2;             // write Buffer2 address to backbuffer register
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);        // point to back buffer for drawing
    clear_screen();                                    // clear the back buffer (Buffer2)

    while (1) {                                        // infinite animation loop
        // erase previous boxes and lines from this buffer
        for (int i = 0; i < NUM_BOXES; i++) {          // loop through all boxes
            draw_box(prev_x[i][buf_idx], prev_y[i][buf_idx], 0x0000); // erase old box with black
        }
        for (int i = 0; i < NUM_BOXES - 1; i++) {     // loop through adjacent box pairs
            draw_line(prev_x[i][buf_idx] + BOX_SIZE / 2,  // erase old line from center of box i
                      prev_y[i][buf_idx] + BOX_SIZE / 2,
                      prev_x[i + 1][buf_idx] + BOX_SIZE / 2, // to center of box i+1
                      prev_y[i + 1][buf_idx] + BOX_SIZE / 2,
                      0x0000);                         // draw in black to erase
        }

        // draw boxes at current positions
        for (int i = 0; i < NUM_BOXES; i++) {          // loop through all boxes
            draw_box(box_x[i], box_y[i], box_color[i]); // draw filled box in its color
        }

        // draw lines connecting adjacent boxes (chain)
        for (int i = 0; i < NUM_BOXES - 1; i++) {     // loop through adjacent pairs
            draw_line(box_x[i] + BOX_SIZE / 2,        // from center of box i
                      box_y[i] + BOX_SIZE / 2,
                      box_x[i + 1] + BOX_SIZE / 2,    // to center of box i+1
                      box_y[i + 1] + BOX_SIZE / 2,
                      0xFFFF);                         // draw in white
        }

        // save current positions for erasing in this buffer later
        for (int i = 0; i < NUM_BOXES; i++) {          // loop through all boxes
            prev_x[i][buf_idx] = box_x[i];            // remember current x for this buffer
            prev_y[i][buf_idx] = box_y[i];            // remember current y for this buffer
        }

        // update box positions and bounce off edges
        for (int i = 0; i < NUM_BOXES; i++) {          // loop through all boxes
            box_x[i] += box_dx[i];                    // move x by direction (+1 or -1)
            box_y[i] += box_dy[i];                    // move y by direction (+1 or -1)

            if (box_x[i] <= 0)                        // if box hit the left edge
                box_dx[i] = 1;                        // reverse to move right
            if (box_x[i] >= SCREEN_X - BOX_SIZE)     // if box hit the right edge
                box_dx[i] = -1;                       // reverse to move left
            if (box_y[i] <= 0)                        // if box hit the top edge
                box_dy[i] = 1;                        // reverse to move down
            if (box_y[i] >= SCREEN_Y - BOX_SIZE)     // if box hit the bottom edge
                box_dy[i] = -1;                       // reverse to move up
        }

        wait_for_vsync();                             // swap front and back buffers on VGA vsync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);   // update draw pointer to the new back buffer
        buf_idx = 1 - buf_idx;                        // toggle buffer index (0->1 or 1->0)
    }

    return 0;                                         // never reached
}

/* wait_for_vsync: synchronizes with VGA controller */
void wait_for_vsync(void) {
    volatile int *pixel_ctrl_ptr = (int *)PIXEL_BUF_CTRL; // pointer to pixel buffer controller
    *pixel_ctrl_ptr = 1;                               // write 1 to request buffer swap

    volatile int *status = (int *)(PIXEL_BUF_CTRL + 0xC); // pointer to status register at 0xFF20302C
    while ((*status & 0x01) != 0)                      // poll S bit until swap is complete
        ;                                              // wait
}

/* clear_screen: sets every pixel to black */
void clear_screen(void) {
    for (int y = 0; y < SCREEN_Y; y++)                  // loop through all rows
        for (int x = 0; x < SCREEN_X; x++)              // loop through all columns
            plot_pixel(x, y, 0x0000);                   // set pixel to black
}

/* draw_box: draws a filled square of BOX_SIZE x BOX_SIZE pixels */
void draw_box(int x, int y, short int color) {
    for (int dy = 0; dy < BOX_SIZE; dy++) {             // loop through box rows
        for (int dx = 0; dx < BOX_SIZE; dx++) {         // loop through box columns
            int px = x + dx;                           // compute pixel x coordinate
            int py = y + dy;                           // compute pixel y coordinate
            if (px >= 0 && px < SCREEN_X && py >= 0 && py < SCREEN_Y) // bounds check
                plot_pixel(px, py, color);             // draw pixel if within screen
        }
    }
}

/* draw_line: Bresenham's line-drawing algorithm */
void draw_line(int x0, int y0, int x1, int y1, short int color) {
    int is_steep = abs(y1 - y0) > abs(x1 - x0);        // check if steep

    if (is_steep) {                                     // if steep, swap x and y
        swap(&x0, &y0);                                // swap x0 with y0
        swap(&x1, &y1);                                // swap x1 with y1
    }
    if (x0 > x1) {                                     // if going right to left
        swap(&x0, &x1);                                // swap x0 with x1
        swap(&y0, &y1);                                // swap y0 with y1
    }

    int deltax = x1 - x0;                              // horizontal distance
    int deltay = abs(y1 - y0);                          // vertical distance
    int error = -(deltax / 2);                          // initialize error
    int y = y0;                                         // start y
    int y_step;                                         // y direction

    if (y0 < y1)                                       // if going down
        y_step = 1;                                    // step positive
    else                                               // if going up
        y_step = -1;                                   // step negative

    for (int x = x0; x <= x1; x++) {                    // loop x0 to x1
        if (is_steep)                                  // if steep
            plot_pixel(y, x, color);                   // plot swapped
        else                                           // if not steep
            plot_pixel(x, y, color);                   // plot normal

        error = error + deltay;                        // accumulate error
        if (error > 0) {                               // threshold exceeded
            y = y + y_step;                            // adjust y
            error = error - deltax;                    // reduce error
        }
    }
}

/* swap: exchanges two integer values */
void swap(int *a, int *b) {
    int temp = *a;                                     // store first
    *a = *b;                                           // overwrite first
    *b = temp;                                         // overwrite second
}

/* plot_pixel: writes a 16-bit color to pixel at (x, y) */
void plot_pixel(int x, int y, short int line_color) {
    volatile short int *one_pixel_address;              // pointer to pixel
    one_pixel_address = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)); // compute address
    *one_pixel_address = line_color;                   // write color
}
