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
	  "Show curves as overlay."}, 
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

void plugin_getinfo(PluginInfo *info) {
	info->name= name;
	info->nvars= sizeof(varstr)/sizeof(VarStruct);
	info->cfra= &cfra;

	info->varstr= varstr;

	info->init= plugin_init;
	info->seq_doit= (SeqDoit) plugin_seq_doit;
	info->callback= plugin_but_changed;
}

static void hsv_to_rgb (double  h, double  s, double  v,
			double *r, double *g, double *b)
{
	int i;
	double f, w, q, t;

	if (s == 0.0)
		s = 0.000001;
	
	if (h == -1.0)
	{
		*r = v;
		*g = v;
		*b = v;
	}
	else
	{
		if (h == 360.0)
			h = 0.0;
		h = h / 60.0;
		i = (int) h;
		f = h - i;
		w = v * (1.0 - s);
		q = v * (1.0 - (s * f));
		t = v * (1.0 - (s * (1.0 - f)));
		
		switch (i)
		{
		case 0:
			*r = v;
			*g = t;
			*b = w;
			break;
		case 1:
			*r = q;
			*g = v;
			*b = w;
			break;
		case 2:
			*r = w;
			*g = v;
			*b = t;
			break;
		case 3:
			*r = w;
			*g = q;
			*b = v;
			break;
		case 4:
			*r = t;
			*g = w;
			*b = v;
			break;
		case 5:
			*r = v;
			*g = w;
			*b = q;
			break;
		}
	}
}

static void rgb_to_hsv (double  r, double  g, double  b,
			double *h, double *s, double *v)
{
	double max, min, delta;

	max = r;
	if (g > max)
		max = g;
	if (b > max)
		max = b;
	
	min = r;
	if (g < min)
		min = g;
	if (b < min)
		min = b;
	
	*v = max;
	
	if (max != 0.0)
		*s = (max - min) / max;
	else
		*s = 0.0;
	
	if (*s == 0.0)
		*h = -1.0;
	else
	{
		delta = max - min;
		
		if (r == max)
			*h = (g - b) / delta;
		else if (g == max)
			*h = 2.0 + (b - r) / delta;
		else if (b == max)
			*h = 4.0 + (r - g) / delta;
		
		*h = *h * 60.0;
		
		if (*h < 0.0)
			*h = *h + 360;
	}
}

void plugin_seq_doit(Cast *cast, float facf0, float facf1, int width, 
	int height, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *out, ImBuf *use) {
	char *dest, *src1;
	int x, y, c;
	double gamma_table[256];
	double uv_table[256];
	float *destf = out->rect_float;
	float *src1f = ibuf1->rect_float;
	
	if (!ibuf1) return;

	dest= (char *) out->rect;
	src1= (char *) ibuf1->rect;

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
			double h,s,v,r,g,b;
			double fac;

			if (ibuf1->rect_float) rgb_to_hsv(src1f[0], src1f[1],
				src1f[2],&h,&s,&v);
			else rgb_to_hsv((double) src1[0]/255.0,
				   (double) src1[1]/255.0,
				   (double) src1[2]/255.0,
				   &h, &s, &v);
			v = gamma_table[(int) (v * 255.0)] / 255.0;

			fac = uv_table[(int) (255.0 * v)];

			s *= fac;
			if (s >= 1.0) {
				s = 1.0;
			}
			hsv_to_rgb(h,s,v, &r, &g, &b);
			
			if (out->rect_float) {
				destf[0] = r;
				destf[1] = g;
				destf[2] = b;
				destf = destf + 4;
				src1f +=4;
			} else {
				dest[0] = r*255.0;
				dest[1] = g*255.0;
				dest[2] = b*255.0;
				dest += 4;
			}

			src1 += 4;
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
