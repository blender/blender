#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "BKE_utildefines.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"

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
 * BMESH_TODO:
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

static short testedgesidef(float *v1, float *v2, float *v3)
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
 * COMPUTE POLY NORMAL
 *
 * Computes the normal of a planar 
 * polygon See Graphics Gems for 
 * computing newell normal.
 *
*/
#define FEQ(f1, f2) (ABS((double)(f1)-(double)(f2)) < 0.1)

static void compute_poly_normal(float normal[3], float (*verts)[3], int nverts)
{

	float u[3],  v[3], w[3];/*, *w, v1[3], v2[3];*/
	float n[3] = {0.0, 0.0, 0.0} /*, l, v1[3], v2[3] */;
	/* double l2; */
	int i /*, s=0 */;

	/*this fixes some weird numerical error*/
	verts[0][0] += 0.0001f;
	verts[0][1] += 0.0001f;
	verts[0][2] += 0.0001f;

	for(i = 0; i < nverts; i++){
		copy_v3_v3(u, verts[i]);
		copy_v3_v3(v, verts[(i+1) % nverts]);
		copy_v3_v3(w, verts[(i+2) % nverts]);
		
#if 0
		VECSUB(v1, w, v);
		VECSUB(v2, v, u);
		Normalize_d(v1);
		Normalize_d(v2);

		l = INPR(v1, v2);
		if (ABS(l-1.0) < 0.1)
			continue;
#endif
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

	if(normalize_v3_v3(normal, n) == 0.0f) {
		normal[2] = 1.0f; /* other axis set to 0.0 */
	}

#if 0
	l = n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
	l = sqrt(l);
	/*fast square root, newton/babylonian method:
	l2 = l*0.1;

	l2 = (l/l2 + l2)*0.5;
	l2 = (l/l2 + l2)*0.5;
	l2 = (l/l2 + l2)*0.5;
	*/

	if (l == 0.0) {
		normal[0] = 0.0f;
		normal[1] = 0.0f;
		normal[2] = 1.0f;

		return;
	} else l = 1.0f / l;

	n[0] *= l;
	n[1] *= l;
	n[2] *= l;
	
	normal[0] = (float) n[0];
	normal[1] = (float) n[1];
	normal[2] = (float) n[2];
#endif
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

float BM_face_area(BMFace *f)
{
	BMLoop *l;
	BMIter iter;
	float (*verts)[3], stackv[100][3];
	float area, center[3];
	int i;

	if (f->len <= 100) 
		verts = stackv;
	else verts = MEM_callocN(sizeof(float)*f->len*3, "bm_face_area tmp");

	i = 0;
	BM_ITER(l, &iter, NULL, BM_LOOPS_OF_FACE, f) {
		copy_v3_v3(verts[i], l->v->co);
		i++;
	}

	compute_poly_center(center, &area, verts, f->len);

	if (f->len > 100)
		MEM_freeN(verts);

	return area;
}
/*
computes center of face in 3d.  uses center of bounding box.
*/

int BM_Compute_Face_Center(BMesh *bm, BMFace *f, float center[3])
{
	BMIter iter;
	BMLoop *l;
	float min[3], max[3];
	int i;

	INIT_MINMAX(min, max);
	l = BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, f);
	for (i=0; l; l=BMIter_Step(&iter), i++) {
		DO_MINMAX(l->v->co, min, max);
	}

	mid_v3_v3v3(center, min, max);
	
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
		normal_tri_v3( norm,v1, v2, v3);	

		add_v3_v3(avgn, norm);
	}

	/*what was this bit for?*/
	if(avgn[0] == 0.0f && avgn[1] == 0.0f && avgn[2] == 0.0f) {
		avgn[0] = 0.0f;
		avgn[1] = 0.0f;
		avgn[2] = 1.0f;
	} else {
		/* XXX, why is this being divided and _then_ normalized
		 * division could be removed? - campbell */
		avgn[0] /= nverts;
		avgn[1] /= nverts;
		avgn[2] /= nverts;
		normalize_v3(avgn);
	}
	
	for(i = 0; i < nverts; i++){
		v1 = verts[i];
		copy_v3_v3(temp, v1);
		mag = 0.0;
		mag += (temp[0] * avgn[0]);
		mag += (temp[1] * avgn[1]);
		mag += (temp[2] * avgn[2]);
		
		temp[0] = (avgn[0] * mag);
		temp[1] = (avgn[1] * mag);
		temp[2] = (avgn[2] * mag);

		sub_v3_v3v3(v1, v1, temp);
	}	
}

/*
  BM LEGAL EDGES

  takes in a face and a list of edges, and sets to NULL any edge in
  the list that bridges a concave region of the face or intersects
  any of the faces's edges.
*/
#if 0 /* needs BLI math double versions of these functions */
static void shrink_edged(double *v1, double *v2, double fac)
{
	double mid[3];

	mid_v3_v3v3(mid, v1, v2);

	sub_v3_v3v3(v1, v1, mid);
	sub_v3_v3v3(v2, v2, mid);

	mul_v3_fl(v1, fac);
	mul_v3_fl(v2, fac);

	add_v3_v3v3(v1, v1, mid);
	add_v3_v3v3(v2, v2, mid);
}
#endif

static void shrink_edgef(float *v1, float *v2, float fac)
{
	float mid[3];

	mid_v3_v3v3(mid, v1, v2);

	sub_v3_v3v3(v1, v1, mid);
	sub_v3_v3v3(v2, v2, mid);

	mul_v3_fl(v1, fac);
	mul_v3_fl(v2, fac);

	add_v3_v3v3(v1, v1, mid);
	add_v3_v3v3(v2, v2, mid);
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

	cross_v3_v3v3(axis, normal, up);

	angle = saacos(dot_v3v3(normal, up));

	if (angle == 0.0) return;

	axis_angle_to_quat(q, axis, (float)angle);
	quat_to_mat3(mat, q);

	for(i = 0;  i < nverts;  i++)
		mul_m3_v3(mat, verts[i]);
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

void BM_Face_UpdateNormal(BMesh *bm, BMFace *f)
{
	float projverts[200][3];
	float (*proj)[3] = f->len < 200 ? projverts : MEM_mallocN(sizeof(float)*f->len*3, "projvertsn");
	BMIter iter;
	BMLoop *l;
	int i=0;

	if (f->len < 3) return;
	
	BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
		copy_v3_v3(proj[i], l->v->co);
		i += 1;
	}

	bmesh_update_face_normal(bm, f, proj);

	if (projverts != proj) MEM_freeN(proj);
}

void BM_Edge_UpdateNormals(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMFace *f;
	
	f = BMIter_New(&iter, bm, BM_FACES_OF_EDGE, e);
	for (; f; f=BMIter_Step(&iter)) {
		BM_Face_UpdateNormal(bm, f);
	}

	BM_Vert_UpdateNormal(bm, e->v1);
	BM_Vert_UpdateNormal(bm, e->v2);
}

void BM_Vert_UpdateNormal(BMesh *bm, BMVert *v)
{
	BMIter iter;
	BMFace *f;
	int len=0;

	v->no[0] = v->no[1] = v->no[2] = 0.0f;

	f = BMIter_New(&iter, bm, BM_FACES_OF_VERT, v);
	for (; f; f=BMIter_Step(&iter), len++) {
		add_v3_v3v3(v->no, f->no, v->no);
	}

	if (!len) return;

	mul_v3_fl(v->no, 1.0f/(float)len);
}

void BM_Vert_UpdateAllNormals(BMesh *bm, BMVert *v)
{
	BMIter iter;
	BMFace *f;
	int len=0;

	v->no[0] = v->no[1] = v->no[2] = 0.0f;

	f = BMIter_New(&iter, bm, BM_FACES_OF_VERT, v);
	for (; f; f=BMIter_Step(&iter), len++) {
		BM_Face_UpdateNormal(bm, f);
		add_v3_v3v3(v->no, f->no, v->no);
	}

	if (!len) return;

	mul_v3_fl(v->no, 1.0f/(float)len);
}

void bmesh_update_face_normal(BMesh *bm, BMFace *f, float (*projectverts)[3])
{
	BMIter iter;
	BMLoop *l;
	int i;

	if(f->len > 4) {
		i = 0;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			copy_v3_v3(projectverts[i], l->v->co);
			l = l->next;
			i += 1;
		}

		compute_poly_normal(f->no, projectverts, f->len);	
	}
	else if(f->len == 3){
		BMVert *v1, *v2, *v3;
		v1 = bm_firstfaceloop(f)->v;
		v2 = bm_firstfaceloop(f)->next->v;
		v3 = bm_firstfaceloop(f)->next->next->v;
		normal_tri_v3( f->no,v1->co, v2->co, v3->co);
	}
	else if(f->len == 4){
		BMVert *v1, *v2, *v3, *v4;
		v1 = bm_firstfaceloop(f)->v;
		v2 = bm_firstfaceloop(f)->next->v;
		v3 = bm_firstfaceloop(f)->next->next->v;
		v4 = bm_firstfaceloop(f)->prev->v;
		normal_quad_v3( f->no,v1->co, v2->co, v3->co, v4->co);
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
 *  Reverses the winding of a face.
 *  Note that this updates the calculated 
 *  normal.
*/
void BM_flip_normal(BMesh *bm, BMFace *f)
{	
	bmesh_loop_reverse(bm, f);
	BM_Face_UpdateNormal(bm, f);
}

/* detects if two line segments cross each other (intersects).
   note, there could be more winding cases then there needs to be. */
static int linecrosses(double *v1, double *v2, double *v3, double *v4)
{
	int w1, w2, w3, w4, w5;
	
	/*w1 = winding(v1, v3, v4);
	w2 = winding(v2, v3, v4);
	w3 = winding(v3, v1, v2);
	w4 = winding(v4, v1, v2);
	
	return (w1 == w2) && (w3 == w4);*/

	w1 = testedgeside(v1, v3, v2);
	w2 = testedgeside(v2, v4, v1);
	w3 = !testedgeside(v1, v2, v3);
	w4 = testedgeside(v3, v2, v4);
	w5 = !testedgeside(v3, v1, v4);
	return w1 == w2 && w2 == w3 && w3 == w4 && w4==w5;
}

/* detects if two line segments cross each other (intersects).
   note, there could be more winding cases then there needs to be. */
static int linecrossesf(float *v1, float *v2, float *v3, float *v4)
{
	int w1, w2, w3, w4, w5 /*, ret*/;
	float mv1[2], mv2[2], mv3[2], mv4[2];
	
	/*now test winding*/
	w1 = testedgesidef(v1, v3, v2);
	w2 = testedgesidef(v2, v4, v1);
	w3 = !testedgesidef(v1, v2, v3);
	w4 = testedgesidef(v3, v2, v4);
	w5 = !testedgesidef(v3, v1, v4);
	
	if (w1 == w2 && w2 == w3 && w3 == w4 && w4==w5)
		return 1;
	
#define GETMIN2_AXIS(a, b, ma, mb, axis) ma[axis] = MIN2(a[axis], b[axis]), mb[axis] = MAX2(a[axis], b[axis])
#define GETMIN2(a, b, ma, mb) GETMIN2_AXIS(a, b, ma, mb, 0); GETMIN2_AXIS(a, b, ma, mb, 1);
	
	GETMIN2(v1, v2, mv1, mv2);
	GETMIN2(v3, v4, mv3, mv4);
	
	/*do an interval test on the x and y axes*/
	/*first do x axis*/
	#define T FLT_EPSILON*15
	if (ABS(v1[1]-v2[1]) < T && ABS(v3[1]-v4[1]) < T &&
	    ABS(v1[1]-v3[1]) < T) 
	{
		return (mv4[0] >= mv1[0] && mv3[0] <= mv2[0]);
	}

	/*now do y axis*/
	if (ABS(v1[0]-v2[0]) < T && ABS(v3[0]-v4[0]) < T &&
	    ABS(v1[0]-v3[0]) < T)
	{
		return (mv4[1] >= mv1[1] && mv3[1] <= mv2[1]);
	}

	return 0; 
}

/*
   BM POINT IN FACE
   
  Projects co onto face f, and returns true if it is inside
  the face bounds.  Note that this uses a best-axis projection
  test, instead of projecting co directly into f's orientation
  space, so there might be accuracy issues.
*/
int BM_Point_In_Face(BMesh *bm, BMFace *f, float co[3])
{
	int xn, yn, zn, ax, ay;
	float co2[3], cent[3] = {0.0f, 0.0f, 0.0f}, out[3] = {FLT_MAX*0.5f, FLT_MAX*0.5f, 0};
	BMLoop *l;
	int crosses = 0;
	float eps = 1.0f+(float)FLT_EPSILON*150.0f;
	
	if (dot_v3v3(f->no, f->no) <= FLT_EPSILON*10)
		BM_Face_UpdateNormal(bm, f);
	
	/* find best projection of face XY, XZ or YZ: barycentric weights of
	   the 2d projected coords are the same and faster to compute
	   
	   this probably isn't all that accurate, but it has the advantage of
	   being fast (especially compared to projecting into the face orientation)
	*/
	xn= (float)fabs(f->no[0]);
	yn= (float)fabs(f->no[1]);
	zn= (float)fabs(f->no[2]);
	if(zn>=xn && zn>=yn) {ax= 0; ay= 1;}
	else if(yn>=xn && yn>=zn) {ax= 0; ay= 2;}
	else {ax= 1; ay= 2;}

	co2[0] = co[ax];
	co2[1] = co[ay];
	co2[2] = 0;
	
	l = bm_firstfaceloop(f);
	do {
		cent[0] += l->v->co[ax];
		cent[1] += l->v->co[ay];
		l = l->next;
	} while (l != bm_firstfaceloop(f));
	
	mul_v2_fl(cent, 1.0f/(float)f->len);
	
	l = bm_firstfaceloop(f);
	do {
		float v1[3], v2[3];
		
		v1[0] = (l->prev->v->co[ax] - cent[ax])*eps + cent[ax];
		v1[1] = (l->prev->v->co[ay] - cent[ay])*eps + cent[ay];
		v1[2] = 0.0f;
		
		v2[0] = (l->v->co[ax] - cent[ax])*eps + cent[ax];
		v2[1] = (l->v->co[ay] - cent[ay])*eps + cent[ay];
		v2[2] = 0.0f;
		
		crosses += linecrossesf(v1, v2, co2, out) != 0;
		
		l = l->next;
	} while (l != bm_firstfaceloop(f));
	
	return crosses%2 != 0;
}

static int goodline(float (*projectverts)[3], BMFace *f, int v1i,
		 int v2i, int v3i, int UNUSED(nvert)) {
	BMLoop *l = bm_firstfaceloop(f);
	double v1[3], v2[3], v3[3], pv1[3], pv2[3];
	int i;

	VECCOPY(v1, projectverts[v1i]);
	VECCOPY(v2, projectverts[v2i]);
	VECCOPY(v3, projectverts[v3i]);
	
	if (testedgeside(v1, v2, v3)) return 0;
	
	//for (i=0; i<nvert; i++) {
	do {
		i = BM_GetIndex(l->v);
		if (i == v1i || i == v2i || i == v3i) {
			l = (BMLoop*)l->next;
			continue;
		}
		
		VECCOPY(pv1, projectverts[BM_GetIndex(l->v)]);
		VECCOPY(pv2, projectverts[BM_GetIndex(((BMLoop*)l->next)->v)]);
		
		//if (linecrosses(pv1, pv2, v1, v3)) return 0;
		if (point_in_triangle(v1, v2, v3, pv1)) return 0;
		if (point_in_triangle(v3, v2, v1, pv1)) return 0;

		l = (BMLoop*)l->next;
	} while (l != bm_firstfaceloop(f));
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

static BMLoop *find_ear(BMesh *UNUSED(bm), BMFace *f, float (*verts)[3],
			int nvert)
{
	BMVert *v1, *v2, *v3;
	BMLoop *bestear = NULL, *l;
	/*float angle, bestangle = 180.0f;*/
	int isear /*, i=0*/;
	
	l = bm_firstfaceloop(f);
	do {
		isear = 1;
		
		v1 = ((BMLoop*)(l->prev))->v;
		v2 = l->v;
		v3 = ((BMLoop*)(l->next))->v;

		if (BM_Edge_Exist(v1, v3)) isear = 0;

		if (isear && !goodline(verts, f, BM_GetIndex(v1), BM_GetIndex(v2),
			               BM_GetIndex(v3), nvert))
			isear = 0;
		
		if(isear) {
			/*angle = angle_v3v3v3(verts[v1->head.eflag2], verts[v2->head.eflag2], verts[v3->head.eflag2]);
			if(!bestear || ABS(angle-45.0f) < bestangle) {
				bestear = l;
				bestangle = ABS(45.0f-angle);
			}
			
			if (angle > 20 && angle < 90) break;
			if (angle < 100 && i > 5) break;
			i += 1;*/
			bestear = l;
			break;
		}
		l = (BMLoop*)(l->next);
	}
	while(l != bm_firstfaceloop(f));

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
 * triangulate at all) it uses a
 * simple fan triangulation
 *
 * newfaces, if non-null, must be an array of BMFace pointers,
 * with a length equal to f->len.  it will be filled with the new
 * triangles, and will be NULL-terminated.
*/
void BM_Triangulate_Face(BMesh *bm, BMFace *f, float (*projectverts)[3], 
                         int newedgeflag, int newfaceflag, BMFace **newfaces)
{
	int i, done, nvert, nf_i = 0;
	BMLoop *l, *newl, *nextloop;
	BMVert *v;

	/*copy vertex coordinates to vertspace array*/
	i = 0;
	l = bm_firstfaceloop(f);
	do{
		copy_v3_v3(projectverts[i], l->v->co);
		BM_SetIndex(l->v, i);
		i++;
		l = (BMLoop*)(l->next);
	}while(l != bm_firstfaceloop(f));
	
	///bmesh_update_face_normal(bm, f, projectverts);

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
			f = BM_Split_Face(bm, l->f, ((BMLoop*)(l->prev))->v, 
			                  ((BMLoop*)(l->next))->v, 
			                  &newl, NULL);
			copy_v3_v3(f->no, l->f->no);

			if (!f) {
				printf("yeek! triangulator failed to split face!\n");
				break;
			}

			BMO_SetFlag(bm, newl->e, newedgeflag);
			BMO_SetFlag(bm, f, newfaceflag);
			
			if (newfaces) newfaces[nf_i++] = f;

			/*l = f->loopbase;
			do {
				if (l->v == v) {
					f->loopbase = l;
					break;
				}
				l = l->next;
			} while (l != f->loopbase);*/
		}
	}

	if (f->len > 3){
		l = bm_firstfaceloop(f);
		while (l->f->len > 3){
			nextloop = ((BMLoop*)(l->next->next));
			f = BM_Split_Face(bm, l->f, l->v, nextloop->v, 
			                  &newl, NULL);
			if (!f) {
				printf("triangle fan step of triangulator failed.\n");

				/*NULL-terminate*/
				if (newfaces) newfaces[nf_i] = NULL;
				return;
			}

			if (newfaces) newfaces[nf_i++] = f;
			
			BMO_SetFlag(bm, newl->e, newedgeflag);
			BMO_SetFlag(bm, f, newfaceflag);
			l = nextloop;
		}
	}
	
	/*NULL-terminate*/
	if (newfaces) newfaces[nf_i] = NULL;
}

/*each pair of loops defines a new edge, a split.  this function goes
  through and sets pairs that are geometrically invalid to null.  a
  split is invalid, if it forms a concave angle or it intersects other
  edges in the face, or it intersects another split.  in the case of
  intersecting splits, only the first of the set of intersecting
  splits survives.*/
void BM_LegalSplits(BMesh *bm, BMFace *f, BMLoop *(*loops)[2], int len)
{
	BMIter iter;
	BMLoop *l;
	float v1[3], v2[3], v3[3]/*, v4[3]*/, no[3], mid[3], *p1, *p2, *p3, *p4;
	float out[3] = {-234324.0f, -234324.0f, 0.0f};
	float projectverts[100][3];
	float edgevertsstack[200][3];
	float (*projverts)[3] = projectverts;
	float (*edgeverts)[3] = edgevertsstack;
	float fac1 = 1.0000001f, fac2 = 0.9f; //9999f; //0.999f;
	int i, j, a=0, clen;

	if (f->len > 100) projverts = MEM_mallocN(sizeof(float)*3*f->len, "projvertsb");
	if (len > 100) edgeverts = MEM_mallocN(sizeof(float)*3*2*len, "edgevertsb");
	
	i = 0;
	l = BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, f);
	for (; l; l=BMIter_Step(&iter)) {
		BM_SetIndex(l, i);
		copy_v3_v3(projverts[i], l->v->co);
		i++;
	}
	
	for (i=0; i<len; i++) {
		copy_v3_v3(v1, loops[i][0]->v->co);
		copy_v3_v3(v2, loops[i][1]->v->co);

		shrink_edgef(v1, v2, fac2);
		
		copy_v3_v3(edgeverts[a], v1);
		a++;
		copy_v3_v3(edgeverts[a], v2);
		a++;
	}
	
	compute_poly_normal(no, projverts, f->len);
	poly_rotate_plane(no, projverts, f->len);
	poly_rotate_plane(no, edgeverts, len*2);
	
	l = bm_firstfaceloop(f);
	for (i=0; i<f->len; i++) {
		p1 = projverts[i];
		out[0] = MAX2(out[0], p1[0]) + 0.01f;
		out[1] = MAX2(out[1], p1[1]) + 0.01f;
		out[2] = 0.0f;
		p1[2] = 0.0f;

		//copy_v3_v3(l->v->co, p1);

		l = (BMLoop*) l->next;
	}
	
	for (i=0; i<len; i++) {
		edgeverts[i*2][2] = 0.0f;
		edgeverts[i*2+1][2] = 0.0f;
	}

	/*do convexity test*/
	for (i=0; i<len; i++) {
		copy_v3_v3(v2, edgeverts[i*2]);
		copy_v3_v3(v3, edgeverts[i*2+1]);

		mid_v3_v3v3(mid, v2, v3);
		
		clen = 0;
		for (j=0; j<f->len; j++) {
			p1 = projverts[j];
			p2 = projverts[(j+1)%f->len];
			
			copy_v3_v3(v1, p1);
			copy_v3_v3(v2, p2);

			shrink_edgef(v1, v2, fac1);

			if (linecrossesf(p1, p2, mid, out)) clen++;
		}
		
		if (clen%2 == 0) {
			loops[i][0] = NULL;
		}
	}
	
	/*do line crossing tests*/
	for (i=0; i<f->len; i++) {
		p1 = projverts[i];
		p2 = projverts[(i+1)%f->len];
		
		copy_v3_v3(v1, p1);
		copy_v3_v3(v2, p2);

		shrink_edgef(v1, v2, fac1);

		for (j=0; j<len; j++) {
			if (!loops[j][0]) continue;

			p3 = edgeverts[j*2];
			p4 = edgeverts[j*2+1];

			if (linecrossesf(v1, v2, p3, p4))
			{
				loops[j][0] = NULL;
			}
		}
	}

	for (i=0; i<len; i++) {
		for (j=0; j<len; j++) {
			if (j == i) continue;
			if (!loops[i][0]) continue;
			if (!loops[j][0]) continue;

			p1 = edgeverts[i*2];
			p2 = edgeverts[i*2+1];
			p3 = edgeverts[j*2];
			p4 = edgeverts[j*2+1];

			copy_v3_v3(v1, p1);
			copy_v3_v3(v2, p2);

			shrink_edgef(v1, v2, fac1);

			if (linecrossesf(v1, v2, p3, p4)) {
				loops[i][0]=NULL;
			}
		}
	}
	
	if (projverts != projectverts) MEM_freeN(projverts);
	if (edgeverts != edgevertsstack) MEM_freeN(edgeverts);
}
