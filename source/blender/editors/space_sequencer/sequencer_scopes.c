/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Author: Peter Schlaile < peter [at] schlaile [dot] de >
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/editors/space_sequencer/sequencer_scopes.c
 *  \ingroup spseq
 */


#include <math.h>
#include <string.h>

#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "sequencer_intern.h"

/* XXX, why is this function better then BLI_math version?
 * only difference is it does some normalize after, need to double check on this - campbell */
static void rgb_to_yuv_normalized(const float rgb[3], float yuv[3])
{
	yuv[0] = 0.299f * rgb[0] + 0.587f * rgb[1] + 0.114f * rgb[2];
	yuv[1] = 0.492f * (rgb[2] - yuv[0]);
	yuv[2] = 0.877f * (rgb[0] - yuv[0]);

	/* Normalize */
	yuv[1] *= 255.0f / (122 * 2.0f);
	yuv[1] += 0.5f;

	yuv[2] *= 255.0f / (157 * 2.0f);
	yuv[2] += 0.5f;
}

static void scope_put_pixel(unsigned char *table, unsigned char *pos)
{
	char newval = table[*pos];
	pos[0] = pos[1] = pos[2] = newval;
	pos[3] = 255;
}

static void scope_put_pixel_single(unsigned char *table, unsigned char *pos,
                                   int col)
{
	char newval = table[pos[col]];
	pos[col] = newval;
	pos[3] = 255;
}

static void wform_put_line(int w,
                           unsigned char *last_pos, unsigned char *new_pos)
{
	if (last_pos > new_pos) {
		unsigned char *temp = new_pos;
		new_pos = last_pos;
		last_pos = temp;
	}

	while (last_pos < new_pos) {
		if (last_pos[0] == 0) {
			last_pos[0] = last_pos[1] = last_pos[2] = 32;
			last_pos[3] = 255;
		}
		last_pos += 4 * w;
	}
}

static void wform_put_line_single(
    int w, unsigned char *last_pos, unsigned char *new_pos, int col)
{
	if (last_pos > new_pos) {
		unsigned char *temp = new_pos;
		new_pos = last_pos;
		last_pos = temp;
	}

	while (last_pos < new_pos) {
		if (last_pos[col] == 0) {
			last_pos[col] = 32;
			last_pos[3] = 255;
		}
		last_pos += 4 * w;
	}
}

static void wform_put_border(unsigned char *tgt, int w, int h)
{
	int x, y;

	for (x = 0; x < w; x++) {
		unsigned char *p = tgt + 4 * x;
		p[1] = p[3] = 255.0;
		p[4 * w + 1] = p[4 * w + 3] = 255.0;
		p = tgt + 4 * (w * (h - 1) + x);
		p[1] = p[3] = 255.0;
		p[-4 * w + 1] = p[-4 * w + 3] = 255.0;
	}

	for (y = 0; y < h; y++) {
		unsigned char *p = tgt + 4 * w * y;
		p[1] = p[3] = 255.0;
		p[4 + 1] = p[4 + 3] = 255.0;
		p = tgt + 4 * (w * y + w - 1);
		p[1] = p[3] = 255.0;
		p[-4 + 1] = p[-4 + 3] = 255.0;
	}
}

static void wform_put_gridrow(unsigned char *tgt, float perc, int w, int h)
{
	int i;

	tgt += (int) (perc / 100.0f * h) * w * 4;

	for (i = 0; i < w * 2; i++) {
		tgt[0] = 255;

		tgt += 4;
	}
}

static void wform_put_grid(unsigned char *tgt, int w, int h)
{
	wform_put_gridrow(tgt, 90.0, w, h);
	wform_put_gridrow(tgt, 70.0, w, h);
	wform_put_gridrow(tgt, 10.0, w, h);
}

static ImBuf *make_waveform_view_from_ibuf_byte(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect);
	int x, y;
	unsigned char *src = (unsigned char *) ibuf->rect;
	unsigned char *tgt = (unsigned char *) rval->rect;
	int w = ibuf->x + 3;
	int h = 515;
	float waveform_gamma = 0.2;
	unsigned char wtable[256];

	wform_put_grid(tgt, w, h);

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1) / 256,
		                                 waveform_gamma) * 255);
	}

	for (y = 0; y < ibuf->y; y++) {
		unsigned char *last_p = NULL;

		for (x = 0; x < ibuf->x; x++) {
			unsigned char *rgb = src + 4 * (ibuf->x * y + x);
			float v = (float)rgb_to_luma_byte(rgb) / 255.0f;
			unsigned char *p = tgt;
			p += 4 * (w * ((int) (v * (h - 3)) + 1) + x + 1);

			scope_put_pixel(wtable, p);
			p += 4 * w;
			scope_put_pixel(wtable, p);

			if (last_p != NULL) {
				wform_put_line(w, last_p, p);
			}
			last_p = p;
		}
	}

	wform_put_border(tgt, w, h);
	
	return rval;
}

static ImBuf *make_waveform_view_from_ibuf_float(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect);
	int x, y;
	float *src = ibuf->rect_float;
	unsigned char *tgt = (unsigned char *) rval->rect;
	int w = ibuf->x + 3;
	int h = 515;
	float waveform_gamma = 0.2;
	unsigned char wtable[256];

	wform_put_grid(tgt, w, h);

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1) / 256,
		                                 waveform_gamma) * 255);
	}

	for (y = 0; y < ibuf->y; y++) {
		unsigned char *last_p = NULL;

		for (x = 0; x < ibuf->x; x++) {
			float *rgb = src + 4 * (ibuf->x * y + x);
			float v = rgb_to_luma(rgb);
			unsigned char *p = tgt;

			CLAMP(v, 0.0f, 1.0f);

			p += 4 * (w * ((int) (v * (h - 3)) + 1) + x + 1);

			scope_put_pixel(wtable, p);
			p += 4 * w;
			scope_put_pixel(wtable, p);

			if (last_p != NULL) {
				wform_put_line(w, last_p, p);
			}
			last_p = p;
		}
	}

	wform_put_border(tgt, w, h);
	
	return rval;
}

ImBuf *make_waveform_view_from_ibuf(ImBuf *ibuf)
{
	if (ibuf->rect_float) {
		return make_waveform_view_from_ibuf_float(ibuf);
	}
	else {
		return make_waveform_view_from_ibuf_byte(ibuf);
	}
}


static ImBuf *make_sep_waveform_view_from_ibuf_byte(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(
	    ibuf->x + 3, 515, 32, IB_rect);
	int x, y;
	unsigned char *src = (unsigned char *) ibuf->rect;
	unsigned char *tgt = (unsigned char *) rval->rect;
	int w = ibuf->x + 3;
	int sw = ibuf->x / 3;
	int h = 515;
	float waveform_gamma = 0.2;
	unsigned char wtable[256];

	wform_put_grid(tgt, w, h);

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1) / 256,
		                                 waveform_gamma) * 255);
	}

	for (y = 0; y < ibuf->y; y++) {
		unsigned char *last_p[3] = {NULL, NULL, NULL};

		for (x = 0; x < ibuf->x; x++) {
			int c;
			unsigned char *rgb = src + 4 * (ibuf->x * y + x);
			for (c = 0; c < 3; c++) {
				unsigned char *p = tgt;
				p += 4 * (w * ((rgb[c] * (h - 3)) / 255 + 1)
				          + c * sw + x / 3 + 1);

				scope_put_pixel_single(wtable, p, c);
				p += 4 * w;
				scope_put_pixel_single(wtable, p, c);

				if (last_p[c] != NULL) {
					wform_put_line_single(
					    w, last_p[c], p, c);
				}
				last_p[c] = p;
			}
		}
	}

	wform_put_border(tgt, w, h);
	
	return rval;
}

static ImBuf *make_sep_waveform_view_from_ibuf_float(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect);
	int x, y;
	float *src = ibuf->rect_float;
	unsigned char *tgt = (unsigned char *) rval->rect;
	int w = ibuf->x + 3;
	int sw = ibuf->x / 3;
	int h = 515;
	float waveform_gamma = 0.2;
	unsigned char wtable[256];

	wform_put_grid(tgt, w, h);

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1) / 256,
		                                 waveform_gamma) * 255);
	}

	for (y = 0; y < ibuf->y; y++) {
		unsigned char *last_p[3] = {NULL, NULL, NULL};

		for (x = 0; x < ibuf->x; x++) {
			int c;
			float *rgb = src + 4 * (ibuf->x * y + x);
			for (c = 0; c < 3; c++) {
				unsigned char *p = tgt;
				float v = rgb[c];

				CLAMP(v, 0.0f, 1.0f);

				p += 4 * (w * ((int) (v * (h - 3)) + 1)
				          + c * sw + x / 3 + 1);

				scope_put_pixel_single(wtable, p, c);
				p += 4 * w;
				scope_put_pixel_single(wtable, p, c);

				if (last_p[c] != NULL) {
					wform_put_line_single(
					    w, last_p[c], p, c);
				}
				last_p[c] = p;
			}
		}
	}

	wform_put_border(tgt, w, h);
	
	return rval;
}

ImBuf *make_sep_waveform_view_from_ibuf(ImBuf *ibuf)
{
	if (ibuf->rect_float) {
		return make_sep_waveform_view_from_ibuf_float(ibuf);
	}
	else {
		return make_sep_waveform_view_from_ibuf_byte(ibuf);
	}
}

static void draw_zebra_byte(ImBuf *src, ImBuf *ibuf, float perc)
{
	unsigned int limit = 255.0f * perc / 100.0f;
	unsigned char *p = (unsigned char *) src->rect;
	unsigned char *o = (unsigned char *) ibuf->rect;
	int x;
	int y;

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			unsigned char r = *p++;
			unsigned char g = *p++;
			unsigned char b = *p++;
			unsigned char a = *p++;

			if (r >= limit || g >= limit || b >= limit) {
				if (((x + y) & 0x08) != 0) {
					r = 255 - r;
					g = 255 - g;
					b = 255 - b;
				}
			}
			*o++ = r;
			*o++ = g;
			*o++ = b;
			*o++ = a;
		}
	}
}


static void draw_zebra_float(ImBuf *src, ImBuf *ibuf, float perc)
{
	float limit = perc / 100.0f;
	float *p = src->rect_float;
	unsigned char *o = (unsigned char *) ibuf->rect;
	int x;
	int y;

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			float r = *p++;
			float g = *p++;
			float b = *p++;
			float a = *p++;

			if (r >= limit || g >= limit || b >= limit) {
				if (((x + y) & 0x08) != 0) {
					r = -r;
					g = -g;
					b = -b;
				}
			}

			*o++ = FTOCHAR(r);
			*o++ = FTOCHAR(g);
			*o++ = FTOCHAR(b);
			*o++ = FTOCHAR(a);
		}
	}
}

ImBuf *make_zebra_view_from_ibuf(ImBuf *src, float perc)
{
	ImBuf *ibuf = IMB_allocImBuf(src->x, src->y, 32, IB_rect);

	if (src->rect_float) {
		draw_zebra_float(src, ibuf, perc);
	}
	else {
		draw_zebra_byte(src, ibuf, perc);
	}
	return ibuf;
}

static void draw_histogram_marker(ImBuf *ibuf, int x)
{
	unsigned char *p = (unsigned char *) ibuf->rect;
	int barh = ibuf->y * 0.1;
	int i;

	p += 4 * (x + ibuf->x * (ibuf->y - barh + 1));

	for (i = 0; i < barh - 1; i++) {
		p[0] = p[1] = p[2] = 255;
		p += ibuf->x * 4;
	}
}

static void draw_histogram_bar(ImBuf *ibuf, int x, float val, int col)
{
	unsigned char *p = (unsigned char *) ibuf->rect;
	int barh = ibuf->y * val * 0.9f;
	int i;

	p += 4 * (x + ibuf->x);

	for (i = 0; i < barh; i++) {
		p[col] = 255;
		p += ibuf->x * 4;
	}
}

static ImBuf *make_histogram_view_from_ibuf_byte(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(515, 128, 32, IB_rect);
	int c, x, y;
	unsigned int n;
	unsigned char *src = (unsigned char *) ibuf->rect;

	unsigned int bins[3][256];

	memset(bins, 0, 3 * 256 * sizeof(unsigned int));

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			bins[0][*src++]++;
			bins[1][*src++]++;
			bins[2][*src++]++;
			src++;
		}
	}

	n = 0;
	for (c = 0; c < 3; c++) {
		for (x = 0; x < 256; x++) {
			if (bins[c][x] > n) {
				n = bins[c][x];
			}
		}
	}

	for (c = 0; c < 3; c++) {
		for (x = 0; x < 256; x++) {
			draw_histogram_bar(rval, x * 2 + 1,
			                   ((float) bins[c][x]) / n, c);
			draw_histogram_bar(rval, x * 2 + 2,
			                   ((float) bins[c][x]) / n, c);
		}
	}

	wform_put_border((unsigned char *) rval->rect, rval->x, rval->y);
	
	return rval;
}

static int get_bin_float(float f)
{
	if (f < -0.25f) {
		f = -0.25f;
	}
	else if (f > 1.25f) {
		f = 1.25f;
	}

	return (int) (((f + 0.25f) / 1.5f) * 512);
}

static ImBuf *make_histogram_view_from_ibuf_float(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(515, 128, 32, IB_rect);
	int n, c, x, y;
	float *src = ibuf->rect_float;

	unsigned int bins[3][512];

	memset(bins, 0, 3 * 256 * sizeof(unsigned int));

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			bins[0][get_bin_float(*src++)]++;
			bins[1][get_bin_float(*src++)]++;
			bins[2][get_bin_float(*src++)]++;
			src++;
		}
	}

	draw_histogram_marker(rval, get_bin_float(0.0));
	draw_histogram_marker(rval, get_bin_float(1.0));

	n = 0;
	for (c = 0; c < 3; c++) {
		for (x = 0; x < 512; x++) {
			if (bins[c][x] > n) {
				n = bins[c][x];
			}
		}
	}
	for (c = 0; c < 3; c++) {
		for (x = 0; x < 512; x++) {
			draw_histogram_bar(rval, x + 1, (float) bins[c][x] / n, c);
		}
	}

	wform_put_border((unsigned char *) rval->rect, rval->x, rval->y);
	
	return rval;
}

ImBuf *make_histogram_view_from_ibuf(ImBuf *ibuf)
{
	if (ibuf->rect_float) {
		return make_histogram_view_from_ibuf_float(ibuf);
	}
	else {
		return make_histogram_view_from_ibuf_byte(ibuf);
	}
}

static void vectorscope_put_cross(unsigned char r, unsigned char g, 
                                  unsigned char b,
                                  char *tgt, int w, int h, int size)
{
	float rgb[3], yuv[3];
	char *p;
	int x = 0;
	int y = 0;

	rgb[0] = (float)r / 255.0f;
	rgb[1] = (float)g / 255.0f;
	rgb[2] = (float)b / 255.0f;
	rgb_to_yuv_normalized(rgb, yuv);
			
	p = tgt + 4 * (w * (int) ((yuv[2] * (h - 3) + 1)) 
	               + (int) ((yuv[1] * (w - 3) + 1)));

	if (r == 0 && g == 0 && b == 0) {
		r = 255;
	}

	for (y = -size; y <= size; y++) {
		for (x = -size; x <= size; x++) {
			char *q = p + 4 * (y * w + x);
			q[0] = r; q[1] = g; q[2] = b; q[3] = 255;
		}
	}
}

static ImBuf *make_vectorscope_view_from_ibuf_byte(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(515, 515, 32, IB_rect);
	int x, y;
	char *src = (char *) ibuf->rect;
	char *tgt = (char *) rval->rect;
	float rgb[3], yuv[3];
	int w = 515;
	int h = 515;
	float scope_gamma = 0.2;
	unsigned char wtable[256];

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1) / 256,
		                                 scope_gamma) * 255);
	}

	for (x = 0; x <= 255; x++) {
		vectorscope_put_cross(255,     0, 255 - x, tgt, w, h, 1);
		vectorscope_put_cross(255,     x,      0, tgt, w, h, 1);
		vectorscope_put_cross(255 - x,   255,      0, tgt, w, h, 1);
		vectorscope_put_cross(0,        255,      x, tgt, w, h, 1);
		vectorscope_put_cross(0,    255 - x,    255, tgt, w, h, 1);
		vectorscope_put_cross(x,          0,    255, tgt, w, h, 1);
	}

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			char *src1 = src + 4 * (ibuf->x * y + x);
			char *p;
			
			rgb[0] = (float)src1[0] / 255.0f;
			rgb[1] = (float)src1[1] / 255.0f;
			rgb[2] = (float)src1[2] / 255.0f;
			rgb_to_yuv_normalized(rgb, yuv);
			
			p = tgt + 4 * (w * (int) ((yuv[2] * (h - 3) + 1)) 
			               + (int) ((yuv[1] * (w - 3) + 1)));
			scope_put_pixel(wtable, (unsigned char *)p);
		}
	}

	vectorscope_put_cross(0, 0, 0, tgt, w, h, 3);

	return rval;
}

static ImBuf *make_vectorscope_view_from_ibuf_float(ImBuf *ibuf)
{
	ImBuf *rval = IMB_allocImBuf(515, 515, 32, IB_rect);
	int x, y;
	float *src = ibuf->rect_float;
	char *tgt = (char *) rval->rect;
	float rgb[3], yuv[3];
	int w = 515;
	int h = 515;
	float scope_gamma = 0.2;
	unsigned char wtable[256];

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1) / 256,
		                                 scope_gamma) * 255);
	}

	for (x = 0; x <= 255; x++) {
		vectorscope_put_cross(255,     0, 255 - x, tgt, w, h, 1);
		vectorscope_put_cross(255,     x,      0, tgt, w, h, 1);
		vectorscope_put_cross(255 - x,   255,      0, tgt, w, h, 1);
		vectorscope_put_cross(0,        255,      x, tgt, w, h, 1);
		vectorscope_put_cross(0,    255 - x,    255, tgt, w, h, 1);
		vectorscope_put_cross(x,          0,    255, tgt, w, h, 1);
	}

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			float *src1 = src + 4 * (ibuf->x * y + x);
			char *p;
			
			memcpy(rgb, src1, 3 * sizeof(float));

			CLAMP(rgb[0], 0.0f, 1.0f);
			CLAMP(rgb[1], 0.0f, 1.0f);
			CLAMP(rgb[2], 0.0f, 1.0f);

			rgb_to_yuv_normalized(rgb, yuv);
			
			p = tgt + 4 * (w * (int) ((yuv[2] * (h - 3) + 1)) 
			               + (int) ((yuv[1] * (w - 3) + 1)));
			scope_put_pixel(wtable, (unsigned char *)p);
		}
	}

	vectorscope_put_cross(0, 0, 0, tgt, w, h, 3);

	return rval;
}

ImBuf *make_vectorscope_view_from_ibuf(ImBuf *ibuf)
{
	if (ibuf->rect_float) {
		return make_vectorscope_view_from_ibuf_float(ibuf);
	}
	else {
		return make_vectorscope_view_from_ibuf_byte(ibuf);
	}
}
