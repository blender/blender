#include "bmesh.h"
#include "bmesh_private.h"

#include "BLI_arithb.h"
#include "BKE_utildefines.h"
#include <string.h>

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

static short testedgeside(float *v1, float *v2, float *v3)
/* is v3 to the right of v1-v2 ? With exception: v3==v1 || v3==v2 */
{
	float inp;

	//inp= (v2[cox]-v1[cox])*(v1[coy]-v3[coy]) +(v1[coy]-v2[coy])*(v1[cox]-v3[cox]);
	inp= (v2[0]-v1[0])*(v1[1]-v3[1]) +(v1[1]-v2[1])*(v1[0]-v3[0]);

	if(inp<0.0) return 0;
	else if(inp==0) {
		if(v1[0]==v3[0] && v1[1]==v3[1]) return 0;
		if(v2[0]==v3[0] && v2[1]==v3[1]) return 0;
	}
	return 1;
}

static int point_in_triangle(float *v1, float *v2, float *v3, float *pt)
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

	float *u,  *v;
	int i;

	normal[0] = 0.0;
	normal[1] = 0.0;
	normal[2] = 0.0;
	
	for(i = 0; i < nverts; i++){
		u = verts[i];
		v = verts[(i+1) % nverts];
		
		normal[0] += (u[1] - v[1]) * (u[2] + v[2]);
		normal[1] += (u[2] - v[2]) * (u[0] + v[0]);
		normal[2] += (u[0] - v[0]) * (u[1] + v[1]);
		i++;
	}

	Normalize(normal);
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

	float up[3] = {0.0,0.0,1.0}, axis[3], angle, q[4];
	float mat[3][3];
	int i;

	Crossf(axis, up, normal);
	angle = VecAngle2(normal, up);

	if (angle == 0.0) return;
	
	AxisAngleToQuat(q, axis, angle);
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



int winding(float *a, float *b, float *c)
{
	float v1[3], v2[3], v[3];
	
	VecSubf(v1, b, a);
	VecSubf(v2, b, c);
	
	v1[2] = 0;
	v2[2] = 0;
	
	Normalize(v1);
	Normalize(v2);
	
	Crossf(v, v1, v2);
	return !!(v[2] > 0);
}

/* detects if two line segments cross each other (intersects).
   note, there could be more winding cases then there needs to be. */
int linecrosses(float *v1, float *v2, float *v3, float *v4)
{
	int w1, w2, w3, w4, w5;

	w1 = winding(v1, v3, v2);
	w2 = winding(v2, v4, v1);
	w3 = !winding(v1, v2, v3);
	w4 = winding(v3, v2, v4);
	w5 = !winding(v3, v1, v4);
	return w1 == w2 && w2 == w3 && w3 == w4 && w4==w5;
}

int goodline(float (*projectverts)[3], int v1i, int v2i, int nvert)
{
	/*the hardcoded stuff here, 200000, 0.999, and 1.0001 may be problems
	  in the future, not sure. - joeedh*/
	static float outv[3] = {20000000.0f, 20000000.0f, 0.0f};
	float v1[3], v2[3], p[3], vv1[3], vv2[3], mid[3], a[3], b[3];
	int i = 0, lcount=0;
	
	VECCOPY(v1, projectverts[v1i])
	VECCOPY(v2, projectverts[v2i])
	
	VecAddf(p, v1, v2);
	VecMulf(p, 0.5f);
	
	VecSubf(a, v1, p);
	VecSubf(b, v2, p);
	
	VecMulf(a, 0.999f);
	VecMulf(b, 0.999f);
	
	VecAddf(v1, p, a);
	VecAddf(v2, p, b);
	
	while (i < nvert) {
		VECCOPY(vv1, projectverts[i]);
		VECCOPY(vv2, projectverts[(i+1)%nvert]);
		
		if (linecrosses(vv1, vv2, v1, v2)) return 0;

		VecAddf(mid, vv1, vv2);
		VecMulf(mid, 0.5f);
		
		VecSubf(a, vv1, mid);
		VecSubf(b, vv2, mid);
		
		VecMulf(a, 1.0001f);
		VecMulf(b, 1.0001f);
		
		VecAddf(vv1, mid, a);
		VecAddf(vv2, mid, b);
				
		if (linecrosses(vv1, vv2, p, outv)) lcount += 1;

		i += 1;
	}
	if ((lcount % 2) == 0) return 0;

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

static BMLoop *find_ear(BMFace *f, float (*verts)[3], int nvert)
{
	BMVert *v1, *v2, *v3;
	BMLoop *bestear = NULL, *l;
	float angle, bestangle = 180.0f;
	int isear;
	
	l = f->loopbase;
	do{
		isear = 1;
		
		v1 = ((BMLoop*)(l->head.prev))->v;
		v2 = l->v;
		v3 = ((BMLoop*)(l->head.next))->v;

		if (BM_Edge_Exist(v1, v3)) isear = 0;

		if (isear && !goodline(verts, v1->head.eflag2, v3->head.eflag2, nvert)) isear = 0;
		if(isear){
			angle = VecAngle3(verts[v1->head.eflag2], verts[v2->head.eflag2], verts[v3->head.eflag2]);
			if(!bestear || angle < bestangle){
				bestear = l;
				bestangle = angle;
			}
			if(angle < 90.0)
				break;
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
void BM_Triangulate_Face(BMesh *bm, BMFace *f, float (*projectverts)[3], int newedgeflag, int newfaceflag)
{
	int i, done, nvert;
	BMLoop *l, *newl, *nextloop;

	/*copy vertex coordinates to vertspace array*/
	i = 0;
	l = f->loopbase;
	do{
		VECCOPY(projectverts[i], l->v->co);
		l->v->head.eflag2 = i; /*warning, abuse! never duplicate in tools code! never you hear?*/ /*actually, get rid of this completely, use a new structure for this....*/
		i++;
		l = (BMLoop*)(l->head.next);
	}while(l != f->loopbase);
	
	bmesh_update_face_normal(bm, f, projectverts);

	compute_poly_plane(projectverts, i);
	poly_rotate_plane(f->no, projectverts, i);
	
	nvert = f->len;
	
	done = 0;
	while(!done && f->len > 3){
		done = 1;
		l = find_ear(f, projectverts, nvert);
		if(l) {
			done = 0;
			f = bmesh_sfme(bm, f, ((BMLoop*)(l->head.prev))->v, ((BMLoop*)(l->head.next))->v, &newl);
			BMO_SetFlag(bm, newl->e, newedgeflag);
			BMO_SetFlag(bm, f, newfaceflag);
		}
	}

	if (f->len > 3){
		l = f->loopbase;
		while (l->f->len > 3){
			nextloop = ((BMLoop*)(l->head.next->next));
			bmesh_sfme(bm, l->f, l->v,nextloop->v, &newl);
			BMO_SetFlag(bm, newl->e, newedgeflag);
			BMO_SetFlag(bm, f, newfaceflag);
			l = nextloop;
		}
	}
}
