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
#include "BKE_paint.h"
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
#include "paint_intern.h"
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
	float initial_radius;
	float scale[3];
	int flag;
	float clip_tolerance[3];
	float initial_mouse[2];
	float depth;

	/* Variants */
	float radius;
	float true_location[3];
	float location[3];
	float flip;
	float pressure;
	float mouse[2];

	/* The rest is temporary storage that isn't saved as a property */

	int first_time; /* Beginning of stroke may do some things special */

	bglMats *mats;

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

/* Return modified brush strength. Includes the direction of the brush, positive
   values pull vertices, negative values push. Uses tablet pressure and a
   special multiplier found experimentally to scale the strength factor. */
static float brush_strength(Sculpt *sd, StrokeCache *cache)
{
	Brush *brush = paint_brush(&sd->paint);
	/* Primary strength input; square it to make lower values more sensitive */
	float alpha = brush->alpha * brush->alpha;

	float dir= brush->flag & BRUSH_DIR_IN ? -1 : 1;
	float pressure= 1;
	float flip= cache->flip ? -1:1;

	if(brush->flag & BRUSH_ALPHA_PRESSURE)
		pressure *= cache->pressure;
	
	switch(brush->sculpt_tool){
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
static void sculpt_clip(Sculpt *sd, SculptSession *ss, float *co, const float val[3])
{
	int i;

	for(i=0; i<3; ++i) {
		if(sd->flags & (SCULPT_LOCK_X << i))
			continue;

		if((ss->cache->flag & (CLIP_X << i)) && (fabs(co[i]) <= ss->cache->clip_tolerance[i]))
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
static void calc_area_normal(Sculpt *sd, SculptSession *ss, float out[3], const ListBase* active_verts)
{
	Brush *brush = paint_brush(&sd->paint);
	StrokeCache *cache = ss->cache;
	ActiveData *node = active_verts->first;
	const int view = 0; /* XXX: should probably be a flag, not number: brush_type==SCULPT_TOOL_DRAW ? sculptmode_brush()->view : 0; */
	float out_flip[3];
	float *out_dir = cache->view_normal_symmetry;
	
	out[0]=out[1]=out[2] = out_flip[0]=out_flip[1]=out_flip[2] = 0;

	if(brush->flag & BRUSH_ANCHORED) {
		for(; node; node = node->next)
			add_norm_if(out_dir, out, out_flip, cache->orig_norms[node->Index]);
	}
	else {
		for(; node; node = node->next)
			add_norm_if(out_dir, out, out_flip, ss->mvert[node->Index].no);
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

	calc_area_normal(sd, ss, area_normal, active_verts);
	
	while(node){
		float *co= ss->mvert[node->Index].co;

		const float val[3]= {co[0]+area_normal[0]*ss->cache->radius*node->Fade*ss->cache->scale[0],
		                     co[1]+area_normal[1]*ss->cache->radius*node->Fade*ss->cache->scale[1],
		                     co[2]+area_normal[2]*ss->cache->radius*node->Fade*ss->cache->scale[2]};
		                     
		sculpt_clip(sd, ss, co, val);
		
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

static void do_smooth_brush(Sculpt *s, SculptSession *ss, const ListBase* active_verts)
{
	ActiveData *node= active_verts->first;
	int i;
	
	for(i = 0; i < 2; ++i) {
		while(node){
			float *co= ss->mvert[node->Index].co;
			float avg[3], val[3];
			
			neighbor_average(ss, avg, node->Index);
			val[0] = co[0]+(avg[0]-co[0])*node->Fade;
			val[1] = co[1]+(avg[1]-co[1])*node->Fade;
			val[2] = co[2]+(avg[2]-co[2])*node->Fade;
			
			sculpt_clip(s, ss, co, val);
			node= node->next;
		}
	}
}

static void do_pinch_brush(Sculpt *s, SculptSession *ss, const ListBase* active_verts)
{
 	ActiveData *node= active_verts->first;

	while(node) {
		float *co= ss->mvert[node->Index].co;
		const float val[3]= {co[0]+(ss->cache->location[0]-co[0])*node->Fade,
		                     co[1]+(ss->cache->location[1]-co[1])*node->Fade,
		                     co[2]+(ss->cache->location[2]-co[2])*node->Fade};
		sculpt_clip(s, ss, co, val);
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
		sculpt_clip(sd, ss, co, add);

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

	calc_area_normal(sd, ss, area_normal, active_verts);

	while(node){
		float *disp= &ss->layer_disps[node->Index];
		float *co= ss->mvert[node->Index].co;
		float val[3];
		
		*disp+= node->Fade;
		
		/* Don't let the displacement go past the limit */
		if((lim < 0 && *disp < lim) || (lim > 0 && *disp > lim))
			*disp = lim;
		
		val[0] = ss->mesh_co_orig[node->Index][0]+area_normal[0] * *disp*ss->cache->scale[0];
		val[1] = ss->mesh_co_orig[node->Index][1]+area_normal[1] * *disp*ss->cache->scale[1];
		val[2] = ss->mesh_co_orig[node->Index][2]+area_normal[2] * *disp*ss->cache->scale[2];

		sculpt_clip(sd, ss, co, val);

		node= node->next;
	}
}

static void do_inflate_brush(Sculpt *s, SculptSession *ss, const ListBase *active_verts)
{
	ActiveData *node= active_verts->first;
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
		
		sculpt_clip(s, ss, co, add);

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

/* Projects a point onto a plane along the plane's normal */
static void point_plane_project(float intr[3], float co[3], float plane_normal[3], float plane_center[3])
{
	float p1[3], sub1[3], sub2[3];

	/* Find the intersection between squash-plane and vertex (along the area normal) */
	VecSubf(p1, co, plane_normal);
	VecSubf(sub1, plane_center, p1);
	VecSubf(sub2, co, p1);
	VecSubf(intr, co, p1);
	VecMulf(intr, Inpf(plane_normal, sub1) / Inpf(plane_normal, sub2));
	VecAddf(intr, intr, p1);
}

static int plane_point_side(float co[3], float plane_normal[3], float plane_center[3], int flip)
{
	float delta[3];
	float d;

	VecSubf(delta, co, plane_center);
	d = Inpf(plane_normal, delta);

	if(flip)
		d = -d;

	return d <= 0.0f;
}

static void do_flatten_clay_brush(Sculpt *sd, SculptSession *ss, const ListBase *active_verts, int clay)
{
	ActiveData *node= active_verts->first;
	/* area_normal and cntr define the plane towards which vertices are squashed */
	float area_normal[3];
	float cntr[3], cntr2[3], bstr = 0;
	int flip = 0;

	calc_area_normal(sd, ss, area_normal, active_verts);
	calc_flatten_center(ss, node, cntr);

	if(clay) {
		bstr= brush_strength(sd, ss->cache);
		/* Limit clay application to here */
		cntr2[0]=cntr[0]+area_normal[0]*bstr*ss->cache->scale[0];
		cntr2[1]=cntr[1]+area_normal[1]*bstr*ss->cache->scale[1];
		cntr2[2]=cntr[2]+area_normal[2]*bstr*ss->cache->scale[2];
		flip = bstr < 0;
	}
	
	while(node){
		float *co= ss->mvert[node->Index].co;
		float intr[3], val[3];
		
		if(!clay || plane_point_side(co, area_normal, cntr2, flip)) {
			/* Find the intersection between squash-plane and vertex (along the area normal) */		
			point_plane_project(intr, co, area_normal, cntr);

			VecSubf(val, intr, co);

			if(clay) {
				if(bstr > FLT_EPSILON)
					VecMulf(val, node->Fade / bstr);
				else
					VecMulf(val, node->Fade);
				/* Clay displacement */
				val[0]+=area_normal[0] * ss->cache->scale[0]*node->Fade;
				val[1]+=area_normal[1] * ss->cache->scale[1]*node->Fade;
				val[2]+=area_normal[2] * ss->cache->scale[2]*node->Fade;
			}
			else
				VecMulf(val, fabs(node->Fade));

			VecAddf(val, val, co);

			sculpt_clip(sd, ss, co, val);
		}
		
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
static float tex_strength(Sculpt *sd, SculptSession *ss, float *point, const float len)
{
	Brush *br = paint_brush(&sd->paint);
	MTex *tex = NULL;
	float avg= 1;

	if(br->texact >= 0)
		tex = br->mtex[br->texact];

	if(!tex) {
		avg= 1;
	}
	else if(tex->brush_map_mode == MTEX_MAP_MODE_3D) {
		float jnk;

		/* Get strength by feeding the vertex 
		   location directly into a texture */
		externtex(tex, point, &avg,
			  &jnk, &jnk, &jnk, &jnk);
	}
	else if(ss->texcache) {
		const float bsize= ss->cache->pixel_radius * 2;
		const float rot= tex->rot + ss->cache->rotation;
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
		if(tex->brush_map_mode == MTEX_MAP_MODE_TILED) {
			const int sx= (const int)tex->size[0];
			const int sy= (const int)tex->size[1];
			
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
		}
		else if(tex->brush_map_mode == MTEX_MAP_MODE_FIXED) {
			float fx= (point_2d[0] - ss->cache->mouse[0]) / bsize;
			float fy= (point_2d[1] - ss->cache->mouse[1]) / bsize;

			float angle= atan2(fy, fx) - rot;
			float flen= sqrtf(fx*fx + fy*fy);
			
			fx = flen * cos(angle) + 0.5;
			fy = flen * sin(angle) + 0.5;

			avg= get_texcache_pixel_bilinear(ss, fx * ss->texcache_side, fy * ss->texcache_side);
		}
	}

	avg*= brush_curve_strength(br, len, ss->cache->radius); /* Falloff curve */

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

static void do_brush_action(Sculpt *sd, SculptSession *ss, StrokeCache *cache)
{
	Brush *brush = paint_brush(&sd->paint);
	float av_dist;
	ListBase active_verts={0,0};
	ListBase *grab_active_verts = &ss->cache->grab_active_verts[ss->cache->symmetry];
	ActiveData *adata= 0;
	float *vert;
	Mesh *me= NULL; /*XXX: get_mesh(OBACT); */
	const float bstrength= brush_strength(sd, cache);
	KeyBlock *keyblock= NULL; /*XXX: ob_get_keyblock(OBACT); */
	Brush *b = brush;
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
					adata->Fade= tex_strength(sd, ss, vert, av_dist) * bstrength;
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
			do_smooth_brush(sd, ss, &active_verts);
			break;
		case SCULPT_TOOL_PINCH:
			do_pinch_brush(sd, ss, &active_verts);
			break;
		case SCULPT_TOOL_INFLATE:
			do_inflate_brush(sd, ss, &active_verts);
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

static void do_symmetrical_brush_actions(Sculpt *sd, SculptSession *ss)
{
	StrokeCache *cache = ss->cache;
	const char symm = sd->flags & 7;
	int i;

	VecCopyf(cache->location, cache->true_location);
	VecCopyf(cache->grab_delta_symmetry, cache->grab_delta);
	cache->symmetry = 0;
	do_brush_action(sd, ss, cache);

	for(i = 1; i <= symm; ++i) {
		if(symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5))) {
			calc_brushdata_symm(cache, i);
			do_brush_action(sd, ss, cache);
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

static void sculpt_update_tex(Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);

	if(ss->texcache) {
		MEM_freeN(ss->texcache);
		ss->texcache= NULL;
	}

	/* Need to allocate a bigger buffer for bigger brush size */
	ss->texcache_side = brush->size * 2;
	if(!ss->texcache || ss->texcache_side > ss->texcache_actual) {
		ss->texcache = brush_gen_texture_cache(brush, brush->size);
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
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
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

static int sculpt_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	return ob && ob->mode & OB_MODE_SCULPT;
}

static int sculpt_poll(bContext *C)
{
	return sculpt_mode_poll(C) && paint_poll(C);
}

static void sculpt_undo_push(bContext *C, Sculpt *sd)
{
	Brush *brush = paint_brush(&sd->paint);

	switch(brush->sculpt_tool) {
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

/**** Radial control ****/
static int sculpt_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Brush *brush = paint_brush(p);

	WM_paint_cursor_end(CTX_wm_manager(C), p->paint_cursor);
	p->paint_cursor = NULL;
	brush_radial_control_invoke(op, brush, 1);
	return WM_radial_control_invoke(C, op, event);
}

static int sculpt_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);
	if(ret != OPERATOR_RUNNING_MODAL)
		paint_cursor_start(C, sculpt_poll);
	return ret;
}

static int sculpt_radial_control_exec(bContext *C, wmOperator *op)
{
	Brush *brush = paint_brush(&CTX_data_tool_settings(C)->sculpt->paint);
	int ret = brush_radial_control_exec(op, brush, 1);

	WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);

	return ret;
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
	view3d_unproject(ss->cache->mats, brush_edge, ss->cache->initial_mouse[0] + offset,
		  ss->cache->initial_mouse[1], ss->cache->depth);

	return VecLenf(ss->cache->true_location, brush_edge);
}

static void sculpt_cache_free(StrokeCache *cache)
{
	int i;
	if(cache->orig_norms)
		MEM_freeN(cache->orig_norms);
	if(cache->face_norms)
		MEM_freeN(cache->face_norms);
	if(cache->mats)
		MEM_freeN(cache->mats);
	for(i = 0; i < 8; ++i) 
		BLI_freelistN(&cache->grab_active_verts[i]);
	MEM_freeN(cache);
}

/* Initialize the stroke cache invariants from operator properties */
static void sculpt_update_cache_invariants(Sculpt *sd, SculptSession *ss, bContext *C, wmOperator *op)
{
	StrokeCache *cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
	Brush *brush = paint_brush(&sd->paint);
	ViewContext *vc = paint_stroke_view_context(op->customdata);
	int i;

	ss->cache = cache;

	RNA_float_get_array(op->ptr, "scale", cache->scale);
	cache->flag = RNA_int_get(op->ptr, "flag");
	RNA_float_get_array(op->ptr, "clip_tolerance", cache->clip_tolerance);
	RNA_float_get_array(op->ptr, "initial_mouse", cache->initial_mouse);
	cache->depth = RNA_float_get(op->ptr, "depth");

	cache->mouse[0] = cache->initial_mouse[0];
	cache->mouse[1] = cache->initial_mouse[1];

	/* Truly temporary data that isn't stored in properties */

	cache->mats = MEM_callocN(sizeof(bglMats), "sculpt bglMats");
	view3d_get_transformation(vc, vc->obact, cache->mats);

	sculpt_update_mesh_elements(C);

	/* Initialize layer brush displacements */
	if(brush->sculpt_tool == SCULPT_TOOL_LAYER &&
	   (!ss->layer_disps || !(brush->flag & BRUSH_PERSISTENT))) {
		if(ss->layer_disps)
			MEM_freeN(ss->layer_disps);
		ss->layer_disps = MEM_callocN(sizeof(float) * ss->totvert, "layer brush displacements");
	}

	/* Make copies of the mesh vertex locations and normals for some tools */
	if(brush->sculpt_tool == SCULPT_TOOL_LAYER || (brush->flag & BRUSH_ANCHORED)) {
		if(brush->sculpt_tool != SCULPT_TOOL_LAYER ||
		   !ss->mesh_co_orig || !(brush->flag & BRUSH_PERSISTENT)) {
			if(!ss->mesh_co_orig)
				ss->mesh_co_orig= MEM_mallocN(sizeof(float) * 3 * ss->totvert,
								       "sculpt mesh vertices copy");
			for(i = 0; i < ss->totvert; ++i)
				VecCopyf(ss->mesh_co_orig[i], ss->mvert[i].co);
		}

		if(brush->flag & BRUSH_ANCHORED) {
			cache->orig_norms= MEM_mallocN(sizeof(short) * 3 * ss->totvert, "Sculpt orig norm");
			for(i = 0; i < ss->totvert; ++i) {
				cache->orig_norms[i][0] = ss->mvert[i].no[0];
				cache->orig_norms[i][1] = ss->mvert[i].no[1];
				cache->orig_norms[i][2] = ss->mvert[i].no[2];
			}

			if(ss->face_normals) {
				float *fn = ss->face_normals;
				cache->face_norms= MEM_mallocN(sizeof(float) * 3 * ss->totface, "Sculpt face norms");
				for(i = 0; i < ss->totface; ++i, fn += 3)
					VecCopyf(cache->face_norms[i], fn);
			}
		}
	}

	view3d_unproject(cache->mats, cache->true_location, cache->initial_mouse[0], cache->initial_mouse[1], cache->depth);
	cache->initial_radius = unproject_brush_radius(ss, brush->size);
	cache->rotation = 0;
	cache->first_time = 1;
}

/* Initialize the stroke cache variants from operator properties */
static void sculpt_update_cache_variants(Sculpt *sd, SculptSession *ss, PointerRNA *ptr)
{
	StrokeCache *cache = ss->cache;
	Brush *brush = paint_brush(&sd->paint);
	float grab_location[3];
	
	int dx, dy;

	if(!(brush->flag & BRUSH_ANCHORED))
		RNA_float_get_array(ptr, "location", cache->true_location);
	cache->flip = RNA_boolean_get(ptr, "flip");
	RNA_float_get_array(ptr, "mouse", cache->mouse);
	cache->pressure = RNA_float_get(ptr, "pressure");
	
	/* Truly temporary data that isn't stored in properties */

	cache->previous_pixel_radius = cache->pixel_radius;
	cache->pixel_radius = brush->size;

	if(brush->flag & BRUSH_SIZE_PRESSURE) {
		cache->pixel_radius *= cache->pressure;
		cache->radius = cache->initial_radius * cache->pressure;
	}
	else
		cache->radius = cache->initial_radius;

	if(brush->flag & BRUSH_ANCHORED) {
		dx = cache->mouse[0] - cache->initial_mouse[0];
		dy = cache->mouse[1] - cache->initial_mouse[1];
		cache->pixel_radius = sqrt(dx*dx + dy*dy);
		cache->radius = unproject_brush_radius(ss, cache->pixel_radius);
		cache->rotation = atan2(dy, dx);
	}
	else if(brush->flag & BRUSH_RAKE) {
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
	if(brush->sculpt_tool == SCULPT_TOOL_GRAB) {
		view3d_unproject(cache->mats, grab_location, cache->mouse[0], cache->mouse[1], cache->depth);
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
	float scale[3], clip_tolerance[3] = {0,0,0};
	float mouse[2];
	int flag = 0;

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
	RNA_float_set_array(op->ptr, "initial_mouse", mouse);

	/* Initial screen depth under the mouse */
	RNA_float_set(op->ptr, "depth", read_cached_depth(paint_stroke_view_context(op->customdata), event->x, event->y));

	sculpt_update_cache_invariants(sd, ss, C, op);
}

static void sculpt_brush_stroke_init(bContext *C)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	view3d_operator_needs_opengl(C);

	/* TODO: Shouldn't really have to do this at the start of every
	   stroke, but sculpt would need some sort of notification when
	   changes are made to the texture. */
	sculpt_update_tex(sd, ss);

	sculpt_update_mesh_elements(C);
}

static void sculpt_restore_mesh(Sculpt *sd, SculptSession *ss)
{
	StrokeCache *cache = ss->cache;
	Brush *brush = paint_brush(&sd->paint);
	int i;
	
	/* Restore the mesh before continuing with anchored stroke */
	if((brush->flag & BRUSH_ANCHORED) && ss->mesh_co_orig) {
		for(i = 0; i < ss->totvert; ++i) {
			VecCopyf(ss->mvert[i].co, ss->mesh_co_orig[i]);
			ss->mvert[i].no[0] = cache->orig_norms[i][0];
			ss->mvert[i].no[1] = cache->orig_norms[i][1];
			ss->mvert[i].no[2] = cache->orig_norms[i][2];
		}

		if(ss->face_normals) {
			float *fn = ss->face_normals;
			for(i = 0; i < ss->totface; ++i, fn += 3)
				VecCopyf(fn, cache->face_norms[i]);
		}

		if(brush->sculpt_tool == SCULPT_TOOL_LAYER)
			memset(ss->layer_disps, 0, sizeof(float) * ss->totvert);
	}
}

static void sculpt_post_stroke_free(SculptSession *ss)
{
	BLI_freelistN(&ss->damaged_rects);
	BLI_freelistN(&ss->damaged_verts);
}

static void sculpt_flush_update(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	ARegion *ar = CTX_wm_region(C);
	MultiresModifierData *mmd = ss->multires;

	calc_damaged_verts(ss);

	if(mmd) {
		if(mmd->undo_verts && mmd->undo_verts != ss->mvert)
			MEM_freeN(mmd->undo_verts);
		
		mmd->undo_verts = ss->mvert;
		mmd->undo_verts_tot = ss->totvert;
		multires_mark_as_modified(ob);
	}

	ED_region_tag_redraw(ar);
}

static int sculpt_stroke_test_start(bContext *C, struct wmOperator *op, wmEvent *event)
{
	ViewContext vc;
	float cur_depth;

	view3d_set_viewcontext(C, &vc);
	cur_depth = read_cached_depth(&vc, event->x, event->y);
	
	/* Don't start the stroke until a valid depth is found */
	if(cur_depth < 1.0 - FLT_EPSILON) {
		SculptSession *ss = CTX_data_active_object(C)->sculpt;

		sculpt_brush_stroke_init_properties(C, op, event, ss);
		sculptmode_update_all_projverts(ss);

		return 1;
	}
	else
		return 0;
}

static void sculpt_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	sculpt_update_cache_variants(sd, ss, itemptr);
	sculpt_restore_mesh(sd, ss);
	do_symmetrical_brush_actions(sd, ss);

	/* Cleanup */
	sculpt_flush_update(C);
	sculpt_post_stroke_free(ss);
}

static void sculpt_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	/* Finished */
	if(ss->cache) {
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;		

		request_depth_update(paint_stroke_view_context(stroke)->rv3d);
		sculpt_cache_free(ss->cache);
		ss->cache = NULL;
		sculpt_undo_push(C, sd);
	}
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sculpt_brush_stroke_init(C);

	op->customdata = paint_stroke_new(C, sculpt_stroke_test_start,
					  sculpt_stroke_update_step,
					  sculpt_stroke_done);

	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	op->customdata = paint_stroke_new(C, sculpt_stroke_test_start, sculpt_stroke_update_step, sculpt_stroke_done);

	sculpt_brush_stroke_init(C);

	sculpt_update_cache_invariants(sd, ss, C, op);
	sculptmode_update_all_projverts(ss);

	paint_stroke_exec(C, op);

	sculpt_flush_update(C);
	sculpt_cache_free(ss->cache);

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
	ot->modal= paint_stroke_modal;
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
	RNA_def_float_vector(ot->srna, "initial_mouse", 2, NULL, INT_MIN, INT_MAX, "initial_mouse", "", INT_MIN, INT_MAX);

	/* The initial screen depth of the mouse */
	RNA_def_float(ot->srna, "depth", 0.0f, 0.0f, FLT_MAX, "depth", "", 0.0f, FLT_MAX);
}

/**** Reset the copy of the mesh that is being sculpted on (currently just for the layer brush) ****/

static int sculpt_set_persistent_base(bContext *C, wmOperator *op)
{
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	if(ss) {
		if(ss->layer_disps)
			MEM_freeN(ss->layer_disps);
		ss->layer_disps = NULL;

		if(ss->mesh_co_orig)
			MEM_freeN(ss->mesh_co_orig);
		ss->mesh_co_orig = NULL;
	}

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Persistent Base";
	ot->idname= "SCULPT_OT_set_persistent_base";
	
	/* api callbacks */
	ot->exec= sculpt_set_persistent_base;
	ot->poll= sculpt_mode_poll;
	
	ot->flag= OPTYPE_REGISTER;
}

/**** Toggle operator for turning sculpt mode on or off ****/

static int sculpt_toggle_mode(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);

	if(ob->mode & OB_MODE_SCULPT) {
		multires_force_update(ob);

		/* Leave sculptmode */
		ob->mode &= ~OB_MODE_SCULPT;

		free_sculptsession(&ob->sculpt);
	}
	else {
		/* Enter sculptmode */

		ob->mode |= OB_MODE_SCULPT;
		
		/* Create persistent sculpt mode data */
		if(!ts->sculpt)
			ts->sculpt = MEM_callocN(sizeof(Sculpt), "sculpt mode data");

		/* Create sculpt mode session data */
		if(ob->sculpt)
			free_sculptsession(&ob->sculpt);
		ob->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");

		paint_init(&ts->sculpt->paint, PAINT_CURSOR_SCULPT);
		
		paint_cursor_start(C, sculpt_poll);

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
	WM_operatortype_append(SCULPT_OT_set_persistent_base);
}
