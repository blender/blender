/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Author: Peter Schlaile < peter [at] schlaile [dot] de >
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */

#include "BSE_seqscopes.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "BKE_utildefines.h"
#include <math.h>
#include <string.h>

static void rgb_to_yuv(float rgb[3], float yuv[3]) {
        yuv[0]= 0.299*rgb[0] + 0.587*rgb[1] + 0.114*rgb[2];
        yuv[1]= 0.492*(rgb[2] - yuv[0]);
        yuv[2]= 0.877*(rgb[0] - yuv[0]);

        /* Normalize */
        yuv[1]*= 255.0/(122*2.0);
        yuv[1]+= 0.5;

        yuv[2]*= 255.0/(157*2.0);
        yuv[2]+= 0.5;
}

static void scope_put_pixel(unsigned char* table, unsigned char * pos)
{
	char newval = table[*pos];
	pos[0] = pos[1] = pos[2] = newval;
	pos[3] = 255;
}

static void wform_put_line(int w,
			   unsigned char * last_pos, unsigned char * new_pos)
{
	if (last_pos > new_pos) {
		unsigned char* temp = new_pos;
		new_pos = last_pos;
		last_pos = temp;
	}

	while (last_pos < new_pos) {
		if (last_pos[0] == 0) {
			last_pos[0] = last_pos[1] = last_pos[2] = 32;
			last_pos[3] = 255;
		}
		last_pos += 4*w;
	}
}

static struct ImBuf *make_waveform_view_from_ibuf_byte(struct ImBuf * ibuf)
{
	struct ImBuf * rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect, 0);
	int x,y;
	unsigned char* src = (unsigned char*) ibuf->rect;
	unsigned char* tgt = (unsigned char*) rval->rect;
	int w = ibuf->x + 3;
	int h = 515;
	float waveform_gamma = 0.2;
	unsigned char wtable[256];

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1)/256, 
						 waveform_gamma)*255);
	}

	for (y = 0; y < ibuf->y; y++) {
		unsigned char * last_p = 0;

		for (x = 0; x < ibuf->x; x++) {
			unsigned char * rgb = src + 4 * (ibuf->x * y + x);
			float v = 1.0 * 
				(  0.299*rgb[0] 
				 + 0.587*rgb[1] 
				 + 0.114*rgb[2]) / 255.0;
			unsigned char * p = tgt;
			p += 4 * (w * ((int) (v * (h - 3)) + 1) + x + 1);

			scope_put_pixel(wtable, p);
			p += 4 * w;
			scope_put_pixel(wtable, p);

			if (last_p != 0) {
				wform_put_line(w, last_p, p);
			}
			last_p = p;
		}
	}

	for (x = 0; x < w; x++) {
		unsigned char * p = tgt + 4 * x;
		p[1] = p[3] = 255.0;
		p[4 * w + 1] = p[4 * w + 3] = 255.0;
		p = tgt + 4 * (w * (h - 1) + x);
		p[1] = p[3] = 255.0;
		p[-4 * w + 1] = p[-4 * w + 3] = 255.0;
	}

	for (y = 0; y < h; y++) {
		unsigned char * p = tgt + 4 * w * y;
		p[1] = p[3] = 255.0;
		p[4 + 1] = p[4 + 3] = 255.0;
		p = tgt + 4 * (w * y + w - 1);
		p[1] = p[3] = 255.0;
		p[-4 + 1] = p[-4 + 3] = 255.0;
	}
	
	return rval;
}

static struct ImBuf *make_waveform_view_from_ibuf_float(struct ImBuf * ibuf)
{
	struct ImBuf * rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect, 0);
	int x,y;
	float* src = ibuf->rect_float;
	unsigned char* tgt = (unsigned char*) rval->rect;
	int w = ibuf->x + 3;
	int h = 515;
	float waveform_gamma = 0.2;
	unsigned char wtable[256];

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1)/256, 
						 waveform_gamma)*255);
	}

	for (y = 0; y < ibuf->y; y++) {
		unsigned char * last_p = 0;

		for (x = 0; x < ibuf->x; x++) {
			float * rgb = src + 4 * (ibuf->x * y + x);
			float v = 1.0 * 
				(  0.299*rgb[0] 
				 + 0.587*rgb[1] 
				 + 0.114*rgb[2]);
			unsigned char * p = tgt;

			CLAMP(v, 0.0, 1.0);

			p += 4 * (w * ((int) (v * (h - 3)) + 1) + x + 1);

			scope_put_pixel(wtable, p);
			p += 4 * w;
			scope_put_pixel(wtable, p);

			if (last_p != 0) {
				wform_put_line(w, last_p, p);
			}
			last_p = p;
		}
	}

	for (x = 0; x < w; x++) {
		unsigned char * p = tgt + 4 * x;
		p[1] = p[3] = 255.0;
		p[4 * w + 1] = p[4 * w + 3] = 255.0;
		p = tgt + 4 * (w * (h - 1) + x);
		p[1] = p[3] = 255.0;
		p[-4 * w + 1] = p[-4 * w + 3] = 255.0;
	}

	for (y = 0; y < h; y++) {
		unsigned char * p = tgt + 4 * w * y;
		p[1] = p[3] = 255.0;
		p[4 + 1] = p[4 + 3] = 255.0;
		p = tgt + 4 * (w * y + w - 1);
		p[1] = p[3] = 255.0;
		p[-4 + 1] = p[-4 + 3] = 255.0;
	}
	
	return rval;
}

struct ImBuf *make_waveform_view_from_ibuf(struct ImBuf * ibuf)
{
	if (ibuf->rect_float) {
		return make_waveform_view_from_ibuf_float(ibuf);
	} else {
		return make_waveform_view_from_ibuf_byte(ibuf);
	}
}


static void vectorscope_put_cross(unsigned char r, unsigned char g, 
				  unsigned char b, 
				  char * tgt, int w, int h, int size)
{
	float rgb[3], yuv[3];
	char * p;
	int x = 0;
	int y = 0;

	rgb[0]= (float)r/255.0;
	rgb[1]= (float)g/255.0;
	rgb[2]= (float)b/255.0;
	rgb_to_yuv(rgb, yuv);
			
	p = tgt + 4 * (w * (int) ((yuv[2] * (h - 3) + 1)) 
		       + (int) ((yuv[1] * (w - 3) + 1)));

	if (r == 0 && g == 0 && b == 0) {
		r = 255;
	}

	for (y = -size; y <= size; y++) {
		for (x = -size; x <= size; x++) {
			char * q = p + 4 * (y * w + x);
			q[0] = r; q[1] = g; q[2] = b; q[3] = 255;
		}
	}
}

static struct ImBuf *make_vectorscope_view_from_ibuf_byte(struct ImBuf * ibuf)
{
	struct ImBuf * rval = IMB_allocImBuf(515, 515, 32, IB_rect, 0);
	int x,y;
	char* src = (char*) ibuf->rect;
	char* tgt = (char*) rval->rect;
	float rgb[3], yuv[3];
	int w = 515;
	int h = 515;
	float scope_gamma = 0.2;
	unsigned char wtable[256];

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1)/256, 
						 scope_gamma)*255);
	}

	for (x = 0; x <= 255; x++) {
		vectorscope_put_cross(255   ,     0,255 - x, tgt, w, h, 1);
		vectorscope_put_cross(255   ,     x,      0, tgt, w, h, 1);
		vectorscope_put_cross(255- x,   255,      0, tgt, w, h, 1);
		vectorscope_put_cross(0,        255,      x, tgt, w, h, 1);
		vectorscope_put_cross(0,    255 - x,    255, tgt, w, h, 1);
		vectorscope_put_cross(x,          0,    255, tgt, w, h, 1);
	}

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			char * src1 = src + 4 * (ibuf->x * y + x);
			char * p;
			
			rgb[0]= (float)src1[0]/255.0;
			rgb[1]= (float)src1[1]/255.0;
			rgb[2]= (float)src1[2]/255.0;
			rgb_to_yuv(rgb, yuv);
			
			p = tgt + 4 * (w * (int) ((yuv[2] * (h - 3) + 1)) 
				       + (int) ((yuv[1] * (w - 3) + 1)));
			scope_put_pixel(wtable, (unsigned char*)p);
		}
	}

	vectorscope_put_cross(0, 0, 0, tgt, w, h, 3);

	return rval;
}

static struct ImBuf *make_vectorscope_view_from_ibuf_float(struct ImBuf * ibuf)
{
	struct ImBuf * rval = IMB_allocImBuf(515, 515, 32, IB_rect, 0);
	int x,y;
	float* src = ibuf->rect_float;
	char* tgt = (char*) rval->rect;
	float rgb[3], yuv[3];
	int w = 515;
	int h = 515;
	float scope_gamma = 0.2;
	unsigned char wtable[256];

	for (x = 0; x < 256; x++) {
		wtable[x] = (unsigned char) (pow(((float) x + 1)/256, 
						 scope_gamma)*255);
	}

	for (x = 0; x <= 255; x++) {
		vectorscope_put_cross(255   ,     0,255 - x, tgt, w, h, 1);
		vectorscope_put_cross(255   ,     x,      0, tgt, w, h, 1);
		vectorscope_put_cross(255- x,   255,      0, tgt, w, h, 1);
		vectorscope_put_cross(0,        255,      x, tgt, w, h, 1);
		vectorscope_put_cross(0,    255 - x,    255, tgt, w, h, 1);
		vectorscope_put_cross(x,          0,    255, tgt, w, h, 1);
	}

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			float * src1 = src + 4 * (ibuf->x * y + x);
			char * p;
			
			memcpy(rgb, src1, 3 * sizeof(float));

			CLAMP(rgb[0], 0.0, 1.0);
			CLAMP(rgb[1], 0.0, 1.0);
			CLAMP(rgb[2], 0.0, 1.0);

			rgb_to_yuv(rgb, yuv);
			
			p = tgt + 4 * (w * (int) ((yuv[2] * (h - 3) + 1)) 
				       + (int) ((yuv[1] * (w - 3) + 1)));
			scope_put_pixel(wtable, (unsigned char*)p);
		}
	}

	vectorscope_put_cross(0, 0, 0, tgt, w, h, 3);

	return rval;
}

struct ImBuf *make_vectorscope_view_from_ibuf(struct ImBuf * ibuf)
{
	if (ibuf->rect_float) {
		return make_vectorscope_view_from_ibuf_float(ibuf);
	} else {
		return make_vectorscope_view_from_ibuf_byte(ibuf);
	}
}
