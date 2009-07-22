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
	float (*face_norms)[3]; /* Copy of the mesh faces' normals */
	float rotation; /* Texture rotation (radians) for anchored and rake modes */
	int pixel_radius, previous_pixel_radius;
	ListBase grab_active_verts[8]; /* The same list of verts is used throught grab stroke */
	float grab_delta[3], grab_delta_symmetry[3];
	float old_grab_location[3];
	int symmetry; /* Symmetry index between 0 and 7 */
	float view_normal[3], view_normal_symmetry[3];
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
	/* Primary strength input; square it to make lower values more sensitive */
	float alpha = sd->brush->alpha * sd->brush->alpha;

	float dir= sd->brush->flag & BRUSH_DIR_IN ? -1 : 1;
	float pressure= 1;
	float flip= cache->flip ? -1:1;

	if(sd->brush->flag & BRUSH_ALPHA_PRESSURE)
		pressure *= cache->pressure;
	
	switch(sd->brush->sculpt_tool){
	case SCULPT_TOOL_DRAW:
	case SCULPT_TOOL_INFLATE:
	case SCULPT_TOOL_CLAY:
	case SCULPT_TOOL_FLATTEN:
	case SCULPT_TOOL_LAYER:
		return alpha * dir * pressure * flip; /*XXX: not sure why? was multiplied by G.vd->grid */;
	case SCULPT_TOOL_SMOOTH:
		return alpha * 4 * pressure;
	case SCULPT_TOOL_PINCH:
		return alpha / 2 * dir * pressure * flip;
	case SCULPT_TOOL_GRAB:
		return 1;
	default:
		return 0;
	}
}

/* Handles clipping against a mirror modifier and SCULPT_LOCK axis flags */
static void sculpt_clip(Sculpt *sd, float *co, const float val[3])
{
	int i;

	for(i=0; i<3; ++i) {
		if(sd->flags & (SCULPT_LOCK_X << i))
			continue;

		if((sd->session->cache->flag & (CLIP_X << i)) && (fabs(co[i]) <= sd->session->cache->clip_tolerance[i]))
			co[i]= 0.0f;
		else
			co[i]= val[i];
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
	
	while(node){
		float *co= ss->mvert[node->Index].co;

		const float val[3]= {co[0]+area_normal[0]*ss->cache->radius*node->Fade*ss->cache->scale[0],
		                     co[1]+area_normal[1]*ss->cache->radius*node->Fade*ss->cache->scale[1],
		                     co[2]+area_normal[2]*ss->cache->radius*node->Fade*ss->cache->scale[2]};
		                     
		sculpt_clip(sd, co, val);
		
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

static void do_smooth_brush(Sculpt *s, const ListBase* active_verts)
{
	ActiveData *node= active_verts->first;
	int i;
	
	for(i = 0; i < 2; ++i) {
		while(node){
			float *co= s->session->mvert[node->Index].co;
			float avg[3], val[3];
			
			neighbor_average(s->session, avg, node->Index);
			val[0] = co[0]+(avg[0]-co[0])*node->Fade;
			val[1] = co[1]+(avg[1]-co[1])*node->Fade;
			val[2] = co[2]+(avg[2]-co[2])*node->Fade;
			
			sculpt_clip(s, co, val);
			node= node->next;
		}
	}
}

static void do_pinch_brush(Sculpt *s, const ListBase* active_verts)
{
 	ActiveData *node= active_verts->first;

	while(node) {
		float *co= s->session->mvert[node->Index].co;
		const float val[3]= {co[0]+(s->session->cache->location[0]-co[0])*node->Fade,
		                     co[1]+(s->session->cache->location[1]-co[1])*node->Fade,
		                     co[2]+(s->session->cache->location[2]-co[2])*node->Fade};
		sculpt_clip(s, co, val);
		node= node->next;
	}
}

static void do_grab_brush(Sculpt *sd, SculptSession *ss)
{
	ActiveData *node= ss->cache->grab_active_verts[ss->cache->symmetry].first;
	float add[3];
	float grab_delta[3];
	
	VecCopyf(grab_delta, ss->cache->grab_delta_symmetry);
	
	while(node) {
		float *co= ss->mvert[node->Index].co;
		
		VecCopyf(add, grab_delta);
		VecMulf(add, node->Fade);
		VecAddf(add, add, co);
		sculpt_clip(sd, co, add);

		node= node->next;
	}
	
}

static void do_layer_brush(Sculpt *sd, SculptSession *ss, const ListBase *active_verts)
{
	float area_normal[3];
	ActiveData *node= active_verts->first;
	float lim= ss->cache->radius / 4;

	if(ss->cache->flip)
		lim = -lim;

	calc_area_normal(sd, area_normal, active_verts);

	while(node){
		float *disp= &ss->cache->layer_disps[node->Index];
		float *co= ss->mvert[node->Index].co;
		float val[3];
		
		*disp+= node->Fade;
		
		/* Don't let the displacement go past the limit */
		if((lim < 0 && *disp < lim) || (lim > 0 && *disp > lim))
			*disp = lim;
		
		val[0] = ss->cache->mesh_store[node->Index][0]+area_normal[0] * *disp*ss->cache->scale[0];
		val[1] = ss->cache->mesh_store[node->Index][1]+area_normal[1] * *disp*ss->cache->scale[1];
		val[2] = ss->cache->mesh_store[node->Index][2]+area_normal[2] * *disp*ss->cache->scale[2];

		sculpt_clip(sd, co, val);

		node= node->next;
	}
}

static void do_inflate_brush(Sculpt *s, const ListBase *active_verts)
{
	ActiveData *node= active_verts->first;
	SculptSession *ss = s->session;
	float add[3];
	
	while(node) {
		float *co= ss->mvert[node->Index].co;
		short *no= ss->mvert[node->Index].no;

		add[0]= no[0]/ 32767.0f;
		add[1]= no[1]/ 32767.0f;
		add[2]= no[2]/ 32767.0f;
		VecMulf(add, node->Fade * ss->cache->radius);
		add[0]*= ss->cache->scale[0];
		add[1]*= ss->cache->scale[1];
		add[2]*= ss->cache->scale[2];
		VecAddf(add, add, co);
		
		sculpt_clip(s, co, add);

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

static void do_flatten_clay_brush(Sculpt *sd, SculptSession *ss, const ListBase *active_verts, int clay)
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
		VecMulf(val, fabs(node->Fade));
		VecAddf(val, val, co);
		
		if(clay) {
			/* Clay brush displaces after flattening */
			float tmp[3];
			VecCopyf(tmp, area_normal);
			VecMulf(tmp, ss->cache->radius * node->Fade * 0.1);
			VecAddf(val, val, tmp);
		}

		sculpt_clip(sd, co, val);
		
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
			do_smooth_brush(sd, &active_verts);
			break;
		case SCULPT_TOOL_PINCH:
			do_pinch_brush(sd, &active_verts);
			break;
		case SCULPT_TOOL_INFLATE:
			do_inflate_brush(sd, &active_verts);
			break;
		case SCULPT_TOOL_GRAB:
			do_grab_brush(sd, ss);
			break;
		case SCULPT_TOOL_LAYER:
			do_layer_brush(sd, ss, &active_verts);
			break;
		case SCULPT_TOOL_FLATTEN:
			do_flatten_clay_brush(sd, ss, &active_verts, 0);
			break;
		case SCULPT_TOOL_CLAY:
			do_flatten_clay_brush(sd, ss, &active_verts, 1);
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
		ss->mface = dm->getTessFaceDataArray(dm, CD_MFACE);
		ss->face_normals = dm->getTessFaceDataArray(dm, CD_NORMAL);
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

static int sculpt_mode_poll(bContext *C)
{
	return G.f & G_SCULPTMODE;
}

static int sculpt_poll(bContext *C)
{
	return G.f & G_SCULPTMODE && CTX_wm_area(C)->spacetype == SPACE_VIEW3D &&
		CTX_wm_region(C)->regiontype == RGN_TYPE_WINDOW;
}

/*** Sculpt Cursor ***/
static void draw_paint_cursor(bContext *C, int x, int y, void *customdata)
{
	Sculpt *sd= CTX_data_tool_settings(C)->sculpt;
	
	glColor4ub(255, 100, 100, 128);
	glEnable( GL_LINE_SMOOTH );
	glEnable(GL_BLEND);

	glTranslatef((float)x, (float)y, 0.0f);
	glutil_draw_lined_arc(0.0, M_PI*2.0, sd->brush->size, 40);
	glTranslatef((float)-x, (float)-y, 0.0f);

	if(sd->session && sd->session->cache && sd->brush && (sd->brush->flag & BRUSH_SMOOTH_STROKE)) {
		ARegion *ar = CTX_wm_region(C);
		sdrawline(x, y, sd->session->cache->mouse[0] - ar->winrct.xmin, sd->session->cache->mouse[1] - ar->winrct.ymin);
	}

	glDisable(GL_BLEND);
	glDisable( GL_LINE_SMOOTH );
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
	ot->poll= sculpt_mode_poll;

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

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
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
	if(cache->face_norms)
		MEM_freeN(cache->face_norms);
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

	cache->mouse[0] = cache->initial_mouse[0];
	cache->mouse[1] = cache->initial_mouse[1];

	/* Truly temporary data that isn't stored in properties */

	view3d_set_viewcontext(C, &cache->vc);

	cache->mats = MEM_callocN(sizeof(bglMats), "sculpt bglMats");
	view3d_get_transformation(&cache->vc, cache->vc.obact, cache->mats);

	sculpt_update_mesh_elements(C);

	if(sd->brush->sculpt_tool == SCULPT_TOOL_LAYER)
		cache->layer_disps = MEM_callocN(sizeof(float) * sd->session->totvert, "layer brush displacements");

	/* Make copies of the mesh vertex locations and normals for some tools */
	if(sd->brush->sculpt_tool == SCULPT_TOOL_LAYER || (sd->brush->flag & BRUSH_ANCHORED)) {
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

			if(sd->session->face_normals) {
				float *fn = sd->session->face_normals;
				cache->face_norms= MEM_mallocN(sizeof(float) * 3 * sd->session->totface, "Sculpt face norms");
				for(i = 0; i < sd->session->totface; ++i, fn += 3)
					VecCopyf(cache->face_norms[i], fn);
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
	SculptSession *ss = sd->session;
	StrokeCache *cache = ss->cache;
	int i;
	
	/* Restore the mesh before continuing with anchored stroke */
	if((sd->brush->flag & BRUSH_ANCHORED) && cache->mesh_store) {
		for(i = 0; i < ss->totvert; ++i) {
			VecCopyf(ss->mvert[i].co, cache->mesh_store[i]);
			ss->mvert[i].no[0] = cache->orig_norms[i][0];
			ss->mvert[i].no[1] = cache->orig_norms[i][1];
			ss->mvert[i].no[2] = cache->orig_norms[i][2];
		}

		if(ss->face_normals) {
			float *fn = ss->face_normals;
			for(i = 0; i < ss->totface; ++i, fn += 3)
				VecCopyf(fn, cache->face_norms[i]);
		}

		if(sd->brush->sculpt_tool == SCULPT_TOOL_LAYER)
			memset(cache->layer_disps, 0, sizeof(float) * ss->totvert);
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

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static int sculpt_smooth_stroke(Sculpt *s, int output[2], wmEvent *event)
{
	output[0] = event->x;
	output[1] = event->y;

	if(s->brush->flag & BRUSH_SMOOTH_STROKE && s->brush->sculpt_tool != SCULPT_TOOL_GRAB) {
		StrokeCache *cache = s->session->cache;
		float u = .9, v = 1.0 - u;
		int dx = cache->mouse[0] - event->x, dy = cache->mouse[1] - event->y;
		int radius = 50;

		/* If the mouse is moving within the radius of the last move,
		   don't update the mouse position. This allows sharp turns. */
		if(dx*dx + dy*dy < radius*radius)
			return 0;

		output[0] = event->x * v + cache->mouse[0] * u;
		output[1] = event->y * v + cache->mouse[1] * u;
	}

	return 1;
}

/* Returns zero if the stroke dots should not be spaced, non-zero otherwise */
int sculpt_space_stroke_enabled(Sculpt *s)
{
	Brush *br = s->brush;
	return (br->flag & BRUSH_SPACE) && !(br->flag & BRUSH_ANCHORED) && (br->sculpt_tool != SCULPT_TOOL_GRAB);
}

/* Put the location of the next sculpt stroke dot into the stroke RNA and apply it to the mesh */
static void sculpt_brush_stroke_add_step(bContext *C, wmOperator *op, wmEvent *event, int mouse[2])
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	StrokeCache *cache = sd->session->cache;
	PointerRNA itemptr;
	float cur_depth;
	float center[3];

	cur_depth = read_cached_depth(&cache->vc, mouse[0], mouse[1]);
	unproject(sd->session->cache->mats, center, mouse[0], mouse[1], cur_depth);
				
	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);
	RNA_float_set_array(&itemptr, "location", center);
	RNA_int_set_array(&itemptr, "mouse", mouse);
	RNA_boolean_set(&itemptr, "flip", event->shift);
	sculpt_update_cache_variants(sd, &itemptr);
				
	sculpt_restore_mesh(sd);
	do_symmetrical_brush_actions(sd, cache);
}

/* For brushes with stroke spacing enabled, moves mouse in steps
   towards the final mouse location. */
static int sculpt_space_stroke(bContext *C, wmOperator *op, wmEvent *event, Sculpt *s, const int final_mouse[2])
{
	StrokeCache *cache = s->session->cache;
	int cnt = 0;

	if(sculpt_space_stroke_enabled(s)) {
		float vec[2] = {final_mouse[0] - cache->mouse[0], final_mouse[1] - cache->mouse[1]};
		int mouse[2] = {cache->mouse[0], cache->mouse[1]};
		float length, scale;
		int steps = 0, i;

		/* Normalize the vector between the last stroke dot and the goal */
		length = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		if(length > FLT_EPSILON) {
			scale = s->brush->spacing / length;
			vec[0] *= scale;
			vec[1] *= scale;

			steps = (int)(length / s->brush->spacing);
			for(i = 0; i < steps; ++i, ++cnt) {
				mouse[0] += vec[0];
				mouse[1] += vec[1];
				sculpt_brush_stroke_add_step(C, op, event, mouse);
			}
		}
	}

	return cnt;
}

static int sculpt_brush_stroke_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	ARegion *ar = CTX_wm_region(C);
	float cur_depth;

	sculpt_update_mesh_elements(C);

	if(!sd->session->cache) {
		ViewContext vc;
		view3d_set_viewcontext(C, &vc);
		cur_depth = read_cached_depth(&vc, event->x, event->y);

		/* Don't start the stroke until a valid depth is found */
		if(cur_depth < 1.0 - FLT_EPSILON) {
			sculpt_brush_stroke_init_properties(C, op, event, sd->session);
			sculptmode_update_all_projverts(sd->session);
		}

		ED_region_tag_redraw(ar);
	}

	if(sd->session->cache) {
		int mouse[2];

		if(sculpt_smooth_stroke(sd, mouse, event)) {
			if(sculpt_space_stroke_enabled(sd)) {
				if(!sculpt_space_stroke(C, op, event, sd, mouse))
					ED_region_tag_redraw(ar);
			}
			else
				sculpt_brush_stroke_add_step(C, op, event, mouse);

			sculpt_flush_update(C);
			sculpt_post_stroke_free(sd->session);
		}
		else
			ED_region_tag_redraw(ar);
	}

	/* Finished */
	if(event->type == LEFTMOUSE && event->val == 0) {
		if(sd->session->cache) {
			request_depth_update(sd->session->cache->vc.rv3d);
			sculpt_cache_free(sd->session->cache);
			sd->session->cache = NULL;
			sculpt_undo_push(C, sd);
		}

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
	ot->flag= OPTYPE_REGISTER|OPTYPE_BLOCKING;

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

		WM_event_add_notifier(C, NC_SCENE|ND_MODE, CTX_data_scene(C));
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
