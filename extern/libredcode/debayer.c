#include "debayer.h"

/* pretty simple but astonishingly very effective "debayer" function 
 */

void redcode_ycbcr2rgb_fullscale(
	int ** planes, int width, int height, float * out)
{
	int x,y;
	int pix_max = 4096;
	int mask = pix_max - 1;
	float *o;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int i = x + y*width;
			int i_p = (y > 0) ? i-width : i;
			int i_n = (y < (height-1)) ? i + width : i;
			float y1n = planes[0][i_n] & mask;
			float y1  = planes[0][i] & mask;
			float cb  = (planes[1][i] & mask)   - pix_max/2;
			float cr  = (planes[2][i] & mask)   - pix_max/2;
			float y2  = (planes[3][i] & mask);
			float y2p = (planes[3][i_p] & mask);

			float b_ = cb /(pix_max/2);
			float r_ = cr /(pix_max/2);
			float g_ = 0.0;
		
			float y_[4] = {y1 / pix_max, 
				       (y2 + y2p)/2 / pix_max, 
				       (y1 + y1n)/2 / pix_max, 
				       y2 / pix_max};

			int j;
			int yc = 0;

			o = out + (2*height-1-2*y)*2*4*width 
				+ x*2*4;

			for (j = 0; j < 8; j += 4) {
				o[j+0] = r_ + y_[yc];
				o[j+1] = g_ + y_[yc];
				o[j+2] = b_ + y_[yc];
				o[j+3] = 1.0;
				yc++;
			}
			
			o = out + (2*height-1-2*y)*2*4*width 
				+ x*2*4 - 2*4*width;

			for (j = 0; j < 8; j += 4) {
				o[j+0] = r_ + y_[yc];
				o[j+1] = g_ + y_[yc];
				o[j+2] = b_ + y_[yc];
				o[j+3] = 1.0;
				yc++;
			}
		}
	}
}

void redcode_ycbcr2rgb_halfscale(
	int ** planes, int width, int height, float * out)
{
	int x,y;
	int pix_max = 4096;
	int mask = pix_max - 1;

	for (y = 0; y < height; y++) {
		float *o = out + width * (height - y - 1);
		for (x = 0; x < width; x++) {
			int i = y*height + x;
			float y1  = (planes[0][i] & mask);
			float cb  = (planes[1][i] & mask)  - pix_max/2;
			float cr  = (planes[2][i] & mask)  - pix_max/2;
			float y2  = (planes[3][i] & mask);

			float b_ = cb /(pix_max/2);
			float r_ = cr /(pix_max/2);
			float g_ = 0.0;
			
			float y = (y1 + y2)/2 / pix_max;

			*o++ = r_ + y;
			*o++ = g_ + y;
			*o++ = b_ + y;
			*o++ = 1.0;
		}
	}
}


void redcode_ycbcr2rgb_quarterscale(
	int ** planes, int width, int height, float * out)
{
	int x,y;
	int pix_max = 4096;
	int mask = pix_max - 1;

	for (y = 0; y < height; y += 2) {
		float *o = out + (width/2) * (height/2 - y/2 - 1);
		for (x = 0; x < width; x += 2) {
			int i = y * width + x;
			float y1  = planes[0][i] & mask;
			float cb  = (planes[1][i] & mask)  - pix_max/2;
			float cr  = (planes[2][i] & mask)  - pix_max/2;
			float y2  = planes[3][i] & mask;

			float b_ = cb /(pix_max/2);
			float r_ = cr /(pix_max/2);
			float g_ = 0.0;
			
			float y = (y1 + y2)/2 / pix_max;
			
			*o++ = r_ + y;
			*o++ = g_ + y;
			*o++ = b_ + y;
			*o++ = 1.0;
		}
	}
}

