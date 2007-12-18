/*  cloth.c
*
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_curve_types.h"
#include "DNA_cloth_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_linklist.h"

#include "BKE_curve.h"
#include "BKE_cloth.h"
#include "BKE_collisions.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_utildefines.h"

#include "BIF_editdeform.h"
#include "BIF_editkey.h"
#include "DNA_screen_types.h"
#include "BSE_headerbuttons.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "mydevice.h"

#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#define GL_GLEXT_LEGACY 1
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#if defined(__sun__) && !defined(__sparc__)
#include <mesa/glu.h>
#else
#include <GL/glu.h>
#endif
#endif

#ifdef _WIN32
void tstart ( void )
{}
void tend ( void )
{
}
double tval()
{
	return 0;
}
#else
#include <sys/time.h>
			 static struct timeval _tstart, _tend;
	 static struct timezone tz;
	 void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}
double tval()
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec + ( double ) _tstart.tv_usec/ ( 1000*1000 );
	t2 = ( double ) _tend.tv_sec + ( double ) _tend.tv_usec/ ( 1000*1000 );
	return t2-t1;
}
#endif

/* Our available solvers. */
// 255 is the magic reserved number, so NEVER try to put 255 solvers in here!
// 254 = MAX!
static CM_SOLVER_DEF solvers [] =
{
	{ "Implicit", CM_IMPLICIT, implicit_init, implicit_solver, implicit_free },
};

/* ********** cloth engine ******* */
/* Prototypes for internal functions.
*/
static void cloth_to_object (Object *ob,  DerivedMesh *dm, ClothModifierData *clmd);
static void cloth_from_mesh (Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float framenr);
static int cloth_from_object(Object *ob, ClothModifierData *clmd, DerivedMesh *dm, DerivedMesh *olddm, float framenr);

int cloth_build_springs ( ClothModifierData *clmd, DerivedMesh *dm );
static void cloth_apply_vgroup(ClothModifierData *clmd, DerivedMesh *dm, short vgroup);
/******************************************************************************
*
* External interface called by modifier.c clothModifier functions.
*
******************************************************************************/
/**
 * cloth_init -  creates a new cloth simulation.
 *
 * 1. create object
 * 2. fill object with standard values or with the GUI settings if given 
 */
void cloth_init (ClothModifierData *clmd)
{
	/* Initialize our new data structure to reasonable values. */
	clmd->sim_parms->gravity [0] = 0.0;
	clmd->sim_parms->gravity [1] = 0.0;
	clmd->sim_parms->gravity [2] = -9.81;
	clmd->sim_parms->structural = 100.0;
	clmd->sim_parms->shear = 100.0;
	clmd->sim_parms->bending = 1.0;
	clmd->sim_parms->Cdis = 5.0;
	clmd->sim_parms->Cvi = 1.0;
	clmd->sim_parms->mass = 1.0f;
	clmd->sim_parms->stepsPerFrame = 5;
	clmd->sim_parms->sim_time = 1.0;
	clmd->sim_parms->flags = CLOTH_SIMSETTINGS_FLAG_RESET;
	clmd->sim_parms->solver_type = 0; 
	clmd->sim_parms->preroll = 0;
	clmd->sim_parms->maxspringlen = 10;
	clmd->coll_parms->self_friction = 5.0;
	clmd->coll_parms->friction = 10.0;
	clmd->coll_parms->loop_count = 1;
	clmd->coll_parms->epsilon = 0.01;
	clmd->coll_parms->selfepsilon = 0.49;
	
	/* These defaults are copied from softbody.c's
	* softbody_calc_forces() function.
	*/
	clmd->sim_parms->eff_force_scale = 1000.0;
	clmd->sim_parms->eff_wind_scale = 250.0;

	// also from softbodies
	clmd->sim_parms->maxgoal = 1.0;
	clmd->sim_parms->mingoal = 0.0;
	clmd->sim_parms->defgoal = 0.0;
	clmd->sim_parms->goalspring = 100.0;
	clmd->sim_parms->goalfrict = 0.0;
}

// unused in the moment, cloth needs quads from mesh
DerivedMesh *CDDM_convert_to_triangle(DerivedMesh *dm)
{
	/*
	DerivedMesh *result = NULL;
	int i;
	int numverts = dm->getNumVerts(dm);
	int numedges = dm->getNumEdges(dm);
	int numfaces = dm->getNumFaces(dm);

	MVert *mvert = CDDM_get_verts(dm);
	MEdge *medge = CDDM_get_edges(dm);
	MFace *mface = CDDM_get_faces(dm);

	MVert *mvert2;
	MFace *mface2;
	unsigned int numtris=0;
	unsigned int numquads=0;
	int a = 0;
	int random = 0;
	int firsttime = 0;
	float vec1[3], vec2[3], vec3[3], vec4[3], vec5[3];
	float mag1=0, mag2=0;

	for(i = 0; i < numfaces; i++)
	{
	if(mface[i].v4)
	numquads++;
	else
	numtris++;	
}

	result = CDDM_from_template(dm, numverts, 0, numtris + 2*numquads);

	if(!result)
	return NULL;

	// do verts
	mvert2 = CDDM_get_verts(result);
	for(a=0; a<numverts; a++) 
	{
	MVert *inMV;
	MVert *mv = &mvert2[a];

	inMV = &mvert[a];

	DM_copy_vert_data(dm, result, a, a, 1);
	*mv = *inMV;
}


	// do faces
	mface2 = CDDM_get_faces(result);
	for(a=0, i=0; a<numfaces; a++) 
	{
	MFace *mf = &mface2[i];
	MFace *inMF;
	inMF = &mface[a];

		
		// DM_copy_face_data(dm, result, a, i, 1);

		// *mf = *inMF;
		

	if(mface[a].v4 && random==1)
	{
	mf->v1 = mface[a].v2;
	mf->v2 = mface[a].v3;
	mf->v3 = mface[a].v4;
}
	else
	{
	mf->v1 = mface[a].v1;
	mf->v2 = mface[a].v2;
	mf->v3 = mface[a].v3;
}

	mf->v4 = 0;
	mf->flag |= ME_SMOOTH;

	test_index_face(mf, NULL, 0, 3);

	if(mface[a].v4)
	{
	MFace *mf2;

	i++;

	mf2 = &mface2[i];
			
			// DM_copy_face_data(dm, result, a, i, 1);

			// *mf2 = *inMF;
			

	if(random==1)
	{
	mf2->v1 = mface[a].v1;
	mf2->v2 = mface[a].v2;
	mf2->v3 = mface[a].v4;
}
	else
	{
	mf2->v1 = mface[a].v4;
	mf2->v2 = mface[a].v1;
	mf2->v3 = mface[a].v3;
}
	mf2->v4 = 0;
	mf2->flag |= ME_SMOOTH;

	test_index_face(mf2, NULL, 0, 3);
}

	i++;
}

	CDDM_calc_edges(result);
	CDDM_calc_normals(result);

	return result;
	*/
	
	return NULL;
}


DerivedMesh *CDDM_create_tearing(ClothModifierData *clmd, DerivedMesh *dm)
{
	/*
	DerivedMesh *result = NULL;
	unsigned int i = 0, a = 0, j=0;
	int numverts = dm->getNumVerts(dm);
	int numedges = dm->getNumEdges(dm);
	int numfaces = dm->getNumFaces(dm);

	MVert *mvert = CDDM_get_verts(dm);
	MEdge *medge = CDDM_get_edges(dm);
	MFace *mface = CDDM_get_faces(dm);

	MVert *mvert2;
	MFace *mface2;
	unsigned int numtris=0;
	unsigned int numquads=0;
	EdgeHash *edgehash = NULL;
	Cloth *cloth = clmd->clothObject;
	ClothSpring *springs = cloth->springs;
	unsigned int numsprings = cloth->numsprings;
	
	// create spring tearing hash
	edgehash = BLI_edgehash_new();
	
	for(i = 0; i < numsprings; i++)
	{
	if((springs[i].flags & CSPRING_FLAG_DEACTIVATE)
	&&(!BLI_edgehash_haskey(edgehash, springs[i].ij, springs[i].kl)))
	{
	BLI_edgehash_insert(edgehash, springs[i].ij, springs[i].kl, NULL);
	BLI_edgehash_insert(edgehash, springs[i].kl, springs[i].ij, NULL);
	j++;
}
}
	
	// printf("found %d tears\n", j);
	
	result = CDDM_from_template(dm, numverts, 0, numfaces);

	if(!result)
	return NULL;

	// do verts
	mvert2 = CDDM_get_verts(result);
	for(a=0; a<numverts; a++) 
	{
	MVert *inMV;
	MVert *mv = &mvert2[a];

	inMV = &mvert[a];

	DM_copy_vert_data(dm, result, a, a, 1);
	*mv = *inMV;
}


	// do faces
	mface2 = CDDM_get_faces(result);
	for(a=0, i=0; a<numfaces; a++) 
	{
	MFace *mf = &mface2[i];
	MFace *inMF;
	inMF = &mface[a];

		
		// DM_copy_face_data(dm, result, a, i, 1);

		// *mf = *inMF;
		
		
	if((!BLI_edgehash_haskey(edgehash, mface[a].v1, mface[a].v2))
	&&(!BLI_edgehash_haskey(edgehash, mface[a].v2, mface[a].v3))
	&&(!BLI_edgehash_haskey(edgehash, mface[a].v3, mface[a].v4))
	&&(!BLI_edgehash_haskey(edgehash, mface[a].v4, mface[a].v1)))
	{
	mf->v1 = mface[a].v1;
	mf->v2 = mface[a].v2;
	mf->v3 = mface[a].v3;
	mf->v4 = mface[a].v4;
	
	test_index_face(mf, NULL, 0, 4);
	
	i++;
}
}

	CDDM_lower_num_faces(result, i);
	CDDM_calc_edges(result);
	CDDM_calc_normals(result);
	
	BLI_edgehash_free(edgehash, NULL);

	return result;
	*/
	
	return NULL;
}

int modifiers_indexInObject(Object *ob, ModifierData *md_seek);

void cloth_clear_cache(Object *ob, ClothModifierData *clmd, float framenr)
{
	int stack_index = -1;
	
	if(!(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_CCACHE_PROTECT))
	{
		stack_index = modifiers_indexInObject(ob, (ModifierData *)clmd);
		
		BKE_ptcache_id_clear((ID *)ob, PTCACHE_CLEAR_AFTER, framenr, stack_index);
	}
}
static void cloth_write_cache(Object *ob, ClothModifierData *clmd, float framenr)
{
	FILE *fp = NULL;
	int stack_index = -1;
	unsigned int a;
	Cloth *cloth = clmd->clothObject;
	
	if(!cloth)
		return;
	
	stack_index = modifiers_indexInObject(ob, (ModifierData *)clmd);
	
	fp = BKE_ptcache_id_fopen((ID *)ob, 'w', framenr, stack_index);
	if(!fp) return;
	
	for(a = 0; a < cloth->numverts; a++)
	{
		fwrite(&cloth->x[a], sizeof(float),4,fp);
		fwrite(&cloth->xconst[a], sizeof(float),4,fp);
		fwrite(&cloth->v[a], sizeof(float),4,fp);
	}
	
	fclose(fp);
}
static int cloth_read_cache(Object *ob, ClothModifierData *clmd, float framenr)
{
	FILE *fp = NULL;
	int stack_index = -1;
	unsigned int a, ret = 1;
	Cloth *cloth = clmd->clothObject;
	
	if(!cloth)
		return 0;
	
	stack_index = modifiers_indexInObject(ob, (ModifierData *)clmd);
	
	fp = BKE_ptcache_id_fopen((ID *)ob, 'r', framenr, stack_index);
	if(!fp)
		ret = 0;
	else {
		for(a = 0; a < cloth->numverts; a++)
		{
			if(fread(&cloth->x[a], sizeof(float), 4, fp) != 4) 
			{
				ret = 0;
				break;
			}
			if(fread(&cloth->xconst[a], sizeof(float), 4, fp) != 4) 
			{
				ret = 0;
				break;
			}
			if(fread(&cloth->v[a], sizeof(float), 4, fp) != 4) 
			{
				ret = 0;
				break;
			}
		}
		
		fclose(fp);
	}
	
	if(clmd->sim_parms->solver_type == 0)
		implicit_set_positions(clmd);
		
	return ret;
}

#define AMBIENT 50
#define DECAY 0.04f
#define ALMOST_EQUAL(a, b) ((fabs(a-b)<0.00001f)?1:0)

	// cube vertices
GLfloat cv[][3] = {
	{1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
 {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}
};

	// edges have the form edges[n][0][xyz] + t*edges[n][1][xyz]
float edges[12][2][3] = {
	{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
 {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
 {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
 {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},

 {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
 {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
 {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
 {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},

 {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
 {{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
 {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},
 {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}}
};

void light_ray(unsigned char* _texture_data, int _ray_templ[4096][3], int x, int y, int z, int n, float decay)
{
	int xx = x, yy = y, zz = z, i = 0;
	int offset;

	int l = 255;
	float d;

	do {
		offset = ((((zz*n) + yy)*n + xx) << 2);
		if (_texture_data[offset + 2] > 0)
			_texture_data[offset + 2] = (unsigned char) ((_texture_data[offset + 2] + l)*0.5f);
		else
			_texture_data[offset + 2] = (unsigned char) l;
		d = _texture_data[offset+1];
		if (l > AMBIENT) {
			l -= d*decay;
			if (l < AMBIENT)
				l = AMBIENT;
		}

		i++;
		xx = x + _ray_templ[i][0];
		yy = y + _ray_templ[i][1];
		zz = z + _ray_templ[i][2];
		
	} while ((xx>=0)&&(xx<n)&&(yy>=0)&&(yy<n)&&(zz>=0)&&(zz<n));
}

void cast_light(unsigned char* _texture_data, int _ray_templ[4096][3], float *_light_dir, int n /*edgelen*/)
{
	int i,j;
	int sx = (_light_dir[0]>0) ? 0 : n-1;
	int sy = (_light_dir[1]>0) ? 0 : n-1;
	int sz = (_light_dir[2]>0) ? 0 : n-1;

	float decay = 1.0f/(n*DECAY);

	for (i=0; i<n; i++)
		for (j=0; j<n; j++) {
		if (!ALMOST_EQUAL(_light_dir[0], 0))
			light_ray(_texture_data, _ray_templ, sx,i,j,n,decay);
		if (!ALMOST_EQUAL(_light_dir[1], 0))
			light_ray(_texture_data, _ray_templ, i,sy,j,n,decay);
		if (!ALMOST_EQUAL(_light_dir[2], 0))
			light_ray(_texture_data, _ray_templ, i,j,sz,n,decay);
		}
}

void gen_ray_templ(int _ray_templ[4096][3], float *_light_dir, int edgelen)
{
	float fx = 0.0f, fy = 0.0f, fz = 0.0f;
	int x = 0, y = 0, z = 0;
	float lx = _light_dir[0] + 0.000001f, ly = _light_dir[1] + 0.000001f, lz = _light_dir[2] + 0.000001f;
	int xinc = (lx > 0) ? 1 : -1;
	int yinc = (ly > 0) ? 1 : -1;
	int zinc = (lz > 0) ? 1 : -1;
	float tx, ty, tz;
	int i = 1;
	int len = 0;
	int maxlen = 3*edgelen*edgelen;
	_ray_templ[0][0] = _ray_templ[0][2] = _ray_templ[0][2] = 0;
	
	while (len <= maxlen)
	{
		// fx + t*lx = (x+1)   ->   t = (x+1-fx)/lx
		tx = (x+xinc-fx)/lx;
		ty = (y+yinc-fy)/ly;
		tz = (z+zinc-fz)/lz;

		if ((tx<=ty)&&(tx<=tz)) {
			_ray_templ[i][0] = _ray_templ[i-1][0] + xinc;
			x =+ xinc;
			fx = x;

			if (ALMOST_EQUAL(ty,tx)) {
				_ray_templ[i][1] = _ray_templ[i-1][1] + yinc;
				y += yinc;
				fy = y;
			} else {
				_ray_templ[i][1] = _ray_templ[i-1][1];
				fy += tx*ly;
			}

			if (ALMOST_EQUAL(tz,tx)) {
				_ray_templ[i][2] = _ray_templ[i-1][2] + zinc;
				z += zinc;
				fz = z;
			} else {
				_ray_templ[i][2] = _ray_templ[i-1][2];
				fz += tx*lz;
			}
		} else if ((ty<tx)&&(ty<=tz)) {
			_ray_templ[i][0] = _ray_templ[i-1][0];
			fx += ty*lx;

			_ray_templ[i][1] = _ray_templ[i-1][1] + yinc;
			y += yinc;
			fy = y;

			if (ALMOST_EQUAL(tz,ty)) {
				_ray_templ[i][2] = _ray_templ[i-1][2] + zinc;
				z += zinc;
				fz = z;
			} else {
				_ray_templ[i][2] = _ray_templ[i-1][2];
				fz += ty*lz;
			}
		} else {
			// assert((tz<tx)&&(tz<ty));
			if((tz<tx)&&(tz<ty))
				break;
			
			_ray_templ[i][0] = _ray_templ[i-1][0];
			fx += tz*lx;
			_ray_templ[i][1] = _ray_templ[i-1][1];
			fy += tz*ly;
			_ray_templ[i][2] = _ray_templ[i-1][2] + zinc;
			z += zinc;
			fz = z;
		}

		len = _ray_templ[i][0]*_ray_templ[i][0]
				+ _ray_templ[i][1]*_ray_templ[i][1]
				+ _ray_templ[i][2]*_ray_templ[i][2];
		i++;
	}
}
/*
int intersect_edges(float ret[12][3], float a, float b, float c, float d)
{
	int i;
	float t;
	Vec3 p;
	int num = 0;

	for (i=0; i<12; i++) {
		t = -(a*edges[i][0][0] + b*edges[i][0][1] + c*edges[i][0][2] + d)
			/ (a*edges[i][1][0] + b*edges[i][1][1] + c*edges[i][1][2]);
		if ((t>0)&&(t<2)) {
			ret[num][0] = edges[i][0][0] + edges[i][1][0]*t;
			ret[num][1] = edges[i][0][1] + edges[i][1][1]*t;
			ret[num][2] = edges[i][0][2] + edges[i][1][2]*t;
			num++;
		}
	}

	return num;
}

void draw_slices(float m[][4])
{
	int i;

	Vec3 viewdir(m[0][2], m[1][2], m[2][2]);
	viewdir.Normalize();
		// find cube vertex that is closest to the viewer
	for (i=0; i<8; i++) {
		float x = cv[i][0] + viewdir[0];
		float y = cv[i][1] + viewdir[1];
		float z = cv[i][2] + viewdir[2];
		if ((x>=-1.0f)&&(x<=1.0f)
				   &&(y>=-1.0f)&&(y<=1.0f)
				   &&(z>=-1.0f)&&(z<=1.0f))
		{
			break;
		}
	}
	if(i != 8) return;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDisable(GL_DEPTH_TEST);
		// our slices are defined by the plane equation a*x + b*y +c*z + d = 0
		// (a,b,c), the plane normal, are given by viewdir
		// d is the parameter along the view direction. the first d is given by
		// inserting previously found vertex into the plane equation
	float d0 = -(viewdir[0]*cv[i][0] + viewdir[1]*cv[i][1] + viewdir[2]*cv[i][2]);
	float dd = 2*d0/64.0f;
	int n = 0;
	for (float d = -d0; d < d0; d += dd) {
					// intersect_edges returns the intersection points of all cube edges with
			// the given plane that lie within the cube
		float pt[12][3];
		int num = intersect_edges(pt, viewdir[0], viewdir[1], viewdir[2], d);

		if (num > 2) {
			// sort points to get a convex polygon
			// std::sort(pt.begin()+1, pt.end(), Convexcomp(pt[0], viewdir));
			int shuffled = 1;
			
			while(shuffled)
			{
				int j;
				shuffled = 0;
				
				for(j = 0; j < num-1; j++)
				{
					// Vec3 va = a-p0, vb = b-p0;
					// return dot(up, cross(va, vb)) >= 0;
					float va[3], vb[3], vc[3];
					
					VECSUB(va, pt[j], pt[0]);
					VECSUB(vb, pt[j+1], pt[0]);
					Crossf(vc, va, vb);
					
					if(INPR(viewdir, vc)>= 0)
					{
						float temp[3];
						
						VECCOPY(temp, pt[j]);
						VECCOPY(pt[j], pt[j+1]);
						VECCOPY(pt[j+1], temp);
						
						shuffled = 1;
					}
				}
			}

			glEnable(GL_TEXTURE_3D);
			glEnable(GL_FRAGMENT_PROGRAM_ARB);
			glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, _prog[0]);
			glActiveTextureARB(GL_TEXTURE0_ARB);
			glBindTexture(GL_TEXTURE_3D, _txt[0]);
			glBegin(GL_POLYGON);
			for (i=0; i<num; i++){
				glColor3f(1.0, 1.0, 1.0);
				glTexCoord3d((pt[i][0]+1.0)/2.0, (-pt[i][1]+1)/2.0, (pt[i][2]+1.0)/2.0);
				glVertex3f(pt[i][0], pt[i][1], pt[i][2]);
			}
			glEnd();
		}
		n++;
	}
}


void draw(void)
{
	int i;

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, -_dist,  0, 0, 0,  0, 1, 0);

	float m[4][4];
	build_rotmatrix(m, _quat);

	glMultMatrixf(&m[0][0]);

	if (_draw_cube)
		draw_cube();
	draw_slices(m, _draw_slice_outline);

	if (_dispstring != NULL) {
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixd(_ortho_m);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glDisable(GL_TEXTURE_3D);
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glRasterPos2i(-_sx/2 + 10, _sy/2 - 15);

		print_string(_dispstring);

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixd(_persp_m);
		glMatrixMode(GL_MODELVIEW);
	}
}*/

/************************************************
 * clothModifier_do - main simulation function
************************************************/
DerivedMesh *clothModifier_do(ClothModifierData *clmd,Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{
	unsigned int i;
	unsigned int numverts = -1;
	unsigned int numedges = -1;
	unsigned int numfaces = -1;
	MVert *mvert = NULL;
	MEdge *medge = NULL;
	MFace *mface = NULL;
	DerivedMesh *result = NULL;
	Cloth *cloth = clmd->clothObject;
	unsigned int framenr = (float)G.scene->r.cfra;
	float current_time = bsystem_time(ob, (float)G.scene->r.cfra, 0.0);
	ListBase *effectors = NULL;
	float deltaTime = current_time - clmd->sim_parms->sim_time;
	unsigned char* _texture_data=NULL;	
	float _light_dir[3];
	int _ray_templ[4096][3];

	clmd->sim_parms->dt = 1.0f / (clmd->sim_parms->stepsPerFrame * G.scene->r.frs_sec);

	result = CDDM_copy(dm);
	
	if(!result)
	{
		return dm;
	}
	
	numverts = result->getNumVerts(result);
	numedges = result->getNumEdges(result);
	numfaces = result->getNumFaces(result);
	mvert = CDDM_get_verts(result);
	medge = CDDM_get_edges(result);
	mface = CDDM_get_faces(result);

	clmd->sim_parms->sim_time = current_time;
	
	if ( current_time < clmd->sim_parms->firstframe )
		return result;
	
	// only be active during a specific period:
	// that's "first frame" and "last frame" on GUI
	/*
	if ( clmd->clothObject )
	{
	if ( clmd->sim_parms->cache )
	{
	if ( current_time < clmd->sim_parms->firstframe )
	{
	int frametime = cloth_cache_first_frame ( clmd );
	if ( cloth_cache_search_frame ( clmd, frametime ) )
	{
	cloth_cache_get_frame ( clmd, frametime );
	cloth_to_object ( ob, result, clmd );
}
	return result;
}
	else if ( current_time > clmd->sim_parms->lastframe )
	{
	int frametime = cloth_cache_last_frame ( clmd );
	if ( cloth_cache_search_frame ( clmd, frametime ) )
	{
	cloth_cache_get_frame ( clmd, frametime );
	cloth_to_object ( ob, result, clmd );
}
	return result;
}
	else if ( ABS ( deltaTime ) >= 2.0f ) // no timewarps allowed
	{
	if ( cloth_cache_search_frame ( clmd, framenr ) )
	{
	cloth_cache_get_frame ( clmd, framenr );
	cloth_to_object ( ob, result, clmd );
}
	clmd->sim_parms->sim_time = current_time;
	return result;
}
}
}
	*/
	
	if(deltaTime == 1.0f)
	{
		if ((clmd->clothObject == NULL) || (numverts != clmd->clothObject->numverts) ) 
		{
			cloth_clear_cache(ob, clmd, 0);
			
			if(!cloth_from_object (ob, clmd, result, dm, framenr))
				return result;

			if(clmd->clothObject == NULL)
				return result;

			cloth = clmd->clothObject;
		}
		/*
		deltaTime = 0;
		while( deltaTime < 1.0)
		{
			step(cloth->m_fc, 0.1);
			deltaTime+=0.1;
		}
		*/
		clmd->clothObject->old_solver_type = clmd->sim_parms->solver_type;

		// Insure we have a clmd->clothObject, in case allocation failed.
		if (clmd->clothObject != NULL) 
		{
			if(!cloth_read_cache(ob, clmd, framenr))
			{
				// Force any pinned verts to their constrained location.
				// has to be commented for verlet
				for ( i = 0; i < clmd->clothObject->numverts; i++ )
				{
					// Save the previous position.
					VECCOPY ( cloth->xold[i], cloth->xconst[i] );
					VECCOPY ( cloth->current_xold[i], cloth->x[i] );
					// Get the current position.
					VECCOPY ( cloth->xconst[i], mvert[i].co );
					Mat4MulVecfl ( ob->obmat, cloth->xconst[i] );
				}
				
				tstart();
				
				/* Call the solver. */
				
				if (solvers [clmd->sim_parms->solver_type].solver)
					solvers [clmd->sim_parms->solver_type].solver (ob, framenr, clmd, effectors);
				
				tend();
				
				printf("Cloth simulation time: %f\n", tval());
				
				cloth_write_cache(ob, clmd, framenr);
			}

			// Copy the result back to the object.
			cloth_to_object (ob, result, clmd);
			
			// bvh_free(clmd->clothObject->tree);
			// clmd->clothObject->tree = bvh_build(clmd, clmd->coll_parms->epsilon);
		} 

	}
	else if ( ( deltaTime <= 0.0f ) || ( deltaTime > 1.0f ) )
	{
		if ( clmd->clothObject != NULL )
		{
			if(cloth_read_cache(ob, clmd, framenr))
				cloth_to_object (ob, result, clmd);
		}
		else
		{
			cloth_clear_cache(ob, clmd, 0);
		}
	}
	
	cloth = clmd->clothObject;
	/*
	if(cloth)
	{
		if (_texture_data == NULL)
			_texture_data = (unsigned char*) malloc((30+2)*(30+2)*(30+2)*4);
	
		for (i=0; i<(30+2)*(30+2)*(30+2); i++) {
			_texture_data[(i<<2)] = (unsigned char) (cloth->m_fc->T[i] * 255.0f);
			_texture_data[(i<<2)+1] = (unsigned char) (cloth->m_fc->d[i] * 255.0f);
			_texture_data[(i<<2)+2] = 0;
			_texture_data[(i<<2)+3] = 255;
		}
		
		// from ligth constructor
		_light_dir[0] = -1.0f;
		_light_dir[1] = 0.5f;
		_light_dir[2] = 0.0f;
		
		gen_ray_templ(_ray_templ, _light_dir, 30 + 2);
	
		cast_light(_texture_data, _ray_templ, _light_dir, 30+2);
	
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 30+2, 30+2, 30+2, 0, GL_RGBA, GL_UNSIGNED_BYTE, _texture_data);
		free(_texture_data);
	}
	*/
	return result;
}

/* frees all */
void cloth_free_modifier (ClothModifierData *clmd)
{
	Cloth	*cloth = NULL;

	if(!clmd)
		return;

	cloth = clmd->clothObject;

	// free our frame cache
	// cloth_clear_cache(ob, clmd, 0);

	/* Calls the solver and collision frees first as they
	* might depend on data in clmd->clothObject. */

	if (cloth) 
	{	
		f_free(cloth->m_fc);
		
		// If our solver provides a free function, call it
		if (cloth->old_solver_type < 255 && solvers [cloth->old_solver_type].free) 
		{	
			solvers [cloth->old_solver_type].free (clmd);
		}
		
		// Free the verts.
		if (cloth->verts != NULL)
			MEM_freeN (cloth->verts);
		
		// Free the faces.
		if ( cloth->mfaces != NULL )
			MEM_freeN ( cloth->mfaces );
		
		// Free the verts.
		if ( cloth->x != NULL )
			MEM_freeN ( cloth->x );
		
		// Free the verts.
		if ( cloth->xold != NULL )
			MEM_freeN ( cloth->xold );
		
		// Free the verts.
		if ( cloth->v != NULL )
			MEM_freeN ( cloth->v );
		
		// Free the verts.
		if ( cloth->current_x != NULL )
			MEM_freeN ( cloth->current_x );
		
		// Free the verts.
		if ( cloth->current_xold != NULL )
			MEM_freeN ( cloth->current_xold );
		
		// Free the verts.
		if ( cloth->current_v != NULL )
			MEM_freeN ( cloth->current_v );
		
		// Free the verts.
		if ( cloth->xconst != NULL )
			MEM_freeN ( cloth->xconst );
		
		cloth->verts = NULL;
		cloth->numverts = -1;
		
		// Free the springs.
		if ( cloth->springs != NULL )
		{
			LinkNode *search = cloth->springs;
			while(search)
			{
				ClothSpring *spring = search->link;
						
				MEM_freeN ( spring );
				search = search->next;
			}
			BLI_linklist_free(cloth->springs, NULL);
		
			cloth->springs = NULL;
		}

		cloth->numsprings = -1;
		
		// free BVH collision tree
		if(cloth->tree)
			bvh_free((BVH *)cloth->tree);
		
		// free BVH self collision tree
		if(cloth->selftree)
			bvh_free((BVH *)cloth->selftree);
		
		if(cloth->edgehash)
			BLI_edgehash_free ( cloth->edgehash, NULL );
		
		MEM_freeN (cloth);
		clmd->clothObject = NULL;
	}

}



/******************************************************************************
*
* Internal functions.
*
******************************************************************************/

/**
 * cloth_to_object - copies the deformed vertices to the object.
 *
 * This function is a modified version of the softbody.c:softbody_to_object() function.
 **/
static void cloth_to_object (Object *ob,  DerivedMesh *dm, ClothModifierData *clmd)
{
	unsigned int	i = 0;
	MVert *mvert = NULL;
	unsigned int numverts;
	Cloth *cloth = clmd->clothObject;

	if (clmd->clothObject) {
		/* inverse matrix is not uptodate... */
		Mat4Invert (ob->imat, ob->obmat);

		mvert = CDDM_get_verts(dm);
		numverts = dm->getNumVerts(dm);

		for (i = 0; i < numverts; i++)
		{
			VECCOPY (mvert[i].co, cloth->x[i]);
			Mat4MulVecfl (ob->imat, mvert[i].co);	/* cloth is in global coords */
		}
	}
}


/**
 * cloth_apply_vgroup - applies a vertex group as specified by type
 *
 **/
static void cloth_apply_vgroup(ClothModifierData *clmd, DerivedMesh *dm, short vgroup)
{
	unsigned int i = 0;
	unsigned int j = 0;
	MDeformVert	*dvert = NULL;
	Cloth *clothObj = NULL;
	unsigned int numverts = 0;
	float goalfac = 0;
	ClothVertex *verts = NULL;

	clothObj = clmd->clothObject;

	if ( !dm )
		return;

	numverts = dm->getNumVerts(dm);

	/* vgroup is 1 based, decrement so we can match the right group. */
	--vgroup;

	verts = clothObj->verts;

	for ( i = 0; i < numverts; i++, verts++ )
	{
		// LATER ON, support also mass painting here
		if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
		{
			dvert = dm->getVertData ( dm, i, CD_MDEFORMVERT );
			if ( dvert )
			{
				for ( j = 0; j < dvert->totweight; j++ )
				{
					if ( dvert->dw[j].def_nr == vgroup )
					{
						verts->goal = dvert->dw [j].weight;

						goalfac= ABS ( clmd->sim_parms->maxgoal - clmd->sim_parms->mingoal );
						verts->goal  = ( float ) pow ( verts->goal , 4.0f );

						if ( dvert->dw [j].weight >=SOFTGOALSNAP )
						{
							verts->flags |= CVERT_FLAG_PINNED;
						}

						// TODO enable mass painting here, for the moment i let "goals" go first

						break;
					}
				}
			}
		}
	}
}
		
// only meshes supported at the moment
static int cloth_from_object(Object *ob, ClothModifierData *clmd, DerivedMesh *dm, DerivedMesh *olddm, float framenr)
{
	unsigned int i;
	unsigned int numverts = dm->getNumVerts(dm);
	MVert *mvert = CDDM_get_verts(dm);
	float tnull[3] = {0,0,0};
	Cloth *cloth = NULL;
	
	/* If we have a clothObject, free it. */
	if (clmd->clothObject != NULL)
		cloth_free_modifier (clmd);

	/* Allocate a new cloth object. */
	clmd->clothObject = MEM_callocN (sizeof(Cloth), "cloth");
	if (clmd->clothObject) 
	{
		clmd->clothObject->old_solver_type = 255;
		clmd->clothObject->edgehash = NULL;
	}
	else if (clmd->clothObject == NULL) 
	{
		modifier_setError (&(clmd->modifier), "Out of memory on allocating clmd->clothObject.");
		return 0;
	}
	
	cloth = clmd->clothObject;
	
	cloth->m_fc = f_init();

	switch (ob->type)
	{
		case OB_MESH:
		
			// mesh input objects need DerivedMesh
			if ( !dm )
				return 0;

			cloth_from_mesh (ob, clmd, dm, framenr);

			if ( clmd->clothObject != NULL )
			{
				/* create springs */
				clmd->clothObject->springs = NULL;
				clmd->clothObject->numsprings = -1;
					
				/* set initial values */
				for (i = 0; i < numverts; ++i)
				{
					VECCOPY (clmd->clothObject->x[i], mvert[i].co);
					Mat4MulVecfl(ob->obmat, clmd->clothObject->x[i]);
	
					clmd->clothObject->verts [i].mass = clmd->sim_parms->mass;
					if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
						clmd->clothObject->verts [i].goal= clmd->sim_parms->defgoal;
					else
						clmd->clothObject->verts [i].goal= 0.0;
					clmd->clothObject->verts [i].flags = 0;
					VECCOPY(clmd->clothObject->xold[i], clmd->clothObject->x[i]);
					VECCOPY(clmd->clothObject->xconst[i], clmd->clothObject->x[i]);
					VECCOPY(clmd->clothObject->current_xold[i], clmd->clothObject->x[i]);
					VecMulf(clmd->clothObject->v[i], 0.0);
	
					clmd->clothObject->verts [i].impulse_count = 0;
					VECCOPY ( clmd->clothObject->verts [i].impulse, tnull );
				}
				
				if (!cloth_build_springs (clmd, dm) )
				{
					modifier_setError (&(clmd->modifier), "Can't build springs.");
					return 0;
				}  
	
				/* apply / set vertex groups */
				if (clmd->sim_parms->vgroup_mass > 0)
					cloth_apply_vgroup (clmd, olddm, clmd->sim_parms->vgroup_mass);
	
				/* init our solver */
				if (solvers [clmd->sim_parms->solver_type].init)
					solvers [clmd->sim_parms->solver_type].init (ob, clmd);
	
				clmd->clothObject->tree = bvh_build_from_float3(CDDM_get_faces(dm), dm->getNumFaces(dm), clmd->clothObject->x, numverts, clmd->coll_parms->epsilon);
				
				clmd->clothObject->selftree = bvh_build_from_float3(NULL, 0, clmd->clothObject->x, numverts, clmd->coll_parms->selfepsilon);
				
				// save initial state
				cloth_write_cache(ob, clmd, framenr-1);
			}
			return 1;
			default: return 0; // TODO - we do not support changing meshes
	}
	
	return 0;
}

static void cloth_from_mesh (Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float framenr)
{
	unsigned int numverts = dm->getNumVerts(dm);
	unsigned int numfaces = dm->getNumFaces(dm);
	MFace *mface = CDDM_get_faces(dm);

	/* Allocate our vertices.
	*/
	clmd->clothObject->numverts = numverts;
	clmd->clothObject->verts = MEM_callocN (sizeof (ClothVertex) * clmd->clothObject->numverts, "clothVertex");
	if (clmd->clothObject->verts == NULL) 
	{
		cloth_free_modifier (clmd);
		modifier_setError (&(clmd->modifier), "Out of memory on allocating clmd->clothObject->verts.");
		return;
	}
	
	clmd->clothObject->x = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 4, "Cloth MVert_x" );
	if ( clmd->clothObject->x == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->x." );
		return;
	}
	
	clmd->clothObject->xold = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 4, "Cloth MVert_xold" );
	if ( clmd->clothObject->xold == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->xold." );
		return;
	}
	
	clmd->clothObject->v = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 4, "Cloth MVert_v" );
	if ( clmd->clothObject->v == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->v." );
		return;
	}
	
	clmd->clothObject->current_x = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 4, "Cloth MVert_current_x" );
	if ( clmd->clothObject->current_x == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->current_x." );
		return;
	}
	
	clmd->clothObject->current_xold = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 4, "Cloth MVert_current_xold" );
	if ( clmd->clothObject->current_xold == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->current_xold." );
		return;
	}
	
	clmd->clothObject->current_v = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 4, "Cloth MVert_current_v" );
	if ( clmd->clothObject->current_v == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->current_v." );
		return;
	}
	
	clmd->clothObject->xconst = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 4, "Cloth MVert_xconst" );
	if ( clmd->clothObject->xconst == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->xconst." );
		return;
	}

	// save face information
	clmd->clothObject->numfaces = numfaces;
	clmd->clothObject->mfaces = MEM_callocN (sizeof (MFace) * clmd->clothObject->numfaces, "clothMFaces");
	if (clmd->clothObject->mfaces == NULL) 
	{
		cloth_free_modifier (clmd);
		modifier_setError (&(clmd->modifier), "Out of memory on allocating clmd->clothObject->mfaces.");
		return;
	}
	memcpy(clmd->clothObject->mfaces, mface, sizeof(MFace)*numfaces);

	/* Free the springs since they can't be correct if the vertices
	* changed.
	*/
	if (clmd->clothObject->springs != NULL)
		MEM_freeN (clmd->clothObject->springs);

}

/***************************************************************************************
* SPRING NETWORK BUILDING IMPLEMENTATION BEGIN
***************************************************************************************/

// be carefull: implicit solver has to be resettet when using this one!
// --> only for implicit handling of this spring!
int cloth_add_spring ( ClothModifierData *clmd, unsigned int indexA, unsigned int indexB, float restlength, int spring_type)
{
	Cloth *cloth = clmd->clothObject;
	ClothSpring *spring = NULL;
	
	if(cloth)
	{
		// TODO: look if this spring is already there
		
		spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );
		
		spring->ij = indexA;
		spring->kl = indexB;
		spring->restlen =  restlength;
		spring->type = spring_type;
		spring->flags = 0;
		
		cloth->numsprings++;
	
		BLI_linklist_append ( &cloth->springs, spring );
		
		return 1;
	}
	return 0;
}

int cloth_build_springs ( ClothModifierData *clmd, DerivedMesh *dm )
{
	Cloth *cloth = clmd->clothObject;
	ClothSpring *spring = NULL, *tspring = NULL, *tspring2 = NULL;
	unsigned int struct_springs = 0, shear_springs=0, bend_springs = 0;
	unsigned int i = 0, j = 0, akku_count;
	unsigned int numverts = dm->getNumVerts ( dm );
	unsigned int numedges = dm->getNumEdges ( dm );
	unsigned int numfaces = dm->getNumFaces ( dm );
	MEdge *medge = CDDM_get_edges ( dm );
	MFace *mface = CDDM_get_faces ( dm );
	unsigned int index2 = 0; // our second vertex index
	LinkNode **edgelist = NULL;
	EdgeHash *edgehash = NULL;
	LinkNode *search = NULL, *search2 = NULL;
	float temp[3], akku, min, max;
	LinkNode *node = NULL, *node2 = NULL;
	
	// error handling
	if ( numedges==0 )
		return 0;

	cloth->springs = NULL;

	edgelist = MEM_callocN ( sizeof ( LinkNode * ) * numverts, "cloth_edgelist_alloc" );
	for ( i = 0; i < numverts; i++ )
	{
		edgelist[i] = NULL;
	}

	if ( cloth->springs )
		MEM_freeN ( cloth->springs );

	// create spring network hash
	edgehash = BLI_edgehash_new();

	// structural springs
	for ( i = 0; i < numedges; i++ )
	{
		spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

		if ( spring )
		{
			spring->ij = medge[i].v1;
			spring->kl = medge[i].v2;
			VECSUB ( temp, cloth->x[spring->kl], cloth->x[spring->ij] );
			spring->restlen =  sqrt ( INPR ( temp, temp ) );
			spring->type = CLOTH_SPRING_TYPE_STRUCTURAL;
			spring->flags = 0;
			struct_springs++;
			
			if(!i)
				node2 = BLI_linklist_append_fast ( &cloth->springs, spring );
			else
				node2 = BLI_linklist_append_fast ( &node->next, spring );
			node = node2;
		}
	}
	
	// calc collision balls *slow*
	// better: use precalculated list with O(1) index access to all springs of a vertex
	// missing for structural since it's not needed for building bending springs
	for ( i = 0; i < numverts; i++ )
	{
		akku_count = 0;
		akku = 0.0;
		cloth->verts[i].collball=0;
		min = 1e22f;
		max = -1e22f;
		
		search = cloth->springs;
		for ( j = 0; j < struct_springs; j++ )
		{
			if ( !search )
				break;

			tspring = search->link;
			
			if((tspring->ij == i) || (tspring->kl == i))
			{
				akku += spring->restlen;
				akku_count++;
    				min = MIN2(spring->restlen,min);
    				max = MAX2(spring->restlen,max);
			}
		}
		
		if (akku_count > 0) {
			cloth->verts[i].collball = akku/(float)akku_count*clmd->coll_parms->selfepsilon;
		}
		else cloth->verts[i].collball=0;
	}
	
	// shear springs
	for ( i = 0; i < numfaces; i++ )
	{
		spring = ( ClothSpring *) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

		spring->ij = mface[i].v1;
		spring->kl = mface[i].v3;
		VECSUB ( temp, cloth->x[spring->kl], cloth->x[spring->ij] );
		spring->restlen =  sqrt ( INPR ( temp, temp ) );
		spring->type = CLOTH_SPRING_TYPE_SHEAR;

		BLI_linklist_append ( &edgelist[spring->ij], spring );
		BLI_linklist_append ( &edgelist[spring->kl], spring );
		shear_springs++;

		node2 = BLI_linklist_append_fast ( &node->next, spring );
		node = node2;

		if ( mface[i].v4 )
		{
			spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

			spring->ij = mface[i].v2;
			spring->kl = mface[i].v4;
			VECSUB ( temp, cloth->x[spring->kl], cloth->x[spring->ij] );
			spring->restlen =  sqrt ( INPR ( temp, temp ) );
			spring->type = CLOTH_SPRING_TYPE_SHEAR;

			BLI_linklist_append ( &edgelist[spring->ij], spring );
			BLI_linklist_append ( &edgelist[spring->kl], spring );
			shear_springs++;

			node2 = BLI_linklist_append_fast ( &node->next, spring );
			node = node2;
		}
	}
	
	// bending springs
	search2 = cloth->springs;
	for ( i = struct_springs; i < struct_springs+shear_springs; i++ )
	{
		if ( !search2 )
			break;

		tspring2 = search2->link;
		search = edgelist[tspring2->kl];
		while ( search )
		{
			tspring = search->link;
			index2 = ( ( tspring->ij==tspring2->kl ) ? ( tspring->kl ) : ( tspring->ij ) );
			
			// check for existing spring
			// check also if startpoint is equal to endpoint
			if ( !BLI_edgehash_haskey ( edgehash, index2, tspring2->ij )
			&& !BLI_edgehash_haskey ( edgehash, tspring2->ij, index2 )
			&& ( index2!=tspring2->ij ) )
			{
				spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

				spring->ij = tspring2->ij;
				spring->kl = index2;
				VECSUB ( temp, cloth->x[index2], cloth->x[tspring2->ij] );
				spring->restlen =  sqrt ( INPR ( temp, temp ) );
				spring->type = CLOTH_SPRING_TYPE_BENDING;
				BLI_edgehash_insert ( edgehash, spring->ij, index2, NULL );
				bend_springs++;

				node2 = BLI_linklist_append_fast ( &node->next, spring );
				node = node2;
			}
			search = search->next;
		}
		search2 = search2->next;
	}
	
	cloth->numspringssave = cloth->numsprings = struct_springs + shear_springs + bend_springs;
	cloth->numothersprings = struct_springs + shear_springs;
	
	for ( i = 0; i < numverts; i++ )
	{
		BLI_linklist_free ( edgelist[i],NULL );
	}
	if ( edgelist )
		MEM_freeN ( edgelist );
	
	cloth->edgehash = edgehash;

	return 1;

} /* cloth_build_springs */
/***************************************************************************************
* SPRING NETWORK BUILDING IMPLEMENTATION END
***************************************************************************************/

#define F_ITER 20
#define m_diffusion 0.00001f
#define m_viscosity 0.000f
#define m_buoyancy  1.5f
#define m_cooling   1.0f
#define m_vc_eps    4.0f

#define GRID_SIZE 30
#define SIZE ((GRID_SIZE+2)*(GRID_SIZE+2)*(GRID_SIZE+2))
#define _I(x,y,z) (((z)<<10)+((y)<<5)+x)

#define SWAPFPTR(x,y) {float *t=x;x=y;y=t;}

float buffers[10][SIZE];
float sd[SIZE], su[SIZE], sv[SIZE], sw[SIZE], sT[SIZE];


// nothing to do in this mode
// add code for 2nd mode
void set_bnd(int b, float* x, int N)
{
}

void lin_solve(int b, float *x, float *x0, float a, float c, int N)
{
	float cRecip = 1.0 / c;
	int i, j, k, l;
	for (l=0; l<F_ITER; l++) 
	{
		for (k=1; k<=N; k++)
		{
			for (j=1; j<=N; j++)
			{
				for (i=1; i<=N; i++)
				{
					x[_I(i,j,k)] = (x0[_I(i,j,k)] + a*(
					x[_I(i-1,j,k)]+x[_I(i+1,j,k)]+
					x[_I(i,j-1,k)]+x[_I(i,j+1,k)]+
					x[_I(i,j,k-1)]+x[_I(i,j,k+1)]))*cRecip;
				}
			}
		}
		set_bnd(b, x, N);
	}
}

void add_source(float* src, float *dst, float dt, int N)
{
	int i, size=(N+2)*(N+2)*(N+2);

	for (i=0; i<size; i++)
		dst[i] += src[i]*dt;
}

void add_buoyancy(float *v, float* T, float dt, float buoyancy, int N)
{
	int i, size=(N+2)*(N+2)*(N+2);

	for (i=0; i<size; i++)
		v[i] += -T[i]*buoyancy*dt;
}

void diffuse(int b, float* x0, float* x, float diff, float dt, int N)
{
	float a=dt*diff*N*N*N;
	lin_solve(b, x, x0, a, 1 + 6 * a, N);
}


void advect(int b, float* x0, float* x, float* uu, float* vv, float* ww, float dt, int N)
{
	int i, j, k, i0, j0, k0, i1, j1, k1;
	float sx0, sx1, sy0, sy1, sz0, sz1, v0, v1;
	float xx, yy, zz, dt0;
	dt0 = dt*N;
	for (k=1; k<=N; k++)
	{
		for (j=1; j<=N; j++)
		{
			for (i=1; i<=N; i++)
			{
				xx = i-dt0*uu[_I(i,j,k)];
				yy = j-dt0*vv[_I(i,j,k)];
				zz = k-dt0*ww[_I(i,j,k)];
				if (xx<0.5) xx=0.5f; if (xx>N+0.5) xx=N+0.5f; i0=(int)xx; i1=i0+1;
				if (yy<0.5) yy=0.5f; if (yy>N+0.5) yy=N+0.5f; j0=(int)yy; j1=j0+1;
				if (zz<0.5) zz=0.5f; if (zz>N+0.5) zz=N+0.5f; k0=(int)zz; k1=k0+1;
				sx1 = xx-i0; sx0 = 1-sx1;
				sy1 = yy-j0; sy0 = 1-sy1;
				sz1 = zz-k0; sz0 = 1-sz1;
				v0 = sx0*(sy0*x0[_I(i0,j0,k0)]+sy1*x0[_I(i0,j1,k0)])+sx1*(sy0*x0[_I(i1,j0,k0)]+sy1*x0[_I(i1,j1,k0)]);
				v1 = sx0*(sy0*x0[_I(i0,j0,k1)]+sy1*x0[_I(i0,j1,k1)])+sx1*(sy0*x0[_I(i1,j0,k1)]+sy1*x0[_I(i1,j1,k1)]);
				x[_I(i,j,k)] = sz0*v0 + sz1*v1;
			}
		}
	}
	set_bnd(b,x, N);
}

void advect_cool(int b, float* x0, float* x, float* y0, float* y, float* uu, float* vv, float* ww, float dt, float cooling, int N)
{
	int i, j, k, i0, j0, k0, i1, j1, k1;
	float sx0, sx1, sy0, sy1, sz0, sz1, v0, v1;
	float xx, yy, zz, dt0, c0;
	dt0 = dt*N;
	c0 = 1.0f - cooling*dt;
	for (k=1; k<=N; k++)
	{
		for (j=1; j<=N; j++)
		{
			for (i=1; i<=N; i++)
			{
				xx = i-dt0*uu[_I(i,j,k)];
				yy = j-dt0*vv[_I(i,j,k)];
				zz = k-dt0*ww[_I(i,j,k)];
				if (xx<0.5) xx=0.5f; if (xx>N+0.5) xx=N+0.5f; i0=(int)xx; i1=i0+1;
				if (yy<0.5) yy=0.5f; if (yy>N+0.5) yy=N+0.5f; j0=(int)yy; j1=j0+1;
				if (zz<0.5) zz=0.5f; if (zz>N+0.5) zz=N+0.5f; k0=(int)zz; k1=k0+1;
				sx1 = xx-i0; sx0 = 1-sx1;
				sy1 = yy-j0; sy0 = 1-sy1;
				sz1 = zz-k0; sz0 = 1-sz1;
				v0 = sx0*(sy0*x0[_I(i0,j0,k0)]+sy1*x0[_I(i0,j1,k0)])+sx1*(sy0*x0[_I(i1,j0,k0)]+sy1*x0[_I(i1,j1,k0)]);
				v1 = sx0*(sy0*x0[_I(i0,j0,k1)]+sy1*x0[_I(i0,j1,k1)])+sx1*(sy0*x0[_I(i1,j0,k1)]+sy1*x0[_I(i1,j1,k1)]);
				x[_I(i,j,k)] = sz0*v0 + sz1*v1;
				v0 = sx0*(sy0*y0[_I(i0,j0,k0)]+sy1*y0[_I(i0,j1,k0)])+sx1*(sy0*y0[_I(i1,j0,k0)]+sy1*y0[_I(i1,j1,k0)]);
				v1 = sx0*(sy0*y0[_I(i0,j0,k1)]+sy1*y0[_I(i0,j1,k1)])+sx1*(sy0*y0[_I(i1,j0,k1)]+sy1*y0[_I(i1,j1,k1)]);
				y[_I(i,j,k)] = (sz0*v0 + sz1*v1)*c0;
			}
		}
	}
	set_bnd(b,x, N);
	set_bnd(b,y, N);
}

void fproject(float* u, float* u0, float* v, float* v0, float* w, float* w0, int N)
{
	float* p = u0;	float* div = v0;	// temporary buffers, use old velocity buffers
	int i, j, k;
	float h;
	h = 1.0f/N;
	for (k=1; k<=N; k++) {
		for (j=1; j<=N; j++) {
			for (i=1; i<=N; i++) {
				div[_I(i,j,k)] = -h*(
				u[_I(i+1,j,k)]-u[_I(i-1,j,k)]+
				v[_I(i,j+1,k)]-v[_I(i,j-1,k)]+
				w[_I(i,j,k+1)]-w[_I(i,j,k-1)])*0.5;
				p[_I(i,j,k)] = 0;
			}
		}
	}
	set_bnd(0, div, N); 
	set_bnd(0, p, N);
	lin_solve(0, p, div, 1, 6, N);
	
	for (k=1; k<=N; k++) {
		for (j=1; j<=N; j++) {
			for (i=1; i<=N; i++) {
				u[_I(i,j,k)] -= (p[_I(i+1,j,k)]-p[_I(i-1,j,k)])*0.5*N;
				v[_I(i,j,k)] -= (p[_I(i,j+1,k)]-p[_I(i,j-1,k)])*0.5*N;
				w[_I(i,j,k)] -= (p[_I(i,j,k+1)]-p[_I(i,j,k-1)])*0.5*N;
			}
		}
	}
	set_bnd(1, u, N); 
	set_bnd(2, v, N);
	set_bnd(3, w, N);
}

void vorticity_confinement(float *T0, float* u, float* u0, float* v, float* v0, float* w, float* w0, float dt, float vc_eps, int N)
{
	int i,j,k,ijk;
	float *curlx = u0, *curly = v0, *curlz=w0, *curl=T0;		// temp buffers
	float dt0 = dt * vc_eps;
	float x,y,z;


	for (k=1; k<N; k++) {
		for (j=1; j<N; j++) {
			for (i=1; i<N; i++) {
				ijk = _I(i,j,k);
					// curlx = dw/dy - dv/dz
				x = curlx[ijk] = (w[_I(i,j+1,k)] - w[_I(i,j-1,k)]) * 0.5f -
						(v[_I(i,j,k+1)] - v[_I(i,j,k-1)]) * 0.5f;

					// curly = du/dz - dw/dx
				y = curly[ijk] = (u[_I(i,j,k+1)] - u[_I(i,j,k-1)]) * 0.5f -
						(w[_I(i+1,j,k)] - w[_I(i-1,j,k)]) * 0.5f;

					// curlz = dv/dx - du/dy
				z = curlz[ijk] = (v[_I(i+1,j,k)] - v[_I(i-1,j,k)]) * 0.5f -
						(u[_I(i,j+1,k)] - u[_I(i,j-1,k)]) * 0.5f;

					// curl = |curl|
				curl[ijk] = sqrtf(x*x+y*y+z*z);
			}
		}
	}

	for (k=1; k<N; k++) {
		for (j=1; j<N; j++) {
			for (i=1; i<N; i++) {
				float Nx = (curl[_I(i+1,j,k)] - curl[_I(i-1,j,k)]) * 0.5f;
				float Ny = (curl[_I(i,j+1,k)] - curl[_I(i,j-1,k)]) * 0.5f;
				float Nz = (curl[_I(i,j,k+1)] - curl[_I(i,j,k-1)]) * 0.5f;
				float len1 = 1.0f/(sqrtf(Nx*Nx+Ny*Ny+Nz*Nz)+0.0000001f);
				ijk = _I(i,j,k);
				Nx *= len1;
				Ny *= len1;
				Nz *= len1;
				u[ijk] += (Ny*curlz[ijk] - Nz*curly[ijk]) * dt0;
				v[ijk] += (Nz*curlx[ijk] - Nx*curlz[ijk]) * dt0;
				w[ijk] += (Nx*curly[ijk] - Ny*curlx[ijk]) * dt0;
			}
		}
	}
}



#define DIFFUSE
#define ADVECT

void vel_step(float *su, float *sv, float *sw, float* u, float* u0, float* v, float* v0, float* w, float* w0, float *T, float *T0, float dt, int N)
{
	add_source(su, u, dt, N);
	add_source(sv, v, dt, N);
	add_source(sw, w, dt, N);
	
	// external force
	add_buoyancy(v, T, dt, m_buoyancy, N); // better is using gravity normal vector instead of v
	
	vorticity_confinement(T0, u, u0, v, v0, w, w0, dt, m_vc_eps, N);

#ifdef DIFFUSE
	SWAPFPTR(u0, u); SWAPFPTR(v0, v); SWAPFPTR(w0, w);
	diffuse(1, u0, u, m_viscosity, dt, N);
	diffuse(2, v0, v, m_viscosity, dt, N);
	diffuse(3, w0, w, m_viscosity, dt, N);
	fproject(u, u0, v, v0, w, w0, N);
#endif
#ifdef ADVECT
	SWAPFPTR(u0, u); SWAPFPTR(v0, v); SWAPFPTR(w0, w);
	advect(1, u0, u, u0, v0, w0, dt, N);
	advect(2, v0, v, u0, v0, w0, dt, N);
	advect(3, w0, w, u0, v0, w0, dt, N);
	fproject(u, u0, v, v0, w, w0, N);
#endif
}

void dens_step(float *sd, float *d, float *d0, float *u, float *v, float *w, float dt, int N)
{
	add_source(sd, d, dt, N);
#ifdef DIFFUSE
	SWAPFPTR(d0, d);
	diffuse(0, d0, d, m_diffusion, dt, N);
#endif
#ifdef ADVECT
	SWAPFPTR(d0, d);
	advect(0, d0, d, u, v, w, dt, N);
#endif
}

void dens_temp_step(float *sd, float *sT, float *T, float *T0, float *d, float *d0, float *u, float *v, float *w, float dt, int N)
{
	add_source(sd, d, dt, N);
	add_source(sT, T, dt, N);
	SWAPFPTR(d0, d);
	diffuse(0, d0, d, m_diffusion, dt, N);
	SWAPFPTR(d0, d);
	SWAPFPTR(T0, T);
	advect_cool(0, d0, d, T0, T, u, v, w, dt, m_cooling, N);
}

void step(fc *m_fc, float dt)
{
	vel_step(su, sv, sw, m_fc->u, m_fc->u0, m_fc->v, m_fc->v0, m_fc->w, m_fc->w0, m_fc->T, m_fc->T0, dt, GRID_SIZE);
	dens_temp_step(sd, sT, m_fc->T, m_fc->T0, m_fc->d, m_fc->d0, m_fc->u, m_fc->v, m_fc->w, dt, GRID_SIZE);
}



void clear_buffer(float* x)
{
	int i;
	for (i=0; i<SIZE; i++) {
		x[i] = 0.0f;
	}
}

void clear_sources(float *sd, float *su, float *sv)
{
	int i;
	for (i=0; i<SIZE; i++) {
		sd[i] = su[i] = sv[i] = 0.0f;
	}
}

fc *f_init(void)
{
	int i;
	int size;
	
	fc *m_fc = MEM_callocN(sizeof(fc),
				      "f_c");
	for (i=0; i<10; i++)
		clear_buffer(buffers[i]);

	i=0;
	m_fc->d=buffers[i++]; m_fc->d0=buffers[i++];
	m_fc->T=buffers[i++]; m_fc->T0=buffers[i++];
	m_fc->u=buffers[i++]; m_fc->u0=buffers[i++];
	m_fc->v=buffers[i++]; m_fc->v0=buffers[i++];
	m_fc->w=buffers[i++]; m_fc->w0=buffers[i++];

	clear_sources(sd, su, sv);

	size=(GRID_SIZE+2)*(GRID_SIZE+2)*(GRID_SIZE+2);
	for (i=0; i<size; i++)
		m_fc->v[i] = -0.5f;
	
	return m_fc;
}

void f_free(fc *m_fc)
{
	if(m_fc)
		MEM_freeN(m_fc);
}

