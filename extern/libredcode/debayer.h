#ifndef __redcode_debayer_h_included__
#define __redcode_debayer_h_included__ 1

void redcode_ycbcr2rgb_fullscale(
	int ** planes, int width, int height, float * out);
void redcode_ycbcr2rgb_halfscale(
	int ** planes, int width, int height, float * out);
void redcode_ycbcr2rgb_quarterscale(
	int ** planes, int width, int height, float * out);

#endif
