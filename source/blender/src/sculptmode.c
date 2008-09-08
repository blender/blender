/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the Sculpt Mode tools
 *
 * BDR_sculptmode.h
 *
 */

#include "GHOST_Types.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_color_types.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_sculpt.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_colortools.h"

#include "BIF_editkey.h"
#include "BIF_editview.h"
#include "BIF_glutil.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_radialcontrol.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BDR_sculptmode.h"

#include "BSE_drawview.h"
#include "BSE_edit.h"
#include "BSE_view.h"

#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "multires.h"
#include "mydevice.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h" /*for multitex_ext*/

#include "GPU_draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Number of vertices to average in order to determine the flatten distance */
#define FLATTEN_SAMPLE_SIZE 10

/* Texture cache size */
#define TC_SIZE 256

/* ===== STRUCTS =====
 *
 */

/* Used by vertex_users to store face indices in a list */
typedef struct IndexNode {
	struct IndexNode* next,* prev;
	int Index;
} IndexNode;


/* ActiveData stores an Index into the mvert array of Mesh, plus Fade, which
   stores how far the vertex is from the brush center, scaled to the range [0,1]. */
typedef struct ActiveData {
	struct ActiveData *next, *prev;
	unsigned int Index;
	float Fade;
	float dist;
} ActiveData;

typedef struct BrushActionSymm {
	float center_3d[3];
	char index;

	float up[3], right[3], out[3];

	/* Grab brush */
	float grab_delta[3];
} BrushActionSymm;

typedef struct BrushAction {
	BrushActionSymm symm;

	char firsttime;

	/* Some brushes need access to original mesh vertices */
 	vec3f *mesh_store;
	short (*orig_norms)[3];

	short mouse[2];
	float size_3d;

	float prev_radius;
	float radius;

	float *layer_disps;
	char flip;

	char clip[3];
	float cliptol[3];

	float anchored_rot;

	/* Grab brush */
	ListBase grab_active_verts[8];
	float depth;

	/* Adjust brush strength along each axis
	   to adjust for object scaling */
	float scale[3];
} BrushAction;

typedef struct RectNode {
	struct RectNode *next, *prev;
	rcti r;
} RectNode;

/* Used to store to 2D screen coordinates of each vertex in the mesh. */
typedef struct ProjVert {
	short co[2];
	
	/* Used to mark whether a vertex is inside a rough bounding box
	   containing the brush. */
	char inside;
} ProjVert;

static Object *active_ob= NULL;

SculptData *sculpt_data(void)
{
	return &G.scene->sculptdata;
}

void sculpt_init_session(void);
void init_brushaction(BrushAction *a, short *, short *);
void sculpt_undo_push(const short);

SculptSession *sculpt_session(void)
{
	if(!sculpt_data()->session)
		sculpt_init_session();
	return sculpt_data()->session;
}

/* ===== MEMORY =====
 * 
 * Allocate/initialize/free data
 */

void sculpt_init_session(void)
{
	if(sculpt_data()->session)
		sculptsession_free(G.scene);
	sculpt_data()->session= MEM_callocN(sizeof(SculptSession), "SculptSession");
}

/* vertex_users is an array of Lists that store all the faces that use a
   particular vertex. vertex_users is in the same order as mesh.mvert */
void calc_vertex_users()
{
	SculptSession *ss= sculpt_session();
	int i,j;
	IndexNode *node= NULL;
	Mesh *me= get_mesh(OBACT);

	sculpt_vertexusers_free(ss);
	
	/* For efficiency, use vertex_users_mem as a memory pool (may be larger
	   than necessary if mesh has triangles, but only one alloc is needed.) */
	ss->vertex_users= MEM_callocN(sizeof(ListBase) * me->totvert, "vertex_users");
	ss->vertex_users_size= me->totvert;
	ss->vertex_users_mem= MEM_callocN(sizeof(IndexNode)*me->totface*4, "vertex_users_mem");
	node= ss->vertex_users_mem;

	/* Find the users */
	for(i=0; i<me->totface; ++i){
		for(j=0; j<(me->mface[i].v4?4:3); ++j, ++node) {
			node->Index=i;
			BLI_addtail(&ss->vertex_users[((unsigned int*)(&me->mface[i]))[j]], node);
		}
	}
}

/* ===== INTERFACE =====
 */

void sculptmode_rem_tex(void *junk0,void *junk1)
{
	MTex *mtex= G.scene->sculptdata.mtex[G.scene->sculptdata.texact];
	if(mtex) {
		SculptSession *ss= sculpt_session();
		if(mtex->tex) mtex->tex->id.us--;
		MEM_freeN(mtex);
		G.scene->sculptdata.mtex[G.scene->sculptdata.texact]= NULL;
		/* Clear brush preview */
		if(ss->texcache) {
			MEM_freeN(ss->texcache);
			ss->texcache= NULL;
		}
		BIF_undo_push("Unlink brush texture");
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWOOPS, 0);
	}
}

/* ===== OPENGL =====
 *
 * Simple functions to get data from the GL
 */

/* Store the modelview and projection matrices and viewport. */
void init_sculptmatrices()
{
	SculptSession *ss= sculpt_session();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMultMatrixf(OBACT->obmat);

	if(!ss->mats)
		ss->mats = MEM_callocN(sizeof(bglMats), "sculpt bglmats");
	bgl_get_mats(ss->mats);
	
	glPopMatrix();

}

/* Uses window coordinates (x,y) to find the depth in the GL depth buffer. If
   available, G.vd->depths is used so that the brush doesn't sculpt on top of
   itself (G.vd->depths is only updated at the end of a brush stroke.) */
float get_depth(short x, short y)
{
	float depth;

	if(x<0 || y<0) return 1;
	if(x>=curarea->winx || y>=curarea->winy) return 1;

	if(G.vd->depths && x<G.vd->depths->w && y<G.vd->depths->h)
		return G.vd->depths->depths[y*G.vd->depths->w+x];
	
	x+= curarea->winrct.xmin;
	y+= curarea->winrct.ymin;
	
	glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

	return depth;
}

/* Uses window coordinates (x,y) and depth component z to find a point in
   modelspace */
void unproject(float out[3], const short x, const short y, const float z)
{
	SculptSession *ss= sculpt_session();
	double ux, uy, uz;

        gluUnProject(x,y,z, ss->mats->modelview, ss->mats->projection,
		     (GLint *)ss->mats->viewport, &ux, &uy, &uz );
	out[0] = ux;
	out[1] = uy;
	out[2] = uz;
}

/* Convert a point in model coordinates to 2D screen coordinates. */
static void projectf(const float v[3], float p[2])
{
	SculptSession *ss= sculpt_session();
	double ux, uy, uz;

	gluProject(v[0],v[1],v[2], ss->mats->modelview, ss->mats->projection,
		   (GLint *)ss->mats->viewport, &ux, &uy, &uz);
	p[0]= ux;
	p[1]= uy;
}

static void project(const float v[3], short p[2])
{
	float f[2];
	projectf(v, f);

	p[0]= f[0];
	p[1]= f[1];
}

/* ===== Sculpting =====
 *
 */

/* Return modified brush size. Uses current tablet pressure (if available) to
   shrink the brush. Skipped for grab brush because only the first mouse down
   size is used, which is small if the user has just touched the pen to the
   tablet */
char brush_size()
{
	const BrushData *b= sculptmode_brush();
	float size= b->size;
	float pressure= get_pressure();
	short activedevice= get_activedevice();
	
	if(sculpt_data()->brush_type!=GRAB_BRUSH) {
		const float size_factor= G.scene->sculptdata.tablet_size / 10.0f;
		if(ELEM(activedevice, DEV_STYLUS, DEV_ERASER))
			size*= G.scene->sculptdata.tablet_size==0?1:
			       (1-size_factor) + pressure*size_factor;
	}
	
	return size;
}

/* Return modified brush strength. Includes the direction of the brush, positive
   values pull vertices, negative values push. Uses tablet pressure and a
   special multiplier found experimentally to scale the strength factor. */
float brush_strength(BrushAction *a)
{
	const BrushData* b= sculptmode_brush();
	float dir= b->dir==1 ? 1 : -1;
	float pressure= 1;
	short activedevice= get_activedevice();
	float flip= a->flip ? -1:1;
	float anchored = b->flag & SCULPT_BRUSH_ANCHORED ? 25 : 1;

	const float strength_factor= G.scene->sculptdata.tablet_strength / 10.0f;
	if(ELEM(activedevice, DEV_STYLUS, DEV_ERASER))
		pressure= G.scene->sculptdata.tablet_strength==0?1:
				  (1-strength_factor) + get_pressure()*strength_factor;
	
	/* Flip direction for eraser */
	if(activedevice==DEV_ERASER)
		dir= -dir;

	switch(G.scene->sculptdata.brush_type){
	case DRAW_BRUSH:
	case LAYER_BRUSH:
		return b->strength / 5000.0f * dir * pressure * flip * anchored * G.vd->grid;
	case SMOOTH_BRUSH:
		return b->strength / 50.0f * pressure * anchored;
	case PINCH_BRUSH:
		return b->strength / 1000.0f * dir * pressure * flip * anchored;
	case GRAB_BRUSH:
		return 1;
	case INFLATE_BRUSH:
		return b->strength / 5000.0f * dir * pressure * flip * anchored;
	case FLATTEN_BRUSH:
		return b->strength / 500.0f * pressure * anchored;
	default:
		return 0;
	}
}

/* For clipping against a mirror modifier */
void sculpt_clip(const BrushAction *a, float *co, const float val[3])
{
	char i;
	for(i=0; i<3; ++i) {
		if(a->clip[i] && (fabs(co[i]) <= a->cliptol[i]))
			co[i]= 0.0f;
		else
			co[i]= val[i];
	}		
}

void sculpt_axislock(float *co)
{
	SculptData *sd = sculpt_data();
	if (sd->axislock == AXISLOCK_X+AXISLOCK_Y+AXISLOCK_Z) return;
	if (G.vd->twmode == V3D_MANIP_LOCAL) {
		float mat[3][3], imat[3][3];
		Mat3CpyMat4(mat, OBACT->obmat);
		Mat3Inv(imat, mat);
		Mat3MulVecfl(mat, co);
		if (sd->axislock & AXISLOCK_X) co[0] = 0.0;
		if (sd->axislock & AXISLOCK_Y) co[1] = 0.0;
		if (sd->axislock & AXISLOCK_Z) co[2] = 0.0;		
		Mat3MulVecfl(imat, co);
	} else {
		if (sd->axislock & AXISLOCK_X) co[0] = 0.0;
		if (sd->axislock & AXISLOCK_Y) co[1] = 0.0;
		if (sd->axislock & AXISLOCK_Z) co[2] = 0.0;		
	}
}

static void add_norm_if(float view_vec[3], float out[3], float out_flip[3], const short no[3])
{
	float fno[3] = {no[0], no[1], no[2]};

	Normalize(fno);

	if((Inpf(view_vec, fno)) > 0) {
		VecAddf(out, out, fno);
	} else {
		VecAddf(out_flip, out_flip, fno); /* out_flip is used when out is {0,0,0} */
	}
}

/* Currently only for the draw brush; finds average normal for all active
   vertices */
void calc_area_normal(float out[3], const BrushAction *a, const float *outdir, const ListBase* active_verts)
{
	Mesh *me = get_mesh(OBACT);
	ActiveData *node = active_verts->first;
	SculptData *sd = sculpt_data();
	const int view = sd->brush_type==DRAW_BRUSH ? sculptmode_brush()->view : 0;
	float out_flip[3];
	
	out[0]=out[1]=out[2] = out_flip[0]=out_flip[1]=out_flip[2] = 0;

	if(sculptmode_brush()->flag & SCULPT_BRUSH_ANCHORED) {
		for(; node; node = node->next)
			add_norm_if(((BrushAction*)a)->symm.out, out, out_flip, a->orig_norms[node->Index]);
	}
	else {
		for(; node; node = node->next)
			add_norm_if(((BrushAction*)a)->symm.out, out, out_flip, me->mvert[node->Index].no);
	}

	if (out[0]==0.0 && out[1]==0.0 && out[2]==0.0) {
		VECCOPY(out, out_flip);
	}
	
	Normalize(out);

	if(outdir) {
		out[0] = outdir[0] * view + out[0] * (10-view);
		out[1] = outdir[1] * view + out[1] * (10-view);
		out[2] = outdir[2] * view + out[2] * (10-view);
	}
	
	Normalize(out);
}

void do_draw_brush(const BrushAction *a, const ListBase* active_verts)
{
	Mesh *me= get_mesh(OBACT);
	float area_normal[3];
	ActiveData *node= active_verts->first;

	calc_area_normal(area_normal, a, a->symm.out, active_verts);
	
	sculpt_axislock(area_normal);
	
	while(node){
		float *co= me->mvert[node->Index].co;
		
		const float val[3]= {co[0]+area_normal[0]*node->Fade*a->scale[0],
		                     co[1]+area_normal[1]*node->Fade*a->scale[1],
		                     co[2]+area_normal[2]*node->Fade*a->scale[2]};
		                     
		sculpt_clip(a, co, val);
		
		node= node->next;
	}
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
   a smoothed location for vert. Skips corner vertices (used by only one
   polygon.) */
vec3f neighbor_average(const int vert)
{
	SculptSession *ss= sculpt_session();
	Mesh *me= get_mesh(OBACT);
	int i, skip= -1, total=0;
	IndexNode *node= ss->vertex_users[vert].first;
	vec3f avg= {0,0,0};
	char ncount= BLI_countlist(&ss->vertex_users[vert]);
	MFace *f;
		
	/* Don't modify corner vertices */
	if(ncount==1) {
		VecCopyf(&avg.x, me->mvert[vert].co);
		return avg;
	}

	while(node){
		f= &me->mface[node->Index];
		
		if(f->v4) {
			skip= (f->v1==vert?2:
			       f->v2==vert?3:
			       f->v3==vert?0:
			       f->v4==vert?1:-1);
		}

		for(i=0; i<(f->v4?4:3); ++i) {
			if(i != skip && (ncount!=2 || BLI_countlist(&ss->vertex_users[(&f->v1)[i]]) <= 2)) {
				VecAddf(&avg.x,&avg.x,me->mvert[(&f->v1)[i]].co);
				++total;
			}
		}

		node= node->next;
	}

	if(total>0) {
		avg.x/= total;
		avg.y/= total;
		avg.z/= total;
	}
	else
		VecCopyf(&avg.x, me->mvert[vert].co);

	return avg;
}

void do_smooth_brush(const BrushAction *a, const ListBase* active_verts)
{
	ActiveData *node= active_verts->first;
	Mesh *me= get_mesh(OBACT);

	while(node){
		float *co= me->mvert[node->Index].co;
		const vec3f avg= neighbor_average(node->Index);
		const float val[3]= {co[0]+(avg.x-co[0])*node->Fade,
		                     co[1]+(avg.y-co[1])*node->Fade,
		                     co[2]+(avg.z-co[2])*node->Fade};
		sculpt_clip(a, co, val);
		node= node->next;
	}
}

void do_pinch_brush(const BrushAction *a, const ListBase* active_verts)
{
	Mesh *me= get_mesh(OBACT);
 	ActiveData *node= active_verts->first;

	while(node) {
		float *co= me->mvert[node->Index].co;
		const float val[3]= {co[0]+(a->symm.center_3d[0]-co[0])*node->Fade,
		                     co[1]+(a->symm.center_3d[1]-co[1])*node->Fade,
		                     co[2]+(a->symm.center_3d[2]-co[2])*node->Fade};
		sculpt_clip(a, co, val);
		node= node->next;
	}
}

void do_grab_brush(BrushAction *a)
{
	Mesh *me= get_mesh(OBACT);
	ActiveData *node= a->grab_active_verts[a->symm.index].first;
	float add[3];
	float grab_delta[3];
	
	VecCopyf(grab_delta, a->symm.grab_delta);
	sculpt_axislock(grab_delta);
	
	while(node) {
		float *co= me->mvert[node->Index].co;
		
		VecCopyf(add, grab_delta);
		VecMulf(add, node->Fade);
		VecAddf(add, add, co);
		sculpt_clip(a, co, add);

		node= node->next;
	}
	
}

void do_layer_brush(BrushAction *a, const ListBase *active_verts)
{
	Mesh *me= get_mesh(OBACT);
	float area_normal[3];
	ActiveData *node= active_verts->first;
	const float bstr= brush_strength(a);

	calc_area_normal(area_normal, a, NULL, active_verts);

	while(node){
		float *disp= &a->layer_disps[node->Index];
		
		if((bstr > 0 && *disp < bstr) ||
		  (bstr < 0 && *disp > bstr)) {
		  	float *co= me->mvert[node->Index].co;
		  	
			*disp+= node->Fade;

			if(bstr < 0) {
				if(*disp < bstr)
					*disp = bstr;
			} else {
				if(*disp > bstr)
					*disp = bstr;
			}

			{
				const float val[3]= {a->mesh_store[node->Index].x+area_normal[0] * *disp*a->scale[0],
				                     a->mesh_store[node->Index].y+area_normal[1] * *disp*a->scale[1],
				                     a->mesh_store[node->Index].z+area_normal[2] * *disp*a->scale[2]};
				sculpt_clip(a, co, val);
			}
		}

		node= node->next;
	}
}

void do_inflate_brush(const BrushAction *a, const ListBase *active_verts)
{
	ActiveData *node= active_verts->first;
	float add[3];
	Mesh *me= get_mesh(OBACT);
	
	while(node) {
		float *co= me->mvert[node->Index].co;
		short *no= me->mvert[node->Index].no;

		add[0]= no[0]/ 32767.0f;
		add[1]= no[1]/ 32767.0f;
		add[2]= no[2]/ 32767.0f;
		VecMulf(add, node->Fade);
		add[0]*= a->scale[0];
		add[1]*= a->scale[1];
		add[2]*= a->scale[2];
		VecAddf(add, add, co);
		
		sculpt_clip(a, co, add);

		node= node->next;
	}
}

void calc_flatten_center(Mesh *me, ActiveData *node, float co[3])
{
	ActiveData *outer[FLATTEN_SAMPLE_SIZE];
	int i;
	
	for(i = 0; i < FLATTEN_SAMPLE_SIZE; ++i)
		outer[i] = node;
		
	for(; node; node = node->next) {
		for(i = 0; i < FLATTEN_SAMPLE_SIZE; ++i) {
			if(node->dist > outer[i]->dist) {
				outer[i] = node;
				break;
			}
		}
	}
	
	co[0] = co[1] = co[2] = 0.0f;
	for(i = 0; i < FLATTEN_SAMPLE_SIZE; ++i)
		VecAddf(co, co, me->mvert[outer[i]->Index].co);
	VecMulf(co, 1.0f / FLATTEN_SAMPLE_SIZE);
}

void do_flatten_brush(const BrushAction *a, const ListBase *active_verts)
{
	Mesh *me= get_mesh(OBACT);
	ActiveData *node= active_verts->first;
	/* area_normal and cntr define the plane towards which vertices are squashed */
	float area_normal[3];
	float cntr[3];

	calc_area_normal(area_normal, a, a->symm.out, active_verts);
	calc_flatten_center(me, node, cntr);

	while(node){
		float *co= me->mvert[node->Index].co;
		float p1[3], sub1[3], sub2[3], intr[3], val[3];
		
		/* Find the intersection between squash-plane and vertex (along the area normal) */
		VecSubf(p1, co, area_normal);
		VecSubf(sub1, cntr, p1);
		VecSubf(sub2, co, p1);
		VecSubf(intr, co, p1);
		VecMulf(intr, Inpf(area_normal, sub1) / Inpf(area_normal, sub2));
		VecAddf(intr, intr, p1);
		
		VecSubf(val, intr, co);
		VecMulf(val, node->Fade);
		VecAddf(val, val, co);
		
		sculpt_clip(a, co, val);
		
		node= node->next;
	}
}

/* Uses the brush curve control to find a strength value between 0 and 1 */
float curve_strength(float p, const float len)
{
	if(p > len) p= len;
	return curvemapping_evaluateF(G.scene->sculptdata.cumap, 0, p/len);
}

/* Uses symm to selectively flip any axis of a coordinate. */
void flip_coord(float co[3], const char symm)
{
	if(symm & SYMM_X)
		co[0]= -co[0];
	if(symm & SYMM_Y)
		co[1]= -co[1];
	if(symm & SYMM_Z)
		co[2]= -co[2];
}

/* Use the warpfac field in MTex to store a rotation value for sculpt textures. Value is in degrees */
float sculpt_tex_angle(void)
{
	SculptData *sd= sculpt_data();
	if(sd->texact!=-1 && sd->mtex[sd->texact])
		return sd->mtex[sd->texact]->warpfac;
	return 0;
}

void set_tex_angle(const float f)
{
	SculptData *sd = sculpt_data();
	if(sd->texact != -1 && sd->mtex[sd->texact])
		sd->mtex[sd->texact]->warpfac = f;
}
	
float to_rad(const float deg)
{
	return deg * (M_PI/180.0f);
}

float to_deg(const float rad)
{
	return rad * (180.0f/M_PI);
}

/* Get a pixel from the texcache at (px, py) */
static unsigned char get_texcache_pixel(const SculptSession *ss, int px, int py)
{
	unsigned *p;
	p = ss->texcache + py * ss->texcache_w + px;
	return ((unsigned char*)(p))[0];
}

static float get_texcache_pixel_bilinear(const SculptSession *ss, float u, float v)
{
	int x, y, x2, y2;
	const int tc_max = TC_SIZE - 1;
	float urat, vrat, uopp;

	if(u < 0) u = 0;
	else if(u >= TC_SIZE) u = tc_max;
	if(v < 0) v = 0;
	else if(v >= TC_SIZE) v = tc_max;

	x = floor(u);
	y = floor(v);
	x2 = x + 1;
	y2 = y + 1;

	if(x2 > TC_SIZE) x2 = tc_max;
	if(y2 > TC_SIZE) y2 = tc_max;
	
	urat = u - x;
	vrat = v - y;
	uopp = 1 - urat;
		
	return ((get_texcache_pixel(ss, x, y) * uopp +
		 get_texcache_pixel(ss, x2, y) * urat) * (1 - vrat) + 
		(get_texcache_pixel(ss, x, y2) * uopp +
		 get_texcache_pixel(ss, x2, y2) * urat) * vrat) / 255.0;
}

/* Return a multiplier for brush strength on a particular vertex. */
float tex_strength(BrushAction *a, float *point, const float len,const unsigned vindex)
{
	SculptData *sd= sculpt_data();
	SculptSession *ss= sculpt_session();
	float avg= 1;

	if(sd->texact==-1 || !sd->mtex[sd->texact])
		avg= 1;
	else if(sd->texrept==SCULPTREPT_3D) {
		/* Get strength by feeding the vertex location directly
		   into a texture */
		float jnk;
		const float factor= 0.01;
		MTex mtex;
		memset(&mtex,0,sizeof(MTex));
		mtex.tex= sd->mtex[sd->texact]->tex;
		mtex.projx= 1;
		mtex.projy= 2;
		mtex.projz= 3;
		VecCopyf(mtex.size, sd->mtex[sd->texact]->size);
		VecMulf(mtex.size, factor);
		if(!sd->texsep)
			mtex.size[1]= mtex.size[2]= mtex.size[0];
		
		externtex(&mtex,point,&avg,&jnk,&jnk,&jnk,&jnk);
	}
	else if(ss->texcache) {
		const float bsize= a->radius * 2;
		const float rot= to_rad(sculpt_tex_angle()) + a->anchored_rot;
		int px, py;
		float flip[3], point_2d[2];

		/* If the active area is being applied for symmetry, flip it
		   across the symmetry axis in order to project it. This insures
		   that the brush texture will be oriented correctly. */
		VecCopyf(flip, point);
		flip_coord(flip, a->symm.index);
		projectf(flip, point_2d);

		/* For Tile and Drag modes, get the 2D screen coordinates of the
		   and scale them up or down to the texture size. */
		if(sd->texrept==SCULPTREPT_TILE) {
			const int sx= (const int)sd->mtex[sd->texact]->size[0];
			const int sy= (const int)sd->texsep ? sd->mtex[sd->texact]->size[1] : sx;
			
			float fx= point_2d[0];
			float fy= point_2d[1];
			
			float angle= atan2(fy, fx) - rot;
			float len= sqrtf(fx*fx + fy*fy);
			
			if(rot<0.001 && rot>-0.001) {
				px= point_2d[0];
				py= point_2d[1];
			} else {
				px= len * cos(angle) + 2000;
				py= len * sin(angle) + 2000;
			}
			if(sx != 1)
				px %= sx-1;
			if(sy != 1)
				py %= sy-1;
			avg= get_texcache_pixel_bilinear(ss, TC_SIZE*px/sx, TC_SIZE*py/sy);
		} else {
			float fx= (point_2d[0] - a->mouse[0]) / bsize;
			float fy= (point_2d[1] - a->mouse[1]) / bsize;

			float angle= atan2(fy, fx) - rot;
			float len= sqrtf(fx*fx + fy*fy);
			
			fx = len * cos(angle) + 0.5;
			fy = len * sin(angle) + 0.5;

			avg= get_texcache_pixel_bilinear(ss, fx * TC_SIZE, fy * TC_SIZE);
		}
	}

	if(sd->texfade)
		avg*= curve_strength(len, a->size_3d); /* Smooth curve */

	return avg;
}

/* Mark area around the brush as damaged. projverts are marked if they are
   inside the area and the damaged rectangle in 2D screen coordinates is 
   added to damaged_rects. */
void sculpt_add_damaged_rect(BrushAction *a)
{
	short p[2];
	RectNode *rn= MEM_mallocN(sizeof(RectNode),"RectNode");
	Mesh *me= get_mesh(OBACT);
	SculptSession *ss = sculpt_session();
	const float radius = a->radius > a->prev_radius ? a->radius : a->prev_radius;
	unsigned i;

	/* Find center */
	project(a->symm.center_3d, p);
	rn->r.xmin= p[0] - radius;
	rn->r.ymin= p[1] - radius;
	rn->r.xmax= p[0] + radius;
	rn->r.ymax= p[1] + radius;

	BLI_addtail(&sculpt_session()->damaged_rects, rn);

	/* Update insides */
	for(i=0; i<me->totvert; ++i) {
		if(!ss->projverts[i].inside) {
			if(ss->projverts[i].co[0] > rn->r.xmin && ss->projverts[i].co[1] > rn->r.ymin &&
			   ss->projverts[i].co[0] < rn->r.xmax && ss->projverts[i].co[1] < rn->r.ymax) {
				ss->projverts[i].inside= 1;
			}
		}
	}
}

/* Clears the depth buffer in each modified area. */
void sculpt_clear_damaged_areas(SculptSession *ss)
{
	RectNode *rn= NULL;

	for(rn = ss->damaged_rects.first; rn; rn = rn->next) {
		rcti clp = rn->r;
		rcti *win = &curarea->winrct;
		
		clp.xmin += win->xmin;
		clp.xmax += win->xmin;
		clp.ymin += win->ymin;
		clp.ymax += win->ymin;
		
		if(clp.xmin < win->xmax && clp.xmax > win->xmin &&
		   clp.ymin < win->ymax && clp.ymax > win->ymin) {
			if(clp.xmin < win->xmin) clp.xmin = win->xmin;
			if(clp.ymin < win->ymin) clp.ymin = win->ymin;
			if(clp.xmax > win->xmax) clp.xmax = win->xmax;
			if(clp.ymax > win->ymax) clp.ymax = win->ymax;

			glScissor(clp.xmin + 1, clp.ymin + 1,
				  clp.xmax - clp.xmin - 2,
				  clp.ymax - clp.ymin - 2);
		}
		
		glClear(GL_DEPTH_BUFFER_BIT);
	}
}

void do_brush_action(BrushAction *a)
{
	int i;
	float av_dist;
	ListBase active_verts={0,0};
	ActiveData *adata= 0;
	float *vert;
	Mesh *me= get_mesh(OBACT);
	const float bstrength= brush_strength(a);
	KeyBlock *keyblock= ob_get_keyblock(OBACT);
	SculptData *sd = sculpt_data();
	SculptSession *ss = sculpt_session();

	sculpt_add_damaged_rect(a);

	/* Build a list of all vertices that are potentially within the brush's
	   area of influence. Only do this once for the grab brush. */
	if((sd->brush_type != GRAB_BRUSH) || a->firsttime) {
		for(i=0; i<me->totvert; ++i) {
			/* Projverts.inside provides a rough bounding box */
			if(ss->projverts[i].inside) {
				vert= ss->vertexcosnos ? &ss->vertexcosnos[i*6] : me->mvert[i].co;
				av_dist= VecLenf(a->symm.center_3d, vert);
				if(av_dist < a->size_3d) {
					adata= (ActiveData*)MEM_mallocN(sizeof(ActiveData), "ActiveData");

					adata->Index = i;
					/* Fade is used to store the final strength at which the brush
					   should modify a particular vertex. */
					adata->Fade= tex_strength(a, vert, av_dist, i) * bstrength;
					adata->dist = av_dist;

					if(sd->brush_type == GRAB_BRUSH && a->firsttime)
						BLI_addtail(&a->grab_active_verts[a->symm.index], adata);
					else
						BLI_addtail(&active_verts, adata);
				}
			}
		}
	}

	/* Only act if some verts are inside the brush area */
	if(active_verts.first || (sd->brush_type == GRAB_BRUSH && a->grab_active_verts[a->symm.index].first)) {
		/* Apply one type of brush action */
		switch(G.scene->sculptdata.brush_type){
		case DRAW_BRUSH:
			do_draw_brush(a, &active_verts);
			break;
		case SMOOTH_BRUSH:
			do_smooth_brush(a, &active_verts);
			break;
		case PINCH_BRUSH:
			do_pinch_brush(a, &active_verts);
			break;
		case INFLATE_BRUSH:
			do_inflate_brush(a, &active_verts);
			break;
		case GRAB_BRUSH:
			do_grab_brush(a);
			break;
		case LAYER_BRUSH:
			do_layer_brush(a, &active_verts);
			break;
		case FLATTEN_BRUSH:
			do_flatten_brush(a, &active_verts);
			break;
		}
	
		/* Copy the modified vertices from mesh to the active key */
		if(keyblock) {
			float *co= keyblock->data;
			if(co) {
				if(sd->brush_type == GRAB_BRUSH)
					adata = a->grab_active_verts[a->symm.index].first;
				else
					adata = active_verts.first;

				for(; adata; adata= adata->next)
					if(adata->Index < keyblock->totelem)
						VecCopyf(&co[adata->Index*3], me->mvert[adata->Index].co);
			}
		}

		if(ss->vertexcosnos)
			BLI_freelistN(&active_verts);
		else {
			if(sd->brush_type != GRAB_BRUSH)
				addlisttolist(&ss->damaged_verts, &active_verts);
		}
	}
}

/* Flip all the editdata across the axis/axes specified by symm. Used to
   calculate multiple modifications to the mesh when symmetry is enabled. */
void calc_brushdata_symm(BrushAction *a, const char symm)
{
	flip_coord(a->symm.center_3d, symm);
	flip_coord(a->symm.up, symm);
	flip_coord(a->symm.right, symm);
	flip_coord(a->symm.out, symm);
	
	a->symm.index= symm;

	flip_coord(a->symm.grab_delta, symm);
}

void do_symmetrical_brush_actions(BrushAction *a, short co[2], short pr_co[2])
{
	const char symm = sculpt_data()->symm;
	BrushActionSymm orig;
	int i;

	init_brushaction(a, co, pr_co);
	orig = a->symm;
	do_brush_action(a);

	for(i = 1; i <= symm; ++i) {
		if(symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5))) {
			// Restore the original symmetry data
			a->symm = orig;

			calc_brushdata_symm(a, i);
			do_brush_action(a);
		}
	}

	a->symm = orig;
}

void add_face_normal(vec3f *norm, const MFace* face)
{
	Mesh *me= get_mesh(OBACT);

	vec3f c= {me->mvert[face->v1].co[0],me->mvert[face->v1].co[1],me->mvert[face->v1].co[2]};
	vec3f b= {me->mvert[face->v2].co[0],me->mvert[face->v2].co[1],me->mvert[face->v2].co[2]};
	vec3f a= {me->mvert[face->v3].co[0],me->mvert[face->v3].co[1],me->mvert[face->v3].co[2]};
	vec3f s1, s2;

	VecSubf(&s1.x,&a.x,&b.x);
	VecSubf(&s2.x,&c.x,&b.x);

	norm->x+= s1.y * s2.z - s1.z * s2.y;
	norm->y+= s1.z * s2.x - s1.x * s2.z;
	norm->z+= s1.x * s2.y - s1.y * s2.x;
}

void update_damaged_vert(Mesh *me, ListBase *lb)
{
	ActiveData *vert;
       
	for(vert= lb->first; vert; vert= vert->next) {
		vec3f norm= {0,0,0};		
		IndexNode *face= sculpt_session()->vertex_users[vert->Index].first;

		while(face){
			add_face_normal(&norm,&me->mface[face->Index]);
			face= face->next;
		}
		Normalize(&norm.x);
		
		me->mvert[vert->Index].no[0]=norm.x*32767;
		me->mvert[vert->Index].no[1]=norm.y*32767;
		me->mvert[vert->Index].no[2]=norm.z*32767;
	}
}

void calc_damaged_verts(ListBase *damaged_verts, BrushAction *a)
{
	Mesh *me= get_mesh(OBACT);
	int i;
	
	for(i=0; i<8; ++i)
		update_damaged_vert(me, &a->grab_active_verts[i]);

	update_damaged_vert(me, damaged_verts);
	BLI_freelistN(damaged_verts);
	damaged_verts->first = damaged_verts->last = NULL;
}

void projverts_clear_inside(SculptSession *ss)
{
	Mesh *me = get_mesh(OBACT);
	if(me) {
		int i;
		for(i = 0; i < me->totvert; ++i)
			ss->projverts[i].inside = 0;
	}
}

BrushData *sculptmode_brush(void)
{
	SculptData *sd= &G.scene->sculptdata;

	BrushData *bd = 
		(sd->brush_type==DRAW_BRUSH ? &sd->drawbrush :
		 sd->brush_type==SMOOTH_BRUSH ? &sd->smoothbrush :
		 sd->brush_type==PINCH_BRUSH ? &sd->pinchbrush :
		 sd->brush_type==INFLATE_BRUSH ? &sd->inflatebrush :
		 sd->brush_type==GRAB_BRUSH ? &sd->grabbrush :
		 sd->brush_type==LAYER_BRUSH ? &sd->layerbrush :
		 sd->brush_type==FLATTEN_BRUSH ? &sd->flattenbrush : NULL);

	if(!bd) {
		sculptdata_init(G.scene);
		bd = &sd->drawbrush;
	}

	return bd;
}

void sculptmode_update_tex()
{
	SculptData *sd= sculpt_data();
	SculptSession *ss= sculpt_session();
	MTex *mtex = sd->mtex[sd->texact];
	TexResult texres = {0};
	float x, y, step=2.0/TC_SIZE, co[3];
	int hasrgb, ix, iy;
	
	/* Skip Default brush shape and non-textures */
	if(sd->texact == -1 || !sd->mtex[sd->texact]) return;

	if(ss->texcache) {
		MEM_freeN(ss->texcache);
		ss->texcache= NULL;
	}
	
	ss->texcache_w = ss->texcache_h = TC_SIZE;
	ss->texcache = MEM_callocN(sizeof(int) * ss->texcache_w * ss->texcache_h, "Sculpt Texture cache");
	
	if(mtex && mtex->tex) {
		BKE_image_get_ibuf(sd->mtex[sd->texact]->tex->ima, NULL);
		
		/*do normalized cannonical view coords for texture*/
		for (y=-1.0, iy=0; iy<TC_SIZE; iy++, y += step) {
			for (x=-1.0, ix=0; ix<TC_SIZE; ix++, x += step) {
				co[0]= x;
				co[1]= y;
				co[2]= 0.0f;
				
				/* This is copied from displace modifier code */
				hasrgb = multitex_ext(mtex->tex, co, NULL, NULL, 1, &texres);
			
				/* if the texture gave an RGB value, we assume it didn't give a valid
				 * intensity, so calculate one (formula from do_material_tex).
				 * if the texture didn't give an RGB value, copy the intensity across
				 */
				if(hasrgb & TEX_RGB)
					texres.tin = (0.35 * texres.tr + 0.45 *
					              texres.tg + 0.2 * texres.tb);

				texres.tin = texres.tin * 255.0;
				((char*)ss->texcache)[(iy*TC_SIZE+ix)*4] = (char)texres.tin;
				((char*)ss->texcache)[(iy*TC_SIZE+ix)*4+1] = (char)texres.tin;
				((char*)ss->texcache)[(iy*TC_SIZE+ix)*4+2] = (char)texres.tin;
				((char*)ss->texcache)[(iy*TC_SIZE+ix)*4+3] = (char)texres.tin;
			}
		}
	}
}

/* pr_mouse is only used for the grab brush, can be NULL otherwise */
void init_brushaction(BrushAction *a, short *mouse, short *pr_mouse)
{
	SculptData *sd = sculpt_data();
	const float mouse_depth = get_depth(mouse[0], mouse[1]);
	float brush_edge_loc[3], zero_loc[3], oldloc[3];
	ModifierData *md;
	int i;
	const char flip = (get_qual() == LR_SHIFTKEY);
 	const char anchored = sculptmode_brush()->flag & SCULPT_BRUSH_ANCHORED;
 	short orig_mouse[2], dx=0, dy=0;

	a->flip = flip;
	a->symm.index = 0;

	if(a->firsttime) 
		a->depth = mouse_depth;
	
	/* Convert the location and size of the brush to
	   modelspace coords */
	if(a->firsttime || !anchored) {
 		unproject(a->symm.center_3d, mouse[0], mouse[1], mouse_depth);
 		a->mouse[0] = mouse[0];
 		a->mouse[1] = mouse[1];
 	}
 
 	if(anchored) {
 		project(a->symm.center_3d, orig_mouse);
 		dx = mouse[0] - orig_mouse[0];
 		dy = mouse[1] - orig_mouse[1];
 	}
 
 	if(anchored) {
 		unproject(brush_edge_loc, mouse[0], mouse[1], a->depth);
 		a->anchored_rot = atan2(dy, dx);
 	}
 	else
 		unproject(brush_edge_loc, mouse[0] + brush_size(), mouse[1], mouse_depth);
 
	a->size_3d = VecLenf(a->symm.center_3d, brush_edge_loc);

	a->prev_radius = a->radius;

	if(anchored)
 		a->radius = sqrt(dx*dx + dy*dy);
 	else
 		a->radius = brush_size();

	/* Set the pivot to allow the model to rotate around the center of the brush */
	if(get_depth(mouse[0],mouse[1]) < 1.0)
		VecCopyf(sd->pivot, a->symm.center_3d);

	/* Now project the Up, Right, and Out normals from view to model coords */
	unproject(zero_loc, 0, 0, 0);
	unproject(a->symm.up, 0, -1, 0);
	unproject(a->symm.right, 1, 0, 0);
	unproject(a->symm.out, 0, 0, -1);
	VecSubf(a->symm.up, a->symm.up, zero_loc);
	VecSubf(a->symm.right, a->symm.right, zero_loc);
	VecSubf(a->symm.out, a->symm.out, zero_loc);
	Normalize(a->symm.up);
	Normalize(a->symm.right);
	Normalize(a->symm.out);
	
	/* Initialize mirror modifier clipping */
	for(i=0; i<3; ++i) {
		a->clip[i]= 0;
		a->cliptol[i]= 0;
	}
	for(md= OBACT->modifiers.first; md; md= md->next) {
		if(md->type==eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
			const MirrorModifierData *mmd = (MirrorModifierData*) md;
			
			if(mmd->flag & MOD_MIR_CLIPPING) {
				a->clip[mmd->axis]= 1;
				if(mmd->tolerance > a->cliptol[mmd->axis])
					a->cliptol[mmd->axis] = mmd->tolerance;
			}
		}
	}

	if(sd->brush_type == GRAB_BRUSH) {
		float gcenter[3];

		/* Find the delta */
		unproject(gcenter, mouse[0], mouse[1], a->depth);
		unproject(oldloc, pr_mouse[0], pr_mouse[1], a->depth);
		VecSubf(a->symm.grab_delta, gcenter, oldloc);
	}
	else if(sd->brush_type == LAYER_BRUSH) {
		Mesh *me= get_mesh(OBACT);

		if(!a->layer_disps)
			a->layer_disps= MEM_callocN(sizeof(float)*me->totvert,"Layer disps");
	}

	if(sd->brush_type == LAYER_BRUSH || anchored) {
 		Mesh *me= get_mesh(OBACT);
 		unsigned i;
 
 		if(!a->mesh_store) {
 			a->mesh_store= MEM_mallocN(sizeof(vec3f) * me->totvert, "Sculpt mesh store");
 			for(i = 0; i < me->totvert; ++i)
 				VecCopyf(&a->mesh_store[i].x, me->mvert[i].co);
  		}

		if(anchored && a->layer_disps)
			memset(a->layer_disps, 0, sizeof(float) * me->totvert);

		if(anchored && !a->orig_norms) {
			a->orig_norms= MEM_mallocN(sizeof(short) * 3 * me->totvert, "Sculpt orig norm");
			for(i = 0; i < me->totvert; ++i) {
				a->orig_norms[i][0] = me->mvert[i].no[0];
				a->orig_norms[i][1] = me->mvert[i].no[1];
				a->orig_norms[i][2] = me->mvert[i].no[2];
			}
		}
  	}
}
void sculptmode_set_strength(const int delta)
{
	int val = sculptmode_brush()->strength + delta;
	if(val < 1) val = 1;
	if(val > 100) val = 100;
	sculptmode_brush()->strength= val;
}

static void sculpt_radialcontrol_callback(const int mode, const int val)
{
	SculptSession *ss = sculpt_session();
	BrushData *br = sculptmode_brush();

	if(mode == RADIALCONTROL_SIZE)
		br->size = val;
	else if(mode == RADIALCONTROL_STRENGTH)
		br->strength = val;
	else if(mode == RADIALCONTROL_ROTATION)
		set_tex_angle(val);

	ss->radialcontrol = NULL;
}

/* Returns GL handle to brush texture */
static GLuint sculpt_radialcontrol_calctex()
{
	SculptData *sd= sculpt_data();
	SculptSession *ss= sculpt_session();
	int i, j;
	const int tsz = TC_SIZE;
	float *texdata= MEM_mallocN(sizeof(float)*tsz*tsz, "Brush preview");
	GLuint tex;

	if(sd->texrept!=SCULPTREPT_3D)
		sculptmode_update_tex();
	for(i=0; i<tsz; ++i)
		for(j=0; j<tsz; ++j) {
			float magn= sqrt(pow(i-tsz/2,2)+pow(j-tsz/2,2));
			if(sd->texfade)
				texdata[i*tsz+j]= curve_strength(magn,tsz/2);
			else
				texdata[i*tsz+j]= magn < tsz/2 ? 1 : 0;
		}
	if(sd->texact != -1 && ss->texcache) {
		for(i=0; i<tsz; ++i)
			for(j=0; j<tsz; ++j) {
				const int col= ss->texcache[i*tsz+j];
				texdata[i*tsz+j]*= (((char*)&col)[0]+((char*)&col)[1]+((char*)&col)[2])/3.0f/255.0f;
			}
	}
		
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tsz, tsz, 0, GL_ALPHA, GL_FLOAT, texdata);
	MEM_freeN(texdata);

	return tex;
}

void sculpt_radialcontrol_start(int mode)
{
	SculptData *sd = sculpt_data();
	SculptSession *ss = sculpt_session();
	BrushData *br = sculptmode_brush();
	int orig=1, max=100;

	if(mode == RADIALCONTROL_SIZE) {
		orig = br->size;
		max = 200;
	}
	else if(mode == RADIALCONTROL_STRENGTH) {
		orig = br->strength;
		max = 100;
	}
	else if(mode == RADIALCONTROL_ROTATION) {
		if(sd->texact!=-1 && sd->mtex[sd->texact]) {
			orig = sculpt_tex_angle();
			max = 360;
		}
		else
			mode = RADIALCONTROL_NONE;
	}

	if(mode != RADIALCONTROL_NONE) {
		ss->radialcontrol= radialcontrol_start(mode, sculpt_radialcontrol_callback, orig, max,
						       sculpt_radialcontrol_calctex());
	}
}

void sculpt_paint_brush(char clear)
{
	if(sculpt_data()->flags & SCULPT_DRAW_BRUSH) {
		static short mvalo[2];
		short mval[2];
		const short rad= sculptmode_brush()->size;

		getmouseco_areawin(mval);
		
		persp(PERSP_WIN);
		if(clear)
			fdrawXORcirc(mval[0], mval[1], rad);
		else
			draw_sel_circle(mval, mvalo, rad, rad, 0);
		
		mvalo[0]= mval[0];
		mvalo[1]= mval[1];
	}
}

void sculptmode_selectbrush_menu(void)
{
	SculptData *sd= sculpt_data();
	int val;
	
	pupmenu_set_active(sd->brush_type);
	
	val= pupmenu("Select Brush%t|Draw|Smooth|Pinch|Inflate|Grab|Layer|Flatten");

	if(val>0) {
		sd->brush_type= val;

		BIF_undo_push("Brush type");
		
		allqueue(REDRAWVIEW3D, 1);
		allqueue(REDRAWBUTSEDIT, 1);
	}
}

void sculptmode_update_all_projverts(float *vertcosnos)
{
	SculptSession *ss = sculpt_session();
	Mesh *me= get_mesh(OBACT);
	unsigned i;

	if(!ss->projverts)
		ss->projverts = MEM_mallocN(sizeof(ProjVert)*me->totvert,"ProjVerts");

	for(i=0; i<me->totvert; ++i) {
		project(vertcosnos ? &vertcosnos[i * 6] : me->mvert[i].co, ss->projverts[i].co);
		ss->projverts[i].inside= 0;
	}
}

void sculptmode_draw_wires(SculptSession *ss, int only_damaged, Mesh *me)
{
	int i;

	bglPolygonOffset(1.0);
	glDepthMask(0);
	BIF_ThemeColor((OBACT==OBACT)?TH_ACTIVE:TH_SELECT);

	for(i=0; i<me->totedge; i++) {
		MEdge *med= &me->medge[i];

		if((!only_damaged || (ss->projverts[med->v1].inside || ss->projverts[med->v2].inside)) &&
		   (med->flag & ME_EDGEDRAW)) {
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, &med->v1);
		}
	}

	glDepthMask(1);
	bglPolygonOffset(0.0);
}

void sculptmode_draw_mesh(int only_damaged) 
{
	Mesh *me= get_mesh(OBACT);
	int i, j, dt, drawCurrentMat = 1, matnr= -1;
	SculptSession *ss = sculpt_session();

	persp(PERSP_VIEW);
	mymultmatrix(OBACT->obmat);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	GPU_set_object_materials(G.scene, OBACT, 0, NULL);
	glEnable(GL_CULL_FACE);

	glShadeModel(GL_SMOOTH);

	glVertexPointer(3, GL_FLOAT, sizeof(MVert), &me->mvert[0].co);
	glNormalPointer(GL_SHORT, sizeof(MVert), &me->mvert[0].no);

	dt= MIN2(G.vd->drawtype, OBACT->dt);
	if(dt==OB_WIRE)
		glColorMask(0,0,0,0);

	for(i=0; i<me->totface; ++i) {
		MFace *f= &me->mface[i];
		char inside= 0;
		int new_matnr= f->mat_nr + 1;
		
		if(new_matnr != matnr)
			drawCurrentMat= GPU_enable_material(matnr = new_matnr, NULL);
		
		/* If only_damaged!=0, only draw faces that are partially
		   inside the area(s) modified by the brush */
		if(only_damaged) {
			for(j=0; j<(f->v4?4:3); ++j) {
				if(ss->projverts[*((&f->v1)+j)].inside) {
					inside= 1;
					break;
				}
			}
		}
		else
			inside= 1;
			
		if(inside && drawCurrentMat)
			glDrawElements(f->v4?GL_QUADS:GL_TRIANGLES, f->v4?4:3, GL_UNSIGNED_INT, &f->v1);
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glColorMask(1,1,1,1);

	if(dt==OB_WIRE || (OBACT->dtx & OB_DRAWWIRE))
		sculptmode_draw_wires(ss, only_damaged, me);

	glDisable(GL_DEPTH_TEST);
}

void sculptmode_correct_state(void)
{
	if(!sculpt_session())
		sculpt_init_session();

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	
	if(!sculpt_session()->vertex_users) calc_vertex_users();
}

/* Checks whether full update mode (slower) needs to be used to work with modifiers */
char sculpt_modifiers_active(Object *ob)
{
	ModifierData *md;
	
	for(md= modifiers_getVirtualModifierList(ob); md; md= md->next) {
		if(md->mode & eModifierMode_Realtime)
			return 1;
	}
	
	return 0;
}

void sculpt(void)
{
	SculptData *sd= sculpt_data();
	SculptSession *ss= sculpt_session();
	Object *ob= OBACT;
	/* lastSigMouse is for the rake, to store the last place the mouse movement was significant */
	short mouse[2], mvalo[2], lastSigMouse[2],firsttime=1, mousebut;
	short modifier_calculations= 0;
	BrushAction *a = MEM_callocN(sizeof(BrushAction), "brush action");
	short spacing= 32000;
	int scissor_box[4];
	float offsetRot;
	int smooth_stroke = 0, i;
	int anchored;

	if(!(G.f & G_SCULPTMODE) || G.obedit || !ob || ob->id.lib || !get_mesh(ob) || (get_mesh(ob)->totface == 0))
		return;
	if(!(ob->lay & G.vd->lay))
		error("Active object is not in this layer");
	if(ob_get_keyblock(ob)) {
		if(!(ob->shapeflag & OB_SHAPE_LOCK)) {
			error("Cannot sculpt on unlocked shape key");
			return;
		}
	}
	
	if(!ss) {
		sculpt_init_session();
		ss= sd->session;
	}

	anchored = sculptmode_brush()->flag & SCULPT_BRUSH_ANCHORED;
	smooth_stroke = (sd->flags & SCULPT_INPUT_SMOOTH) && (sd->brush_type != GRAB_BRUSH) && !anchored;

	if(smooth_stroke)
		sculpt_stroke_new(256);

	ss->damaged_rects.first = ss->damaged_rects.last = NULL;
	ss->damaged_verts.first = ss->damaged_verts.last = NULL;
	ss->vertexcosnos = NULL;

	/* Check that vertex users are up-to-date */
	if(ob != active_ob || !ss->vertex_users || ss->vertex_users_size != get_mesh(ob)->totvert) {
		sculpt_vertexusers_free(ss);
		calc_vertex_users();
		if(ss->projverts)
			MEM_freeN(ss->projverts);
		ss->projverts = NULL;
		active_ob= ob;
	}
		
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	persp(PERSP_VIEW);
	
	getmouseco_areawin(mvalo);

	/* Init texture
	   FIXME: Shouldn't be doing this every time! */
	if(sd->texrept!=SCULPTREPT_3D)
		sculptmode_update_tex();

	getmouseco_areawin(mouse);
	mvalo[0]= mouse[0];
	mvalo[1]= mouse[1];
	lastSigMouse[0]=mouse[0];
	lastSigMouse[1]=mouse[1];
	mousebut = L_MOUSE;

	/* If modifier_calculations is true, then extra time must be spent
	   updating the mesh. This takes a *lot* longer, so it's worth
	   skipping if the modifier stack is empty. */
	modifier_calculations= sculpt_modifiers_active(ob);

	init_sculptmatrices();

	if(modifier_calculations)
		ss->vertexcosnos= mesh_get_mapped_verts_nors(ob);
	sculptmode_update_all_projverts(ss->vertexcosnos);

	/* Set scaling adjustment */
	a->scale[0]= 1.0f / ob->size[0];
	a->scale[1]= 1.0f / ob->size[1];
	a->scale[2]= 1.0f / ob->size[2];

	/* Capture original copy */
	if(sd->flags & SCULPT_DRAW_FAST)
		glAccum(GL_LOAD, 1);

	/* Get original scissor box */
	glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
	
	/* For raking, get the original angle*/
	offsetRot=sculpt_tex_angle();
	
	while (get_mbut() & mousebut) {
		getmouseco_areawin(mouse);
		/* If rake, and the mouse has moved over 10 pixels (euclidean) (prevents jitter) then get the new angle */
		if (sd->rake && (pow(lastSigMouse[0]-mouse[0],2)+pow(lastSigMouse[1]-mouse[1],2))>100){
			/*Nasty looking, but just orig + new angle really*/
			set_tex_angle(offsetRot+180.+to_deg(atan2((float)(mouse[1]-lastSigMouse[1]),(float)(mouse[0]-lastSigMouse[0]))));
			lastSigMouse[0]=mouse[0];
			lastSigMouse[1]=mouse[1];
		}
		
		if(firsttime || mouse[0]!=mvalo[0] || mouse[1]!=mvalo[1] ||
		   sculptmode_brush()->flag & SCULPT_BRUSH_AIRBRUSH) {
			a->firsttime = firsttime;
			firsttime= 0;

			if(smooth_stroke)
				sculpt_stroke_add_point(mouse[0], mouse[1]);

			spacing+= sqrt(pow(mvalo[0]-mouse[0],2)+pow(mvalo[1]-mouse[1],2));

			if(modifier_calculations && !ss->vertexcosnos)
				ss->vertexcosnos= mesh_get_mapped_verts_nors(ob);

			if(G.scene->sculptdata.brush_type != GRAB_BRUSH) {
				if(anchored) {
					Mesh *me = get_mesh(ob);
					
 					/* Restore the mesh before continuing with anchored stroke */
 					if(a->mesh_store) {
 						for(i = 0; i < me->totvert; ++i) {
 							VecCopyf(me->mvert[i].co, &a->mesh_store[i].x);
							me->mvert[i].no[0] = a->orig_norms[i][0];
							me->mvert[i].no[1] = a->orig_norms[i][1];
							me->mvert[i].no[2] = a->orig_norms[i][2];
						}
 					}
					
  					do_symmetrical_brush_actions(a, mouse, NULL);
  				}
				else {
					if(smooth_stroke) {
						sculpt_stroke_apply(a);
					}
					else if(sd->spacing==0 || spacing>sd->spacing) {
						do_symmetrical_brush_actions(a, mouse, NULL);
						spacing= 0;
					}
				}
			}
			else {
				do_symmetrical_brush_actions(a, mouse, mvalo);
				unproject(sd->pivot, mouse[0], mouse[1], a->depth);
			}

			if(modifier_calculations || ob_get_keyblock(ob))
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

			if(modifier_calculations || sd->brush_type == GRAB_BRUSH || !(sd->flags & SCULPT_DRAW_FAST)) {
				calc_damaged_verts(&ss->damaged_verts, a);
				scrarea_do_windraw(curarea);
				screen_swapbuffers();
			} else { /* Optimized drawing */
				calc_damaged_verts(&ss->damaged_verts, a);

				/* Draw the stored image to the screen */
				glAccum(GL_RETURN, 1);

				sculpt_clear_damaged_areas(ss);
				
				/* Draw all the polygons that are inside the modified area(s) */
				glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
				sculptmode_draw_mesh(1);
				glAccum(GL_LOAD, 1);

				projverts_clear_inside(ss);

				persp(PERSP_WIN);
				glDisable(GL_DEPTH_TEST);
				
				/* Draw cursor */
				if(sculpt_data()->flags & SCULPT_DRAW_BRUSH)
					fdrawXORcirc((float)mouse[0],(float)mouse[1],sculptmode_brush()->size);
				if(smooth_stroke)
					sculpt_stroke_draw();
				
				myswapbuffers();
			}

			BLI_freelistN(&ss->damaged_rects);
			ss->damaged_rects.first = ss->damaged_rects.last = NULL;
	
			mvalo[0]= mouse[0];
			mvalo[1]= mouse[1];

			if(ss->vertexcosnos) {
				MEM_freeN(ss->vertexcosnos);
				ss->vertexcosnos= NULL;
			}

		}
		else BIF_wait_for_statechange();
	}

	/* Set the rotation of the brush back to what it was before any rake */
	set_tex_angle(offsetRot);
	
	if(smooth_stroke) {
		sculpt_stroke_apply_all(a);
		calc_damaged_verts(&ss->damaged_verts, a);
		BLI_freelistN(&ss->damaged_rects);
	}

	if(a->layer_disps) MEM_freeN(a->layer_disps);
	if(a->mesh_store) MEM_freeN(a->mesh_store);
	if(a->orig_norms) MEM_freeN(a->orig_norms);
	for(i=0; i<8; ++i)
		BLI_freelistN(&a->grab_active_verts[i]);
	MEM_freeN(a);
	sculpt_stroke_free();

	sculpt_undo_push(G.scene->sculptdata.brush_type);

	if(G.vd->depths) G.vd->depths->damaged= 1;
	
	allqueue(REDRAWVIEW3D, 0);
}

void sculpt_undo_push(const short brush_type)
{
	switch(brush_type) {
	case DRAW_BRUSH:
		BIF_undo_push("Draw Brush"); break;
	case SMOOTH_BRUSH:
		BIF_undo_push("Smooth Brush"); break;
	case PINCH_BRUSH:
		BIF_undo_push("Pinch Brush"); break;
	case INFLATE_BRUSH:
		BIF_undo_push("Inflate Brush"); break;
	case GRAB_BRUSH:
		BIF_undo_push("Grab Brush"); break;
	case LAYER_BRUSH:
		BIF_undo_push("Layer Brush"); break;
	case FLATTEN_BRUSH:
 		BIF_undo_push("Flatten Brush"); break;
	default:
		BIF_undo_push("Sculpting"); break;
	}
}

void set_sculptmode(void)
{
	if(G.f & G_SCULPTMODE) {
		Mesh *me= get_mesh(OBACT);
		
		G.f &= ~G_SCULPTMODE;

		sculptsession_free(G.scene);
		if(me && me->pv) 
			mesh_pmv_off(OBACT, me);
	} 
	else {
		G.f |= G_SCULPTMODE;

		/* Called here to sanity-check the brush */
		sculptmode_brush();

		sculpt_init_session();
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
	}
	
	active_ob= NULL;

	allqueue(REDRAWVIEW3D, 1);
	allqueue(REDRAWBUTSEDIT, 0);
}

/* Partial Mesh Visibility */

/* mode: 0=hide outside selection, 1=hide inside selection */
static void sculptmode_do_pmv(Object *ob, rcti *hb_2d, int mode)
{
	Mesh *me= get_mesh(ob);
	float hidebox[6][3];
	vec3f plane_normals[4];
	float plane_ds[4];
	unsigned i, j;
	unsigned ndx_show, ndx_hide;
	MVert *nve;
	unsigned face_cnt_show= 0, face_ndx_show= 0;
	unsigned edge_cnt_show= 0, edge_ndx_show= 0;
	unsigned *old_map= NULL;
	const unsigned SHOW= 0, HIDE=1;

	/* Convert hide box from 2D to 3D */
	unproject(hidebox[0], hb_2d->xmin, hb_2d->ymax, 1);
	unproject(hidebox[1], hb_2d->xmax, hb_2d->ymax, 1);
	unproject(hidebox[2], hb_2d->xmax, hb_2d->ymin, 1);
	unproject(hidebox[3], hb_2d->xmin, hb_2d->ymin, 1);
	unproject(hidebox[4], hb_2d->xmin, hb_2d->ymax, 0);
	unproject(hidebox[5], hb_2d->xmax, hb_2d->ymin, 0);
	
	/* Calculate normals for each side of hide box */
	CalcNormFloat(hidebox[0], hidebox[1], hidebox[4], &plane_normals[0].x);
	CalcNormFloat(hidebox[1], hidebox[2], hidebox[5], &plane_normals[1].x);
	CalcNormFloat(hidebox[2], hidebox[3], hidebox[5], &plane_normals[2].x);
	CalcNormFloat(hidebox[3], hidebox[0], hidebox[4], &plane_normals[3].x);
	
	/* Calculate D for each side of hide box */
	for(i= 0; i<4; ++i)
		plane_ds[i]= hidebox[i][0]*plane_normals[i].x + hidebox[i][1]*plane_normals[i].y +
			hidebox[i][2]*plane_normals[i].z;
	
	/* Add partial visibility to mesh */
	if(!me->pv) {
		me->pv= MEM_callocN(sizeof(PartialVisibility),"PartialVisibility");
	} else {
		old_map= MEM_callocN(sizeof(unsigned)*me->pv->totvert,"PMV oldmap");
		for(i=0; i<me->pv->totvert; ++i) {
			old_map[i]= me->pv->vert_map[i]<me->totvert?0:1;
		}
		mesh_pmv_revert(ob, me);
	}
	
	/* Kill sculpt data */
	active_ob= NULL;
	
	/* Initalize map with which verts are to be hidden */
	me->pv->vert_map= MEM_mallocN(sizeof(unsigned)*me->totvert, "PMV vertmap");
	me->pv->totvert= me->totvert;
	me->totvert= 0;
	for(i=0; i<me->pv->totvert; ++i) {
		me->pv->vert_map[i]= mode ? HIDE:SHOW;
		for(j=0; j<4; ++j) {
			if(me->mvert[i].co[0] * plane_normals[j].x +
			   me->mvert[i].co[1] * plane_normals[j].y +
			   me->mvert[i].co[2] * plane_normals[j].z < plane_ds[j] ) {
				me->pv->vert_map[i]= mode ? SHOW:HIDE; /* Vert is outside the hide box */
				break;
			}
		}
		if(old_map && old_map[i]) me->pv->vert_map[i]= 1;
		if(!me->pv->vert_map[i]) ++me->totvert;

	}
	if(old_map) MEM_freeN(old_map);

	/* Find out how many faces to show */
	for(i=0; i<me->totface; ++i) {
		if(!me->pv->vert_map[me->mface[i].v1] &&
		   !me->pv->vert_map[me->mface[i].v2] &&
		   !me->pv->vert_map[me->mface[i].v3]) {
			if(me->mface[i].v4) {
				if(!me->pv->vert_map[me->mface[i].v4])
					++face_cnt_show;
			}
			else ++face_cnt_show;
		}
	}
	/* Find out how many edges to show */
	for(i=0; i<me->totedge; ++i) {
		if(!me->pv->vert_map[me->medge[i].v1] &&
		   !me->pv->vert_map[me->medge[i].v2])
			++edge_cnt_show;
	}

	/* Create new vert array and reset each vert's map with map[old]=new index */
	nve= MEM_mallocN(sizeof(MVert)*me->pv->totvert, "PMV verts");
	ndx_show= 0; ndx_hide= me->totvert;
	for(i=0; i<me->pv->totvert; ++i) {
		if(me->pv->vert_map[i]) {
			me->pv->vert_map[i]= ndx_hide;
			nve[me->pv->vert_map[i]]= me->mvert[i];
			++ndx_hide;
		} else {
			me->pv->vert_map[i]= ndx_show;
			nve[me->pv->vert_map[i]]= me->mvert[i];
			++ndx_show;
		}
	}
	CustomData_free_layer_active(&me->vdata, CD_MVERT, me->pv->totvert);
	me->mvert= CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, nve, me->totvert);

	/* Create new face array */
	me->pv->old_faces= me->mface;
	me->pv->totface= me->totface;
	me->mface= MEM_mallocN(sizeof(MFace)*face_cnt_show, "PMV faces");
	for(i=0; i<me->totface; ++i) {
		MFace *pr_f= &me->pv->old_faces[i];
		char show= 0;

		if(me->pv->vert_map[pr_f->v1] < me->totvert &&
		   me->pv->vert_map[pr_f->v2] < me->totvert &&
		   me->pv->vert_map[pr_f->v3] < me->totvert) {
			if(pr_f->v4) {
				if(me->pv->vert_map[pr_f->v4] < me->totvert)
					show= 1;
			}
			else show= 1;
		}

		if(show) {
			MFace *cr_f= &me->mface[face_ndx_show];
			*cr_f= *pr_f;
			cr_f->v1= me->pv->vert_map[pr_f->v1];
			cr_f->v2= me->pv->vert_map[pr_f->v2];
			cr_f->v3= me->pv->vert_map[pr_f->v3];
			cr_f->v4= pr_f->v4 ? me->pv->vert_map[pr_f->v4] : 0;
			test_index_face(cr_f,NULL,0,pr_f->v4?4:3);
			++face_ndx_show;
		}
	}
	me->totface= face_cnt_show;
	CustomData_set_layer(&me->fdata, CD_MFACE, me->mface);

	/* Create new edge array */
	me->pv->old_edges= me->medge;
	me->pv->totedge= me->totedge;
	me->medge= MEM_mallocN(sizeof(MEdge)*edge_cnt_show, "PMV edges");
	me->pv->edge_map= MEM_mallocN(sizeof(int)*me->pv->totedge,"PMV edgemap");
	for(i=0; i<me->totedge; ++i) {
		if(me->pv->vert_map[me->pv->old_edges[i].v1] < me->totvert &&
		   me->pv->vert_map[me->pv->old_edges[i].v2] < me->totvert) {
			MEdge *cr_e= &me->medge[edge_ndx_show];
			me->pv->edge_map[i]= edge_ndx_show;
			*cr_e= me->pv->old_edges[i];
			cr_e->v1= me->pv->vert_map[me->pv->old_edges[i].v1];
			cr_e->v2= me->pv->vert_map[me->pv->old_edges[i].v2];
			++edge_ndx_show;
		}
		else me->pv->edge_map[i]= -1;
	}
	me->totedge= edge_cnt_show;
	CustomData_set_layer(&me->edata, CD_MEDGE, me->medge);

	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
}

static rcti sculptmode_pmv_box()
{
	short down[2], mouse[2];
	rcti ret;

	getmouseco_areawin(down);

	while((get_mbut()&L_MOUSE) || (get_mbut()&R_MOUSE)) {
		getmouseco_areawin(mouse);

		scrarea_do_windraw(curarea);

		persp(PERSP_WIN);
		glLineWidth(2);
		setlinestyle(2);
		sdrawXORline(down[0],down[1],mouse[0],down[1]);
		sdrawXORline(mouse[0],down[1],mouse[0],mouse[1]);
		sdrawXORline(mouse[0],mouse[1],down[0],mouse[1]);
		sdrawXORline(down[0],mouse[1],down[0],down[1]);
		setlinestyle(0);
		glLineWidth(1);
		persp(PERSP_VIEW);

		screen_swapbuffers();
		backdrawview3d(0);
	}

	ret.xmin= down[0]<mouse[0]?down[0]:mouse[0];
	ret.ymin= down[1]<mouse[1]?down[1]:mouse[1];
	ret.xmax= down[0]>mouse[0]?down[0]:mouse[0];
	ret.ymax= down[1]>mouse[1]?down[1]:mouse[1];
	return ret;
}

void sculptmode_pmv(int mode)
{
	Object *ob= OBACT;
	rcti hb_2d;
	
	if(ob_get_key(ob)) {
		error("Cannot hide mesh with shape keys enabled");
		return;
	}
	
	hb_2d= sculptmode_pmv_box(); /* Get 2D hide box */
	
	sculptmode_correct_state();

	waitcursor(1);

	if(hb_2d.xmax-hb_2d.xmin > 3 && hb_2d.ymax-hb_2d.ymin > 3) {
		init_sculptmatrices();

		sculptmode_do_pmv(ob,&hb_2d,mode);
	}
	else mesh_pmv_off(ob, get_mesh(ob));

	scrarea_do_windraw(curarea);

	BIF_undo_push("Partial mesh hide");

	waitcursor(0);
}
