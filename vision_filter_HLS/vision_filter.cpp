#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include <stdlib.h>

#define WIDTH 320
#define HEIGHT 240

typedef ap_axiu<32, 1, 1, 1> video_pixel;

const int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
const int Gy[3][3] = {{1, 2, 1}, {0, 0, 0}, {-1, -2, -1}};

void vision_filter(hls::stream<video_pixel>& in_stream, hls::stream<video_pixel>& out_stream) {
    #pragma HLS INTERFACE axis port=in_stream
    #pragma HLS INTERFACE axis port=out_stream
    #pragma HLS INTERFACE ap_ctrl_none port=return

    ap_uint<32> line_buf[2][WIDTH];
    #pragma HLS ARRAY_PARTITION variable=line_buf complete dim=1

    ap_uint<32> window[3][3];
    #pragma HLS ARRAY_PARTITION variable=window complete dim=0

    video_pixel pixel_in, pixel_out;

    // Mandatory infinite loop for ap_ctrl_none AXI-Stream hardware
    while(1) {
        for (int row = 0; row < HEIGHT; row++) {
            for (int col = 0; col < WIDTH; col++) {
                #pragma HLS PIPELINE II=1

                in_stream.read(pixel_in);

                // Shift window horizontally
                for (int i = 0; i < 3; i++) {
                    window[i][0] = window[i][1];
                    window[i][1] = window[i][2];
                }

                // Update window vertically
                window[0][2] = line_buf[0][col];
                window[1][2] = line_buf[1][col];
                window[2][2] = pixel_in.data;

                // Update line buffers
                line_buf[0][col] = line_buf[1][col];
                line_buf[1][col] = pixel_in.data;

                // Sobel math
                int val_x = 0, val_y = 0;
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        int pixel_val = (window[i][j] >> 8) & 0xFF; // Green channel
                        val_x += pixel_val * Gx[i][j];
                        val_y += pixel_val * Gy[i][j];
                    }
                }

                int edge_val = abs(val_x) + abs(val_y);
                if (edge_val > 255) edge_val = 255;
                if (row < 2 || col < 2) edge_val = 0; // Clear borders

                // Pack RGBA (White edges on black background)
                pixel_out.data = (0xFF << 24) | (edge_val << 16) | (edge_val << 8) | edge_val;

                // Force byte-validity flags HIGH so DMA doesn't drop pixels
                pixel_out.keep = -1;
                pixel_out.strb = -1;
                pixel_out.user = pixel_in.user;
                pixel_out.id   = pixel_in.id;
                pixel_out.dest = pixel_in.dest;

                // Set TLAST HIGH ONLY on the absolute last pixel of the frame
                if (row == (HEIGHT - 1) && col == (WIDTH - 1)) {
                    pixel_out.last = 1;
                } else {
                    pixel_out.last = 0;
                }

                out_stream.write(pixel_out);
            }
        }
    }
}
