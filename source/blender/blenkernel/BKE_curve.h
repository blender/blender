/**
 * blenlib/BKE_curve.h (mar-2001 nzc)
 *	
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_CURVE_H
#define BKE_CURVE_H

struct Curve;
struct ListBase;
struct Object;
struct Nurb;
struct ListBase;
struct BezTriple;
struct BevList;


int copyintoExtendedArray(float *old, int oldx, int oldy, float *newp, int newx, int newy);
void unlink_curve( struct Curve *cu);
void free_curve( struct Curve *cu);
struct Curve *add_curve(int type);
struct Curve *copy_curve( struct Curve *cu);
void make_local_curve( struct Curve *cu);
void test_curve_type( struct Object *ob);
void tex_space_curve( struct Curve *cu);
int count_curveverts( struct ListBase *nurb);
void freeNurb( struct Nurb *nu);
void freeNurblist( struct ListBase *lb);
struct Nurb *duplicateNurb( struct Nurb *nu);
void duplicateNurblist( struct ListBase *lb1,  struct ListBase *lb2);
void test2DNurb( struct Nurb *nu);
void minmaxNurb( struct Nurb *nu, float *min, float *max);
void extend_spline(float * pnts, int in, int out);
void calcknots(float *knots, short aantal, short order, short type);
void makecyclicknots(float *knots, short pnts, short order);
void makeknots( struct Nurb *nu, short uv, short type);
void basisNurb(float t, short order, short pnts, float *knots, float *basis, int *start, int *end);
void makeNurbfaces( struct Nurb *nu, float *data);
void makeNurbcurve_forw(struct Nurb *nu, float *data);
void makeNurbcurve( struct Nurb *nu, float *data, int dim);
void maakbez(float q0, float q1, float q2, float q3, float *p, int it);
void make_orco_surf( struct Curve *cu);
void makebevelcurve( struct Object *ob,  struct ListBase *disp);
short bevelinside(struct BevList *bl1,struct BevList *bl2);
int vergxcobev(const void *a1, const void *a2);
void calc_bevel_sin_cos(float x1, float y1, float x2, float y2, float *sina, float *cosa);
void alfa_bezpart( struct BezTriple *prevbezt,  struct BezTriple *bezt,  struct Nurb *nu, float *data_a);
void makeBevelList( struct Object *ob);
void calchandleNurb( struct BezTriple *bezt, struct BezTriple *prev,  struct BezTriple *next, int mode);
void calchandlesNurb( struct Nurb *nu);
void testhandlesNurb( struct Nurb *nu);
void autocalchandlesNurb( struct Nurb *nu, int flag);
void autocalchandlesNurb_all(int flag);
void sethandlesNurb(short code);
void swapdata(void *adr1, void *adr2, int len);
void switchdirectionNurb( struct Nurb *nu);

#endif

