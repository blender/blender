/*
 * Gamma Correction Plugin (RGB Version) 0.01
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

char name[]= "Gamma Correction";

VarStruct varstr[]= {
	{ NUMSLI|FLO, "St M:", 0.0,	-1.0,	1.0, "Setup Main"}, 
	{ NUMSLI|FLO, "Gn M:",  1.0,	0.0,	10.0,"Gain Main"},
	{ NUMSLI|FLO, "Ga M:", 1.0,	0.0,	10.0, "Gamma Main"},

	{ NUMSLI|FLO, "St R:", 0.0,	-1.0,	1.0, "Setup Red"}, 
	{ NUMSLI|FLO, "Gn R:",  1.0,	0.0,	10.0,"Gain Red"},
	{ NUMSLI|FLO, "Ga R:", 1.0,	0.0,	10.0, "Gamma Red"},

	{ NUMSLI|FLO, "St G:", 0.0,	-1.0,	1.0, "Setup Green"}, 
	{ NUMSLI|FLO, "Gn G:",  1.0,	0.0,	10.0,"Gain Green"},
	{ NUMSLI|FLO, "Ga G:", 1.0,	0.0,	10.0, "Gamma Green"},

	{ NUMSLI|FLO, "St B:", 0.0,	-1.0,	1.0, "Setup Blue"}, 
	{ NUMSLI|FLO, "Gn B:",  1.0,	0.0,	10.0,"Gain Blue"},
	{ NUMSLI|FLO, "Ga B:", 1.0,	0.0,	10.0, "Gamma Blue"},
};

typedef struct Cast {
	float setup_m;
	float gain_m;
	float gamma_m;

	float setup_r;
	float gain_r;
	float gamma_r;

	float setup_g;
	float gain_g;
	float gamma_g;

	float setup_b;
	float gain_b;
	float gamma_b;
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

static void make_gamma_table(float setup, float gain, float gamma,
			     unsigned char * table)
{
	int y;

	for (y = 0; y < 256; y++) {
	        float v = 1.0 * y / 255;
		v += setup;
		v *= gain;
		v = pow(v, gamma);
		if ( v > 1.0) {
			v = 1.0;
		} else if (v < 0.0) {
			v = 0.0;
		}
		table[y] = v * 255;
	}

}

void plugin_seq_doit(Cast *cast, float facf0, float facf1, int width, 
	int height, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *out, ImBuf *use) {
	char *dest, *src1, *src2;
	int x, y, c;
	float rgb[3];
	float yuv[3];
	unsigned char gamma_table_m[256];
	unsigned char gamma_table_r[256];
	unsigned char gamma_table_g[256];
	unsigned char gamma_table_b[256];
	
	if (!ibuf1) return;

	dest= (char *) out->rect;
	src1= (char *) ibuf1->rect;

	make_gamma_table(cast->setup_m, cast->gain_m, cast->gamma_m,
			 gamma_table_m);
	make_gamma_table(cast->setup_r, cast->gain_r, cast->gamma_r,
			 gamma_table_r);
	make_gamma_table(cast->setup_g, cast->gain_g, cast->gamma_g,
			 gamma_table_g);
	make_gamma_table(cast->setup_b, cast->gain_b, cast->gamma_b,
			 gamma_table_b);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			*dest++ = gamma_table_r[gamma_table_m[*src1++]];
			*dest++ = gamma_table_g[gamma_table_m[*src1++]];
			*dest++ = gamma_table_b[gamma_table_m[*src1++]];
			dest++; src1++;
		}
	}
}
