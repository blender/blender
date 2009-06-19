/*
 * $Id: sculptmode.c 18309 2009-01-04 07:47:11Z nicholasbishop $
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
 */

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

#include "BKE_brush.h"
#include "BKE_context.h"
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
#include "BKE_multires.h"
#include "BKE_sculpt.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_colortools.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_util.h"
#include "ED_view3d.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "IMB_imbuf_types.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h" /*for multitex_ext*/

#include "GPU_draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Number of vertices to average in order to determine the flatten distance */
#define FLATTEN_SAMPLE_SIZE 10

/* ===== STRUCTS =====
 *
 */

/* ActiveData stores an Index into the mvert array of Mesh, plus Fade, which
   stores how far the vertex is from the brush center, scaled to the range [0,1]. */
typedef struct ActiveData {
	struct ActiveData *next, *prev;
	unsigned int Index;
	float Fade;
	float dist;
} ActiveData;

typedef enum StrokeFlags {
	CLIP_X = 1,
	CLIP_Y = 2,
	CLIP_Z = 4
} StrokeFlags;

/* Cache stroke properties. Used because
   RNA property lookup isn't particularly fast.

   For descriptions of these settings, check the operator properties.
*/
typedef struct StrokeCache {
	/* Invariants */
	float radius;
	float scale[3];
	int flag;
	float clip_tolerance[3];
	int initial_mouse[2];
	float depth;

	/* Variants */
	float true_location[3];
	float location[3];
	float flip;
	float pressure;
	int mouse[2];

	/* The rest is temporary storage that isn't saved as a property */

	int first_time; /* Beginning of stroke may do some things special */

	ViewContext vc;
	bglMats *mats;

	float *layer_disps; /* Displacements for each vertex */
 	float (*mesh_store)[3]; /* Copy of the mesh vertices' locations */
	short (*orig_norms)[3]; /* Copy of the mesh vertices' normals */
	float rotation; /* Texture rotation (radians) for anchored and rake modes */
	int pixel_radius, previous_pixel_radius;
	ListBase grab_active_verts[8]; /* The same list of verts is used throught grab stroke */
	float grab_delta[3], grab_delta_symmetry[3];
	float old_grab_location[3];
	int symmetry; /* Symmetry index between 0 and 7 */
	float view_normal[3], view_normal_symmetry[3];
	int last_dot[2]; /* Last location of stroke application */
	int last_rake[2]; /* Last location of updating rake rotation */
} StrokeCache;

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

/* ===== OPENGL =====
 *
 * Simple functions to get data from the GL
 */

/* Uses window coordinates (x,y) and depth component z to find a point in
   modelspace */
static void unproject(bglMats *mats, float out[3], const short x, const short y, const float z)
{
	double ux, uy, uz;

        gluUnProject(x,y,z, mats->modelview, mats->projection,
		     (GLint *)mats->viewport, &ux, &uy, &uz );
	out[0] = ux;
	out[1] = uy;
	out[2] = uz;
}

/* Convert a point in model coordinates to 2D screen coordinates. */
static void projectf(bglMats *mats, const float v[3], float p[2])
{
	double ux, uy, uz;

	gluProject(v[0],v[1],v[2], mats->modelview, mats->projection,
		   (GLint *)mats->viewport, &ux, &uy, &uz);
	p[0]= ux;
	p[1]= uy;
}

static void project(bglMats *mats, const float v[3], short p[2])
{
	float f[2];
	projectf(mats, v, f);

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
static char brush_size(Sculpt *sd)
{
	float size= sd->brush->size;
	
	if((sd->brush->sculpt_tool != SCULPT_TOOL_GRAB) && (sd->brush->flag & BRUSH_SIZE_PRESSURE))
		size *= sd->session->cache->pressure;

	return size;
}

/* Return modified brush strength. Includes the direction of the brush, positive
   values pull vertices, negative values push. Uses tablet pressure and a
   special multiplier found experimentally to scale the strength factor. */
static float brush_strength(Sculpt *sd, StrokeCache *cache)
{
	float dir= sd->brush->flag & BRUSH_DIR_IN ? -1 : 1;
	float pressure= 1;
	float flip= cache->flip ? -1:1;
	float anchored = sd->brush->flag & BRUSH_ANCHORED ? 25 : 1;

	if(sd->brush->flag & BRUSH_ALPHA_PRESSURE)
		pressure *= cache->pressure;
	
	switch(sd->brush->sculpt_tool){
	case SCULPT_TOOL_DRAW:
	case SCULPT_TOOL_LAYER:
		return sd->brush->alpha / 50.0f * dir * pressure * flip * anchored; /*XXX: not sure why? multiplied by G.vd->grid */;
	case SCULPT_TOOL_SMOOTH:
		return sd->brush->alpha / .5 * pressure * anchored;
	case SCULPT_TOOL_PINCH:
		return sd->brush->alpha / 10.0f * dir * pressure * flip * anchored;
	case SCULPT_TOOL_GRAB:
		return 1;
	case SCULPT_TOOL_INFLATE:
		return sd->brush->alpha / 50.0f * dir * pressure * flip * anchored;
	case SCULPT_TOOL_FLATTEN:
		return sd->brush->alpha / 5.0f * pressure * anchored;
	default:
		return 0;
	}
}

/* For clipping against a mirror modifier */
static void sculpt_clip(StrokeCache *cache, float *co, const float val[3])
{
	int i;
	for(i=0; i<3; ++i) {
		if((cache->flag & (CLIP_X << i)) && (fabs(co[i]) <= cache->clip_tolerance[i]))
			co[i]= 0.0f;
		else
			co[i]= val[i];
	}		
}

static void sculpt_axislock(Sculpt *sd, float *co)
{
	if(sd->flags == (SCULPT_LOCK_X|SCULPT_LOCK_Y|SCULPT_LOCK_Z))
		return;

	if(sd->session->cache->vc.v3d->twmode == V3D_MANIP_LOCAL) {
		float mat[3][3], imat[3][3];
		Mat3CpyMat4(mat, sd->session->cache->vc.obact->obmat);
		Mat3Inv(imat, mat);
		Mat3MulVecfl(mat, co);
		if (sd->flags & SCULPT_LOCK_X) co[0] = 0.0;
		if (sd->flags & SCULPT_LOCK_Y) co[1] = 0.0;
		if (sd->flags & SCULPT_LOCK_Z) co[2] = 0.0;		
		Mat3MulVecfl(imat, co);
	} else {
		if (sd->flags & SCULPT_LOCK_X) co[0] = 0.0;
		if (sd->flags & SCULPT_LOCK_Y) co[1] = 0.0;
		if (sd->flags & SCULPT_LOCK_Z) co[2] = 0.0;		
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
static void calc_area_normal(Sculpt *sd, float out[3], const ListBase* active_verts)
{
	StrokeCache *cache = sd->session->cache;
	ActiveData *node = active_verts->first;
	const int view = 0; /* XXX: should probably be a flag, not number: sd->brush_type==SCULPT_TOOL_DRAW ? sculptmode_brush()->view : 0; */
	float out_flip[3];
	float *out_dir = cache->view_normal_symmetry;
	
	out[0]=out[1]=out[2] = out_flip[0]=out_flip[1]=out_flip[2] = 0;

	if(sd->brush->flag & BRUSH_ANCHORED) {
		for(; node; node = node->next)
			add_norm_if(out_dir, out, out_flip, cache->orig_norms[node->Index]);
	}
	else {
		for(; node; node = node->next)
			add_norm_if(out_dir, out, out_flip, sd->session->mvert[node->Index].no);
	}

	if (out[0]==0.0 && out[1]==0.0 && out[2]==0.0) {
		VECCOPY(out, out_flip);
	}
	
	Normalize(out);

	if(out_dir) {
		out[0] = out_dir[0] * view + out[0] * (10-view);
		out[1] = out_dir[1] * view + out[1] * (10-view);
		out[2] = out_dir[2] * view + out[2] * (10-view);
	}
	
	Normalize(out);
}

static void do_draw_brush(Sculpt *sd, SculptSession *ss, const ListBase* active_verts)
{
	float area_normal[3];
	ActiveData *node= active_verts->first;

	calc_area_normal(sd, area_normal, active_verts);
	
	sculpt_axislock(sd, area_normal);
	
	while(node){
		float *co= ss->mvert[node->Index].co;
		
		const float val[3]= {co[0]+area_normal[0]*node->Fade*ss->cache->scale[0],
		                     co[1]+area_normal[1]*node->Fade*ss->cache->scale[1],
		                     co[2]+area_normal[2]*node->Fade*ss->cache->scale[2]};
		                     
		sculpt_clip(ss->cache, co, val);
		
		node= node->next;
	}
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
   a smoothed location for vert. Skips corner vertices (used by only one
   polygon.) */
static void neighbor_average(SculptSession *ss, float avg[3], const int vert)
{
	int i, skip= -1, total=0;
	IndexNode *node= ss->fmap[vert].first;
	char ncount= BLI_countlist(&ss->fmap[vert]);
	MFace *f;

	avg[0] = avg[1] = avg[2] = 0;
		
	/* Don't modify corner vertices */
	if(ncount==1) {
		VecCopyf(avg, ss->mvert[vert].co);
		return;
	}

	while(node){
		f= &ss->mface[node->index];
		
		if(f->v4) {
			skip= (f->v1==vert?2:
			       f->v2==vert?3:
			       f->v3==vert?0:
			       f->v4==vert?1:-1);
		}

		for(i=0; i<(f->v4?4:3); ++i) {
			if(i != skip && (ncount!=2 || BLI_countlist(&ss->fmap[(&f->v1)[i]]) <= 2)) {
				VecAddf(avg, avg, ss->mvert[(&f->v1)[i]].co);
				++total;
			}
		}

		node= node->next;
	}

	if(total>0)
		VecMulf(avg, 1.0f / total);
	else
		VecCopyf(avg, ss->mvert[vert].co);
}

static void do_smooth_brush(SculptSession *ss, const ListBase* active_verts)
{
	ActiveData *node= active_verts->first;

	while(node){
		float *co= ss->mvert[node->Index].co;
		float avg[3], val[3];

		neighbor_average(ss, avg, node->Index);
		val[0] = co[0]+(avg[0]-co[0])*node->Fade;
		val[1] = co[1]+(avg[1]-co[1])*node->Fade;
		val[2] = co[2]+(avg[2]-co[2])*node->Fade;

		sculpt_clip(ss->cache, co, val);
		node= node->next;
	}
}

static void do_pinch_brush(SculptSession *ss, const ListBase* active_verts)
{
 	ActiveData *node= active_verts->first;

	while(node) {
		float *co= ss->mvert[node->Index].co;
		const float val[3]= {co[0]+(ss->cache->location[0]-co[0])*node->Fade,
		                     co[1]+(ss->cache->location[1]-co[1])*node->Fade,
		                     co[2]+(ss->cache->location[2]-co[2])*node->Fade};
		sculpt_clip(ss->cache, co, val);
		node= node->next;
	}
}

static void do_grab_brush(Sculpt *sd, SculptSession *ss)
{
	ActiveData *node= ss->cache->grab_active_verts[ss->cache->symmetry].first;
	float add[3];
	float grab_delta[3];
	
	VecCopyf(grab_delta, ss->cache->grab_delta_symmetry);
	sculpt_axislock(sd, grab_delta);
	
	while(node) {
		float *co= ss->mvert[node->Index].co;
		
		VecCopyf(add, grab_delta);
		VecMulf(add, node->Fade);
		VecAddf(add, add, co);
		sculpt_clip(ss->cache, co, add);

		node= node->next;
	}
	
}

static void do_layer_brush(Sculpt *sd, SculptSession *ss, const ListBase *active_verts)
{
	float area_normal[3];
	ActiveData *node= active_verts->first;
	const float bstr= brush_strength(sd, ss->cache);

	calc_area_normal(sd, area_normal, active_verts);

	while(node){
		float *disp= &ss->cache->layer_disps[node->Index];
		
		if((bstr > 0 && *disp < bstr) ||
		  (bstr < 0 && *disp > bstr)) {
		  	float *co= ss->mvert[node->Index].co;
		  	
			*disp+= node->Fade;

			if(bstr < 0) {
				if(*disp < bstr)
					*disp = bstr;
			} else {
				if(*disp > bstr)
					*disp = bstr;
			}

			{
				const float val[3]= {ss->cache->mesh_store[node->Index][0]+area_normal[0] * *disp*ss->cache->scale[0],
				                     ss->cache->mesh_store[node->Index][1]+area_normal[1] * *disp*ss->cache->scale[1],
				                     ss->cache->mesh_store[node->Index][2]+area_normal[2] * *disp*ss->cache->scale[2]};
				sculpt_clip(ss->cache, co, val);
			}
		}

		node= node->next;
	}
}

static void do_inflate_brush(SculptSession *ss, const ListBase *active_verts)
{
	ActiveData *node= active_verts->first;
	float add[3];
	
	while(node) {
		float *co= ss->mvert[node->Index].co;
		short *no= ss->mvert[node->Index].no;

		add[0]= no[0]/ 32767.0f;
		add[1]= no[1]/ 32767.0f;
		add[2]= no[2]/ 32767.0f;
		VecMulf(add, node->Fade);
		add[0]*= ss->cache->scale[0];
		add[1]*= ss->cache->scale[1];
		add[2]*= ss->cache->scale[2];
		VecAddf(add, add, co);
		
		sculpt_clip(ss->cache, co, add);

		node= node->next;
	}
}

static void calc_flatten_center(SculptSession *ss, ActiveData *node, float co[3])
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
		VecAddf(co, co, ss->mvert[outer[i]->Index].co);
	VecMulf(co, 1.0f / FLATTEN_SAMPLE_SIZE);
}

static void do_flatten_brush(Sculpt *sd, SculptSession *ss, const ListBase *active_verts)
{
	ActiveData *node= active_verts->first;
	/* area_normal and cntr define the plane towards which vertices are squashed */
	float area_normal[3];
	float cntr[3];

	calc_area_normal(sd, area_normal, active_verts);
	calc_flatten_center(ss, node, cntr);

	while(node){
		float *co= ss->mvert[node->Index].co;
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
		
		sculpt_clip(ss->cache, co, val);
		
		node= node->next;
	}
}


/* Uses symm to selectively flip any axis of a coordinate. */
static void flip_coord(float out[3], float in[3], const char symm)
{
	if(symm & SCULPT_SYMM_X)
		out[0]= -in[0];
	else
		out[0]= in[0];
	if(symm & SCULPT_SYMM_Y)
		out[1]= -in[1];
	else
		out[1]= in[1];
	if(symm & SCULPT_SYMM_Z)
		out[2]= -in[2];
	else
		out[2]= in[2];
}

/* Get a pixel from the texcache at (px, py) */
static unsigned char get_texcache_pixel(const SculptSession *ss, int px, int py)
{
	unsigned *p;
	p = ss->texcache + py * ss->texcache_side + px;
	return ((unsigned char*)(p))[0];
}

static float get_texcache_pixel_bilinear(const SculptSession *ss, float u, float v)
{
	int x, y, x2, y2;
	const int tc_max = ss->texcache_side - 1;
	float urat, vrat, uopp;

	if(u < 0) u = 0;
	else if(u >= ss->texcache_side) u = tc_max;
	if(v < 0) v = 0;
	else if(v >= ss->texcache_side) v = tc_max;

	x = floor(u);
	y = floor(v);
	x2 = x + 1;
	y2 = y + 1;

	if(x2 > ss->texcache_side) x2 = tc_max;
	if(y2 > ss->texcache_side) y2 = tc_max;
	
	urat = u - x;
	vrat = v - y;
	uopp = 1 - urat;
		
	return ((get_texcache_pixel(ss, x, y) * uopp +
		 get_texcache_pixel(ss, x2, y) * urat) * (1 - vrat) + 
		(get_texcache_pixel(ss, x, y2) * uopp +
		 get_texcache_pixel(ss, x2, y2) * urat) * vrat) / 255.0;
}

/* Return a multiplier for brush strength on a particular vertex. */
static float tex_strength(Sculpt *sd, float *point, const float len)
{
	SculptSession *ss= sd->session;
	Brush *br = sd->brush;
	float avg= 1;

	if(br->texact==-1 || !br->mtex[br->texact])
		avg= 1;
	else if(br->tex_mode==BRUSH_TEX_3D) {
		/* Get strength by feeding the vertex location directly
		   into a texture */
		float jnk;
		const float factor= 0.01;
		MTex mtex;
		memset(&mtex,0,sizeof(MTex));
		mtex.tex= br->mtex[br->texact]->tex;
		mtex.projx= 1;
		mtex.projy= 2;
		mtex.projz= 3;
		VecCopyf(mtex.size, br->mtex[br->texact]->size);
		VecMulf(mtex.size, factor);
		if(!sd->texsep)
			mtex.size[1]= mtex.size[2]= mtex.size[0];
		
		externtex(&mtex,point,&avg,&jnk,&jnk,&jnk,&jnk);
	}
	else if(ss->texcache) {
		const float bsize= ss->cache->pixel_radius * 2;
		const float rot= sd->brush->rot + ss->cache->rotation;
		int px, py;
		float flip[3], point_2d[2];

		/* If the active area is being applied for symmetry, flip it
		   across the symmetry axis in order to project it. This insures
		   that the brush texture will be oriented correctly. */
		VecCopyf(flip, point);
		flip_coord(flip, flip, ss->cache->symmetry);
		projectf(ss->cache->mats, flip, point_2d);

		/* For Tile and Drag modes, get the 2D screen coordinates of the
		   and scale them up or down to the texture size. */
		if(br->tex_mode==BRUSH_TEX_TILE) {
			const int sx= (const int)br->mtex[br->texact]->size[0];
			const int sy= (const int)sd->texsep ? br->mtex[br->texact]->size[1] : sx;
			
			float fx= point_2d[0];
			float fy= point_2d[1];
			
			float angle= atan2(fy, fx) - rot;
			float flen= sqrtf(fx*fx + fy*fy);
			
			if(rot<0.001 && rot>-0.001) {
				px= point_2d[0];
				py= point_2d[1];
			} else {
				px= flen * cos(angle) + 2000;
				py= flen * sin(angle) + 2000;
			}
			if(sx != 1)
				px %= sx-1;
			if(sy != 1)
				py %= sy-1;
			avg= get_texcache_pixel_bilinear(ss, ss->texcache_side*px/sx, ss->texcache_side*py/sy);
		} else {
			float fx= (point_2d[0] - ss->cache->mouse[0]) / bsize;
			float fy= (point_2d[1] - ss->cache->mouse[1]) / bsize;

			float angle= atan2(fy, fx) - rot;
			float flen= sqrtf(fx*fx + fy*fy);
			
			fx = flen * cos(angle) + 0.5;
			fy = flen * sin(angle) + 0.5;

			avg= get_texcache_pixel_bilinear(ss, fx * ss->texcache_side, fy * ss->texcache_side);
		}
	}

	avg*= brush_curve_strength(sd->brush, len, ss->cache->radius); /* Falloff curve */

	return avg;
}

/* Mark area around the brush as damaged. projverts are marked if they are
   inside the area and the damaged rectangle in 2D screen coordinates is 
   added to damaged_rects. */
static void sculpt_add_damaged_rect(SculptSession *ss)
{
	short p[2];
	RectNode *rn= MEM_mallocN(sizeof(RectNode),"RectNode");
	const float radius = MAX2(ss->cache->pixel_radius, ss->cache->previous_pixel_radius);
	unsigned i;

	/* Find center */
	project(ss->cache->mats, ss->cache->location, p);
	rn->r.xmin= p[0] - radius;
	rn->r.ymin= p[1] - radius;
	rn->r.xmax= p[0] + radius;
	rn->r.ymax= p[1] + radius;

	BLI_addtail(&ss->damaged_rects, rn);

	/* Update insides */
	for(i=0; i<ss->totvert; ++i) {
		if(!ss->projverts[i].inside) {
			if(ss->projverts[i].co[0] > rn->r.xmin && ss->projverts[i].co[1] > rn->r.ymin &&
			   ss->projverts[i].co[0] < rn->r.xmax && ss->projverts[i].co[1] < rn->r.ymax) {
				ss->projverts[i].inside= 1;
			}
		}
		// XXX: remember to fix this!
		// temporary pass
		ss->projverts[i].inside = 1;
	}
}

/* Clears the depth buffer in each modified area. */
#if 0
static void sculpt_clear_damaged_areas(SculptSession *ss)
{
	RectNode *rn= NULL;

	for(rn = ss->damaged_rects.first; rn; rn = rn->next) {
		rcti clp = rn->r;
		rcti *win = NULL; /*XXX: &curarea->winrct; */
		
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
#endif
static void do_brush_action(Sculpt *sd, StrokeCache *cache)
{
	SculptSession *ss = sd->session;
	float av_dist;
	ListBase active_verts={0,0};
	ListBase *grab_active_verts = &ss->cache->grab_active_verts[ss->cache->symmetry];
	ActiveData *adata= 0;
	float *vert;
	Mesh *me= NULL; /*XXX: get_mesh(OBACT); */
	const float bstrength= brush_strength(sd, cache);
	KeyBlock *keyblock= NULL; /*XXX: ob_get_keyblock(OBACT); */
	Brush *b = sd->brush;
	int i;

	sculpt_add_damaged_rect(ss);

	/* Build a list of all vertices that are potentially within the brush's
	   area of influence. Only do this once for the grab brush. */
	if((b->sculpt_tool != SCULPT_TOOL_GRAB) || cache->first_time) {
		for(i=0; i<ss->totvert; ++i) {
			/* Projverts.inside provides a rough bounding box */
			if(ss->multires || ss->projverts[i].inside) {
				//vert= ss->vertexcosnos ? &ss->vertexcosnos[i*6] : a->verts[i].co;
				vert= ss->mvert[i].co;
				av_dist= VecLenf(ss->cache->location, vert);
				if(av_dist < cache->radius) {
					adata= (ActiveData*)MEM_mallocN(sizeof(ActiveData), "ActiveData");

					adata->Index = i;
					/* Fade is used to store the final strength at which the brush
					   should modify a particular vertex. */
					adata->Fade= tex_strength(sd, vert, av_dist) * bstrength;
					adata->dist = av_dist;

					if(b->sculpt_tool == SCULPT_TOOL_GRAB && cache->first_time)
						BLI_addtail(grab_active_verts, adata);
					else
						BLI_addtail(&active_verts, adata);
				}
			}
		}
	}

	/* Only act if some verts are inside the brush area */
	if(active_verts.first || (b->sculpt_tool == SCULPT_TOOL_GRAB && grab_active_verts->first)) {
		/* Apply one type of brush action */
		switch(b->sculpt_tool){
		case SCULPT_TOOL_DRAW:
			do_draw_brush(sd, ss, &active_verts);
			break;
		case SCULPT_TOOL_SMOOTH:
			do_smooth_brush(ss, &active_verts);
			break;
		case SCULPT_TOOL_PINCH:
			do_pinch_brush(ss, &active_verts);
			break;
		case SCULPT_TOOL_INFLATE:
			do_inflate_brush(ss, &active_verts);
			break;
		case SCULPT_TOOL_GRAB:
			do_grab_brush(sd, ss);
			break;
		case SCULPT_TOOL_LAYER:
			do_layer_brush(sd, ss, &active_verts);
			break;
		case SCULPT_TOOL_FLATTEN:
			do_flatten_brush(sd, ss, &active_verts);
			break;
		}
	
		/* Copy the modified vertices from mesh to the active key */
		if(keyblock && !ss->multires) {
			float *co= keyblock->data;
			if(co) {
				if(b->sculpt_tool == SCULPT_TOOL_GRAB)
					adata = grab_active_verts->first;
				else
					adata = active_verts.first;

				for(; adata; adata= adata->next)
					if(adata->Index < keyblock->totelem)
						VecCopyf(&co[adata->Index*3], me->mvert[adata->Index].co);
			}
		}

		if(ss->vertexcosnos && !ss->multires)
			BLI_freelistN(&active_verts);
		else {
			if(b->sculpt_tool != SCULPT_TOOL_GRAB)
				addlisttolist(&ss->damaged_verts, &active_verts);
		}
	}
}

/* Flip all the editdata across the axis/axes specified by symm. Used to
   calculate multiple modifications to the mesh when symmetry is enabled. */
static void calc_brushdata_symm(StrokeCache *cache, const char symm)
{
	flip_coord(cache->location, cache->true_location, symm);
	flip_coord(cache->view_normal_symmetry, cache->view_normal, symm);
	flip_coord(cache->grab_delta_symmetry, cache->grab_delta, symm);
	cache->symmetry= symm;
}

static void do_symmetrical_brush_actions(Sculpt *sd, StrokeCache *cache)
{
	const char symm = sd->flags & 7;
	int i;

	/* Brush spacing: only apply dot if next dot is far enough away */
	if((sd->brush->flag & BRUSH_SPACE) && !(sd->brush->flag & BRUSH_ANCHORED) && !cache->first_time) {
		int dx = cache->last_dot[0] - cache->mouse[0];
		int dy = cache->last_dot[1] - cache->mouse[1];
		if(sqrt(dx*dx+dy*dy) < sd->brush->spacing)
			return;
	}
	memcpy(cache->last_dot, cache->mouse, sizeof(int) * 2);

	VecCopyf(cache->location, cache->true_location);
	VecCopyf(cache->grab_delta_symmetry, cache->grab_delta);
	cache->symmetry = 0;
	do_brush_action(sd, cache);

	for(i = 1; i <= symm; ++i) {
		if(symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5))) {
			calc_brushdata_symm(cache, i);
			do_brush_action(sd, cache);
		}
	}

	cache->first_time = 0;
}

static void add_face_normal(vec3f *norm, MVert *mvert, const MFace* face, float *fn)
{
	vec3f c= {mvert[face->v1].co[0],mvert[face->v1].co[1],mvert[face->v1].co[2]};
	vec3f b= {mvert[face->v2].co[0],mvert[face->v2].co[1],mvert[face->v2].co[2]};
	vec3f a= {mvert[face->v3].co[0],mvert[face->v3].co[1],mvert[face->v3].co[2]};
	vec3f s1, s2;
	float final[3];

	VecSubf(&s1.x,&a.x,&b.x);
	VecSubf(&s2.x,&c.x,&b.x);

	final[0] = s1.y * s2.z - s1.z * s2.y;
	final[1] = s1.z * s2.x - s1.x * s2.z;
	final[2] = s1.x * s2.y - s1.y * s2.x;

	if(fn)
		VecCopyf(fn, final);

	norm->x+= final[0];
	norm->y+= final[1];
	norm->z+= final[2];
}

static void update_damaged_vert(SculptSession *ss, ListBase *lb)
{
	ActiveData *vert;
       
	for(vert= lb->first; vert; vert= vert->next) {
		vec3f norm= {0,0,0};		
		IndexNode *face= ss->fmap[vert->Index].first;

		while(face){
			float *fn = NULL;
			if(ss->face_normals)
				fn = &ss->face_normals[face->index*3];
			add_face_normal(&norm, ss->mvert, &ss->mface[face->index], fn);
			face= face->next;
		}
		Normalize(&norm.x);
		
		ss->mvert[vert->Index].no[0]=norm.x*32767;
		ss->mvert[vert->Index].no[1]=norm.y*32767;
		ss->mvert[vert->Index].no[2]=norm.z*32767;
	}
}

static void calc_damaged_verts(SculptSession *ss)
{
	int i;
	
	for(i=0; i<8; ++i)
		update_damaged_vert(ss, &ss->cache->grab_active_verts[i]);
	update_damaged_vert(ss, &ss->damaged_verts);
	BLI_freelistN(&ss->damaged_verts);
	ss->damaged_verts.first = ss->damaged_verts.last = NULL;
}

#if 0
static void projverts_clear_inside(SculptSession *ss)
{
	int i;
	for(i = 0; i < ss->totvert; ++i)
		ss->projverts[i].inside = 0;
}
#endif

static void sculpt_update_tex(Sculpt *sd)
{
	SculptSession *ss= sd->session;

	if(ss->texcache) {
		MEM_freeN(ss->texcache);
		ss->texcache= NULL;
	}

	/* Need to allocate a bigger buffer for bigger brush size */
	ss->texcache_side = sd->brush->size * 2;
	if(!ss->texcache || ss->texcache_side > ss->texcache_actual) {
		ss->texcache = brush_gen_texture_cache(sd->brush, sd->brush->size);
		ss->texcache_actual = ss->texcache_side;
	}
}

void sculptmode_selectbrush_menu(void)
{
	/* XXX: I guess menus belong elsewhere too?

	Sculpt *sd= sculpt_data();
	int val;
	
	pupmenu_set_active(sd->brush_type);
	
	val= pupmenu("Select Brush%t|Draw|Smooth|Pinch|Inflate|Grab|Layer|Flatten");

	if(val>0) {
		sd->brush_type= val;

		allqueue(REDRAWVIEW3D, 1);
		allqueue(REDRAWBUTSEDIT, 1);
	}*/
}

static void sculptmode_update_all_projverts(SculptSession *ss)
{
	unsigned i;

	if(!ss->projverts)
		ss->projverts = MEM_mallocN(sizeof(ProjVert)*ss->totvert,"ProjVerts");

	for(i=0; i<ss->totvert; ++i) {
		project(ss->cache->mats, ss->vertexcosnos ? &ss->vertexcosnos[i * 6] : ss->mvert[i].co,
			ss->projverts[i].co);
		ss->projverts[i].inside= 0;
	}
}

/* Checks whether full update mode (slower) needs to be used to work with modifiers */
char sculpt_modifiers_active(Object *ob)
{
	ModifierData *md;
	
	for(md= modifiers_getVirtualModifierList(ob); md; md= md->next) {
		if(md->mode & eModifierMode_Realtime && md->type != eModifierType_Multires)
			return 1;
	}
	
	return 0;
}

/* Sculpt mode handles multires differently from regular meshes, but only if
   it's the last modifier on the stack and it is not on the first level */
static struct MultiresModifierData *sculpt_multires_active(Object *ob)
{
	ModifierData *md;
	
	for(md= modifiers_getVirtualModifierList(ob); md; md= md->next) {
		if(md->type == eModifierType_Multires && !md->next) {
			MultiresModifierData *mmd = (MultiresModifierData*)md;
			if(mmd->lvl != 1)
				return mmd;
		}
	}

	return NULL;
}

static void sculpt_update_mesh_elements(bContext *C)
{
	SculptSession *ss = CTX_data_tool_settings(C)->sculpt->session;
	Object *ob = CTX_data_active_object(C);
	int oldtotvert = ss->totvert;

	if((ss->multires = sculpt_multires_active(ob))) {
		DerivedMesh *dm = mesh_get_derived_final(CTX_data_scene(C), ob, CD_MASK_BAREMESH);
		ss->totvert = dm->getNumVerts(dm);
		ss->totface = dm->getNumFaces(dm);
		ss->mvert = dm->getVertDataArray(dm, CD_MVERT);
		ss->mface = dm->getFaceDataArray(dm, CD_MFACE);
		ss->face_normals = dm->getFaceDataArray(dm, CD_NORMAL);
	}
	else {
		Mesh *me = get_mesh(ob);
		ss->totvert = me->totvert;
		ss->totface = me->totface;
		ss->mvert = me->mvert;
		ss->mface = me->mface;
		ss->face_normals = NULL;
	}

	if(ss->totvert != oldtotvert) {
		if(ss->projverts) MEM_freeN(ss->projverts);
		ss->projverts = NULL;

		if(ss->fmap) MEM_freeN(ss->fmap);
		if(ss->fmap_mem) MEM_freeN(ss->fmap_mem);
		create_vert_face_map(&ss->fmap, &ss->fmap_mem, ss->mface, ss->totvert, ss->totface);
		ss->fmap_size = ss->totvert;
	}
}

/* XXX: lots of drawing code (partial redraw), has to go elsewhere */
#if 0
void sculptmode_draw_wires(SculptSession *ss, int only_damaged)
{
	Mesh *me = get_mesh(OBACT);
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
	int i, j, dt, drawCurrentMat = 1, matnr= -1;
	SculptSession *ss = sculpt_session();

	sculpt_update_mesh_elements(ss, OBACT);

	persp(PERSP_VIEW);
	mymultmatrix(OBACT->obmat);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	/* XXX: GPU_set_object_materials(G.scene, OBACT, 0, NULL); */
	glEnable(GL_CULL_FACE);

	glShadeModel(GL_SMOOTH);

	glVertexPointer(3, GL_FLOAT, sizeof(MVert), &cache->mvert[0].co);
	glNormalPointer(GL_SHORT, sizeof(MVert), &cache->mvert[0].no);

	dt= MIN2(G.vd->drawtype, OBACT->dt);
	if(dt==OB_WIRE)
		glColorMask(0,0,0,0);

	for(i=0; i<ss->totface; ++i) {
		MFace *f= &ss->mface[i];
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
		sculptmode_draw_wires(ss, only_damaged);

	glDisable(GL_DEPTH_TEST);
}
#endif

static int sculpt_poll(bContext *C)
{
	return G.f & G_SCULPTMODE && CTX_wm_area(C)->spacetype == SPACE_VIEW3D &&
		CTX_wm_region(C)->regiontype == RGN_TYPE_WINDOW;
}

/*** Sculpt Cursor ***/
static void draw_paint_cursor(bContext *C, int x, int y, void *customdata)
{
	Sculpt *sd= CTX_data_tool_settings(C)->sculpt;
	
	glTranslatef((float)x, (float)y, 0.0f);
	
	glColor4ub(255, 100, 100, 128);
	glEnable( GL_LINE_SMOOTH );
	glEnable(GL_BLEND);
	glutil_draw_lined_arc(0.0, M_PI*2.0, sd->brush->size, 40);
	glDisable(GL_BLEND);
	glDisable( GL_LINE_SMOOTH );
	
	glTranslatef((float)-x, (float)-y, 0.0f);
}

static void toggle_paint_cursor(bContext *C)
{
	Sculpt *s = CTX_data_scene(C)->toolsettings->sculpt;

	if(s->session->cursor) {
		WM_paint_cursor_end(CTX_wm_manager(C), s->session->cursor);
		s->session->cursor = NULL;
	}
	else {
		s->session->cursor =
			WM_paint_cursor_activate(CTX_wm_manager(C), sculpt_poll, draw_paint_cursor, NULL);
	}
}

static void sculpt_undo_push(bContext *C, Sculpt *sd)
{
	switch(sd->brush->sculpt_tool) {
	case SCULPT_TOOL_DRAW:
		ED_undo_push(C, "Draw Brush"); break;
	case SCULPT_TOOL_SMOOTH:
		ED_undo_push(C, "Smooth Brush"); break;
	case SCULPT_TOOL_PINCH:
		ED_undo_push(C, "Pinch Brush"); break;
	case SCULPT_TOOL_INFLATE:
		ED_undo_push(C, "Inflate Brush"); break;
	case SCULPT_TOOL_GRAB:
		ED_undo_push(C, "Grab Brush"); break;
	case SCULPT_TOOL_LAYER:
		ED_undo_push(C, "Layer Brush"); break;
	case SCULPT_TOOL_FLATTEN:
 		ED_undo_push(C, "Flatten Brush"); break;
	default:
		ED_undo_push(C, "Sculpting"); break;
	}
}

static int sculpt_brush_curve_preset_exec(bContext *C, wmOperator *op)
{
	brush_curve_preset(CTX_data_scene(C)->toolsettings->sculpt->brush, RNA_enum_get(op->ptr, "mode"));
	return OPERATOR_FINISHED;
}

static void SCULPT_OT_brush_curve_preset(wmOperatorType *ot)
{
	static EnumPropertyItem prop_mode_items[] = {
		{BRUSH_PRESET_SHARP, "SHARP", 0, "Sharp Curve", ""},
		{BRUSH_PRESET_SMOOTH, "SMOOTH", 0, "Smooth Curve", ""},
		{BRUSH_PRESET_MAX, "MAX", 0, "Max Curve", ""},
		{0, NULL, 0, NULL, NULL}};

	ot->name= "Preset";
	ot->idname= "SCULPT_OT_brush_curve_preset";

	ot->exec= sculpt_brush_curve_preset_exec;
	ot->poll= sculpt_poll;

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "mode", prop_mode_items, BRUSH_PRESET_SHARP, "Mode", "");
}

/**** Radial control ****/
static int sculpt_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	toggle_paint_cursor(C);
	brush_radial_control_invoke(op, CTX_data_scene(C)->toolsettings->sculpt->brush, 1);
	return WM_radial_control_invoke(C, op, event);
}

static int sculpt_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);
	if(ret != OPERATOR_RUNNING_MODAL)
		toggle_paint_cursor(C);
	return ret;
}

static int sculpt_radial_control_exec(bContext *C, wmOperator *op)
{
	return brush_radial_control_exec(op, CTX_data_scene(C)->toolsettings->sculpt->brush, 1);
}

static void SCULPT_OT_radial_control(wmOperatorType *ot)
{
	WM_OT_radial_control_partial(ot);

	ot->name= "Sculpt Radial Control";
	ot->idname= "SCULPT_OT_radial_control";

	ot->invoke= sculpt_radial_control_invoke;
	ot->modal= sculpt_radial_control_modal;
	ot->exec= sculpt_radial_control_exec;
	ot->poll= sculpt_poll;

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/**** Operator for applying a stroke (various attributes including mouse path)
      using the current brush. ****/

static float unproject_brush_radius(SculptSession *ss, float offset)
{
	float brush_edge[3];

	/* In anchored mode, brush size changes with mouse loc, otherwise it's fixed using the brush radius */
	unproject(ss->cache->mats, brush_edge, ss->cache->initial_mouse[0] + offset,
		  ss->cache->initial_mouse[1], ss->cache->depth);

	return VecLenf(ss->cache->true_location, brush_edge);
}

static void sculpt_cache_free(StrokeCache *cache)
{
	if(cache->layer_disps)
		MEM_freeN(cache->layer_disps);
	if(cache->mesh_store)
		MEM_freeN(cache->mesh_store);
	if(cache->orig_norms)
		MEM_freeN(cache->orig_norms);
	if(cache->mats)
		MEM_freeN(cache->mats);
	MEM_freeN(cache);
}

/* Initialize the stroke cache invariants from operator properties */
static void sculpt_update_cache_invariants(Sculpt *sd, bContext *C, wmOperator *op)
{
	StrokeCache *cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
	int i;

	sd->session->cache = cache;

	RNA_float_get_array(op->ptr, "scale", cache->scale);
	cache->flag = RNA_int_get(op->ptr, "flag");
	RNA_float_get_array(op->ptr, "clip_tolerance", cache->clip_tolerance);
	RNA_int_get_array(op->ptr, "initial_mouse", cache->initial_mouse);
	cache->depth = RNA_float_get(op->ptr, "depth");

	/* Truly temporary data that isn't stored in properties */

	view3d_set_viewcontext(C, &cache->vc);

	cache->mats = MEM_callocN(sizeof(bglMats), "sculpt bglMats");
	view3d_get_transformation(&cache->vc, cache->vc.obact, cache->mats);

	sculpt_update_mesh_elements(C);

	/* Make copies of the mesh vertex locations and normals for some tools */
	if(sd->brush->sculpt_tool == SCULPT_TOOL_LAYER || (sd->brush->flag & BRUSH_ANCHORED)) {
		cache->layer_disps = MEM_callocN(sizeof(float) * sd->session->totvert, "layer brush displacements");
		cache->mesh_store= MEM_mallocN(sizeof(float) * 3 * sd->session->totvert, "sculpt mesh vertices copy");
		for(i = 0; i < sd->session->totvert; ++i)
			VecCopyf(cache->mesh_store[i], sd->session->mvert[i].co);

		if(sd->brush->flag & BRUSH_ANCHORED) {
			cache->orig_norms= MEM_mallocN(sizeof(short) * 3 * sd->session->totvert, "Sculpt orig norm");
			for(i = 0; i < sd->session->totvert; ++i) {
				cache->orig_norms[i][0] = sd->session->mvert[i].no[0];
				cache->orig_norms[i][1] = sd->session->mvert[i].no[1];
				cache->orig_norms[i][2] = sd->session->mvert[i].no[2];
			}
		}
	}

	unproject(cache->mats, cache->true_location, cache->initial_mouse[0], cache->initial_mouse[1], cache->depth);
	cache->radius = unproject_brush_radius(sd->session, brush_size(sd));
	cache->rotation = 0;
	cache->first_time = 1;
}

/* Initialize the stroke cache variants from operator properties */
static void sculpt_update_cache_variants(Sculpt *sd, PointerRNA *ptr)
{
	StrokeCache *cache = sd->session->cache;
	float grab_location[3];
	int dx, dy;

	if(!(sd->brush->flag & BRUSH_ANCHORED))
		RNA_float_get_array(ptr, "location", cache->true_location);
	cache->flip = RNA_boolean_get(ptr, "flip");
	RNA_int_get_array(ptr, "mouse", cache->mouse);
	
	/* Truly temporary data that isn't stored in properties */

	cache->previous_pixel_radius = cache->pixel_radius;
	cache->pixel_radius = brush_size(sd);

	if(sd->brush->flag & BRUSH_ANCHORED) {
		dx = cache->mouse[0] - cache->initial_mouse[0];
		dy = cache->mouse[1] - cache->initial_mouse[1];
		cache->pixel_radius = sqrt(dx*dx + dy*dy);
		cache->radius = unproject_brush_radius(sd->session, cache->pixel_radius);
		cache->rotation = atan2(dy, dx);
	}
	else if(sd->brush->flag & BRUSH_RAKE) {
		int update;

		dx = cache->last_rake[0] - cache->mouse[0];
		dy = cache->last_rake[1] - cache->mouse[1];

		update = dx*dx + dy*dy > 100;

		/* To prevent jitter, only update the angle if the mouse has moved over 10 pixels */
		if(update && !cache->first_time)
			cache->rotation = M_PI_2 + atan2(dy, dx);

		if(update || cache->first_time) {
			cache->last_rake[0] = cache->mouse[0];
			cache->last_rake[1] = cache->mouse[1];
		}
	}

	/* Find the grab delta */
	if(sd->brush->sculpt_tool == SCULPT_TOOL_GRAB) {
		unproject(cache->mats, grab_location, cache->mouse[0], cache->mouse[1], cache->depth);
		if(!cache->first_time)
			VecSubf(cache->grab_delta, grab_location, cache->old_grab_location);
		VecCopyf(cache->old_grab_location, grab_location);
	}
}

/* Initialize stroke operator properties */
static void sculpt_brush_stroke_init_properties(bContext *C, wmOperator *op, wmEvent *event, SculptSession *ss)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob= CTX_data_active_object(C);
	ModifierData *md;
	ViewContext vc;
	float scale[3], clip_tolerance[3] = {0,0,0};
	int mouse[2], flag = 0;

	/* Set scaling adjustment */
	scale[0] = 1.0f / ob->size[0];
	scale[1] = 1.0f / ob->size[1];
	scale[2] = 1.0f / ob->size[2];
	RNA_float_set_array(op->ptr, "scale", scale);

	/* Initialize mirror modifier clipping */
	for(md= ob->modifiers.first; md; md= md->next) {
		if(md->type==eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
			const MirrorModifierData *mmd = (MirrorModifierData*) md;
			
			/* Mark each axis that needs clipping along with its tolerance */
			if(mmd->flag & MOD_MIR_CLIPPING) {
				flag |= CLIP_X << mmd->axis;
				if(mmd->tolerance > clip_tolerance[mmd->axis])
					clip_tolerance[mmd->axis] = mmd->tolerance;
			}
		}
	}
	RNA_int_set(op->ptr, "flag", flag);
	RNA_float_set_array(op->ptr, "clip_tolerance", clip_tolerance);

	/* Initial mouse location */
	mouse[0] = event->x;
	mouse[1] = event->y;
	RNA_int_set_array(op->ptr, "initial_mouse", mouse);

	/* Initial screen depth under the mouse */
	view3d_set_viewcontext(C, &vc);
	RNA_float_set(op->ptr, "depth", read_cached_depth(&vc, event->x, event->y));

	sculpt_update_cache_invariants(sd, C, op);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	view3d_operator_needs_opengl(C);
	sculpt_brush_stroke_init_properties(C, op, event, sd->session);

	sculptmode_update_all_projverts(sd->session);

	/* TODO: Shouldn't really have to do this at the start of every
	   stroke, but sculpt would need some sort of notification when
	   changes are made to the texture. */
	sculpt_update_tex(sd);

	/* add modal handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static void sculpt_restore_mesh(Sculpt *sd)
{
	StrokeCache *cache = sd->session->cache;
	int i;
	
	/* Restore the mesh before continuing with anchored stroke */
	if((sd->brush->flag & BRUSH_ANCHORED) && cache->mesh_store) {
		for(i = 0; i < sd->session->totvert; ++i) {
			VecCopyf(sd->session->mvert[i].co, cache->mesh_store[i]);
			sd->session->mvert[i].no[0] = cache->orig_norms[i][0];
			sd->session->mvert[i].no[1] = cache->orig_norms[i][1];
			sd->session->mvert[i].no[2] = cache->orig_norms[i][2];
		}
	}
}

static void sculpt_post_stroke_free(SculptSession *ss)
{
	BLI_freelistN(&ss->damaged_rects);
	BLI_freelistN(&ss->damaged_verts);
}

static void sculpt_flush_update(bContext *C)
{
	Sculpt *s = CTX_data_tool_settings(C)->sculpt;
	ARegion *ar = CTX_wm_region(C);
	MultiresModifierData *mmd = s->session->multires;

	calc_damaged_verts(s->session);

	if(mmd) {
		if(mmd->undo_verts && mmd->undo_verts != s->session->mvert)
			MEM_freeN(mmd->undo_verts);
		
		mmd->undo_verts = s->session->mvert;
		mmd->undo_verts_tot = s->session->totvert;
		multires_mark_as_modified(CTX_data_active_object(C));
	}

	ED_region_tag_redraw(ar);
}

static int sculpt_brush_stroke_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	PointerRNA itemptr;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	float center[3];
	int mouse[2] = {event->x, event->y};

	sculpt_update_mesh_elements(C);

	unproject(sd->session->cache->mats, center, event->x, event->y,
		  read_cached_depth(&sd->session->cache->vc, event->x, event->y));

	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);
	RNA_float_set_array(&itemptr, "location", center);
	RNA_int_set_array(&itemptr, "mouse", mouse);
	RNA_boolean_set(&itemptr, "flip", event->shift);
	sculpt_update_cache_variants(sd, &itemptr);

	sculpt_restore_mesh(sd);
	do_symmetrical_brush_actions(CTX_data_tool_settings(C)->sculpt, sd->session->cache);

	sculpt_flush_update(C);
	sculpt_post_stroke_free(sd->session);

	/* Finished */
	if(event->type == LEFTMOUSE && event->val == 0) {
		request_depth_update(sd->session->cache->vc.rv3d);

		sculpt_cache_free(sd->session->cache);

		sculpt_undo_push(C, sd);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	view3d_operator_needs_opengl(C);
	sculpt_update_cache_invariants(sd, C, op);
	sculptmode_update_all_projverts(sd->session);
	sculpt_update_tex(sd);

	RNA_BEGIN(op->ptr, itemptr, "stroke") {
		sculpt_update_cache_variants(sd, &itemptr);

		sculpt_restore_mesh(sd);
		do_symmetrical_brush_actions(sd, sd->session->cache);

		sculpt_post_stroke_free(sd->session);
	}
	RNA_END;

	sculpt_flush_update(C);
	sculpt_cache_free(sd->session->cache);

	sculpt_undo_push(C, sd);

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
	ot->flag |= OPTYPE_REGISTER;

	/* identifiers */
	ot->name= "Sculpt Mode";
	ot->idname= "SCULPT_OT_brush_stroke";
	
	/* api callbacks */
	ot->invoke= sculpt_brush_stroke_invoke;
	ot->modal= sculpt_brush_stroke_modal;
	ot->exec= sculpt_brush_stroke_exec;
	ot->poll= sculpt_poll;
	
	/* flags (sculpt does own undo? (ton) */
	ot->flag= OPTYPE_REGISTER;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");

	/* If the object has a scaling factor, brushes also need to be scaled
	   to work as expected. */
	RNA_def_float_vector(ot->srna, "scale", 3, NULL, 0.0f, FLT_MAX, "Scale", "", 0.0f, 1000.0f);

	RNA_def_int(ot->srna, "flag", 0, 0, INT_MAX, "flag", "", 0, INT_MAX);

	/* For mirror modifiers */
	RNA_def_float_vector(ot->srna, "clip_tolerance", 3, NULL, 0.0f, FLT_MAX, "clip_tolerance", "", 0.0f, 1000.0f);

	/* The initial 2D location of the mouse */
	RNA_def_int_vector(ot->srna, "initial_mouse", 2, NULL, INT_MIN, INT_MAX, "initial_mouse", "", INT_MIN, INT_MAX);

	/* The initial screen depth of the mouse */
	RNA_def_float(ot->srna, "depth", 0.0f, 0.0f, FLT_MAX, "depth", "", 0.0f, FLT_MAX);
}

/**** Toggle operator for turning sculpt mode on or off ****/

static int sculpt_toggle_mode(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);

	if(G.f & G_SCULPTMODE) {
		multires_force_update(CTX_data_active_object(C));

		/* Leave sculptmode */
		G.f &= ~G_SCULPTMODE;

		toggle_paint_cursor(C);

		sculptsession_free(ts->sculpt);
	}
	else {
		MTex *mtex; // XXX: temporary

		/* Enter sculptmode */

		G.f |= G_SCULPTMODE;
		
		/* Create persistent sculpt mode data */
		if(!ts->sculpt)
			ts->sculpt = MEM_callocN(sizeof(Sculpt), "sculpt mode data");

		/* Create sculpt mode session data */
		if(ts->sculpt->session)
			MEM_freeN(ts->sculpt->session);
		ts->sculpt->session = MEM_callocN(sizeof(SculptSession), "sculpt session");

		toggle_paint_cursor(C);

		/* If there's no brush, create one */
		brush_check_exists(&ts->sculpt->brush);

		/* XXX: testing: set the brush texture to the first available one */
		if(G.main->tex.first) {
			Tex *tex = G.main->tex.first;
			if(tex->type) {
				mtex = MEM_callocN(sizeof(MTex), "test mtex");
				ts->sculpt->brush->texact = 0;
				ts->sculpt->brush->mtex[0] = mtex;
				mtex->tex = tex;
				mtex->size[0] = mtex->size[1] = mtex->size[2] = 50;
			}
		}
	}

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Sculpt Mode";
	ot->idname= "SCULPT_OT_sculptmode_toggle";
	
	/* api callbacks */
	ot->exec= sculpt_toggle_mode;
	ot->poll= ED_operator_object_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void ED_operatortypes_sculpt()
{
	WM_operatortype_append(SCULPT_OT_radial_control);
	WM_operatortype_append(SCULPT_OT_brush_stroke);
	WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
	WM_operatortype_append(SCULPT_OT_brush_curve_preset);
}

void sculpt(Sculpt *sd)
{
#if 0
	SculptSession *ss= sd->session;
	Object *ob= NULL; /*XXX */
	Mesh *me;
	MultiresModifierData *mmd = NULL;
	/* lastSigMouse is for the rake, to store the last place the mouse movement was significant */
	short mouse[2], mvalo[2], lastSigMouse[2],firsttime=1, mousebut;
	short modifier_calculations= 0;
	BrushAction *a = MEM_callocN(sizeof(BrushAction), "brush action");
	short spacing= 32000;
	int scissor_box[4];
	float offsetRot;
	int smooth_stroke = 0, i;
	int anchored, rake = 0 /* XXX: rake = ? */;

	/* XXX: checking that sculpting is allowed
	if(!(G.f & G_SCULPTMODE) || G.obedit || !ob || ob->id.lib || !get_mesh(ob) || (get_mesh(ob)->totface == 0))
		return;
	if(!(ob->lay & G.vd->lay))
		error("Active object is not in this layer");
	if(ob_get_keyblock(ob)) {
		if(!(ob->shapeflag & OB_SHAPE_LOCK)) {
			error("Cannot sculpt on unlocked shape key");
			return;
		}
	}*/
	
	anchored = sd->brush->flag & BRUSH_ANCHORED;
	smooth_stroke = (sd->flags & SCULPT_INPUT_SMOOTH) && (sd->brush->sculpt_tool != SCULPT_TOOL_GRAB) && !anchored;

	if(smooth_stroke)
		sculpt_stroke_new(256);

	ss->damaged_rects.first = ss->damaged_rects.last = NULL;
	ss->damaged_verts.first = ss->damaged_verts.last = NULL;
	ss->vertexcosnos = NULL;

	mmd = sculpt_multires_active(ob);

	/* Check that vertex users are up-to-date */
	if(ob != active_ob || !ss->vertex_users || ss->vertex_users_size != cache->totvert) {
		sculpt_vertexusers_free(ss);
		calc_vertex_users(ss);
		if(ss->projverts)
			MEM_freeN(ss->projverts);
		ss->projverts = NULL;
		active_ob= ob;
	}
		
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	/*XXX:
	persp(PERSP_VIEW);
	getmouseco_areawin(mvalo);*/

	/* Init texture
	   FIXME: Shouldn't be doing this every time! */
	if(sd->tex_mode!=SCULPTREPT_3D)
		sculptmode_update_tex(sd);

	/*XXX: getmouseco_areawin(mouse); */
	mvalo[0]= mouse[0];
	mvalo[1]= mouse[1];
	lastSigMouse[0]=mouse[0];
	lastSigMouse[1]=mouse[1];
	mousebut = 0; /* XXX: L_MOUSE; */

	/* If modifier_calculations is true, then extra time must be spent
	   updating the mesh. This takes a *lot* longer, so it's worth
	   skipping if the modifier stack is empty. */
	modifier_calculations= sculpt_modifiers_active(ob);

	if(modifier_calculations)
		ss->vertexcosnos= mesh_get_mapped_verts_nors(NULL, ob); /* XXX: scene = ? */
	sculptmode_update_all_projverts(ss);

	/* Capture original copy */
	if(sd->flags & SCULPT_DRAW_FAST)
		glAccum(GL_LOAD, 1);

	/* Get original scissor box */
	glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
	
	/* For raking, get the original angle*/
	offsetRot=sculpt_tex_angle(sd);

	me = get_mesh(ob);

	while (/*XXX:get_mbut() & mousebut*/0) {
		/* XXX: getmouseco_areawin(mouse); */
		/* If rake, and the mouse has moved over 10 pixels (euclidean) (prevents jitter) then get the new angle */
		if (rake && (pow(lastSigMouse[0]-mouse[0],2)+pow(lastSigMouse[1]-mouse[1],2))>100){
			/*Nasty looking, but just orig + new angle really*/
			set_tex_angle(sd, offsetRot+180.+to_deg(atan2((float)(mouse[1]-lastSigMouse[1]),(float)(mouse[0]-lastSigMouse[0]))));
			lastSigMouse[0]=mouse[0];
			lastSigMouse[1]=mouse[1];
		}
		
		if(firsttime || mouse[0]!=mvalo[0] || mouse[1]!=mvalo[1] ||
		   sd->brush->flag & BRUSH_AIRBRUSH) {
			a->firsttime = firsttime;
			firsttime= 0;

			if(smooth_stroke)
				sculpt_stroke_add_point(ss->stroke, mouse[0], mouse[1]);

			spacing+= sqrt(pow(mvalo[0]-mouse[0],2)+pow(mvalo[1]-mouse[1],2));

			if(modifier_calculations && !ss->vertexcosnos)
				ss->vertexcosnos= mesh_get_mapped_verts_nors(NULL, ob); /*XXX scene = ? */

			if(sd->brush->sculpt_tool != SCULPT_TOOL_GRAB) {
				if(anchored) {
 					/* Restore the mesh before continuing with anchored stroke */
 					/*if(a->mesh_store) {
 						for(i = 0; i < cache->totvert; ++i) {
 							VecCopyf(cache->mvert[i].co, &a->mesh_store[i].x);
							cache->mvert[i].no[0] = a->orig_norms[i][0];
							cache->mvert[i].no[1] = a->orig_norms[i][1];
							cache->mvert[i].no[2] = a->orig_norms[i][2];
						}
						}*/
					
  					//do_symmetrical_brush_actions(sd, a, mouse, NULL);
  				}
				else {
					if(smooth_stroke) {
						sculpt_stroke_apply(sd, ss->stroke);
					}
					else if(sd->spacing==0 || spacing>sd->spacing) {
						//do_symmetrical_brush_actions(sd, a, mouse, NULL);
						spacing= 0;
					}
				}
			}
			else {
				//do_symmetrical_brush_actions(sd, a, mouse, mvalo);
				//unproject(ss, sd->pivot, mouse[0], mouse[1], a->depth);
			}

			if((!ss->multires && modifier_calculations) || ob_get_keyblock(ob)) {
				/* XXX: DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA); */ }

			if(modifier_calculations || sd->brush->sculpt_tool == SCULPT_TOOL_GRAB || !(sd->flags & SCULPT_DRAW_FAST)) {
				calc_damaged_verts(ss, a);
				/*XXX: scrarea_do_windraw(curarea);
				screen_swapbuffers(); */
			} else { /* Optimized drawing */
				calc_damaged_verts(ss, a);

				/* Draw the stored image to the screen */
				glAccum(GL_RETURN, 1);

				sculpt_clear_damaged_areas(ss);
				
				/* Draw all the polygons that are inside the modified area(s) */
				glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
				/* XXX: sculptmode_draw_mesh(1); */
				glAccum(GL_LOAD, 1);

				projverts_clear_inside(ss);

				/* XXX: persp(PERSP_WIN); */
				glDisable(GL_DEPTH_TEST);
				
				/* Draw cursor */
				if(sd->flags & SCULPT_TOOL_DRAW)
					fdrawXORcirc((float)mouse[0],(float)mouse[1],sd->brush->size);
				/* XXX: if(smooth_stroke)
				   sculpt_stroke_draw();
				
				myswapbuffers(); */
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
		else { /*XXX:BIF_wait_for_statechange();*/ }
	}

	/* Set the rotation of the brush back to what it was before any rake */
	set_tex_angle(sd, offsetRot);
	
	if(smooth_stroke) {
		sculpt_stroke_apply_all(sd, ss->stroke);
		calc_damaged_verts(ss, a);
		BLI_freelistN(&ss->damaged_rects);
	}

	//if(a->layer_disps) MEM_freeN(a->layer_disps);
	//if(a->mesh_store) MEM_freeN(a->mesh_store);
	//if(a->orig_norms) MEM_freeN(a->orig_norms);
	for(i=0; i<8; ++i)
		BLI_freelistN(&a->grab_active_verts[i]);
	MEM_freeN(a);
	sculpt_stroke_free(ss->stroke);
	ss->stroke = NULL;

	if(mmd) {
		if(mmd->undo_verts && mmd->undo_verts != cache->mvert)
			MEM_freeN(mmd->undo_verts);
		
		mmd->undo_verts = cache->mvert;
		mmd->undo_verts_tot = cache->totvert;
	}

	//sculpt_undo_push(sd);

	/* XXX: if(G.vd->depths) G.vd->depths->damaged= 1;
	   allqueue(REDRAWVIEW3D, 0); */
#endif
}

/* Partial Mesh Visibility */

/* XXX: Partial vis. always was a mess, have to figure something out */
#if 0
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

	/* XXX: DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA); */
}

static rcti sculptmode_pmv_box()
{
	/*XXX:	short down[2], mouse[2];
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
	return ret;*/
}

void sculptmode_pmv(int mode)
{
	Object *ob= NULL; /*XXX: OBACT; */
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

	/*XXX: scrarea_do_windraw(curarea); */

	waitcursor(0);
}
#endif
