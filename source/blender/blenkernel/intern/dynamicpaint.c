/**
***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Contributor(s): Miika Hämäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include "MEM_guardedalloc.h"

#include <math.h>
#include "stdio.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_kdtree.h"
#include "BLI_utildefines.h"

#include "BKE_bvhutils.h"	/* bvh tree	*/
#include "BKE_blender.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_colortools.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_report.h"
#include "BKE_texture.h"

#include "DNA_dynamicpaint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"	/* to get temp file path	*/

/* for bake operator	*/
#include "ED_screen.h"
#include "WM_types.h"
#include "WM_api.h"

/* for image output	*/
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "BKE_image.h"
#include "intern/IMB_filetype.h"
#ifdef WITH_OPENEXR
#include "intern/openexr/openexr_api.h"
#endif

/* uv validate	*/
#include "intern/MOD_util.h"

/* Platform independend time	*/
#include "PIL_time.h"

/* to read object material color	*/
#include "DNA_texture_types.h"
#include "../render/intern/include/render_types.h"
#include "../render/intern/include/voxeldata.h"
#include "DNA_material_types.h"
#include "RE_render_ext.h"

#include "BKE_dynamicpaint.h"


#define DPOUTPUT_JPEG 0
#define DPOUTPUT_PNG 1
#define DPOUTPUT_OPENEXR 2

#define DPOUTPUT_PAINT 0
#define DPOUTPUT_WET 1
#define DPOUTPUT_DISPLACE 2

struct Object;
struct Scene;
struct DerivedMesh;
struct DynamicPaintModifierData;

/*
*	Init predefined antialias jitter data
*/
float jitterDistances[5] = {0.0f,
							0.447213595f,
							0.447213595f,
							0.447213595f,
							0.5f};

/* precalculated gaussian factors for 5x super sampling	*/
float gaussianFactors[5] = {	0.996849f,
								0.596145f,
								0.596145f,
								0.596145f,
								0.524141f};
float gaussianTotal = 3.309425f;

/*
*	Neighbouring pixel table x and y list
*/
int neighX[8] = {1,1,0,-1,-1,-1, 0, 1};
int neighY[8] = {0,1,1, 1, 0,-1,-1,-1};


/*
*	Modifier call. Updates derived mesh data if baking.
*/
void dynamicPaint_Modifier_update(struct DynamicPaintModifierData *pmd, DerivedMesh *dm)
{

	if (!pmd->baking) return;

	if((pmd->type & MOD_DYNAMICPAINT_TYPE_CANVAS) && pmd->canvas) {

		if (pmd->canvas->dm) pmd->canvas->dm->release(pmd->canvas->dm);
		pmd->canvas->dm = CDDM_copy(dm);
	}
	else if((pmd->type & MOD_DYNAMICPAINT_TYPE_PAINT) && pmd->paint) {

		if (pmd->paint->dm) pmd->paint->dm->release(pmd->paint->dm);
		pmd->paint->dm = CDDM_copy(dm);
	}

}

/*
*	Free canvas data.
*/
static void dynamicPaint_Modifier_freeCanvas(struct DynamicPaintModifierData *pmd)
{
	if(pmd->canvas)
	{
		if (pmd->canvas->dm)
			pmd->canvas->dm->release(pmd->canvas->dm);
		pmd->canvas->dm = NULL;
	}
}

/*
*	Free paint data.
*/
static void dynamicPaint_Modifier_freePaint(struct DynamicPaintModifierData *pmd)
{
	if(pmd->paint)
	{
		if(pmd->paint->dm)
			pmd->paint->dm->release(pmd->paint->dm);
		pmd->paint->dm = NULL;


		if(pmd->paint->paint_ramp)
			 MEM_freeN(pmd->paint->paint_ramp);
		pmd->paint->paint_ramp = NULL;
	}
}

/*
*	Free whole dp modifier.
*/
void dynamicPaint_Modifier_free(struct DynamicPaintModifierData *pmd)
{
	if(pmd)
	{
		dynamicPaint_Modifier_freeCanvas(pmd);
		dynamicPaint_Modifier_freePaint(pmd);
	}
}

/*
*	Initialize modifier data.
*/
void dynamicPaint_Modifier_createType(struct DynamicPaintModifierData *pmd)
{
	if(pmd)
	{
		if(pmd->type & MOD_DYNAMICPAINT_TYPE_CANVAS)
		{
			if(pmd->canvas)
				dynamicPaint_Modifier_freeCanvas(pmd);

			pmd->canvas = MEM_callocN(sizeof(DynamicPaintCanvasSettings), "DynamicPaintCanvas");
			pmd->canvas->pmd = pmd;

			pmd->canvas->flags = MOD_DPAINT_ANTIALIAS | MOD_DPAINT_MULALPHA | MOD_DPAINT_DRY_LOG;
			pmd->canvas->output = MOD_DPAINT_OUT_PAINT;
			pmd->canvas->effect = 0;
			pmd->canvas->effect_ui = 1;

			pmd->canvas->diss_speed = 300;
			pmd->canvas->dry_speed = 300;
			pmd->canvas->dflat_speed = 300;

			pmd->canvas->disp_depth = 1.0f;
			pmd->canvas->disp_type = MOD_DPAINT_DISP_DISPLACE;
			pmd->canvas->disp_format = MOD_DPAINT_DISPFOR_PNG;

			pmd->canvas->resolution = 256;
			pmd->canvas->start_frame = 1;
			pmd->canvas->end_frame = 100;
			pmd->canvas->substeps = 0;

			pmd->canvas->spread_speed = 1.0f;
			pmd->canvas->drip_speed = 1.0f;
			pmd->canvas->shrink_speed = 1.0f;

			sprintf(pmd->canvas->paint_output_path, "%spaintmap", "/tmp\\");
			sprintf(pmd->canvas->wet_output_path, "%swetmap", "/tmp\\");
			sprintf(pmd->canvas->displace_output_path, "%sdispmap", "/tmp\\");

			pmd->canvas->ui_info[0] = '\0';


			pmd->canvas->dm = NULL;

		}
		else if(pmd->type & MOD_DYNAMICPAINT_TYPE_PAINT)
		{
			if(pmd->paint)
				dynamicPaint_Modifier_freePaint(pmd);

			pmd->paint = MEM_callocN(sizeof(DynamicPaintPainterSettings), "DynamicPaint Paint");
			pmd->paint->pmd = pmd;

			pmd->paint->psys = NULL;

			pmd->paint->flags = MOD_DPAINT_DO_PAINT | MOD_DPAINT_DO_WETNESS | MOD_DPAINT_DO_DISPLACE | MOD_DPAINT_ABS_ALPHA;
			pmd->paint->collision = MOD_DPAINT_COL_VOLUME;
			
			pmd->paint->mat = NULL;
			pmd->paint->r = 1.0f;
			pmd->paint->g = 1.0f;
			pmd->paint->b = 1.0f;
			pmd->paint->alpha = 1.0f;
			pmd->paint->wetness = 1.0f;

			pmd->paint->paint_distance = 0.1f;
			pmd->paint->proximity_falloff = MOD_DPAINT_PRFALL_SHARP;

			pmd->paint->displace_distance = 0.5f;
			pmd->paint->prox_displace_strength = 0.5f;

			pmd->paint->particle_radius = 0.2;
			pmd->paint->particle_smooth = 0.05;

			pmd->paint->dm = NULL;

			/*
			*	Paint proximity falloff colorramp.
			*/
			{
				CBData *ramp;

				pmd->paint->paint_ramp = add_colorband(0);
				ramp = pmd->paint->paint_ramp->data;
				/* Add default smooth-falloff ramp.	*/
				ramp[0].r = ramp[0].g = ramp[0].b = ramp[0].a = 1.0f;
				ramp[0].pos = 0.0f;
				ramp[1].r = ramp[1].g = ramp[1].b = ramp[1].pos = 1.0f;
				ramp[1].a = 0.0f;
				pmd->paint->paint_ramp->tot = 2;
			}
		}
	}
}

void dynamicPaint_Modifier_copy(struct DynamicPaintModifierData *pmd, struct DynamicPaintModifierData *tpmd)
{
	/* Init modifier	*/
	tpmd->type = pmd->type;
	dynamicPaint_Modifier_createType(tpmd);

	/* Copy data	*/
	if (tpmd->canvas) {
		pmd->canvas->pmd = tpmd;

		tpmd->canvas->flags = pmd->canvas->flags;
		tpmd->canvas->output = pmd->canvas->output;
		tpmd->canvas->disp_type = pmd->canvas->disp_type;
		tpmd->canvas->disp_format = pmd->canvas->disp_format;
		tpmd->canvas->effect = pmd->canvas->effect;
		tpmd->canvas->effect_ui = 1;

		tpmd->canvas->resolution = pmd->canvas->resolution;
		tpmd->canvas->start_frame = pmd->canvas->start_frame;
		tpmd->canvas->end_frame = pmd->canvas->end_frame;
		tpmd->canvas->substeps = pmd->canvas->substeps;

		tpmd->canvas->dry_speed = pmd->canvas->dry_speed;
		tpmd->canvas->diss_speed = pmd->canvas->diss_speed;
		tpmd->canvas->disp_depth = pmd->canvas->disp_depth;
		tpmd->canvas->dflat_speed = pmd->canvas->dflat_speed;

		strncpy(tpmd->canvas->paint_output_path, pmd->canvas->paint_output_path, 240);
		strncpy(tpmd->canvas->wet_output_path, pmd->canvas->wet_output_path, 240);
		strncpy(tpmd->canvas->displace_output_path, pmd->canvas->displace_output_path, 240);

		tpmd->canvas->spread_speed = pmd->canvas->spread_speed;
		tpmd->canvas->drip_speed = pmd->canvas->drip_speed;
		tpmd->canvas->shrink_speed = pmd->canvas->shrink_speed;

		strncpy(tpmd->canvas->uvlayer_name, tpmd->canvas->uvlayer_name, 32);

	} else if (tpmd->paint) {
		pmd->paint->pmd = tpmd;

		tpmd->paint->flags = pmd->paint->flags;
		tpmd->paint->collision = pmd->paint->collision;

		tpmd->paint->r = pmd->paint->r;
		tpmd->paint->g = pmd->paint->g;
		tpmd->paint->b = pmd->paint->b;
		tpmd->paint->alpha = pmd->paint->alpha;
		tpmd->paint->wetness = pmd->paint->wetness;

		tpmd->paint->particle_radius = pmd->paint->particle_radius;
		tpmd->paint->particle_smooth = pmd->paint->particle_smooth;
		tpmd->paint->paint_distance = pmd->paint->paint_distance;
		tpmd->paint->psys = pmd->paint->psys;
		tpmd->paint->displace_distance = pmd->paint->displace_distance;
		tpmd->paint->prox_displace_strength = pmd->paint->prox_displace_strength;

		tpmd->paint->paint_ramp = pmd->paint->paint_ramp;

		tpmd->paint->proximity_falloff = pmd->paint->proximity_falloff;
	}
}



void dynamicPaint_Modifier_do(DynamicPaintModifierData *pmd, Scene *scene, Object *ob, DerivedMesh *dm)
{	
	/* Update derived mesh data to modifier if baking	*/
	dynamicPaint_Modifier_update(pmd, dm);
}


/*
*	Tries to find the neighbouring pixel in given (uv space) direction.
*	Result is used by effect system to move paint on the surface.
*
*   px,py : origin pixel x and y
*	n_index : lookup direction index (use neighX,neighY to get final index)
*/
static int dynamicPaint_findNeighbourPixel(DynamicPaintCanvasSettings *canvas, int px, int py, int n_index)
{
	/* Note: Current method only uses polygon edges to detect neighbouring pixels.
	*  -> It doesn't always lead to the optimum pixel but is accurate enough
	*  and faster/simplier than including possible face tip point links)
	*/

	int x,y;
	PaintSurfacePoint *tPoint = NULL;
	PaintSurfacePoint *cPoint = NULL;
	PaintSurface *surface = NULL;

	surface = canvas->surface;

	x = px + neighX[n_index];
	y = py + neighY[n_index];

	if (x<0 || x>=surface->w) return -1;
	if (y<0 || y>=surface->h) return -1;

	tPoint = (&surface->point[x+surface->w*y]);		/* UV neighbour */
	
	cPoint = (&surface->point[px+surface->w*py]);	/* Origin point */

	/*
	*	Check if target point is on same face -> mark it as neighbour
	*   (and if it isn't marked as an "edge pixel")
	*/
	if ((tPoint->index == cPoint->index) && (tPoint->neighbour_pixel == -1)) {
		/* If it's on the same face, it has to be a correct neighbour	*/
		return (x+surface->w*y);
	}


	/*
	*	If the uv neighbour is mapped directly to
	*	a face -> use this point.
	*	
	*	!! Replace with "is uv faces linked" check !!
	*	This should work fine as long as uv island
	*	margin is > 1 pixel.
	*/
	if ((tPoint->index != -1) && (tPoint->neighbour_pixel == -1)) {
		return (x+surface->w*y);
	}

	/*
	*	If we get here, the actual neighbouring pixel
	*	points to a non-linked uv face, and we have to find
	*	it's "real" neighbour.
	*
	*	Simple neighbouring face finding algorithm:
	*	- find closest uv edge to that pixel and get
	*	  the other face connected to that edge on actual mesh
	*	- find corresponding position of that new face edge
	*	  in uv space
	*
	*	TODO: Implement something more accurate / optimized?
	*/
	{
		int numOfFaces = canvas->dm->getNumFaces(canvas->dm);
		MVert *mvert = NULL;
		MFace *mface = NULL;
		MTFace *tface = NULL;

		mvert = canvas->dm->getVertArray(canvas->dm);
		mface = canvas->dm->getFaceArray(canvas->dm);
		tface = DM_get_face_data_layer(canvas->dm, CD_MTFACE);

		/* Get closest edge to that subpixel on UV map	*/
		{
			float pixel[2], dist, t_dist;
			int i, uindex[2], edge1_index, edge2_index, e1_index, e2_index, target_face;

			float closest_point[2], lambda, dir_vec[2];
			int target_uv1, target_uv2, final_pixel[2], final_index;

			float (*s_uv1),(*s_uv2), (*t_uv1), (*t_uv2);

			pixel[0] = ((float)(px + neighX[n_index]) + 0.5f) / (float)surface->w;
			pixel[1] = ((float)(py + neighY[n_index]) + 0.5f) / (float)surface->h;

			/* Get uv indexes for current face part	*/
			if (cPoint->quad) {
				uindex[0] = 0; uindex[1] = 2; uindex[2] = 3;
			}
			else {
				uindex[0] = 0; uindex[1] = 1; uindex[2] = 2;
			}

			/*
			*	Find closest edge to that pixel
			*/
			/* Dist to first edge	*/
			e1_index = cPoint->v1; e2_index = cPoint->v2; edge1_index = uindex[0]; edge2_index = uindex[1];
			dist = dist_to_line_segment_v2(pixel, tface[cPoint->index].uv[edge1_index], tface[cPoint->index].uv[edge2_index]);

			/* Dist to second edge	*/
			t_dist = dist_to_line_segment_v2(pixel, tface[cPoint->index].uv[uindex[1]], tface[cPoint->index].uv[uindex[2]]);
			if (t_dist < dist) {e1_index = cPoint->v2; e2_index = cPoint->v3; edge1_index = uindex[1]; edge2_index = uindex[2]; dist = t_dist;}

			/* Dist to third edge	*/
			t_dist = dist_to_line_segment_v2(pixel, tface[cPoint->index].uv[uindex[2]], tface[cPoint->index].uv[uindex[0]]);
			if (t_dist < dist) {e1_index = cPoint->v3; e2_index = cPoint->v1;  edge1_index = uindex[2]; edge2_index = uindex[0]; dist = t_dist;}


			/*
			*	Now find another face that is linked to that edge
			*/
			target_face = -1;

			for (i=0; i<numOfFaces; i++) {
				/*
				*	Check if both edge vertices share this face
				*/
				int v4 = -1;
				if (mface[i].v4) v4 = mface[i].v4;

				if ((e1_index == mface[i].v1 || e1_index == mface[i].v2 || e1_index == mface[i].v3 || e1_index == v4) &&
					(e2_index == mface[i].v1 || e2_index == mface[i].v2 || e2_index == mface[i].v3 || e2_index == v4)) {
					if (i == cPoint->index) continue;

					target_face = i;

					/*
					*	Get edge UV index
					*/
					if (e1_index == mface[i].v1) target_uv1 = 0;
					else if (e1_index == mface[i].v2) target_uv1 = 1;
					else if (e1_index == mface[i].v3) target_uv1 = 2;
					else target_uv1 = 3;

					if (e2_index == mface[i].v1) target_uv2 = 0;
					else if (e2_index == mface[i].v2) target_uv2 = 1;
					else if (e2_index == mface[i].v3) target_uv2 = 2;
					else target_uv2 = 3;

					break;
				}
			}

			/* If none found return -1	*/
			if (target_face == -1) return -1;

			/*
			*	If target face is connected in UV space as well, just use original index
			*/
			s_uv1 = (float *)tface[cPoint->index].uv[edge1_index];
			s_uv2 = (float *)tface[cPoint->index].uv[edge2_index];
			t_uv1 = (float *)tface[target_face].uv[target_uv1];
			t_uv2 = (float *)tface[target_face].uv[target_uv2];

			//printf("connected UV : %f,%f & %f,%f - %f,%f & %f,%f\n", s_uv1[0], s_uv1[1], s_uv2[0], s_uv2[1], t_uv1[0], t_uv1[1], t_uv2[0], t_uv2[1]);

			if (((s_uv1[0] == t_uv1[0] && s_uv1[1] == t_uv1[1]) &&
				 (s_uv2[0] == t_uv2[0] && s_uv2[1] == t_uv2[1]) ) ||
				((s_uv2[0] == t_uv1[0] && s_uv2[1] == t_uv1[1]) &&
				 (s_uv1[0] == t_uv2[0] && s_uv1[1] == t_uv2[1]) )) return ((px+neighX[n_index]) + surface->w*(py+neighY[n_index]));

			/*
			*	Find a point that is relatively at same edge position
			*	on this other face UV
			*/
			lambda = closest_to_line_v2(closest_point, pixel, tface[cPoint->index].uv[edge1_index], tface[cPoint->index].uv[edge2_index]);
			if (lambda < 0.0f) lambda = 0.0f;
			if (lambda > 1.0f) lambda = 1.0f;

			sub_v2_v2v2(dir_vec, tface[target_face].uv[target_uv2], tface[target_face].uv[target_uv1]);

			mul_v2_fl(dir_vec, lambda);

			copy_v2_v2(pixel, tface[target_face].uv[target_uv1]);
			add_v2_v2(pixel, dir_vec);
			pixel[0] = (pixel[0] * (float)surface->w) - 0.5f;
			pixel[1] = (pixel[1] * (float)surface->h) - 0.5f;

			final_pixel[0] = (int)floor(pixel[0]);
			final_pixel[1] = (int)floor(pixel[1]);

			/* If current pixel uv is outside of texture	*/
			if (final_pixel[0] < 0 || final_pixel[0] >= surface->w) return -1;
			if (final_pixel[1] < 0 || final_pixel[1] >= surface->h) return -1;

			final_index = final_pixel[0] + surface->w * final_pixel[1];

			/* If we ended up to our origin point ( mesh has smaller than pixel sized faces)	*/
			if (final_index == (px+surface->w*py)) return -1;
			/* If found pixel still lies on wrong face ( mesh has smaller than pixel sized faces)	*/
			if (surface->point[final_index].index != target_face) return -1;

			/*
			*	If final point is an "edge pixel", use it's "real" neighbour instead
			*/
			if (surface->point[final_index].neighbour_pixel != -1) final_index = cPoint->neighbour_pixel;

			return final_index;
		}
	}
}

/*
*	Output error message to both ui and console
*/
static void dpError(DynamicPaintCanvasSettings *canvas, char *string)
{

	if (strlen(string)>64) string[63] = '\0';

	/* Add error to canvas ui info label */
	sprintf(canvas->error, string);

	/* Print console output */
	printf("DynamicPaint bake failed: %s\n", canvas->error);
}

/*
*	Create Canvas Surface for baking
*/
static int dynamicPaint_createCanvasSurface(DynamicPaintCanvasSettings *canvas)
{

		int yy;
		int w,h;

		/* Antialias jitter point relative coords	*/
		float jitter5sample[10] =  {0.0f, 0.0f,
								-0.2f, -0.4f,
								0.2f, 0.4f,
								0.4f, -0.2f,
								-0.4f, 0.3f};

		DerivedMesh *dm = canvas->dm;
		int numOfFaces;
		MVert *mvert = NULL;
		MFace *mface = NULL;
		MTFace *tface = NULL;

		PaintSurface *surface = NULL;
		BB2d *faceBB = NULL;
		char uvname[32];

		if (!dm) { dpError(canvas, "Canvas mesh not updated."); return 0;}
		numOfFaces = dm->getNumFaces(dm);


		/* Allocate memory for surface	*/
		canvas->surface = (struct PaintSurface *) MEM_callocN(sizeof(struct PaintSurface), "MPCanvasSurface");
		if (canvas->surface == NULL) {dpError(canvas, "Not enough free memory."); return 0;}

		surface = canvas->surface;
		surface->point = NULL;

		mvert = dm->getVertArray(dm);
		mface = dm->getFaceArray(dm);

		validate_layer_name(&dm->faceData, CD_MTFACE, canvas->uvlayer_name, uvname);
		tface = CustomData_get_layer_named(&dm->faceData, CD_MTFACE, uvname);

		/* Check for validity	*/
		if (!tface) {dpError(canvas, "No UV data on canvas."); return 0;}
		if (canvas->resolution < 16 || canvas->resolution > 8096) {dpError(canvas, "Invalid resolution."); return 0;}
	
		w = h = canvas->resolution;
		surface->w = w;
		surface->h = h;

		/*
		*	Start generating the surface
		*/
		printf("DynamicPaint: Preparing canvas of %ix%i pixels and %i faces.\n", w, h, numOfFaces);

		surface->pixelSamples = (canvas->flags & MOD_DPAINT_ANTIALIAS) ? 5 : 1;
		surface->point = (struct PaintSurfacePoint *) MEM_callocN(w*h*sizeof(struct PaintSurfacePoint), "PaintSurfaceData");
		if (surface->point == NULL) {dpError(canvas, "Not enough free memory."); return 0;}

		/*
		*	Generate a temporary bounding box array for UV faces to optimize
		*	the pixel-inside-a-face search.
		*/
		faceBB = (struct BB2d *) MEM_mallocN(numOfFaces*sizeof(struct BB2d), "MPCanvasFaceBB");
		if (faceBB == NULL) {dpError(canvas, "Not enough free memory."); return 0;}

		for (yy=0; yy<numOfFaces; yy++) {

			int numOfVert = (mface[yy].v4) ? 4 : 3;
			int i;
	
			VECCOPY2D(faceBB[yy].min, tface[yy].uv[0]);
			VECCOPY2D(faceBB[yy].max, tface[yy].uv[0]);

			for (i = 1; i<numOfVert; i++) {
				
				if (tface[yy].uv[i][0] < faceBB[yy].min[0]) faceBB[yy].min[0] = tface[yy].uv[i][0];
				if (tface[yy].uv[i][1] < faceBB[yy].min[1]) faceBB[yy].min[1] = tface[yy].uv[i][1];

				if (tface[yy].uv[i][0] > faceBB[yy].max[0]) faceBB[yy].max[0] = tface[yy].uv[i][0];
				if (tface[yy].uv[i][1] > faceBB[yy].max[1]) faceBB[yy].max[1] = tface[yy].uv[i][1];

			}
		}	// end face loop

		/*
		*	Allocate antialias sample data (without threads due to malloc)
		*	(Non threadable?)
		*/
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int index = xx+w*yy;
				PaintSurfacePoint *cPoint = (&surface->point[index]);

				/* Initialize barycentricWeights	*/
				cPoint->barycentricWeights = (struct Vec3f *) malloc( surface->pixelSamples * sizeof(struct Vec3f ));
				if (cPoint->barycentricWeights == NULL) {dpError(canvas, "Not enough free memory."); return 0;}

			}
		} // end pixel loop

		/*
		*	Loop through every pixel and check
		*	if pixel is uv-mapped on a canvas face.
		*/
		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int i, sample;
				int index = xx+w*yy;
				PaintSurfacePoint *cPoint = (&surface->point[index]);

				short isInside = 0;	/* if point is inside a uv face */

				float d1[2], d2[2], d3[2], point[5][2];
				float dot00,dot01,dot02,dot11,dot12, invDenom, u,v;

				/*
				*	Init per pixel settings
				*/
				cPoint->color[0] = 0.0f;
				cPoint->color[1] = 0.0f;
				cPoint->color[2] = 0.0f;
				cPoint->alpha = 0.0f;
				cPoint->depth = 0.0f;

				cPoint->wetness = 0.0f;
				cPoint->e_alpha = 0.0f;
				cPoint->e_color[0] = 0.0f;
				cPoint->e_color[1] = 0.0f;
				cPoint->e_color[2] = 0.0f;
				cPoint->state = 0;

				cPoint->index = -1;
				cPoint->neighbour_pixel = -1;

				/* Actual pixel center, used when collision is found	*/
				point[0][0] = ((float)xx + 0.5f) / w;
				point[0][1] = ((float)yy + 0.5f) / h;

				/*
				* A pixel middle sample isn't enough to find very narrow polygons
				* So using 4 samples of each corner too
				*/
				point[1][0] = ((float)xx) / w;
				point[1][1] = ((float)yy) / h;

				point[2][0] = ((float)xx+1) / w;
				point[2][1] = ((float)yy) / h;

				point[3][0] = ((float)xx) / w;
				point[3][1] = ((float)yy+1) / h;

				point[4][0] = ((float)xx+1) / w;
				point[4][1] = ((float)yy+1) / h;


				/* Loop through pixel samples, starting from middle point	*/
				for (sample=0; sample<5; sample++) {
						
						/* Loop through every face in the mesh	*/
						for (i=0; i<numOfFaces; i++) {

							/* Check uv bb	*/
							if (faceBB[i].min[0] > (point[sample][0])) continue;
							if (faceBB[i].min[1] > (point[sample][1])) continue;
							if (faceBB[i].max[0] < (point[sample][0])) continue;
							if (faceBB[i].max[1] < (point[sample][1])) continue;

							/*
							*	Calculate point inside a triangle check
							*	for uv0,1,2
							*/
							VECSUB2D(d1,  tface[i].uv[2], tface[i].uv[0]);	// uv2 - uv0
							VECSUB2D(d2,  tface[i].uv[1], tface[i].uv[0]);	// uv1 - uv0
							VECSUB2D(d3,  point[sample], tface[i].uv[0]);	// point - uv0


							dot00 = d1[0]*d1[0] + d1[1]*d1[1];
							dot01 = d1[0]*d2[0] + d1[1]*d2[1];
							dot02 = d1[0]*d3[0] + d1[1]*d3[1];
							dot11 = d2[0]*d2[0] + d2[1]*d2[1];
							dot12 = d2[0]*d3[0] + d2[1]*d3[1];

							invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
							u = (dot11 * dot02 - dot01 * dot12) * invDenom;
							v = (dot00 * dot12 - dot01 * dot02) * invDenom;

							if ((u > 0) && (v > 0) && (u + v < 1)) {isInside=1;} /* is inside a triangle */

							/*
							*	If collision wasn't found but the face is a quad
							*	do another check for the second half
							*/
							if ((!isInside) && mface[i].v4)
							{

								/* change d2 to test the other half	*/
								VECSUB2D(d2,  tface[i].uv[3], tface[i].uv[0]);	// uv3 - uv0

								/* test again	*/
								dot00 = d1[0]*d1[0] + d1[1]*d1[1];
								dot01 = d1[0]*d2[0] + d1[1]*d2[1];
								dot02 = d1[0]*d3[0] + d1[1]*d3[1];
								dot11 = d2[0]*d2[0] + d2[1]*d2[1];
								dot12 = d2[0]*d3[0] + d2[1]*d3[1];

								invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
								u = (dot11 * dot02 - dot01 * dot12) * invDenom;
								v = (dot00 * dot12 - dot01 * dot02) * invDenom;

								if ((u > 0) && (v > 0) && (u + v < 1)) {isInside=2;} /* is inside the second half of the quad */

							}

							/*
							*	If point was inside the face
							*/
							if (isInside != 0) {

								float uv1co[2], uv2co[2], uv3co[2], uv[2];
								int j;

								/* Get triagnle uvs	*/
								if (isInside==1) {
									VECCOPY2D(uv1co, tface[i].uv[0]);
									VECCOPY2D(uv2co, tface[i].uv[1]);
									VECCOPY2D(uv3co, tface[i].uv[2]);
								}
								else {
									VECCOPY2D(uv1co, tface[i].uv[0]);
									VECCOPY2D(uv2co, tface[i].uv[2]);
									VECCOPY2D(uv3co, tface[i].uv[3]);
								}

								/* Add b-weights per anti-aliasing sample	*/
								for (j=0; j<surface->pixelSamples; j++) {

									uv[0] = point[0][0] + jitter5sample[j*2] / w;
									uv[1] = point[0][1] + jitter5sample[j*2+1] / h;

									barycentric_weights_v2(uv1co, uv2co, uv3co, uv, cPoint->barycentricWeights[j].v);
								}

								/* Set surface point face values	*/
								cPoint->index = i;							/* face index */
								cPoint->quad = (isInside == 2) ? 1 : 0;		/* quad or tri part*/

								/* save vertex indexes	*/
								cPoint->v1 = (isInside == 2) ? mface[i].v1 : mface[i].v1;
								cPoint->v2 = (isInside == 2) ? mface[i].v3 : mface[i].v2;
								cPoint->v3 = (isInside == 2) ? mface[i].v4 : mface[i].v3;
								
								sample = 5;	/* make sure we exit sample loop as well */
								break;
							}	// end isInside
						} // end face loop
				}	 // end sample
			}	// end of yy loop
		}	// end of xx loop



		/*
		*	Now loop through every pixel that was left without index
		*	and find if they have neighbouring pixels that have an index.
		*	If so use that polygon as pixel surface.
		*	(To avoid seams on uv island edges.)
		*/
		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int index = xx+w*yy;
				PaintSurfacePoint *cPoint = (&surface->point[index]);

				/* If point isnt't on canvas mesh	*/
				if (cPoint->index == -1) {
					int u_min, u_max, v_min, v_max;
					int u,v, ind;
					float point[2];

					/* get loop area	*/
					u_min = (xx > 0) ? -1 : 0;
					u_max = (xx < (w-1)) ? 1 : 0;
					v_min = (yy > 0) ? -1 : 0;
					v_max = (yy < (h-1)) ? 1 : 0;

					point[0] = ((float)xx + 0.5f) / w;
					point[1] = ((float)yy + 0.5f) / h;

					/* search through defined area for neighbour	*/
					for (u=u_min; u<=u_max; u++)
						for (v=v_min; v<=v_max; v++) {

							/* if not this pixel itself	*/
							if (u!=0 || v!=0) {
								ind = (xx+u)+w*(yy+v);

								/* if neighbour has index	*/
								if (surface->point[ind].index != -1) {

									float uv1co[2], uv2co[2], uv3co[2], uv[2];
									int i = surface->point[ind].index, j;

									/*
									*	Now calculate pixel data for this pixel as it was on polygon surface
									*/
									if (!surface->point[ind].quad) {
										VECCOPY2D(uv1co, tface[i].uv[0]);
										VECCOPY2D(uv2co, tface[i].uv[1]);
										VECCOPY2D(uv3co, tface[i].uv[2]);
									}
									else {
										VECCOPY2D(uv1co, tface[i].uv[0]);
										VECCOPY2D(uv2co, tface[i].uv[2]);
										VECCOPY2D(uv3co, tface[i].uv[3]);
									}

									/* Add b-weights per anti-aliasing sample	*/
									for (j=0; j<surface->pixelSamples; j++) {

										uv[0] = point[0] + jitter5sample[j*2] / w;
										uv[1] = point[1] + jitter5sample[j*2+1] / h;
										barycentric_weights_v2(uv1co, uv2co, uv3co, uv, cPoint->barycentricWeights[j].v);
									}

									/* Set values	*/
									cPoint->neighbour_pixel = ind;				// face index
									cPoint->quad = surface->point[ind].quad;		// quad or tri

									/* save vertex indexes	*/
									cPoint->v1 = (cPoint->quad) ? mface[i].v1 : mface[i].v1;
									cPoint->v2 = (cPoint->quad) ? mface[i].v3 : mface[i].v2;
									cPoint->v3 = (cPoint->quad) ? mface[i].v4 : mface[i].v3;

									u = u_max + 1;	/* make sure we exit outer loop as well */
									break;
								}

						} // end itself check
					} // end uv loop
				}	// end if has index
			}
		} // end pixel loop

		/*
		*	When base loop is over convert found neighbour indexes to real ones
		*	Also count the final number of active surface points
		*/
		surface->active_points = 0;

		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int index = xx+w*yy;
				PaintSurfacePoint *cPoint = (&surface->point[index]);

				if (cPoint->index == -1 && cPoint->neighbour_pixel != -1) cPoint->index = surface->point[cPoint->neighbour_pixel].index;
				if (cPoint->index != -1) surface->active_points++;
			}
		} // end pixel loop

#if 0
		/*
		*	-----------------------------------------------------------------
		*	For debug, output pixel statuses to the color map
		*	-----------------------------------------------------------------
		*/
		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int index = xx+w*yy;
				PaintSurfacePoint *cPoint = (&surface->point[index]);
				cPoint->alpha=1.0f;

				/* Every pixel that is assigned as "edge pixel" gets blue color	*/
				if (cPoint->neighbour_pixel != -1) cPoint->color[2] = 1.0f;
				/* and every pixel that finally got an polygon gets red color	*/
				if (cPoint->index != -1) cPoint->color[0] = 1.0f;
				/* green color shows pixel face index hash	*/
				if (cPoint->index != -1) cPoint->color[1] = (float)(cPoint->index % 255)/256.0f;
			}
		} // end pixel loop

#endif

		/*
		*	If any effect enabled, create surface effect / wet layer
		*	neighbour lists. Processes possibly moving data.
		*/
		if (canvas->effect) {

			#pragma omp parallel for schedule(static)
			for (yy = 0; yy < h; yy++)
			{
				int xx;
				for (xx = 0; xx < w; xx++)
				{
					int i;
					PaintSurfacePoint *cPoint = (&surface->point[xx+w*yy]);

					/* If current point exists find all it's neighbouring pixels	*/
					if (cPoint->index != -1)
					for (i=0; i<8; i++) {

						/* Try to find a neighbouring pixel in defined direction
						*  If not found, -1 is returned */
						cPoint->neighbours[i] = dynamicPaint_findNeighbourPixel(canvas, xx, yy, i);
					}
				}
			} // end pixel loop
		} // effect

		MEM_freeN(faceBB);

		return 1;
}


/*
*	Free canvas surface
*/
static void dynamicPaint_cleanCanvasSurface(DynamicPaintCanvasSettings *canvas)
{

	int w,h,k;
	
	if (!canvas) return;
	if (!canvas->surface) return;

	w = canvas->surface->w;
	h = canvas->surface->h;

	#pragma omp parallel for schedule(static,1)
	for (k = 0; k < w*h; k++)
	{
		free(canvas->surface->point[k].barycentricWeights);
	}

	if (canvas->surface->point) MEM_freeN(canvas->surface->point);
	MEM_freeN(canvas->surface);
}

/*  A modified callback to bvh tree raycast. The tree must bust have been built using bvhtree_from_mesh_faces.
*   userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
*  
*	To optimize paint detection speed this doesn't calculate hit coordinates or normal.
*	If ray hit the second half of a quad, no[0] is set to 1.0f.
*/
static void mesh_faces_spherecast_dp(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh*) userdata;
	MVert *vert	= data->vert;
	MFace *face = data->face + index;
	short quad = 0;

	float *t0, *t1, *t2, *t3;
	t0 = vert[ face->v1 ].co;
	t1 = vert[ face->v2 ].co;
	t2 = vert[ face->v3 ].co;
	t3 = face->v4 ? vert[ face->v4].co : NULL;

	do
	{	
		float dist;
		dist = ray_tri_intersection(ray, hit->dist, t0, t1, t2);

		if(dist >= 0 && dist < hit->dist)
		{
			hit->index = index;
			hit->dist = dist;
			//VECADDFAC(hit->co, ray->origin, ray->direction, dist);
			//normal_tri_v3( hit->no,t0, t1, t2);
			hit->no[0] = (quad) ? 1.0f : 0.0f;
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;
		quad = 1;

	} while(t2);
}

/* A modified callback to bvh tree nearest point. The tree must bust have been built using bvhtree_from_mesh_faces.
*  userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
*  
*	To optimize paint detection speed this doesn't calculate hit normal.
*	If ray hit the second half of a quad, no[0] is set to 1.0f, else 0.0f
*/
static void mesh_faces_nearest_point_dp(void *userdata, int index, const float *co, BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh*) userdata;
	MVert *vert	= data->vert;
	MFace *face = data->face + index;
	short quad = 0;

	float *t0, *t1, *t2, *t3;
	t0 = vert[ face->v1 ].co;
	t1 = vert[ face->v2 ].co;
	t2 = vert[ face->v3 ].co;
	t3 = face->v4 ? vert[ face->v4].co : NULL;

	do
	{	
		float nearest_tmp[3], dist;
		int vertex, edge;
		
		dist = nearest_point_in_tri_surface(t0, t1, t2, co, &vertex, &edge, nearest_tmp);
		if(dist < nearest->dist)
		{
			nearest->index = index;
			nearest->dist = dist;
			VECCOPY(nearest->co, nearest_tmp);
			//normal_tri_v3( nearest->no,t0, t1, t2);
			nearest->no[0] = (quad) ? 1.0f : 0.0f;
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;
		quad = 1;

	} while(t2);
}


/*
*	Calculate inverse matrices for material related objects
*	in case texture is mapped to an object.
*	(obj->imat isn't auto-updated)
*/
static void DynamicPaint_UpdateMaterial(Material *mat)
{
	MTex *mtex = NULL;
	Tex *tex = NULL;
	int tex_nr;

	if (mat == NULL) return;

	/*
	*	Loop through every material texture and check
	*	if they are mapped by other object
	*/
	
	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		/* separate tex switching */
		if(mat->septex & (1<<tex_nr)) continue;
	
		if(mat->mtex[tex_nr]) {
			mtex= mat->mtex[tex_nr];
			tex= mtex->tex;

			if(tex==0) continue;
			
			/* which coords */
			if(mtex->texco==TEXCO_OBJECT) { 
				Object *ob= mtex->object;
				if(ob) {						
					invert_m4_m4(ob->imat, ob->obmat);
				}
			}

		}
	}	// end texture loop

}


static void DynamicPaint_InitMaterialObjects(Object *paintOb, Material *ui_mat)
{

	/*
	*	Calculate inverse transformation matrix
	*	for this object
	*/
	invert_m4_m4(paintOb->imat, paintOb->obmat);

	/* Now process every material linked to this Paint object,
	*  and check if any material uses object mapping. */
	if ((ui_mat == NULL) && paintOb->totcol) {
		/* If using materials that are linked to mesh,
		*  check every material linked to this object*/
		int i;

		for (i=0; i<paintOb->totcol; i++) {
				if (paintOb->matbits[i]) DynamicPaint_UpdateMaterial(paintOb->mat[i]);
			}
	}
	else {
		DynamicPaint_UpdateMaterial(ui_mat);
	}

}
	


/* a modified part of shadeinput.c -> shade_input_set_uv() / shade_input_set_shade_texco() */
static void textured_face_generate_uv(float *uv, float *normal, float *hit, float *v1, float *v2, float *v3)
{

	float detsh, t00, t10, t01, t11, xn, yn, zn;
	int axis1, axis2;

	/* find most stable axis to project */
	xn= fabs(normal[0]);
	yn= fabs(normal[1]);
	zn= fabs(normal[2]);

	if(zn>=xn && zn>=yn) { axis1= 0; axis2= 1; }
	else if(yn>=xn && yn>=zn) { axis1= 0; axis2= 2; }
	else { axis1= 1; axis2= 2; }

	/* compute u,v and derivatives */
	t00= v3[axis1]-v1[axis1]; t01= v3[axis2]-v1[axis2];
	t10= v3[axis1]-v2[axis1]; t11= v3[axis2]-v2[axis2];

	detsh= 1.0f/(t00*t11-t10*t01);
	t00*= detsh; t01*=detsh; 
	t10*=detsh; t11*=detsh;

	uv[0] = (hit[axis1]-v3[axis1])*t11-(hit[axis2]-v3[axis2])*t10;
	uv[1] = (hit[axis2]-v3[axis2])*t00-(hit[axis1]-v3[axis1])*t01;

	/* u and v are in range -1 to 0, we allow a little bit extra but not too much, screws up speedvectors */
	CLAMP(uv[0], -2.0f, 1.0f);
	CLAMP(uv[1], -2.0f, 1.0f);
}

/* a modified part of shadeinput.c -> shade_input_set_uv() / shade_input_set_shade_texco() */
static void textured_face_get_uv(float *uv_co, float *normal, float *uv, int faceIndex, short quad, MTFace *tface)
{
	float *uv1, *uv2, *uv3;
	float l;

	l= 1.0f+uv[0]+uv[1];
		
	uv1= tface[faceIndex].uv[0];
	uv2= (quad) ? tface[faceIndex].uv[2] : tface[faceIndex].uv[1];
	uv3= (quad) ? tface[faceIndex].uv[3] : tface[faceIndex].uv[2];
				
	uv_co[0]= -1.0f + 2.0f*(l*uv3[0]-uv[0]*uv1[0]-uv[1]*uv2[0]);
	uv_co[1]= -1.0f + 2.0f*(l*uv3[1]-uv[0]*uv1[1]-uv[1]*uv2[1]);
	uv_co[2]= 0.0f;	/* texture.c assumes there are 3 coords */
}

/*
*	Edited version of do_material_tex()
*
*	Samples color and alpha from a "Surface" type material
*	on a given point, without need for ShadeInput.
*
*	Keep up-to-date with new mapping settings
*
*	also see shade_input_set_shade_texco() for ORCO settings
*	and shade_input_set_uv() for face uv calculation
*/
void DynamicPaint_SampleSolidMaterial(float color[3], float *alpha, Material *mat, Object *paintOb, float xyz[3], int faceIndex, short isQuad, DerivedMesh *orcoDm)
{
	MTex *mtex = NULL;
	Tex *tex = NULL;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float co[3], xyz_local[3];
	float fact, stencilTin=1.0;
	float texvec[3];
	int tex_nr, rgbnor= 0;
	float uv[3], normal[3];
	MFace *mface;
	int v1, v2, v3;
	MVert *mvert;
	
	/* Get face data	*/
	mvert = orcoDm->getVertArray(orcoDm);
	mface = orcoDm->getFaceArray(orcoDm);
	v1=mface[faceIndex].v1, v2=mface[faceIndex].v2, v3=mface[faceIndex].v3;
	if (isQuad) {v2=mface[faceIndex].v3; v3=mface[faceIndex].v4;}

	normal_tri_v3( normal, mvert[v1].co, mvert[v2].co, mvert[v3].co);

	/* Assign material base values	*/
	color[0] = mat->r;
	color[1] = mat->g;
	color[2] = mat->b;
	*alpha = mat->alpha;

	VECCOPY(xyz_local, xyz);
	mul_m4_v3(paintOb->imat, xyz_local);

	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		
		/* separate tex switching */
		if(mat->septex & (1<<tex_nr)) continue;
		
		if(mat->mtex[tex_nr]) {
			mtex= mat->mtex[tex_nr];
			tex= mtex->tex;
			
			tex= mtex->tex;
			if(tex==0) continue;

			/* which coords */
			if(mtex->texco==TEXCO_ORCO) {
				float l;

				/*
				*	Get generated UV
				*/
				textured_face_generate_uv(uv, normal, xyz_local, mvert[v1].co, mvert[v2].co, mvert[v3].co);

				l= 1.0f+uv[0]+uv[1];

				/* calculate generated coordinate
				*  ** Keep up-to-date with shadeinput.c -> shade_input_set_shade_texco() **/
				co[0]= l*mvert[v3].co[0]-uv[0]*mvert[v1].co[0]-uv[1]*mvert[v2].co[0];
				co[1]= l*mvert[v3].co[1]-uv[0]*mvert[v1].co[1]-uv[1]*mvert[v2].co[1];
				co[2]= l*mvert[v3].co[2]-uv[0]*mvert[v1].co[2]-uv[1]*mvert[v2].co[2];
			}
			else if(mtex->texco==TEXCO_OBJECT) {
				Object *ob= mtex->object;

				VECCOPY(co, xyz);
				/* convert from world space to paint space */
				mul_m4_v3(paintOb->imat, co);
				if(ob) {
					mul_m4_v3(ob->imat, co);
				}
			}
			else if(mtex->texco==TEXCO_GLOB) {
				VECCOPY(co, xyz);
			}
			else if(mtex->texco==TEXCO_UV) {
				MTFace *tface;

				/* Get UV layer */
				if(mtex->uvname[0] != 0) {
					tface = CustomData_get_layer_named(&orcoDm->faceData, CD_MTFACE, mtex->uvname);
				}
				else tface = DM_get_face_data_layer(orcoDm, CD_MTFACE);

				/* Get generated coordinates to calculate UV from */
				textured_face_generate_uv(uv, normal, xyz_local, mvert[v1].co, mvert[v2].co, mvert[v3].co);

				/* Get UV mapping coordinate */
				textured_face_get_uv(co, normal, uv, faceIndex, isQuad, tface);
			}
			else continue;	/* non-supported types get just skipped:
							TEXCO_REFL, TEXCO_NORM, TEXCO_TANGENT
							TEXCO_WINDOW, TEXCO_STRAND, TEXCO_STRESS etc.
							*/

			/* get texture mapping */
			texco_mapping_ext(normal, tex, mtex, co, 0, 0, texvec);

			if(tex->use_nodes && tex->nodetree) {
				/* No support for nodes (yet). */
				continue;
			}
			else {
				rgbnor = multitex_ext(mtex->tex, co, 0, 0, 0, &texres);
			}

			/* texture output */
			if( (rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				rgbnor-= TEX_RGB;
			}

			/* Negate and stencil masks */
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgbnor & TEX_RGB) {
					texres.tr= 1.0-texres.tr;
					texres.tg= 1.0-texres.tg;
					texres.tb= 1.0-texres.tb;
				}
				texres.tin= 1.0-texres.tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgbnor & TEX_RGB) {
					fact= texres.ta;
					texres.ta*= stencilTin;
					stencilTin*= fact;
				}
				else {
					fact= texres.tin;
					texres.tin*= stencilTin;
					stencilTin*= fact;
				}
			}

			/* mapping */
			if(mtex->mapto & (MAP_COL)) {
				float tcol[3];
				
				/* stencil maps on the texture control slider, not texture intensity value */
				tcol[0]=texres.tr; tcol[1]=texres.tg; tcol[2]=texres.tb;
				
				if((rgbnor & TEX_RGB)==0) {
					tcol[0]= mtex->r;
					tcol[1]= mtex->g;
					tcol[2]= mtex->b;
				}
				else if(mtex->mapto & MAP_ALPHA) {
					texres.tin= stencilTin;
				}
				else texres.tin= texres.ta;
				
				if(mtex->mapto & MAP_COL) {
					float colfac= mtex->colfac*stencilTin;
					texture_rgb_blend(color, tcol, color, texres.tin, colfac, mtex->blendtype);
				}
			}

			if(mtex->mapto & MAP_VARS) {
				/* stencil maps on the texture control slider, not texture intensity value */
				
				if(rgbnor & TEX_RGB) {
					if(texres.talpha) texres.tin= texres.ta;
					else texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				}

				if(mtex->mapto & MAP_ALPHA) {
					float alphafac= mtex->alphafac*stencilTin;

					*alpha= texture_value_blend(mtex->def_var, *alpha, texres.tin, alphafac, mtex->blendtype);
					if(*alpha<0.0) *alpha= 0.0;
					else if(*alpha>1.0) *alpha= 1.0;
				}
			}
		}
	}
}


/*
*	Edited version of texture.c -> do_volume_tex()
*
*	Samples color and density from a volume type texture
*	without need for ShadeInput.
*
*	Keep up-to-date with new mapping settings
*/
void DynamicPaint_SampleVolumeMaterial(float color[3], float *alpha, Material *mat, Object *paintOb, float xyz[3])
{

	int mapto_flag  = MAP_DENSITY | MAP_REFLECTION_COL | MAP_TRANSMISSION_COL;
	float *col = color;

	MTex *mtex = NULL;
	Tex *tex = NULL;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	int tex_nr, rgbnor= 0;
	float co[3], texvec[3];
	float fact, stencilTin=1.0;

	/* set base color */
	color[0] = mat->vol.reflection_col[0];
	color[1] = mat->vol.reflection_col[1];
	color[2] = mat->vol.reflection_col[2];
	*alpha = mat->vol.density;
	
	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {

		/* separate tex switching */
		if(mat->septex & (1<<tex_nr)) continue;
		
		if(mat->mtex[tex_nr]) {
			mtex= mat->mtex[tex_nr];
			tex= mtex->tex;

			if(tex==0) continue;

			/* only process if this texture is mapped 
				* to one that we're interested in */
			if (!(mtex->mapto & mapto_flag)) continue;
			texres.nor= NULL;
			
			/* which coords */
			if(mtex->texco==TEXCO_OBJECT) { 
				Object *ob= mtex->object;
				ob= mtex->object;
				if(ob) {						
					VECCOPY(co, xyz);
					mul_m4_v3(ob->imat, co);
				}
			}
			else if(mtex->texco==TEXCO_ORCO) {
				
				{
					Object *ob= paintOb;
					VECCOPY(co, xyz);
					mul_m4_v3(ob->imat, co);
				}
			}
			else if(mtex->texco==TEXCO_GLOB) {							
				VECCOPY(co, xyz);
			}
			else continue;	/* Skip unsupported types */
			

			if(tex->type==TEX_IMAGE) {
				continue;	/* not supported yet */				
			}
			else {
				/* placement */
				if(mtex->projx) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
				else texvec[0]= mtex->size[0]*(mtex->ofs[0]);

				if(mtex->projy) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
				else texvec[1]= mtex->size[1]*(mtex->ofs[1]);

				if(mtex->projz) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
				else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
			}
			rgbnor= multitex_ext(tex, texvec, NULL, NULL, 0, &texres);
			
			/* texture output */
			if( (rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				rgbnor-= TEX_RGB;
			}

			/* Negate and stencil */
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgbnor & TEX_RGB) {
					texres.tr= 1.0-texres.tr;
					texres.tg= 1.0-texres.tg;
					texres.tb= 1.0-texres.tb;
				}
				texres.tin= 1.0-texres.tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgbnor & TEX_RGB) {
					fact= texres.ta;
					texres.ta*= stencilTin;
					stencilTin*= fact;
				}
				else {
					fact= texres.tin;
					texres.tin*= stencilTin;
					stencilTin*= fact;
				}
			}
			
			/* Map values */
			if((mapto_flag & (MAP_EMISSION_COL+MAP_TRANSMISSION_COL+MAP_REFLECTION_COL)) && (mtex->mapto & (MAP_EMISSION_COL+MAP_TRANSMISSION_COL+MAP_REFLECTION_COL))) {
				float tcol[3];
				
				/* stencil maps on the texture control slider, not texture intensity value */
		
				if((rgbnor & TEX_RGB)==0) {
					tcol[0]= mtex->r;
					tcol[1]= mtex->g;
					tcol[2]= mtex->b;
				} else {
					tcol[0]=texres.tr;
					tcol[1]=texres.tg;
					tcol[2]=texres.tb;
					if(texres.talpha)
						texres.tin= texres.ta;
				}
				
				/* used for emit */
				if((mapto_flag & MAP_EMISSION_COL) && (mtex->mapto & MAP_EMISSION_COL)) {
					float colemitfac= mtex->colemitfac*stencilTin;
					texture_rgb_blend(col, tcol, col, texres.tin, colemitfac, mtex->blendtype);
				}
				
				if((mapto_flag & MAP_REFLECTION_COL) && (mtex->mapto & MAP_REFLECTION_COL)) {
					float colreflfac= mtex->colreflfac*stencilTin;
					texture_rgb_blend(col, tcol, col, texres.tin, colreflfac, mtex->blendtype);
				}
				
				if((mapto_flag & MAP_TRANSMISSION_COL) && (mtex->mapto & MAP_TRANSMISSION_COL)) {
					float coltransfac= mtex->coltransfac*stencilTin;
					texture_rgb_blend(col, tcol, col, texres.tin, coltransfac, mtex->blendtype);
				}
			}
			
			if((mapto_flag & MAP_VARS) && (mtex->mapto & MAP_VARS)) {
				/* stencil maps on the texture control slider, not texture intensity value */
				
				/* convert RGB to intensity if intensity info isn't provided */
				if (!(rgbnor & TEX_INT)) {
					if (rgbnor & TEX_RGB) {
						if(texres.talpha) texres.tin= texres.ta;
						else texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
					}
				}
				if((mapto_flag & MAP_DENSITY) && (mtex->mapto & MAP_DENSITY)) {
					float densfac= mtex->densfac*stencilTin;

					*alpha = texture_value_blend(mtex->def_var, *alpha, texres.tin, densfac, mtex->blendtype);
					CLAMP(*alpha, 0.0, 1.0);
				}
			}
		}
	}
}

/*
*	Get material (including linked textures) diffuse color and alpha in given coordinates
*	
*	color,paint : input/output color values
*	pixelCoord : canvas pixel coordinates in global space. used if material is volumetric
*
*	paintHit : point on paint object surface in global space. used by "surface" type materials
*	faceIndex : paintHit face index
*	orcoDm : orco state derived mesh of paint object
*	ui_mat : force material. if NULL, material linked to paintOb mesh is used.
*
*	*"paint object" = object to sample material color from
*/
void DynamicPaint_GetMaterialColor(float *color, float *alpha, Object *paintOb, float pixelCoord[3], float paintHit[3], int faceIndex, short isQuad, DerivedMesh *orcoDm, Material *ui_mat)
{

	Material *material = ui_mat;

	/*
	*	Get face material
	*/
	if (material == NULL) {
		MFace *mface = NULL;
		mface = orcoDm->getFaceArray(orcoDm);
		material = give_current_material(paintOb, mface[faceIndex].mat_nr+1);

		if (material == NULL) return;	/* No material assigned */
	}

	/*
	*	Sample textured material color in given position depending on material type
	*/
	if (material->material_type == MA_TYPE_SURFACE) {
		/* Solid material */
		DynamicPaint_SampleSolidMaterial(color, alpha, material, paintOb, paintHit, faceIndex, isQuad, orcoDm);
	}
	else if (material->material_type == MA_TYPE_VOLUME) {
		/* Volumetric material */
		DynamicPaint_SampleVolumeMaterial(color, alpha, material, paintOb, pixelCoord);
	}
	else if (material->material_type == MA_TYPE_HALO) {
		/* Halo type not supported */
	}
}


/*
*	Mix color values to canvas point.
*
*	cPoint : canvas surface point to do the changes
*	paintFlags : paint object flags
*   paintColor,Alpha,Wetness : to be mixed paint values
*
*	timescale : value used to adjust time dependand
*			    operations when using substeps
*/
void DynamicPaint_MixPaintColors(PaintSurfacePoint *cPoint, int paintFlags, float *paintColor, float *paintAlpha, float *paintWetness, float *timescale)
{

	/* Add paint	*/
	if (!(paintFlags & MOD_DPAINT_ERASE)) {
		float wetness;

		/* If point has previous paint	*/
		if (cPoint->e_alpha > 0)
		{
			/*
			*	Mix colors by the factor, use timescale
			*/
			float factor = (*paintAlpha) * (*timescale);
			float invFact = 1.0f - factor;
			cPoint->e_color[0] = cPoint->e_color[0]*invFact + paintColor[0]*factor;
			cPoint->e_color[1] = cPoint->e_color[1]*invFact + paintColor[1]*factor;
			cPoint->e_color[2] = cPoint->e_color[2]*invFact + paintColor[2]*factor;
		}
		else
		{
			/* else set first color value straight to paint color	*/
			cPoint->e_color[0] = paintColor[0];
			cPoint->e_color[1] = paintColor[1];
			cPoint->e_color[2] = paintColor[2];
		}

		/* alpha */
		if (paintFlags & MOD_DPAINT_ABS_ALPHA) {
			if (cPoint->e_alpha < (*paintAlpha)) cPoint->e_alpha = (*paintAlpha);
		}
		else {
			cPoint->e_alpha += (*paintAlpha) * (*timescale);
			if (cPoint->e_alpha > 1.0f) cPoint->e_alpha = 1.0f;
		}

		/* only increase wetness if it's below paint level	*/
		wetness = (*paintWetness) * cPoint->e_alpha;
		if (cPoint->wetness < wetness) cPoint->wetness = wetness;
	}
	/* Erase paint	*/
	else {
		float a_ratio, a_highest;
		float wetness;
		float invFact = 1.0f - (*paintAlpha);

		/*
		*	Make highest alpha to match erased value
		*	but maintain alpha ratio
		*/
		if (paintFlags & MOD_DPAINT_ABS_ALPHA) {
			a_highest = (cPoint->e_alpha > cPoint->alpha) ? cPoint->e_alpha : cPoint->alpha;
			if (a_highest > invFact) {
				a_ratio = invFact / a_highest;

				cPoint->e_alpha *= a_ratio;
				cPoint->alpha *= a_ratio;
			}
		}
		else {
			cPoint->e_alpha -= (*paintAlpha) * (*timescale);
			if (cPoint->e_alpha < 0.0f) cPoint->e_alpha = 0.0f;
			cPoint->alpha -= (*paintAlpha) * (*timescale);
			if (cPoint->alpha < 0.0f) cPoint->alpha = 0.0f;
		}

		wetness = (1.0f - (*paintWetness)) * cPoint->e_alpha;
		if (cPoint->wetness > wetness) cPoint->wetness = wetness;
	}
}


/*
*	Paint an object mesh to canvas
*/
static int DynamicPaint_PaintMesh(DynamicPaintCanvasSettings *canvas, Vec3f *canvasVerts, DynamicPaintPainterSettings *paint, Object *canvasOb, Object *paintOb, float timescale)
{
	DerivedMesh *dm = NULL;
	MVert *mvert = NULL;
	MFace *mface = NULL;
	PaintSurface *surface = canvas->surface;

	if (!paint->dm) {
		printf("DynamicPaint: Invalid paint dm.\n");
		return 0;
	}

	/* If using material color, we prepare required stuff on texture related objects first	*/
	if (paint->flags & MOD_DPAINT_USE_MATERIAL) DynamicPaint_InitMaterialObjects(paintOb, paint->mat);

	{
		BVHTreeFromMesh treeData = {0};
		int yy;
		int tWidth = surface->w;
		int tHeight = surface->h;

		int numOfVerts;
		int ii;

		/*
		*	Transform collider vertices to world space
		*	(Faster than transforming per pixel
		*   coordinates and normals to object space)
		*/
		dm = CDDM_copy(paint->dm);
		mvert = dm->getVertArray(dm);
		mface = dm->getFaceArray(dm);
		numOfVerts = dm->getNumVerts(dm);

		for (ii=0; ii<numOfVerts; ii++) {
			mul_m4_v3(paintOb->obmat, mvert[ii].co);
		}

		/* Build a bvh tree from transformed vertices	*/
		bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 6);

		if(treeData.tree) {
			#pragma omp parallel for schedule(static)
			for (yy = 0; yy < tHeight; yy++)
			{
				int xx,i;
				for (xx = 0; xx < tWidth; xx++)
				{
					PaintSurfacePoint *cPoint = (&surface->point[xx+tWidth*yy]);
					i = cPoint->index;

					if (i >= 0)
					{
						int ss;
						float ssFactor = 0.0f;	/* super-sampling factor */
						float depth = 0.0f;		/* displace depth */

						float paintColor[3] = {0.0f, 0.0f, 0.0f};
						int numOfHits = 0;
						float paintAlpha = 0.0f;

						/* Supersampling	*/
						for (ss=0; ss<surface->pixelSamples; ss++) {

							float ray_start[3], ray_dir[3];
							float gaus_factor;
							BVHTreeRayHit hit;
							BVHTreeNearest nearest;
							short hit_found = 0;
							float realPos[3];

							/* If it's a proximity hit, store distance rate */
							float distRate = -1.0f;

							/* hit data	*/
							float hitCoord[3];		/* mid-sample hit coordinate */
							int hitFace = -1;		/* mid-sample hit face */
							short hitQuad;			/* mid-sample hit quad status */

							/* Supersampling factor	*/
							if (surface->pixelSamples > 1) {
								gaus_factor = gaussianFactors[ss];
							}
							else {
								gaus_factor = 1.0f;
							}

							/* Get current sample position in world coordinates	*/
							interp_v3_v3v3v3(realPos,
											canvasVerts[cPoint->v1].v,
											canvasVerts[cPoint->v2].v,
											canvasVerts[cPoint->v3].v, cPoint->barycentricWeights[ss].v);
							VECCOPY(ray_start, realPos);
							VECCOPY(ray_dir, cPoint->invNorm);

							hit.index = -1;
							hit.dist = 9999;
							nearest.index = -1;
							nearest.dist = paint->paint_distance * paint->paint_distance; /* find_nearest search uses squared distance */

							/* Check volume collision	*/
							if (paint->collision == MOD_DPAINT_COL_VOLUME || paint->collision == MOD_DPAINT_COL_VOLDIST)
							if(BLI_bvhtree_ray_cast(treeData.tree, ray_start, ray_dir, 0.0f, &hit, mesh_faces_spherecast_dp, &treeData) != -1)
							{
								/* We hit a triangle, now check if collision point normal is facing the point	*/


								/*	For optimization sake, hit point normal isn't calculated in ray cast loop	*/
								int v1=mface[hit.index].v1, v2=mface[hit.index].v2, v3=mface[hit.index].v3, quad=(hit.no[0] == 1.0f);
								float dot;

								if (quad) {v2=mface[hit.index].v3; v3=mface[hit.index].v4;}
							
								/* Get hit normal	*/
								normal_tri_v3( hit.no, mvert[v1].co, mvert[v2].co, mvert[v3].co);
								dot = ray_dir[0]*hit.no[0] + ray_dir[1]*hit.no[1] + ray_dir[2]*hit.no[2];

								/*
								*	If ray and hit normal are facing same direction
								*	hit point is inside a closed mesh.
								*/
								if (dot>=0)
								{
									/* Add factor on supersample filter	*/
									ssFactor += gaus_factor;
									depth += hit.dist;
									hit_found = 1;

									/*
									*	Mark hit info
									*/
									if (hitFace == -1) {
										VECADDFAC(hitCoord, ray_start, ray_dir, hit.dist);	/* Calculate final hit coordinates */
										hitQuad = quad;
										hitFace = hit.index;
									}
								}
							}	// end of raycast
						
							/* Check proximity collision	*/
							if ((paint->collision == MOD_DPAINT_COL_DIST || paint->collision == MOD_DPAINT_COL_VOLDIST) && (!hit_found))
							{
								float proxDist = -1.0f;
								float hitCo[3];
								short hQuad;
								int face;

								/*
								*	If pure distance proximity, find the nearest point on the mesh
								*/
								if (!(paint->flags & MOD_DPAINT_PROX_FACEALIGNED)) {
									if (BLI_bvhtree_find_nearest(treeData.tree, ray_start, &nearest, mesh_faces_nearest_point_dp, &treeData) != -1) {
										proxDist = sqrt(nearest.dist);	/* find_nearest returns a squared distance, so gotta change it back to real distance */
										copy_v3_v3(hitCo, nearest.co);
										hQuad = (nearest.no[0] == 1.0f);
										face = nearest.index;
									}
								}
								else { /*  else cast a ray in surface normal direction	*/
									negate_v3(ray_dir);
									hit.index = -1;
									hit.dist = paint->paint_distance;

									/* Do a face normal directional raycast, and use that distance	*/
									if(BLI_bvhtree_ray_cast(treeData.tree, ray_start, ray_dir, 0.0f, &hit, mesh_faces_spherecast_dp, &treeData) != -1)
									{
										proxDist = hit.dist;
										VECADDFAC(hitCo, ray_start, ray_dir, hit.dist);	/* Calculate final hit coordinates */
										hQuad = (hit.no[0] == 1.0f);
										face = hit.index;
									}
								}

								/* If a hit was found, calculate required values	*/
								if (proxDist >= 0.0f) {
									float dist_rate = proxDist / paint->paint_distance;

										/* Smooth range or color ramp	*/
										if (paint->proximity_falloff == MOD_DPAINT_PRFALL_SMOOTH ||
											paint->proximity_falloff == MOD_DPAINT_PRFALL_RAMP) {

											/* Limit distance to 0.0 - 1.0 */
											if (dist_rate > 1.0f) dist_rate = 1.0f;
											if (dist_rate < 0.0f) dist_rate = 0.0f;

											/* if using smooth falloff, multiply gaussian factor */
											if (paint->proximity_falloff == MOD_DPAINT_PRFALL_SMOOTH) {
												ssFactor += (1.0f - dist_rate) * gaus_factor;
											}
											else ssFactor += gaus_factor;

											if (hitFace == -1) {
												distRate = dist_rate;
											}
										}
										else ssFactor += gaus_factor;

										hit_found = 1;

										if (hitFace == -1) {
											copy_v3_v3(hitCoord, hitCo);
											hitQuad = hQuad;
											hitFace = face;
										}
								}	// proxDist
							}	// end proximity check

							/*
							*	Process color and alpha
							*/
							if (hit_found)
							{
								float sampleColor[3];
								float sampleAlpha = 1.0f;
								float bandres[4];

								sampleColor[0] = paint->r;
								sampleColor[1] = paint->g;
								sampleColor[2] = paint->b;
							
								/* Get material+textures color on hit point if required	*/
								if (paint->flags & MOD_DPAINT_USE_MATERIAL) DynamicPaint_GetMaterialColor(sampleColor, &sampleAlpha, paintOb, realPos, hitCoord, hitFace, hitQuad, paint->dm, paint->mat);

								/* Sample colorband if required	*/
								if ((distRate >= 0.0f) && (paint->proximity_falloff == MOD_DPAINT_PRFALL_RAMP) && do_colorband(paint->paint_ramp, distRate, bandres)) {
									if (!(paint->flags & MOD_DPAINT_RAMP_ALPHA)) {
										sampleColor[0] = bandres[0];
										sampleColor[1] = bandres[1];
										sampleColor[2] = bandres[2];
									}
									sampleAlpha *= bandres[3];
								}

								/* Add AA sample	*/
								paintColor[0] += sampleColor[0];
								paintColor[1] += sampleColor[1];
								paintColor[2] += sampleColor[2];

								paintAlpha += sampleAlpha;
								numOfHits++;
							}
						} // end supersampling


						/* if any sample was inside paint range	*/
						if (ssFactor > 0.01f) {

							/* apply supersampling results	*/
							if (surface->pixelSamples > 1) {
								ssFactor /= gaussianTotal;
							}

							cPoint->state = 2;

							if (paint->flags & MOD_DPAINT_DO_PAINT) {

								float paintWetness = paint->wetness * ssFactor;

								/* Get final pixel color and alpha	*/
								paintColor[0] /= numOfHits;
								paintColor[1] /= numOfHits;
								paintColor[2] /= numOfHits;
								paintAlpha /= numOfHits;

								/* Multiply alpha value by the ui multiplier	*/
								paintAlpha = paintAlpha * ssFactor * paint->alpha;
								if (paintAlpha > 1.0f) paintAlpha = 1.0f;

								/*
								*	Mix paint to the surface
								*/
								DynamicPaint_MixPaintColors(cPoint, paint->flags, paintColor, &paintAlpha, &paintWetness, &timescale);
							}

							if (paint->flags & MOD_DPAINT_DO_DISPLACE) {

								if (paint->flags & MOD_DPAINT_ERASE) {
									cPoint->depth *= (1.0f - ssFactor);
									if (cPoint->depth < 0.0f) cPoint->depth = 0.0f;
								}
								else {
									float normal_scale, tempNorm[3];
									/*
									*	Calculate normal directional scale of canvas object.
									*	(Displace maps work in object space)
									*/
									MVert *canMvert = NULL;
									canMvert = canvas->dm->getVertArray(canvas->dm);
									normal_tri_v3( tempNorm, canMvert[cPoint->v1].co, canMvert[cPoint->v2].co, canMvert[cPoint->v3].co);
									mul_v3_v3 (tempNorm, canvasOb->size);
									normal_scale = len_v3(tempNorm);
									if (normal_scale<0.01) normal_scale = 0.01;

									depth /= canvas->disp_depth * surface->pixelSamples * normal_scale;
									/* do displace	*/
									if (cPoint->depth < depth) cPoint->depth = depth;
								}
							}
						}
					} // end i>0
				} // yy
			} // end of pixel loop
		} // end if tree exists

		/* free bhv tree */
		free_bvhtree_from_mesh(&treeData);
		dm->release(dm);

	}

	return 1;
}



/*
*	Paint a particle system to canvas
*/
static int DynamicPaint_PaintParticles(DynamicPaintCanvasSettings *canvas, ParticleSystem *psys, DynamicPaintPainterSettings *paint, Object *canvasOb, float timescale)
{
	int yy;
	ParticleSettings *part=psys->part;
	ParticleData *pa = NULL;
	PaintSurface *surface = canvas->surface;

	int tWidth = surface->w;
	int tHeight = surface->h;
	KDTree *tree;
	int particlesAdded = 0;
	int invalidParticles = 0;
	int p = 0;

	if (psys->totpart < 1) return 1;

	/*
	*	Build a kd-tree to optimize distance search
	*/
	tree= BLI_kdtree_new(psys->totpart);

	/* loop through particles and insert valid ones	to the tree	*/
	for(p=0, pa=psys->particles; p<psys->totpart; p++, pa++)	{

		/* Proceed only if particle is active	*/
		if(pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN)==0) continue;									
		else if(pa->alive == PARS_DEAD && (part->flag & PART_DIED)==0) continue;									
		else if(pa->flag & PARS_NO_DISP || pa->flag & PARS_UNEXIST) continue;

		/*	for debug purposes check if any NAN particle proceeds
		*	For some reason they get past activity check, this should rule most of them out	*/
		if (isnan(pa->state.co[0]) || isnan(pa->state.co[1]) || isnan(pa->state.co[2])) {invalidParticles++;continue;}

		BLI_kdtree_insert(tree, p, pa->state.co, NULL);
		particlesAdded++;
	}


	if (invalidParticles) {
		printf("Warning: Invalid particle(s) found!\n");
	}

	/* If no suitable particles were found, exit	*/
	if (particlesAdded < 1) {
		BLI_kdtree_free(tree);
		return 1;
	}

	/* balance tree	*/
	BLI_kdtree_balance(tree);


	/*
	*	Loop through every pixel
	*/
	#pragma omp parallel for schedule(static)
	for (yy = 0; yy < tHeight; yy++)
	{
		int xx;
		for (xx = 0; xx < tWidth; xx++) {

			int index = xx+tWidth*yy;
			PaintSurfacePoint *cPoint = (&surface->point[index]);
			int i = cPoint->index;

			/* If this canvas point exists	*/
			if (i >= 0)
			{
				int index;
				float disp_intersect = 0;
				float radius;
				float solidradius = paint->particle_radius;
				float smooth = paint->particle_smooth;
				float strength = 0.0f;

				/* If using per particle radius	*/
				if (paint->flags & MOD_DPAINT_PART_RAD) {
					/*
					*	If we use per particle radius, we have to sample all particles
					*	within max radius range
					*/
					KDTreeNearest *nearest = NULL;
					int n, particles = 0;
					float range = psys->part->size + smooth;

					particles = BLI_kdtree_range_search(tree, range, cPoint->realCoord, NULL, &nearest);
					for(n=0; n<particles; n++) {

						/*
						*	Find particle that produces highest influence
						*/
						ParticleData *pa = psys->particles + nearest[n].index;
						float rad = pa->size + smooth;
						float str,smooth_range;

						if (nearest[n].dist > rad) continue; /* if outside range, continue to next one */

						if ((rad-nearest[n].dist) > disp_intersect) {
							disp_intersect = rad-nearest[n].dist;
							radius = rad;
						}

						/* Continue with paint check	*/
						if (nearest[n].dist < pa->size) {
							/*
							*	If particle is inside the solid range, no need to continue further
							*	since no other particle can have higher influence
							*/
							strength = 1.0f;
							break;
						}

						smooth_range = (nearest[n].dist - pa->size);

						/* do smoothness if enabled	*/
						if (smooth) smooth_range/=smooth;
						str = 1.0f - smooth_range;

						/* if influence is greater, use this one	*/
						if (str > strength) strength = str;

					}	// end particle loop

					if (nearest) MEM_freeN(nearest);

				}
				else {
					/*
					*	With predefined radius, there is no variation between particles.
					*	It's enough to just find the nearest one.
					*/
					KDTreeNearest nearest;
					float smooth_range;
					radius = solidradius + smooth;

					/* Find nearest particle and get distance to it	*/
					index = BLI_kdtree_find_nearest(tree, cPoint->realCoord, NULL, &nearest);
					if (nearest.dist > radius) continue;

					/* distances inside solid radius have maximum influence -> dist = 0	*/
					smooth_range = (nearest.dist - solidradius);
					if (smooth_range<0) smooth_range=0.0f;

					/* do smoothness if enabled	*/
					if (smooth) smooth_range/=smooth;

					strength = 1.0f - smooth_range;
					disp_intersect = radius - nearest.dist;
				}

				if (strength <= 0.0f) continue;

				cPoint->state = 2;

				if (paint->flags & MOD_DPAINT_DO_PAINT) {

					float paintAlpha = paint->alpha * strength;
					float paintWetness = paint->wetness * strength;
					float paintColor[3];

					paintColor[0] = paint->r;
					paintColor[1] = paint->g;
					paintColor[2] = paint->b;

					if (paintAlpha > 1.0f) paintAlpha = 1.0f;

					DynamicPaint_MixPaintColors(cPoint, paint->flags, paintColor, &paintAlpha, &paintWetness, &timescale);

				}

				if (paint->flags & MOD_DPAINT_DO_WETNESS) {
					if (cPoint->wetness < (paint->wetness*strength)) cPoint->wetness = paint->wetness*strength;
				}

				if (paint->flags & MOD_DPAINT_DO_DISPLACE) {
					float sdepth, disp, normal_scale, tempNorm[3];
					MVert *canMvert = NULL;
					canMvert = canvas->dm->getVertArray(canvas->dm);

					/*
					*	Calculate normal directional scale of canvas object.
					*	(Displace maps work in object space)
					*/
					normal_tri_v3( tempNorm, canMvert[cPoint->v1].co, canMvert[cPoint->v2].co, canMvert[cPoint->v3].co);
					mul_v3_v3 (tempNorm, canvasOb->size);
					normal_scale = len_v3(tempNorm);
					if (normal_scale<0.01) normal_scale = 0.01;

					/* change falloff type to inverse square to match real displace depth	*/
					disp_intersect = (1.0f - sqrt(disp_intersect / radius)) * radius;
						
					/* get displace depth	*/
					sdepth = (radius - disp_intersect) / normal_scale;

					if (sdepth<0.0f) sdepth = 0.0f;
					disp = sdepth / canvas->disp_depth;
					if (cPoint->depth < disp) cPoint->depth = disp;
				}
			}
		}
	}

	BLI_kdtree_free(tree);

	return 1;
}


/*
*	Prepare data required by effects for current frame.
*	Returns number of steps required
*/
static int DynamicPaint_Prepare_EffectStep(DynamicPaintCanvasSettings *canvas, float timescale)
{
	double average_dist = 0.0f;
	float minimum_dist = 9999.0f;
	unsigned int count = 0;
	int steps = 1;
	PaintSurface *surface = canvas->surface;
	int yy, w=surface->w, h=surface->h;

	float fastest_effect, speed_factor;

	/*
	*	Calculate current frame neighbouring pixel distances
	*	and average distance between those neighbours
	*/
	#pragma omp parallel for schedule(static)
	for (yy = 0; yy < h; yy++)
	{
		int xx;
		for (xx = 0; xx < w; xx++)
		{
			int i;
			PaintSurfacePoint *cPoint = (&surface->point[xx+w*yy]);

			if (cPoint->index != -1)
			for (i=0; i<8; i++) {
				int x,y, index;
				PaintSurfacePoint *tPoint;

				x = xx + neighX[i];
				y = yy + neighY[i];

				index = x+w*y;
				if (cPoint->neighbours[i] != -1) {

					tPoint = (&surface->point[index]);
					cPoint->neighbour_dist[i] = len_v3v3(cPoint->realCoord, tPoint->realCoord);
					if (cPoint->neighbour_dist[i] < minimum_dist) minimum_dist = cPoint->neighbour_dist[i];

					average_dist += cPoint->neighbour_dist[i];
					count++;
				}
			}
		}

	} // end pixel loop
			
	/*
	*	Note: some other method could be better
	*	Right now double precision may cause noticable error on huge
	*	texture sizes (8096*8096*8 = 500 000 000 added values)
	*/
	average_dist /= (double)count;

	/*	Limit minimum distance (used to define substeps)
	*	to 1/2 of the average
	*/
	if (minimum_dist < (average_dist/2.0f)) minimum_dist=average_dist/2.0f;

	/* Get fastest effect speed and scale other effects according to it	*/
	fastest_effect = canvas->spread_speed;
	if (canvas->drip_speed > fastest_effect) fastest_effect = canvas->drip_speed;
	if (canvas->shrink_speed > fastest_effect) fastest_effect = canvas->shrink_speed;

	/* Number of steps depends on surface res, effect speed and timescale	*/
	speed_factor = (float)surface->w/256.0f * timescale;
	steps = (int)ceil(speed_factor*fastest_effect);

	speed_factor /= (float)steps;

	/*
	*	Now calculate final per pixel speed ratio to neighbour_dist[] array
	*/
	#pragma omp parallel for schedule(static)
	for (yy = 0; yy < h; yy++)
	{
		int xx;
		float dd;
		for (xx = 0; xx < w; xx++)
		{
			int i;
			PaintSurfacePoint *cPoint = (&surface->point[xx+w*yy]);

			if (cPoint->index != -1)
			for (i=0; i<8; i++) {
				if (cPoint->neighbours[i] != -1) {
					dd = (cPoint->neighbour_dist[i]<minimum_dist) ? minimum_dist : cPoint->neighbour_dist[i];
					cPoint->neighbour_dist[i] = minimum_dist / dd * speed_factor;
				}
			}
		}
	} // end pixel loop

	//printf("Average distance is %f, minimum distance is %f\n", average_dist, minimum_dist);

	return steps;
}


/*
*	Clean effect data
*/
static void DynamicPaint_Clean_EffectStep(DynamicPaintCanvasSettings *canvas, float timescale)
{
	//PaintSurface *surface = canvas->surface;
}


/*
*	Processes active effect step.
*/
static void DynamicPaint_Do_EffectStep(DynamicPaintCanvasSettings *canvas, PaintSurfacePoint *prevPoint, float timescale)
{
	PaintSurface *surface = canvas->surface;
	int yy, w=surface->w, h=surface->h;

	/*
	*	Spread Effect
	*/
	if (canvas->effect & MOD_DPAINT_EFFECT_DO_SPREAD)  {

		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int index = xx+w*yy;
				int i, validPoints = 0;
				float totalAlpha = 0.0f;
				PaintSurfacePoint *cPoint = (&surface->point[index]);	/* Current source point */
				PaintSurfacePoint *ePoint;							/* Effect point to shift values into */

				/*
				*	Only reads values from the surface copy (prevPoint[]),
				*	so this one is thread safe
				*/

				/*	Loop through neighbouring pixels	*/
				for (i=0; i<8; i++) {
					int nIndex;
					float factor, alphaAdd = 0.0f;

					nIndex = surface->point[index].neighbours[i];
					if (nIndex == -1) continue;

					/*
					*	Find neighbour cells that have higher wetness
					*	and expand it to this cell as well.
					*/
					ePoint = (&prevPoint[nIndex]);
					validPoints++;
					totalAlpha += ePoint->e_alpha;

					if (ePoint->wetness <= cPoint->wetness) continue;
					factor = ePoint->wetness/8 * (ePoint->wetness - cPoint->wetness) * surface->point[index].neighbour_dist[i] * canvas->spread_speed;

					if (ePoint->e_alpha > cPoint->e_alpha) {
						alphaAdd = ePoint->e_alpha/8 * (ePoint->wetness*ePoint->e_alpha - cPoint->wetness*cPoint->e_alpha) * surface->point[index].neighbour_dist[i] * canvas->spread_speed;
					}


					/* If this pixel has existing paint, we have to blend it properly	*/
					if (cPoint->e_alpha) {
						float invFactor = 1.0f - factor;
						cPoint->e_color[0] = cPoint->e_color[0]*invFactor + ePoint->e_color[0]*factor;
						cPoint->e_color[1] = cPoint->e_color[1]*invFactor + ePoint->e_color[1]*factor;
						cPoint->e_color[2] = cPoint->e_color[2]*invFactor + ePoint->e_color[2]*factor;
							
						cPoint->e_alpha += alphaAdd;
						cPoint->wetness += factor;
					}
					else {
						/* If there is no existing paint, just replace the color */
						cPoint->e_color[0] = ePoint->e_color[0];
						cPoint->e_color[1] = ePoint->e_color[1];
						cPoint->e_color[2] = ePoint->e_color[2];

						cPoint->e_alpha += alphaAdd;
						cPoint->wetness += factor;
					}

					if (cPoint->e_alpha > 1.0f) cPoint->e_alpha = 1.0f;
				}

				/* For antialiasing sake, don't let alpha go much higher than average alpha of neighbours	*/
				if (validPoints && (cPoint->e_alpha > (totalAlpha/validPoints+0.25f))) {
					cPoint->e_alpha = (totalAlpha/validPoints+0.25f);
					if (cPoint->e_alpha>1.0f) cPoint->e_alpha = 1.0f;
				}
			}
		} // end pixel loop
	} // end spread effect


	/*
	*	Drip Effect
	*/
	if (canvas->effect & MOD_DPAINT_EFFECT_DO_DRIP) 
	{
		memcpy(prevPoint, surface->point, surface->w*surface->h*sizeof(struct PaintSurfacePoint));

		//#pragma omp parallel for schedule(static)
		for (yy = 1; yy < h-1; yy++)
		{
			int xx;
			for (xx = 1; xx < w-1; xx++)
			{
				int index = xx+w*yy;
				float factor = 0.0f, drip_strength = 0.0f;
				PaintSurfacePoint *cPoint = (&prevPoint[index]);	/* Current source point to read */
				PaintSurfacePoint *dPoint = (&surface->point[index]);	/* Current source point to write */
				PaintSurfacePoint *ePoint;							/* Effect point to shift values into */
				PaintSurfacePoint *ePoint2;							/* Effect point to shift values into */
				int nIndex;

				/*
				*	Because the neighbour pixels go in 45 degree (pi/4) slices
				*	get drip factor in that range
				*/
				float dirMod = fmod(cPoint->gravity_dir,0.785398163);
				float stFac=dirMod/0.785398163;
				float facTotal;
				int neighPixel,neighPixel2;

				/* Skip if wetness is too low	*/
				drip_strength = cPoint->wetness - 0.1f;
				if (drip_strength < 0) continue;


				/* Get first neighpixel index for neighbours[] array */
				neighPixel = (int)floor(cPoint->gravity_dir/6.2831853f * 8.0f);

				if (neighPixel > 7) neighPixel = 7;	/* Shouldn't happen but just in case */
				if (neighPixel < 0) neighPixel = 0;

				/* Make sure the neighbour exists	*/
				nIndex = surface->point[index].neighbours[neighPixel];
				if (nIndex == -1) continue;
				ePoint = (&surface->point[nIndex]);


				/*
				*	Second neighbouring point
				*/
				neighPixel2 = neighPixel + 1;
				if (neighPixel2 > 7) neighPixel2 = 0;

				/* Make sure the neighbour exists	*/
				nIndex = surface->point[index].neighbours[neighPixel2];
				if (nIndex == -1) continue;
				ePoint2 = (&surface->point[nIndex]);


				/* Do some adjustments to dripping speed	*/
				factor = 0.3 * canvas->drip_speed;
				if (drip_strength < 2.5f) factor *= drip_strength;

				/* Add values to the pixel below -> drip	*/
				ePoint->e_color[0] = cPoint->e_color[0];
				ePoint->e_color[1] = cPoint->e_color[1];
				ePoint->e_color[2] = cPoint->e_color[2];

				ePoint->e_alpha += cPoint->e_alpha * factor * surface->point[index].neighbour_dist[neighPixel] * (1.0f - stFac);
				ePoint->wetness += cPoint->wetness * factor * surface->point[index].neighbour_dist[neighPixel] * (1.0f - stFac);

				if (stFac > 0.0f) {
					ePoint2->e_color[0] = cPoint->e_color[0];
					ePoint2->e_color[1] = cPoint->e_color[1];
					ePoint2->e_color[2] = cPoint->e_color[2];
				}

				ePoint2->e_alpha += cPoint->e_alpha * factor * surface->point[index].neighbour_dist[neighPixel2] * stFac;
				ePoint2->wetness += cPoint->wetness * factor * surface->point[index].neighbour_dist[neighPixel2] * stFac;

				//dPoint->e_alpha -= cPoint->e_alpha * factor;
				facTotal = surface->point[index].neighbour_dist[neighPixel] * (1.0f - stFac) + surface->point[index].neighbour_dist[neighPixel2] * stFac;
				dPoint->wetness -= cPoint->wetness * factor * facTotal;

			}
		} // end pixel loop

		/* Keep wetness values within acceptable range */
		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int index = xx+w*yy;

				PaintSurfacePoint *cPoint = (&surface->point[index]);

				if (cPoint->e_alpha > 1.0f) cPoint->e_alpha=1.0f;
				if (cPoint->wetness > 3.5f) cPoint->wetness=3.5f;

				if (cPoint->e_alpha < 0.0f) cPoint->e_alpha=0.0f;
				if (cPoint->wetness < 0.0f) cPoint->wetness=0.0f;

			}
		} // end pixel loop
	} // end dripping effect



	/*
	*	Shrink Effect
	*/
	if (canvas->effect & MOD_DPAINT_EFFECT_DO_SHRINK)  {

		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < h; yy++)
		{
			int xx;
			for (xx = 0; xx < w; xx++)
			{
				int index = xx+w*yy;
				int i, validPoints = 0;
				float totalAlpha = 0.0f;
				PaintSurfacePoint *cPoint = (&surface->point[index]);	/* Current source point */
				PaintSurfacePoint *ePoint;							/* Effect point to shift values into */

				/*	Loop through neighbouring pixels	*/
				for (i=0; i<8; i++) {
					int nIndex;
					float factor, e_factor, w_factor;

					nIndex = surface->point[index].neighbours[i];

					if (nIndex == -1) continue;

					/*
					*	Find neighbouring cells that have lower alpha
					*	and decrease this point alpha towards that level.
					*/
					ePoint = (&prevPoint[nIndex]);
					validPoints++;
					totalAlpha += ePoint->e_alpha;

					if (cPoint->alpha <= 0.0f && cPoint->e_alpha <= 0.0f && cPoint->wetness <= 0.0f) continue;
					factor = (1.0f - ePoint->alpha)/8 * (cPoint->alpha - ePoint->alpha) * surface->point[index].neighbour_dist[i] * canvas->shrink_speed;
					if (factor < 0.0f) factor = 0.0f;

					e_factor = (1.0f - ePoint->e_alpha)/8 * (cPoint->e_alpha - ePoint->e_alpha) * surface->point[index].neighbour_dist[i] * canvas->shrink_speed;
					if (e_factor < 0.0f) e_factor = 0.0f;

					w_factor = (1.0f - ePoint->wetness)/8 * (cPoint->wetness - ePoint->wetness) * surface->point[index].neighbour_dist[i] * canvas->shrink_speed;
					if (w_factor < 0.0f) w_factor = 0.0f;


					if (factor) {
						cPoint->alpha -= factor;
						if (cPoint->alpha < 0.0f) cPoint->alpha = 0.0f;
						cPoint->wetness -= factor;

					}
					else {
						cPoint->e_alpha -= e_factor;
						if (cPoint->e_alpha < 0.0f) cPoint->e_alpha = 0.0f;
						cPoint->wetness -= w_factor;
						if (cPoint->wetness < 0.0f) cPoint->wetness = 0.0f;
					}
				}
			}
		} // end pixel loop
	} // end spread effect
}



/*
*	Do Dynamic Paint Step. Paints scene paint objects of current frame to canvas.
*/
static int DynamicPaint_DoStep(Scene *scene, Object *ob, DynamicPaintModifierData *pmd, float timescale)
{
	DynamicPaintCanvasSettings *canvas = pmd->canvas;
	PaintSurface *surface = canvas->surface;
	Base *base = NULL;
	MVert *mvert = NULL;
	MFace *mface = NULL;
	MTFace *tface = NULL;
	DerivedMesh *dm = canvas->dm;

	int w, h;
	int yy;

	int canvasNumOfVerts;
	Vec3f *canvasVerts = NULL;
	int canvasNumOfFaces;
	FaceAdv *canvasInvNormals = NULL;

	if (!dm) {
		dpError(canvas, "Invalid canvas mesh."); 
		return 0;
	}

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);
	tface = DM_get_face_data_layer(dm, CD_MTFACE);
	canvasNumOfFaces = dm->getNumFaces(dm);

	w = canvas->surface->w;
	h = canvas->surface->h;

	/*
	*	Make a transformed copy of canvas derived mesh vertices to avoid recalculation.
	*/
	canvasNumOfVerts = dm->getNumVerts(dm);
	canvasVerts =  (struct Vec3f *) MEM_mallocN(canvasNumOfVerts*sizeof(struct Vec3f), "Dynamic Paint Transformed canvas");
	if (canvasVerts == NULL) {dpError(canvas, "Not enough free memory."); return 0;}

	#pragma omp parallel for schedule(static)
	for (yy=0; yy<canvasNumOfVerts; yy++) {
		VECCOPY(canvasVerts[yy].v, mvert[yy].co);
		/* Multiply coordinates by canvas matrix	*/
		mul_m4_v3(ob->obmat, canvasVerts[yy].v);
	}


	/*
	*	Calculate temp per face normals using transformed coordinates.
	*	(To not have to calculate same normal for millions of pixels)
	*/
	canvasInvNormals =  (struct FaceAdv *) MEM_callocN(canvasNumOfFaces*sizeof(struct FaceAdv), "Dynamic Paint canvas normals");
	if (canvasInvNormals == NULL) {dpError(canvas, "Not enough free memory."); return 0;}


	#pragma omp parallel for schedule(static)
	for (yy=0; yy<canvasNumOfFaces; yy++) {

		if (mface[yy].flag & ME_SMOOTH) continue;	/* Only calculate flat faces */
		/*
		*	Transformed normal
		*/
		normal_tri_v3( canvasInvNormals[yy].no, canvasVerts[mface[yy].v3].v, canvasVerts[mface[yy].v2].v, canvasVerts[mface[yy].v1].v);
		if (mface[yy].v4) normal_tri_v3( canvasInvNormals[yy].no_q, canvasVerts[mface[yy].v4].v, canvasVerts[mface[yy].v3].v, canvasVerts[mface[yy].v1].v);
	}


	/*
	*	Prepare each pixel for a new step
	*/
	#pragma omp parallel for schedule(static)
	for (yy = 0; yy < h; yy++)
	{
		int xx;
		float n1[3], n2[3], n3[3];
		for (xx = 0; xx < w; xx++)
		{
			int i,index = xx+w*yy;
			short quad;

			i = canvas->surface->point[index].index;
			quad = canvas->surface->point[index].quad;

			/* only continue if current pixel has 3D coordinates	*/
			if (i >= 0)
			{
				PaintSurfacePoint *cPoint = (&canvas->surface->point[index]);

				/*
				*  Do dissolve / drying / flatten
				*/
				if (cPoint->wetness > 0.0f) {

					/* Every drying step Blends wet paint to the background.	*/
					float invAlpha = 1.0f - cPoint->e_alpha;
					cPoint->color[0] = cPoint->color[0]*invAlpha + cPoint->e_color[0]*cPoint->e_alpha;
					cPoint->color[1] = cPoint->color[1]*invAlpha + cPoint->e_color[1]*cPoint->e_alpha;
					cPoint->color[2] = cPoint->color[2]*invAlpha + cPoint->e_color[2]*cPoint->e_alpha;

					cPoint->state = 1;

					/* only increase alpha if wet paint has higher	*/
					if (cPoint->e_alpha > cPoint->alpha) cPoint->alpha = cPoint->e_alpha;

					/* Now dry it ;o	*/
					if (canvas->flags & MOD_DPAINT_DRY_LOG) cPoint->wetness *= 1.0f - (1.0 / (canvas->dry_speed/timescale));
					else cPoint->wetness -= 1.0f/canvas->dry_speed*timescale;
				}

				/* 	If effect layer is completely dry, make sure it's marked empty */
				if (cPoint->wetness <= 0.0f) {
					cPoint->wetness = 0.0f;
					cPoint->e_alpha = 0.0f;
					cPoint->state = 0;
				}

				if (canvas->flags & (MOD_DPAINT_DISSOLVE)) {

					cPoint->alpha -= 1.0f/canvas->diss_speed*timescale;
					if (cPoint->alpha < 0.0f) cPoint->alpha = 0.0f;

					cPoint->e_alpha -= 1.0f/canvas->diss_speed*timescale;
					if (cPoint->e_alpha < 0.0f) cPoint->e_alpha = 0.0f;
				}

				if (canvas->flags & (MOD_DPAINT_FLATTEN)) {

					cPoint->depth -= 1.0f/canvas->dflat_speed*timescale;
					if (cPoint->depth < 0.0f) cPoint->depth = 0.0f;
				}


				/*
				*	Calculate current 3D-position of each texture pixel
				*/
				interp_v3_v3v3v3(	cPoint->realCoord,
					canvasVerts[cPoint->v1].v,
					canvasVerts[cPoint->v2].v,
					canvasVerts[cPoint->v3].v, cPoint->barycentricWeights[0].v);

				/* Calculate current pixel surface normal	*/
				if(mface[cPoint->index].flag & ME_SMOOTH) {
					normal_short_to_float_v3(n1, mvert[cPoint->v1].no);
					normal_short_to_float_v3(n2, mvert[cPoint->v2].no);
					normal_short_to_float_v3(n3, mvert[cPoint->v3].no);

					interp_v3_v3v3v3(	cPoint->invNorm,
						n1, n2, n3, cPoint->barycentricWeights[0].v);
					normalize_v3(cPoint->invNorm);
					negate_v3(cPoint->invNorm);
				}
				else {
					if (cPoint->quad) {VECCOPY(cPoint->invNorm, canvasInvNormals[cPoint->index].no_q);}
					else {VECCOPY(cPoint->invNorm, canvasInvNormals[cPoint->index].no);}
				}


				/*
				*	Get current gravity direction of pixel in UV space.
				*	World gravity direction is negative z-axis
				*	- if any active effect requires it
				*/
				if (canvas->effect & MOD_DPAINT_EFFECT_DO_DRIP) {
	
					cPoint->gravity_dir = 0.0f;
					cPoint->gravity_rate = 1.0f - abs(cPoint->invNorm[2]);

					if (cPoint->gravity_rate > 0.01)
					{
						float uv1[3], uv2[3], uv3[3], unormal[3];
						int v2i, v3i;

						v2i = (cPoint->quad) ? 2 : 1;
						v3i = (cPoint->quad) ? 3 : 2;

						/*
						*	Apply z-coodrinate (gravity dir) to uv-vertices
						*/
						uv1[0] = tface[cPoint->index].uv[0][0];
						uv1[1] = tface[cPoint->index].uv[0][1];
						uv1[2] = canvasVerts[cPoint->v1].v[2];

						uv2[0] = tface[cPoint->index].uv[v2i][0];
						uv2[1] = tface[cPoint->index].uv[v2i][1];
						uv2[2] = canvasVerts[cPoint->v2].v[2];

						uv3[0] = tface[cPoint->index].uv[v3i][0];
						uv3[1] = tface[cPoint->index].uv[v3i][1];
						uv3[2] = canvasVerts[cPoint->v3].v[2];

						/* Calculate a new normal vector for that generated face */
						normal_tri_v3( unormal, uv1, uv2, uv3);

						/* Normalize u and v part of it	*/
						normalize_v2(unormal);

						/*
						*	use direction of that 2d vector as gravity direction in uv space
						*	(Convert from -pi->pi to 0->2pi to use with neighbour table)
						*/
						cPoint->gravity_dir = fmod((atan2(unormal[1], unormal[0])+6.28318531f), 6.28318531f);
					}
				} // end if (effect)
			} // end i>0
		}	// yy
	} // end of loop xx
	MEM_freeN(canvasInvNormals);


	/*
	*	Loop through every object in scene and
	*	do painting for active paint objects
	*/
	{
		Object *otherobj = NULL;
		ModifierData *md = NULL;

		base = scene->base.first;

		while(base)
		{
			otherobj = base->object;

			if(!otherobj)					
			{												
				base= base->next;						
				continue;			
			}
			base= base->next;

			md = modifiers_findByType(otherobj, eModifierType_DynamicPaint);

			/* check if target has an active dp modifier	*/
			if(md && md->mode & (eModifierMode_Realtime | eModifierMode_Render))					
			{
				DynamicPaintModifierData *pmd2 = (DynamicPaintModifierData *)md;

				/* Make sure we're dealing with a painter	*/
				if((pmd2->type & MOD_DYNAMICPAINT_TYPE_PAINT) && pmd2->paint)
				{
					DynamicPaintPainterSettings *paint = pmd2->paint;

					/*
					*	Make sure at least one influence is enabled
					*/
					if (!(paint->flags & MOD_DPAINT_DO_PAINT || paint->flags & MOD_DPAINT_DO_DISPLACE)) continue;


					/*	Check if painter has a particle system selected
					*	-> if so, do particle painting */
					if (paint->collision == MOD_DPAINT_COL_PSYS)
					{
						if (paint && paint->psys && paint->psys->part && paint->psys->part->type==PART_EMITTER)
						if (psys_check_enabled(otherobj, paint->psys)) {
							/*
							*	Paint a particle system
							*/
							DynamicPaint_PaintParticles(canvas, paint->psys, paint, ob, timescale);
						}
					}							
					else {
						/*
						*	Paint a object mesh
						*/
						DynamicPaint_PaintMesh(canvas, canvasVerts, paint, ob, otherobj, timescale);
					}
				} /* end of collision check (Is valid paint modifier) */
			}
		}
	}

	/* Free per frame canvas data	*/
	MEM_freeN(canvasVerts);


	/*
	*	DO EFFECTS
	*/
	if (canvas->effect)
	{
		unsigned int steps = 1, s;

		/* Allocate memory for surface previous points to read unchanged values from	*/
		PaintSurfacePoint *prevPoint = (struct PaintSurfacePoint *) MEM_mallocN(surface->w*surface->h*sizeof(struct PaintSurfacePoint), "PaintSurfaceDataTemp");
		if (prevPoint == NULL) {dpError(canvas, "Not enough free memory."); return 0;}

		/* Prepare effects and get number of required effect-substeps */
		steps = DynamicPaint_Prepare_EffectStep(canvas, timescale);

		/*
		*	Do Effects steps
		*/
		for (s = 0; s < steps; s++)
		{
			/* Copy current surface to the previous surface array	*/
			memcpy(prevPoint, surface->point, surface->w*surface->h*sizeof(struct PaintSurfacePoint));

			DynamicPaint_Do_EffectStep(canvas, prevPoint, timescale);

		}

		/* Free temporary effect data	*/
		MEM_freeN(prevPoint);
		DynamicPaint_Clean_EffectStep(canvas, timescale);

	} // end effects

	return 1;
}


/*
*	Outputs an image file from canvas data.
*/
void dynamic_paint_output_image(struct DynamicPaintCanvasSettings *canvas, char* filename, short type, short source)
{
		int yy;
		ImBuf* mhImgB = NULL;

		int tWidth, tHeight;
		PaintSurface *surface = canvas->surface;

		if (!surface) {printf("Canvas save failed: Invalid surface.\n");return;}

		/* Validate output file path	*/
		BLI_path_abs(filename, G.main->name);
		BLI_make_existing_file(filename);

		/* Print save message	*/
		if (source == DPOUTPUT_WET) {
			printf("Saving wetmap : %s\n", filename);
		} else if (source == DPOUTPUT_DISPLACE)  {
			printf("Saving displacement map : %s\n", filename);
		} else {
			printf("Saving color map : %s\n", filename);
		}

		tWidth = surface->w;
		tHeight = surface->h;

		/* Init image buffer	*/
		mhImgB = IMB_allocImBuf(tWidth, tHeight, 32, IB_rectfloat);
		if (mhImgB == NULL) {printf("Image save failed: Not enough free memory.\n");return;}

		#pragma omp parallel for schedule(static)
		for (yy = 0; yy < tHeight; yy++)
		{
			int xx;
			for (xx = 0; xx < tWidth; xx++)
			{
				int pos=(xx+tWidth*yy)*4;	/* image buffer position */
				int index=(xx+tWidth*yy);	/* surface point */
				PaintSurfacePoint *cPoint = (&surface->point[index]);
				

				/* Set values of preferred type */
				if (source == DPOUTPUT_WET) {
					float value = (cPoint->wetness > 1.0f) ? 1.0f : cPoint->wetness;
					mhImgB->rect_float[pos]=value;
					mhImgB->rect_float[pos+1]=value;
					mhImgB->rect_float[pos+2]=value;
					mhImgB->rect_float[pos+3]=1.0f;
				}
				else if (source == DPOUTPUT_DISPLACE) {

					float depth = cPoint->depth;

					if (canvas->disp_type == MOD_DPAINT_DISP_DISPLACE) {
						depth = (0.5f - depth);
						if (depth < 0.0f) depth = 0.0f;
						if (depth > 1.0f) depth = 1.0f;
					}

					mhImgB->rect_float[pos]=depth;
					mhImgB->rect_float[pos+1]=depth;
					mhImgB->rect_float[pos+2]=depth;
					mhImgB->rect_float[pos+3]=1.0f;
				}
				else {	/* DPOUTPUT_PAINT	*/

					float invAlpha = 1.0f - cPoint->e_alpha;

					/* If base layer already has a color, blend it	*/
					if (cPoint->alpha) {
						mhImgB->rect_float[pos]   = cPoint->color[0] * invAlpha + cPoint->e_color[0] * cPoint->e_alpha;
						mhImgB->rect_float[pos+1] = cPoint->color[1] * invAlpha + cPoint->e_color[1] * cPoint->e_alpha;
						mhImgB->rect_float[pos+2] = cPoint->color[2] * invAlpha + cPoint->e_color[2] * cPoint->e_alpha;
					}
					else {
						/* Else use effect layer color	*/
						mhImgB->rect_float[pos]   = cPoint->e_color[0];
						mhImgB->rect_float[pos+1] = cPoint->e_color[1];
						mhImgB->rect_float[pos+2] = cPoint->e_color[2];
					}

					/* Set use highest alpha	*/
					mhImgB->rect_float[pos+3] = (cPoint->e_alpha > cPoint->alpha) ? cPoint->e_alpha : cPoint->alpha;

					/* Multiply color by alpha if enabled	*/
					if (canvas->flags & MOD_DPAINT_MULALPHA) {
						mhImgB->rect_float[pos]   *= mhImgB->rect_float[pos+3];
						mhImgB->rect_float[pos+1] *= mhImgB->rect_float[pos+3];
						mhImgB->rect_float[pos+2] *= mhImgB->rect_float[pos+3];
					}
				}
			}
		}

		/* Save image buffer	*/
		if (type == DPOUTPUT_JPEG) {	/* JPEG */
			mhImgB->ftype= JPG|95;
			IMB_rect_from_float(mhImgB);
			imb_savejpeg(mhImgB, filename, IB_rectfloat);
		}
#ifdef WITH_OPENEXR
		else if (type == DPOUTPUT_OPENEXR) {	/* OpenEXR 32-bit float */
			mhImgB->ftype = OPENEXR | OPENEXR_COMPRESS;
			IMB_rect_from_float(mhImgB);
			imb_save_openexr(mhImgB, filename, IB_rectfloat);
		}
#endif
		else {	/* DPOUTPUT_PNG */
			mhImgB->ftype= PNG|95;
			IMB_rect_from_float(mhImgB);
			imb_savepng(mhImgB, filename, IB_rectfloat);
		}

		IMB_freeImBuf(mhImgB);
}


/*
*	Update required data that didn't get updated
*	by ED_update_for_newframe()
*/
static void update_scene_data(struct Main *bmain, int frame)
{

	Tex *tex;
	/*
	*	Loop through textures and update voxel data
	*/
	for (tex= bmain->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_VOXELDATA) {
			cache_voxeldata(tex, frame);
		}
	}
}


/*
*	Do actual bake operation.
*	returns 0 on failture.
*/
static int DynamicPaint_Bake(bContext *C, struct DynamicPaintModifierData *pmd, Object *cObject)
{

	DynamicPaintCanvasSettings *canvas;
	Scene *scene= CTX_data_scene(C);
	wmWindow *win = CTX_wm_window(C);
	int frame = 1;
	int frames;

	canvas = pmd->canvas;
	if (!canvas) {dpError(canvas, "Invalid canvas."); return 0;}

	frames = canvas->end_frame - canvas->start_frame + 1;
	if (frames <= 0) {dpError(canvas, "No frames to bake."); return 0;}

	/*
	*	Set frame to start point (also inits modifier data)
	*/
	frame = canvas->start_frame;
	scene->r.cfra = (int)frame;
	ED_update_for_newframe(CTX_data_main(C), scene, win->screen, 1);
	update_scene_data(CTX_data_main(C), frame);

	/* Init surface	*/
	if (!dynamicPaint_createCanvasSurface(canvas)) return 0;


	/*
	*	Loop through selected frames
	*/
	for (frame=canvas->start_frame; frame<=canvas->end_frame; frame++)
	{
		float timescale = 1.0f / (canvas->substeps+1);
		int st;
		float progress = (frame - canvas->start_frame) / (float)frames * 100;

		/* If user requested stop (esc), quit baking	*/
		if (blender_test_break()) return 0;

		/* Update progress bar cursor */
		WM_timecursor(win, (int)progress);

		printf("DynamicPaint: Baking frame %i\n", frame);

		/*
		*	Do calculations for every substep
		*	Note: these have to be from previous frame
		*/
		if (frame != canvas->start_frame) {
			for (st = 1; st <= canvas->substeps; st++)
			{
				float subframe = ((float) st) / (canvas->substeps+1);

				/* Update frame if we have proceed	*/
				scene->r.cfra = (int)frame - 1;
				scene->r.subframe = subframe;
				ED_update_for_newframe(CTX_data_main(C), scene, win->screen, 1);

				if (!DynamicPaint_DoStep(scene, cObject, pmd, timescale)) return 0;
			}

			/*
			*	Change to next whole frame
			*/
			scene->r.cfra = (int)frame;
			scene->r.subframe = 0.0f;
			ED_update_for_newframe(CTX_data_main(C), scene, win->screen, 1);
			update_scene_data(CTX_data_main(C), frame);

		}

		if (!DynamicPaint_DoStep(scene, cObject, pmd, timescale)) return 0;

		/*
		*	Just in case, check if any output is enabled
		*	Don't cancel the bake because user may have keyframed outputs
		*/
		if (!(canvas->output & MOD_DPAINT_OUT_PAINT || canvas->output & MOD_DPAINT_OUT_WET || canvas->output & MOD_DPAINT_OUT_DISP)) {
			printf("Skipping output for frame %i.\n", frame);
			continue;
		}

		/*
		*	Save output images
		*/
		{
			char filename[250];
			char pad[4];
			char wet[4];
			char disp[4];

			/* Add frame number padding	*/
			if (frame<10) sprintf(pad,"000");
			else if (frame<100) sprintf(pad,"00");
			else if (frame<1000) sprintf(pad,"0");
			else pad[0] = '\0';

			/* Check if paint and wet map filename is same and fix if necessary	*/
			if (!strcmp(canvas->paint_output_path, canvas->wet_output_path)) sprintf(wet,"wet");
			else wet[0] = '\0';
			/* same for displacement map	*/
			if (!strcmp(canvas->paint_output_path, canvas->displace_output_path)) sprintf(disp,"disp");
			else if (!strcmp(canvas->wet_output_path, canvas->displace_output_path)) sprintf(disp,"disp");
			else disp[0] = '\0';

			/* color map	*/
			if (canvas->output & MOD_DPAINT_OUT_PAINT) {
				sprintf(filename, "%s%s%i.png", canvas->paint_output_path, pad, (int)frame);

				dynamic_paint_output_image(canvas, filename, DPOUTPUT_PNG, DPOUTPUT_PAINT);
			}

			/* wetmap	*/
			if (canvas->output & MOD_DPAINT_OUT_WET) {
				sprintf(filename, "%s%s%s%i.png", canvas->wet_output_path, wet, pad, (int)frame);

				dynamic_paint_output_image(canvas, filename, DPOUTPUT_PNG, DPOUTPUT_WET);
			}

			/* displacement map	*/
			if (canvas->output & MOD_DPAINT_OUT_DISP) {
				/* OpenEXR or PNG	*/
				int format = (canvas->disp_format & MOD_DPAINT_DISPFOR_OPENEXR) ? DPOUTPUT_OPENEXR : DPOUTPUT_PNG;
				char ext[4];
				if (canvas->disp_format & MOD_DPAINT_DISPFOR_OPENEXR) sprintf(ext,"exr"); else sprintf(ext,"png");
				sprintf(filename, "%s%s%s%i.%s", canvas->displace_output_path, disp, pad, (int)frame, ext);

				dynamic_paint_output_image(canvas, filename, format, DPOUTPUT_DISPLACE);
			}
		}
	}

	return 1;
}


/*
*	Updates baking status for every paint object in the scene
*	returns 0 if no paint objects found.
*/
static int DynamicPaint_PainterSetBaking(Scene *scene, short baking)
{
	Object *otherobj = NULL;
	ModifierData *md = NULL;
	int count = 0;

	Base *base = scene->base.first;

	while(base)
	{

		otherobj = base->object;

		if(!otherobj)					
		{												
			base= base->next;						
			continue;			
		}

		md = modifiers_findByType(otherobj, eModifierType_DynamicPaint);

		/* check if target has an active dp modifier */
		if(md && md->mode & (eModifierMode_Realtime | eModifierMode_Render))					
		{
			DynamicPaintModifierData *pmd2 = (DynamicPaintModifierData *)md;
			/* Make sure we're dealing with a painter */
			if((pmd2->type & MOD_DYNAMICPAINT_TYPE_PAINT) && pmd2->paint)
			{
				pmd2->baking = baking;
				count++;
			}
		}
		base= base->next;
	}

	if (count) return 1;
	
	return 0;
}


/*
*	An operator call to bake dynamic paint simulation for active object
*/
static int dynamic_paint_bake_all(bContext *C, wmOperator *op)
{

	DynamicPaintModifierData *pmd = NULL;
	Scene *scene = CTX_data_scene(C);
	Object *cObject = CTX_data_active_object(C);
	int status = 0;
	double timer = PIL_check_seconds_timer();

	/*
	*	Get modifier data
	*/
	pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
	if (!pmd) {printf("DynamicPaint bake failed: No Dynamic Paint modifier found.\n"); return 0;}

	/* Make sure we're dealing with a canvas */
	if (!pmd->canvas) {printf("DynamicPaint bake failed: Invalid canvas.\n"); return 0;}

	/*
	*	Set state to baking and init surface
	*/
	pmd->canvas->error[0] = '\0';
	pmd->baking = 1;

	if (DynamicPaint_PainterSetBaking(scene, 1)) {

		G.afbreek= 0;	/* reset blender_test_break*/

		/*  Bake Dynamic Paint	*/
		status = DynamicPaint_Bake(C, pmd, cObject);

		/*
		*	Clean bake stuff
		*/
		pmd->baking = 0;
		DynamicPaint_PainterSetBaking(scene, 0);
		dynamicPaint_cleanCanvasSurface(pmd->canvas);

		/* Restore cursor back to normal	*/
		WM_cursor_restore(CTX_wm_window(C));

	}
	else {
		dpError(pmd->canvas, "No paint objects.");
		status = 0;
	}

	/* Bake was successful:
	*  Report for ended bake and how long it took */
	if (status) {

		/* Format time string	*/
		char timestr[30];
		double time = PIL_check_seconds_timer() - timer;
		int tmp_val;
		timestr[0] = '\0';

		/* days (just in case someone actually has a very slow pc)	*/
		tmp_val = (int)floor(time / 86400.0f);
		if (tmp_val > 0) sprintf(timestr, "%i Day(s) - ", tmp_val);
		/* hours	*/
		time -= 86400.0f * tmp_val;
		tmp_val = (int)floor(time / 3600.0f);
		if (tmp_val > 0) sprintf(timestr, "%s%i h ", timestr, tmp_val);
		/* minutes	*/
		time -= 3600.0f * tmp_val;
		tmp_val = (int)floor(time / 60.0f);
		if (tmp_val > 0) sprintf(timestr, "%s%i min ", timestr, tmp_val);
		/* seconds	*/
		time -= 60.0f * tmp_val;
		tmp_val = (int)ceil(time);
		sprintf(timestr, "%s%i s", timestr, tmp_val);

		/* Show bake info */
		sprintf(pmd->canvas->ui_info, "Bake Complete! (Time: %s)", timestr);
		printf("%s\n", pmd->canvas->ui_info);
	}
	else {
		if (strlen(pmd->canvas->error)) { /* If an error occured */
			sprintf(pmd->canvas->ui_info, "Bake Failed: %s", pmd->canvas->error);
			BKE_report(op->reports, RPT_ERROR, pmd->canvas->ui_info);
		}
		else {	/* User cancelled the bake */
			sprintf(pmd->canvas->ui_info, "Baking Cancelled!");
			BKE_report(op->reports, RPT_WARNING, pmd->canvas->ui_info);
		}

		/* Print failed bake to console */
		printf("Baking Cancelled!\n");

	}

	return status;
}


/***************************** Operators ******************************/

static int dynamicpaint_bake_exec(bContext *C, wmOperator *op)
{

	/* Bake dynamic paint */
	if(!dynamic_paint_bake_all(C, op)) {
		return OPERATOR_CANCELLED;}

	return OPERATOR_FINISHED;
}

void DPAINT_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Dynamic Paint Bake";
	ot->description= "Bake dynamic paint";
	ot->idname= "DPAINT_OT_bake";
	
	/* api callbacks */
	ot->exec= dynamicpaint_bake_exec;
	ot->poll= ED_operator_object_active_editable;
}

