#ifndef __DEBAYER_H__
#define __DEBAYER_H__

void redcode_ycbcr2rgb_fullscale(
	int ** planes, int width, int height, float * out);
void redcode_ycbcr2rgb_halfscale(
	int ** planes, int width, int height, float * out);
void redcode_ycbcr2rgb_quarterscale(
	int ** planes, int width, int height, float * out);

#endif
