/**
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_COLORTOOLS_H
#define BKE_COLORTOOLS_H

struct CurveMapping;
struct CurveMap;
struct ImBuf;
struct rctf;

void 				gamma_correct_rec709(float *c, float gamma);
void				gamma_correct(float *c, float gamma);
float				srgb_to_linearrgb(float c);
float				linearrgb_to_srgb(float c);
void				color_manage_linearize(float *col_to, float *col_from);

void				floatbuf_to_srgb_byte(float *rectf, unsigned char *rectc, int x1, int x2, int y1, int y2, int w);
void				floatbuf_to_byte(float *rectf, unsigned char *rectc, int x1, int x2, int y1, int y2, int w);

struct CurveMapping	*curvemapping_add(int tot, float minx, float miny, float maxx, float maxy);
void				curvemapping_free(struct CurveMapping *cumap);
struct CurveMapping	*curvemapping_copy(struct CurveMapping *cumap);
void				curvemapping_set_black_white(struct CurveMapping *cumap, float *black, float *white);

void				curvemap_remove(struct CurveMap *cuma, int flag);
void				curvemap_insert(struct CurveMap *cuma, float x, float y);
void				curvemap_reset(struct CurveMap *cuma, struct rctf *clipr);
void				curvemap_sethandle(struct CurveMap *cuma, int type);

void				curvemapping_changed(struct CurveMapping *cumap, int rem_doubles);
					
					/* single curve, no table check */
float				curvemap_evaluateF(struct CurveMap *cuma, float value);
					/* single curve, with table check */
float				curvemapping_evaluateF(struct CurveMapping *cumap, int cur, float value);
void				curvemapping_evaluate3F(struct CurveMapping *cumap, float *vecout, const float *vecin);
void				curvemapping_evaluateRGBF(struct CurveMapping *cumap, float *vecout, const float *vecin);
void				curvemapping_evaluate_premulRGBF(struct CurveMapping *cumap, float *vecout, const float *vecin);
void				curvemapping_do_ibuf(struct CurveMapping *cumap, struct ImBuf *ibuf);
void				curvemapping_premultiply(struct CurveMapping *cumap, int restore);
int					curvemapping_RGBA_does_something(struct CurveMapping *cumap);
void				curvemapping_initialize(struct CurveMapping *cumap);
void				curvemapping_table_RGBA(struct CurveMapping *cumap, float **array, int *size);
void				colorcorrection_do_ibuf(struct ImBuf *ibuf, const char *profile);


#endif

