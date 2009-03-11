#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "bmesh_private.h"

/*
 *
 * BME POLYGON.C
 *
 * This file contains code for dealing
 * with polygons (normal/area calculation,
 * tesselation, ect)
 *
 * TODO:
 *   -Add in Tesselator frontend that creates
 *     BMTriangles from copied faces
 *  -Add in Function that checks for and flags
 *   degenerate faces.
 *
*/

/*
 * TEST EDGE SIDE and POINT IN TRIANGLE
 *
 * Point in triangle tests stolen from scanfill.c.
 * Used for tesselator
 *
*/

static short testedgeside(double *v1, double *v2, double *v3)
/* is v3 to the right of v1-v2 ? With exception: v3==v1 || v3==v2 */
{
	double inp;

	//inp= (v2[cox]-v1[cox])*(v1[coy]-v3[coy]) +(v1[coy]-v2[coy])*(v1[cox]-v3[cox]);
	inp= (v2[0]-v1[0])*(v1[1]-v3[1]) +(v1[1]-v2[1])*(v1[0]-v3[0]);

	if(inp<0.0) return 0;
	else if(inp==0) {
		if(v1[0]==v3[0] && v1[1]==v3[1]) return 0;
		if(v2[0]==v3[0] && v2[1]==v3[1]) return 0;
	}
	return 1;
}

static int point_in_triangle(double *v1, double *v2, double *v3, double *pt)
{
	if(testedgeside(v1,v2,pt) && testedgeside(v2,v3,pt) && testedgeside(v3,v1,pt))
		return 1;
	return 0;
}

/*
 * CONVEX ANGLE 
 *
 * Tests whether or not a given angle in
 * a polygon is convex or not. Note that 
 * this assumes that the polygon has been
 * projected to the x/y plane
 *
*/
static int convexangle(float *v1t, float *v2t, float *v3t)
{
	float v1[3], v3[3], n[3];
	VecSubf(v1, v1t, v2t);
	VecSubf(v3, v3t, v2t);

	Normalize(v1);
	Normalize(v3);
	Crossf(n, v1, v3);
	
	if(n[2] < 0.0)
		return 0;

	return 1;
}

/*
 * COMPUTE POLY NORMAL
 *
 * Computes the normal of a planar 
 * polygon See Graphics Gems for 
 * computing newell normal.
 *
*/
static void compute_poly_normal(float normal[3], float (*verts)[3], int nverts)
{

	float *u,  *v;/*, *w, v1[3], v2[3];*/
	double n[3] = {0.0, 0.0, 0.0}, l;
	int i;

	for(i = 0; i < nverts; i++){
		u = verts[i];
		v = verts[(i+1) % nverts];
		/*w = verts[(i+2) % nverts];

		VecSubf(v1, u, v);
		VecSubf(v2, w, v);
		Crossf(normal, v1, v2);
		Normalize(normal);
		
		return;*/
		
		/* newell's method
		
		so thats?:
		(a[1] - b[1]) * (a[2] + b[2]);
		a[1]*b[2] - b[1]*a[2] - b[1]*b[2] + a[1]*a[2]

		odd.  half of that is the cross product. . .what's the
		other half?

		also could be like a[1]*(b[2] + a[2]) - b[1]*(a[2] - b[2])
		*/

		n[0] += (u[1] - v[1]) * (u[2] + v[2]);
		n[1] += (u[2] - v[2]) * (u[0] + v[0]);
		n[2] += (u[0] - v[0]) * (u[1] + v[1]);
	}
	
	l = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
	n[0] /= l;
	n[1] /= l;
	n[2] /= l;
	
	normal[0] = n[0];
	normal[1] = n[1];
	normal[2] = n[2];
}

/*
 * COMPUTE POLY CENTER
 *
 * Computes the centroid and
 * area of a polygon in the X/Y
 * plane.
 *
*/

static int compute_poly_center(float center[3], float *area, float (*verts)[3], int nverts)
{
	int i, j;
	float atmp = 0.0, xtmp = 0.0, ytmp = 0.0, ai;
	
	center[0] = center[1] = center[2] = 0.0;	

	if(nverts < 3) 
		return 0;

	i = nverts-1;
	j = 0;
	
	while(j < nverts){
		ai = verts[i][0] * verts[j][1] - verts[j][0] * verts[i][1];				
		atmp += ai;
		xtmp += (verts[j][0] + verts[i][0]) * ai;
		ytmp += (verts[j][1] + verts[i][1]) * ai;
		i = j;
		j += 1;
	}

	if(area)
		*area = atmp / 2.0f;	
	
	if (atmp != 0){
		center[0] = xtmp /  (3.0f * atmp);
		center[1] = xtmp /  (3.0f * atmp);
		return 1;
	}
	return 0;
}


/*
 * COMPUTE POLY PLANE
 *
 * Projects a set polygon's vertices to 
 * a plane defined by the average
 * of its edges cross products
 *
*/

void compute_poly_plane(float (*verts)[3], int nverts)
{
	
	float avgc[3], norm[3], temp[3], mag, avgn[3];
	float *v1, *v2, *v3;
	int i;
	
	if(nverts < 3) 
		return;

	avgn[0] = avgn[1] = avgn[2] = 0.0;
	avgc[0] = avgc[1] = avgc[2] = 0.0;

	for(i = 0; i < nverts; i++){
		v1 = verts[i];
		v2 = verts[(i+1) % nverts];
		v3 = verts[(i+2) % nverts];
		CalcNormFloat(v1, v2, v3, norm);	
	
		avgn[0] += norm[0];
		avgn[1] += norm[1];
		avgn[2] += norm[2];
	}

	/*what was this bit for?*/
	if(avgn[0] == 0.0 && avgn[1] == 0.0 && avgn[2] == 0.0){
		avgn[0] = 0.0;
		avgn[1] = 0.0;
		avgn[2] = 1.0;
	} else {
		avgn[0] /= nverts;
		avgn[1] /= nverts;
		avgn[2] /= nverts;
		Normalize(avgn);
	}
	
	for(i = 0; i < nverts; i++){
		v1 = verts[i];
		VECCOPY(temp, v1);
		mag = 0.0;
		mag += (temp[0] * avgn[0]);
		mag += (temp[1] * avgn[1]);
		mag += (temp[2] * avgn[2]);
		
		temp[0] = (avgn[0] * mag);
		temp[1] = (avgn[1] * mag);
		temp[2] = (avgn[2] * mag);

		VecSubf(v1, v1, temp);
	}	
}

/*
 * POLY ROTATE PLANE
 *
 * Rotates a polygon so that it's
 * normal is pointing towards the mesh Z axis
 *
*/

void poly_rotate_plane(float normal[3], float (*verts)[3], int nverts)
{

	float up[3] = {0.0f,0.0f,1.0f}, axis[3], q[4];
	float mat[3][3];
	double angle;
	int i;

	compute_poly_normal(normal, verts, nverts);

	Crossf(axis, up, normal);
	axis[0] *= -1;
	axis[1] *= -1;
	axis[2] *= -1;

	angle = saacos(normal[0]*up[0]+normal[1]*up[1] + normal[2]*up[2]);

	if (angle == 0.0f) return;

	AxisAngleToQuatd(q, axis, angle);
	QuatToMat3(q, mat);

	for(i = 0;  i < nverts;  i++)
		Mat3MulVecfl(mat, verts[i]);
}

/*
 * BMESH UPDATE FACE NORMAL
 *
 * Updates the stored normal for the
 * given face. Requires that a buffer
 * of sufficient length to store projected
 * coordinates for all of the face's vertices
 * is passed in as well.
 *
*/

void bmesh_update_face_normal(BMesh *bm, BMFace *f, float (*projectverts)[3])
{
	BMLoop *l;
	int i;

	if(f->len > 4){
		i = 0;
		l = f->loopbase;
		do{
			VECCOPY(projectverts[i], l->v->co);
			l = (BMLoop*)(l->head.next);
		}while(l!=f->loopbase);

		compute_poly_plane(projectverts, f->len);
		compute_poly_normal(f->no, projectverts, f->len);	
	}
	else if(f->len == 3){
		BMVert *v1, *v2, *v3;
		v1 = f->loopbase->v;
		v2 = ((BMLoop*)(f->loopbase->head.next))->v;
		v3 = ((BMLoop*)(f->loopbase->head.next->next))->v;
		CalcNormFloat(v1->co, v2->co, v3->co, f->no);
	}
	else if(f->len == 4){
		BMVert *v1, *v2, *v3, *v4;
		v1 = f->loopbase->v;
		v2 = ((BMLoop*)(f->loopbase->head.next))->v;
		v3 = ((BMLoop*)(f->loopbase->head.next->next))->v;
		v4 = ((BMLoop*)(f->loopbase->head.prev))->v;
		CalcNormFloat4(v1->co, v2->co, v3->co, v4->co, f->no);
	}
	else{ /*horrible, two sided face!*/
		f->no[0] = 0.0;
		f->no[1] = 0.0;
		f->no[2] = 1.0;
	}

}


/*
 * BMESH FLIP NORMAL
 * 
 *  Reverses the winding of a  faces
 *  Note that this does *not* update the calculated 
 *  Normal 
*/
void BM_flip_normal(BMesh *bm, BMFace *f)
{	
	bmesh_loop_reverse(bm, f);
}



int winding(double *a, double *b, double *c)
{
	double v1[3], v2[3], v[3];
	
	VECSUB(v1, b, a);
	VECSUB(v2, b, c);
	
	v1[2] = 0;
	v2[2] = 0;
	
	Normalize_d(v1);
	Normalize_d(v2);
	
	Crossd(v, v1, v2);

	/*!! (turns nonzero into 1) is likely not necassary, 
	  since '>' I *think* should always
	  return 0 or 1, but I'm not totally sure. . .*/
	return !!(v[2] > 0);
}

/* detects if two line segments cross each other (intersects).
   note, there could be more winding cases then there needs to be. */
int linecrosses(double *v1, double *v2, double *v3, double *v4)
{
	int w1, w2, w3, w4, w5;
	
	/*w1 = winding(v1, v3, v4);
	w2 = winding(v2, v3, v4);
	w3 = winding(v3, v1, v2);
	w4 = winding(v4, v1, v2);
	
	return (w1 == w2) && (w3 == w4);*/

	w1 = winding(v1, v3, v2);
	w2 = winding(v2, v4, v1);
	w3 = !winding(v1, v2, v3);
	w4 = winding(v3, v2, v4);
	w5 = !winding(v3, v1, v4);
	return w1 == w2 && w2 == w3 && w3 == w4 && w4==w5;
}

int goodline(float (*projectverts)[3], BMFace *f, int v1i,
	     int v2i, int v3i, int nvert) {
	BMLoop *l = f->loopbase;
	double v1[3], v2[3], v3[3], pv1[3], pv2[3];
	int i;

	VECCOPY(v1, projectverts[v1i]);
	VECCOPY(v2, projectverts[v2i]);
	VECCOPY(v3, projectverts[v3i]);
	
	if (testedgeside(v1, v2, v3)) return 0;
	
	//for (i=0; i<nvert; i++) {
	do {
		i = l->v->head.eflag2;
		if (i == v1i || i == v2i || i == v3i) {
			l = l->head.next;
			continue;
		}
		
		VECCOPY(pv1, projectverts[l->v->head.eflag2]);
		VECCOPY(pv2, projectverts[((BMLoop*)l->head.next)->v->head.eflag2]);
		
		//if (linecrosses(pv1, pv2, v1, v3)) return 0;
		if (point_in_triangle(v1, v2, v3, pv1)) return 0;
		if (point_in_triangle(v3, v2, v1, pv1)) return 0;

		l = l->head.next;
	} while (l != f->loopbase);
	return 1;
}
/*
 * FIND EAR
 *
 * Used by tesselator to find
 * the next triangle to 'clip off'
 * of a polygon while tesselating.
 *
*/

typedef struct quadtree {
	int *dsdf;
	int *dsfds;
} quadtree;

static BMLoop *find_ear(BMesh *bm, BMFace *f, float (*verts)[3], 
			int nvert)
{
	BMVert *v1, *v2, *v3;
	BMLoop *bestear = NULL, *l;
	float angle, bestangle = 180.0f;
	int isear, i=0;
	
	l = f->loopbase;
	do{
		isear = 1;
		
		v1 = ((BMLoop*)(l->head.prev))->v;
		v2 = l->v;
		v3 = ((BMLoop*)(l->head.next))->v;

		if (BM_Edge_Exist(v1, v3)) isear = 0;

		if (isear && !goodline(verts, f, v1->head.eflag2, v2->head.eflag2,
			               v3->head.eflag2, nvert))
			isear = 0;

		if(isear){
			angle = VecAngle3(verts[v1->head.eflag2], verts[v2->head.eflag2], verts[v3->head.eflag2]);
			if(!bestear || ABS(angle-45.0f) < bestangle){
				bestear = l;
				bestangle = ABS(45.0f-angle);
			}
			
			if (angle > 20 && angle < 90) break;
			if (angle < 100 && i > 5) break;
			i += 1;
		}
		l = (BMLoop*)(l->head.next);
	}
	while(l != f->loopbase);

	return bestear;
}

/*
 * BMESH TRIANGULATE FACE
 *
 * Triangulates a face using a 
 * simple 'ear clipping' algorithm
 * that tries to favor non-skinny
 * triangles (angles less than 
 * 90 degrees). If the triangulator
 * has bits left over (or cannot
 * triangulate at all) it uses an 
 * arbitrary triangulation.
 *
 * TODO:
 * -Modify this to try and find ears that will not create a non-manifold face after conversion back to editmesh
 *
*/
void BM_Triangulate_Face(BMesh *bm, BMFace *f, float (*projectverts)[3], 
			 int newedgeflag, int newfaceflag)
{
	int i, done, nvert;
	float no[3];
	BMLoop *l, *newl, *nextloop;
	BMVert *v;

	/*copy vertex coordinates to vertspace array*/
	i = 0;
	l = f->loopbase;
	do{
		VECCOPY(projectverts[i], l->v->co);
		l->v->head.eflag2 = i; /*warning, abuse! never duplicate in tools code! never you hear?*/ /*actually, get rid of this completely, use a new structure for this....*/
		i++;
		l = (BMLoop*)(l->head.next);
	}while(l != f->loopbase);
	
	///bmesh_update_face_normal(bm, f, projectverts);

	/*this fixes some weird numerical error*/
	projectverts[0][0] += 0.0001f;
	projectverts[0][1] += 0.0001f;
	projectverts[0][2] += 0.0001f;

	compute_poly_normal(f->no, projectverts, f->len);
	poly_rotate_plane(f->no, projectverts, i);

	nvert = f->len;

	//compute_poly_plane(projectverts, i);
	for (i=0; i<nvert; i++) {
		projectverts[i][2] = 0.0f;
	}

	done = 0;
	while(!done && f->len > 3){
		done = 1;
		l = find_ear(bm, f, projectverts, nvert);
		if(l) {
			done = 0;
			v = l->v;
			f = BM_Split_Face(bm, l->f, ((BMLoop*)(l->head.prev))->v, 
			                  ((BMLoop*)(l->head.next))->v, 
			                  &newl, NULL, 0);
			if (!f) {
				printf("yeek! triangulator failed to split face!\n");
				break;
			}

			BMO_SetFlag(bm, newl->e, newedgeflag);
			BMO_SetFlag(bm, f, newfaceflag);

			/*l = f->loopbase;
			do {
				if (l->v == v) {
					f->loopbase = l;
					break;
				}
				l = l->head.next;
			} while (l != f->loopbase);*/
		}
	}

	if (f->len > 3){
		l = f->loopbase;
		while (l->f->len > 3){
			nextloop = ((BMLoop*)(l->head.next->next));
			f = BM_Split_Face(bm, l->f, l->v, nextloop->v, 
			                  &newl, NULL, 0);
			if (!f) {
				printf("triangle fan step of triangulator failed.\n");
				return;
			}

			BMO_SetFlag(bm, newl->e, newedgeflag);
			BMO_SetFlag(bm, f, newfaceflag);
			l = nextloop;
		}
	}
}
