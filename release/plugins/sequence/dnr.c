/*
 * Dynamic Noise Reduction (based on the VirtualDub filter by Steven Don)
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

#include <math.h>
#include "plugin.h"
#include <stdio.h>

char name[]= "Dynamic Noise Reduction";

VarStruct varstr[]= {
	{ NUMSLI|INT, "Level:", 10.0,	0.0,	15.0, "Level"}, 
};

typedef struct Cast {
	int level;
} Cast;

float cfra;
void * plugin_private_data;

struct my_data {
	unsigned char lookup_table[65536];
	int last_level;
	float last_cfra;
	int last_width;
	int last_height;
	unsigned char * last_frame;
};

void plugin_seq_doit(Cast *, float, float, int, int, 
                     ImBuf *, ImBuf *, ImBuf *, ImBuf *);

int plugin_seq_getversion(void) { return B_PLUGIN_VERSION;}

static void precalculate(unsigned char * table, int level)
{
	int ap_, bp;

	for (ap_ = 0; ap_ < 256; ap_++) {
		for (bp = 0; bp < 256; bp++) {
			int ap = ap_;
			int diff = ap - bp;
			if (diff < 0) {
				diff = -diff;
			}
			if (diff < level) {
				if (diff > (level >> 1)) {
					ap = (ap + ap + bp)/3;
				} else {
					ap = bp;
				}
			}
		
			*table++ = ap;
		}
	}
}

void plugin_but_changed(int but) { }
void plugin_init() { }

void * plugin_seq_alloc_private_data()
{
	struct my_data * result = (struct my_data*) calloc(
		sizeof(struct my_data), 1);
	result->last_cfra = -1;
	return result;
}

void plugin_seq_free_private_data(void * data)
{
	struct my_data * d = (struct my_data*) data;
	if (d->last_frame) {
		free(d->last_frame);
	}
	free(d);
}

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

static void doit(unsigned char * src_, unsigned char * dst_,
		 unsigned char * table, int width, int height)
{
	int count = width * height;
	unsigned char * src = src_;
	unsigned char * dst = dst_;

	while (count--) {
		*dst = table[(*src++ << 8) | *dst]; dst++;
		*dst = table[(*src++ << 8) | *dst]; dst++;
		*dst = table[(*src++ << 8) | *dst]; dst++;
		*dst++ = *src++;
	}

	memcpy(src_, dst_, width * height * 4);
}

void plugin_seq_doit(Cast *cast, float facf0, float facf1, int width, 
	int height, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *out, ImBuf *use) {

	struct my_data * d = (struct my_data*) plugin_private_data;

	if (!ibuf1) return;

	if (cast->level != d->last_level) {
		precalculate(d->lookup_table, cast->level);
		d->last_level = cast->level;
	}

	if (width != d->last_width || height != d->last_height ||
	    cfra != d->last_cfra + 1)
	{
		free(d->last_frame);
		d->last_frame = (unsigned char*) calloc(width * height, 4);
		
		d->last_width = width;
		d->last_height = height;
	}

	memcpy(out->rect, ibuf1->rect, width * height * 4);

	doit((unsigned char*) out->rect, 
	     d->last_frame, d->lookup_table, width, height);
	
	d->last_cfra = cfra;
}
