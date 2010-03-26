/**
 * blenlib/BKE_curve.h (mar-2001 nzc)
 *	
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_CURVE_H
#define BKE_CURVE_H

struct Curve;
struct ListBase;
struct Object;
struct Scene;
struct Nurb;
struct ListBase;
struct BezTriple;
struct BevList;

#define KNOTSU(nu)	    ( (nu)->orderu+ (nu)->pntsu+ (((nu)->flagu & CU_NURB_CYCLIC) ? (nu->orderu-1) : 0) )
#define KNOTSV(nu)	    ( (nu)->orderv+ (nu)->pntsv+ (((nu)->flagv & CU_NURB_CYCLIC) ? (nu->orderv-1) : 0) )

/* Non cyclic nurbs have 1 less segment */
#define SEGMENTSU(nu)	    ( ((nu)->flagu & CU_NURB_CYCLIC) ? (nu)->pntsu : (nu)->pntsu-1 )
#define SEGMENTSV(nu)	    ( ((nu)->flagv & CU_NURB_CYCLIC) ? (nu)->pntsv : (nu)->pntsv-1 )

#define CU_DO_TILT(cu, nu) (((nu->flag & CU_2D) && (cu->flag & CU_3D)==0) ? 0 : 1)
#define CU_DO_RADIUS(cu, nu) ((CU_DO_TILT(cu, nu) || cu->bevobj || cu->ext1!=0.0 || cu->ext2!=0.0) ? 1:0)


void unlink_curve( struct Curve *cu);
void free_curve( struct Curve *cu);
void BKE_free_editfont(struct Curve *cu);
struct Curve *add_curve(char *name, int type);
struct Curve *copy_curve( struct Curve *cu);
void make_local_curve( struct Curve *cu);
short curve_type( struct Curve *cu);
void test_curve_type( struct Object *ob);
void tex_space_curve( struct Curve *cu);
int count_curveverts( struct ListBase *nurb);
int count_curveverts_without_handles( struct ListBase *nurb);
void freeNurb( struct Nurb *nu);
void freeNurblist( struct ListBase *lb);
struct Nurb *duplicateNurb( struct Nurb *nu);
void duplicateNurblist( struct ListBase *lb1,  struct ListBase *lb2);
void test2DNurb( struct Nurb *nu);
void minmaxNurb( struct Nurb *nu, float *min, float *max);

void makeknots( struct Nurb *nu, short uv);

void makeNurbfaces(struct Nurb *nu, float *coord_array, int rowstride);
void makeNurbcurve(struct Nurb *nu, float *coord_array, float *tilt_array, float *radius_array, int resolu, int stride);
void forward_diff_bezier(float q0, float q1, float q2, float q3, float *p, int it, int stride);
float *make_orco_curve(struct Scene *scene, struct Object *ob);
float *make_orco_surf( struct Object *ob);
void makebevelcurve(struct Scene *scene, struct Object *ob,  struct ListBase *disp, int forRender);

void makeBevelList( struct Object *ob);

void calchandleNurb( struct BezTriple *bezt, struct BezTriple *prev,  struct BezTriple *next, int mode);
void calchandlesNurb( struct Nurb *nu);
void testhandlesNurb( struct Nurb *nu);
void autocalchandlesNurb( struct Nurb *nu, int flag);
void autocalchandlesNurb_all(ListBase *editnurb, int flag);
void sethandlesNurb(ListBase *editnurb, short code);

void switchdirectionNurb( struct Nurb *nu);

void addNurbPoints(struct Nurb *nu, int number);
void addNurbPointsBezier(struct Nurb *nu, int number);

float (*curve_getVertexCos(struct Curve *cu, struct ListBase *lb, int *numVerts_r))[3];
void curve_applyVertexCos(struct Curve *cu, struct ListBase *lb, float (*vertexCos)[3]);

float (*curve_getKeyVertexCos(struct Curve *cu, struct ListBase *lb, float *key))[3];
void curve_applyKeyVertexTilts(struct Curve *cu, struct ListBase *lb, float *key);

/* nurb checks if they can be drawn, also clamp order func */
int check_valid_nurb_u( struct Nurb *nu);
int check_valid_nurb_v( struct Nurb *nu);

int clamp_nurb_order_u( struct Nurb *nu);
int clamp_nurb_order_v( struct Nurb *nu);

#endif

