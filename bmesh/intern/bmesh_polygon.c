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
static int convexangle(float *__v1, float *__v2, float *__v3)
{
	float v1[3], v3[3], n[3];
	VecSubf(v1, __v1, __v2);
	VecSubf(v3, __v3, __v2);

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
		*area = atmp / 2.0;	
	
	if (atmp != 0){
		center[0] = xtmp /  (3.0 * atmp);
		center[1] = xtmp /  (3.0 * atmp);
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
	int i;

	Crossf(axis, up, normal);
	angle = VecAngle2(normal, up);
	
	AxisAngleToQuat(q, axis, angle);
	
	for(i = 0;  i < nverts;  i++)
		QuatMulVecf(q, verts[i]);
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



/*
 * FIND EAR
 *
 * Used by tesselator to find
 * the next triangle to 'clip off'
 * of a polygon while tesselating.
 *
*/

static BMLoop *find_ear(BMFace *f, float (*verts)[3])
{
	BMVert *v1, *v2, *v3;
	BMLoop *bestear = NULL, *l, *l2;
	float angle, bestangle = 180.0f;
	int isear;
	

	l = f->loopbase;
	do{
		isear = 1;
		
		v1 = ((BMLoop*)(l->head.prev))->v;
		v2 = l->v;
		v3 = ((BMLoop*)(l->head.next))->v;
		if(convexangle(verts[v1->head.eflag2], verts[v2->head.eflag2], verts[v3->head.eflag2])){
			for(l2 = ((BMLoop*)(l->head.next->next)); l2 != ((BMLoop*)(l->head.prev)); l2 = ((BMLoop*)(l2->head.next)) ){
				if(point_in_triangle(verts[v1->head.eflag2], verts[v2->head.eflag2],verts[v3->head.eflag2], l2->v->co)){
					isear = 0;
					break;
				}
			}
		}
		if(isear){
			angle = VecAngle3(verts[v1->head.eflag2], verts[v2->head.eflag2], verts[v3->head.eflag2]);
			if((!bestear) && angle < bestangle){
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
void BM_Triangulate_Face(BMesh *bm, BMFace *f, float (*projectverts)[3])
{
	int i, done;
	BMLoop *l, *nextloop;

	/*copy vertex coordinates to vertspace array*/
	i = 0;
	l = f->loopbase;
	do{
		VECCOPY(projectverts[i], l->v->co);
		l->v->head.eflag2 = i; /*warning, abuse! never duplicate in tools code! never you hear?*/ /*actually, get rid of this completely, use a new structure for this....*/
		i++;
		l = (BMLoop*)(l->head.next);
	}while(l != f->loopbase);
	
	compute_poly_plane(projectverts, i);
	poly_rotate_plane(f->no, projectverts, i);

	done = 0;
	while(!done){
		done = 1;
		l = find_ear(f, projectverts);
		if(l){
			done = 0;
			f = bmesh_sfme(bm, f, ((BMLoop*)(l->head.prev))->v, ((BMLoop*)(l->head.next))->v, 0);
		}
	}

	if (f->len > 3){
		l = f->loopbase;
		while (l->f->len > 3){
			nextloop = ((BMLoop*)(l->head.next->next->next));
			bmesh_sfme(bm, l->f, l->v,nextloop->v, 0);
			l = nextloop;
		}
	}
}
