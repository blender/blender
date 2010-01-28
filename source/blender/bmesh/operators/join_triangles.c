#if 0
#include "MEM_guardedalloc.h"
#include "BKE_customdata.h" 
#include "DNA_listBase.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include <string.h>
#include "BKE_utildefines.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_editVert.h"
#include "mesh_intern.h"
#include "ED_mesh.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"

#include "BLI_heap.h"

#include "bmesh.h"

/*
 * JOIN_TRIANGLES.C
 *
 * utility bmesh operators, e.g. transform, 
 * translate, rotate, scale, etc.
 *
*/

/*assumes edges are validated before reaching this point*/
static float measure_facepair(BMesh *bm, BMVert *v1, BMVert *v2, 
							  BMVert *v3, BMVert *v4, float limit)
{
	/*gives a 'weight' to a pair of triangles that join an edge to decide how good a join they would make*/
	/*Note: this is more complicated than it needs to be and should be cleaned up...*/
	float no1[3], no2[3], measure = 0.0f, angle1, angle2, diff;
	float edgeVec1[3], edgeVec2[3], edgeVec3[3], edgeVec4[3];
	float minarea, maxarea, areaA, areaB;

	/*First Test: Normal difference*/
	normal_tri_v3(n1, v1->co, v2->co, v3->co);
	normal_tri_v3(n2, v1->co, v3->co, v4->co);

	if(no1[0] == no2[0] && no1[1] == no2[1] && no1[2] == no2[2]) angle1 = 0.0f;
	else angle1 = angle_v2v2(no1, no2);

	normal_tri_v3(n1, v2->co, v3->co, v4->co);
	normal_tri_v3(n2, v4->co, v1->co, v2->co);

	if(no1[0] == no2[0] && no1[1] == no2[1] && no1[2] == no2[2]) angle2 = 0.0f;
	else angle2 = angle_v2v2(no1, no2);

	measure += (angle1/360.0f) + (angle2/360.0f);
	if(measure > limit) return measure;

	/*Second test: Colinearity*/
	sub_v3_v3v3(edgeVec1, v1->co, v2->co);
	sub_v3_v3v3(edgeVec2, v2->co, v3->co);
	sub_v3_v3v3(edgeVec3, v3->co, v4->co);
	sub_v3_v3v3(edgeVec4, v4->co, v1->co);	

	diff = 0.0;

	diff = (
		fabs(angle_v2v2(edgeVec1, edgeVec2) - 90) +
		fabs(angle_v2v2(edgeVec2, edgeVec3) - 90) +
		fabs(angle_v2v2(edgeVec3, edgeVec4) - 90) +
		fabs(angle_v2v2(edgeVec4, edgeVec1) - 90)) / 360.0f;
	if(!diff) return 0.0;

	measure +=  diff;
	if(measure > limit) return measure;

	/*Third test: Concavity*/
	areaA = area_tri_v3(v1->co, v2->co, v3->co) + area_tri_v3(v1->co, v3->co, v4->co);
	areaB = area_tri_v3(v2->co, v3->co, v4->co) + area_tri_v3(v4->co, v1->co, v2->co);

	if(areaA <= areaB) minarea = areaA;
	else minarea = areaB;

	if(areaA >= areaB) maxarea = areaA;
	else maxarea = areaB;

	if(!maxarea) measure += 1;
	else measure += (1 - (minarea / maxarea));

	return measure;
}

#define T2QUV_LIMIT 0.005
#define T2QCOL_LIMIT 3

static int compareFaceAttribs(BMesh *bm, BMEdge *e)
{
	MTexPoly *tp1, *tp2;
	MLoopCol *lcol1, *lcol2, *lcol3, *lcol4;
	MLoopUV *luv1, *luv2, *luv3, *luv4;
	BMLoop *l1, *l2;
	
	l1 = e->loop
	l2 = (BMLoop*)e->loop->radial.next->data;


}

void bmesh_jointriangles_exec(BMesh *bm, BMOperator *op)
{
	
}

#endif
