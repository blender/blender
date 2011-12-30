/*
 * Color Correction Plugin (YUV Version) 0.01
 *
 * Copyright (c) 2005 Peter Schlaile
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "math.h"
#include "plugin.h"
#include <stdio.h>

char name[]= "Color Correction";

VarStruct varstr[]= {
	{ NUMSLI|FLO, "St Y:", 0.0,	-1.0,	1.0, "Setup Y"}, 
	{ NUMSLI|FLO, "Gn Y:",  1.0,	0.0,	10.0,"Gain Y"},
	{ NUMSLI|FLO, "Ga Y:", 1.0,	0.0,	10.0, "Gamma Y"},

	{ NUMSLI|FLO, "Lo S:",  1.0,	0.0,	10.0,"Saturation Shadows"},
	{ NUMSLI|FLO, "Md S:",  1.0,	0.0,	10.0,"Saturation Midtones"},
	{ NUMSLI|FLO, "Hi S:",  1.0,	0.0,	10.0,"Saturation Highlights"},

	{ NUMSLI|FLO, "MA S:",  1.0,	0.0,	10.0,"Master Saturation"}, 
	{ NUMSLI|FLO, "Lo T:",  0.25, 0.0, 1.0,
	  "Saturation Shadow Thres"}, 
	{ NUMSLI|FLO, "Hi T:",  0.75, 0.0, 1.0,
	  "Saturation Highlights Thres"}, 
	{ TOG|INT,	"Debug", 0.0,	0.0,	1.0,   
	  "Show curves as overlay"}, 
};

typedef struct Cast {
	float setup_y;
	float gain_y;
	float gamma_y;
	
	float sat_shadows;
	float sat_midtones;
	float sat_highlights;

	float master_sat;
	float lo_thres;
	float hi_thres;
	int debug;
} Cast;

float cfra;

void plugin_seq_doit(Cast *, float, float, int, int, ImBuf *, ImBuf *, ImBuf *, ImBuf *);

int plugin_seq_getversion(void) { return B_PLUGIN_VERSION;}
void plugin_but_changed(int but) {}
void plugin_init() {}

void plugin_getinfo(PluginInfo *info)
{
	info->name= name;
	info->nvars= sizeof(varstr)/sizeof(VarStruct);
	info->cfra= &cfra;

	info->varstr= varstr;

	info->init= plugin_init;
	info->seq_doit= (SeqDoit) plugin_seq_doit;
	info->callback= plugin_but_changed;
}

static void rgb_to_yuv(float rgb[3], float yuv[3])
{
	yuv[0]= 0.299*rgb[0] + 0.587*rgb[1] + 0.114*rgb[2];
	yuv[1]= 0.492*(rgb[2] - yuv[0]);
	yuv[2]= 0.877*(rgb[0] - yuv[0]);
	
	/* Normalize */
	yuv[1] /= 0.436;
	yuv[2] /= 0.615;
}

static void yuv_to_rgb(float yuv[3], float rgb[3])
{
	yuv[1] *= 0.436;
	yuv[2] *= 0.615;

	rgb[0] = yuv[2]/0.877 + yuv[0];
	rgb[2] = yuv[1]/0.492 + yuv[0];
	rgb[1] = (yuv[0] - 0.299*rgb[0] - 0.114*rgb[2]) / 0.587;
	if (rgb[0] > 1.0) {
		rgb[0] = 1.0;
	}
	if (rgb[0] < 0.0) {
		rgb[0] = 0.0;
	}
	if (rgb[1] > 1.0) {
		rgb[1] = 1.0;
	}
	if (rgb[1] < 0.0) {
		rgb[1] = 0.0;
	}
	if (rgb[2] > 1.0) {
		rgb[2] = 1.0;
	}
	if (rgb[2] < 0.0) {
		rgb[2] = 0.0;
	}
}

void plugin_seq_doit(Cast *cast, float facf0, float facf1, int width, 
	int height, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *out, ImBuf *use) {
	char *dest, *src1, *src2;
	int x, y, c;
	float rgb[3];
	float yuv[3];
	float gamma_table[256];
	float uv_table[256];
	float *destf = out->rect_float;
	float *src1f;
	
	if (!ibuf1) return;

	dest= (char *) out->rect;
	src1= (char *) ibuf1->rect;
	src1f= ibuf1->rect_float;

	for (y = 0; y < 256; y++) {
		float v = 1.0 * y / 255;
		v += cast->setup_y;
		v *= cast->gain_y;
		v = pow(v, cast->gamma_y);
		if ( v > 1.0) {
			v = 1.0;
		} else if (v < 0.0) {
			v = 0.0;
		}
		gamma_table[y] = v * 255;
	}

	for (y = 0; y < 256; y++) {
		float v = 1.0;
		v *= cast->master_sat;
		if (y < cast->lo_thres * 255) {
			v *= cast->sat_shadows;
		} else if (y > cast->hi_thres * 255) {
			v *= cast->sat_highlights;
		} else {
			v *= cast->sat_midtones;
		}
		uv_table[y] = v;
	}


	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float fac;
			if (out->rect_float) {
				rgb[0]= (float)src1f[0]/255.0;
				rgb[1]= (float)src1f[1]/255.0;
				rgb[2]= (float)src1f[2]/255.0;
			} else {
				rgb[0]= (float)src1[0]/255.0;
				rgb[1]= (float)src1[1]/255.0;
				rgb[2]= (float)src1[2]/255.0;
			}
			rgb_to_yuv(rgb, yuv);

			yuv[0] = gamma_table[(int) (yuv[0] * 255.0)] / 255.0;
			fac = uv_table[(int) (255.0 * yuv[0])];

			yuv[1] = yuv[1] * fac;
			yuv[2] = yuv[2] * fac;
			if (yuv[1] > 1.0) {
				yuv[1] = 1.0;
			}
			if (yuv[1] < -1.0) {
				yuv[1] = -1.0;
			}
			if (yuv[2] > 1.0) {
				yuv[2] = 1.0;
			}
			if (yuv[2] < -1.0) {
				yuv[2] = -1.0;
			}
			yuv_to_rgb(yuv, rgb);
			
			if (out->rect_float) {
				*destf++ = rgb[0];
				*destf++ = rgb[1];
				*destf++ = rgb[2];
				destf++;
				src1f += 4;
			} else {
				*dest++ = rgb[0]*255.0;
				*dest++ = rgb[1]*255.0;
				*dest++ = rgb[2]*255.0;
				dest++;
				src1 += 4;
			}
		}
	}

	if (cast->debug) {
		dest= (char *) out->rect;
		for (c = 0; c < 10; c++) {
			x = 0;
			for (y = 0; y < 256; y++) {
				char val = gamma_table[y];
				while (x < y * width / 255) {
					*dest++ = val;
					*dest++ = val;
					*dest++ = val;
					dest++;
					x++;
				}
			}
		}
		for (c = 0; c < 10; c++) {
			x = 0;
			for (y = 0; y < 256; y++) {
				char val = uv_table[y] * 255.0/10.0;
				while (x < y * width / 255) {
					*dest++ = val;
					*dest++ = val;
					*dest++ = val;
					dest++;
					x++;
				}
			}
		}
	}
}
