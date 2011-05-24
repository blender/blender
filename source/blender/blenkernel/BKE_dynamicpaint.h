/**
 * ***** BEGIN GPL LICENSE BLOCK *****
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

#ifndef BKE_DYNAMIC_PAINT_H_
#define BKE_DYNAMIC_PAINT_H_

typedef struct FaceAdv {
	float no[3];
	float no_q[3];
} FaceAdv;

typedef struct BB2d {
	float min[2], max[2];
} BB2d;

typedef struct Vec3f {
	float v[3];
} Vec3f;


/* Actual surface point	*/
typedef struct PaintSurfacePoint {
	/*
	*	Paint layer data
	*/
	float color[3];
	float alpha;
	float depth;	/* displacement */

	/*
	*	Effect / moving layer data
	*	! Only generated if effects enabled !
	*/
	int neighbours[8];	/* Indexes of 8 neighbouring pixels if exist */
	float neighbour_dist[8];	/*	Distances to all 8 neighbouring pixels */			
	float gravity_dir;	/* UV space direction of gravity */
	float gravity_rate;		/* Gravity strength. (Depends on surface angle.) */


	/* Wet paint is handled at effect layer only
	*  and mixed to surface when drying */
	float e_color[3];
	float e_alpha;
	float wetness;
	short state;	/* 0 = empty or dry
					*  1 = wet paint
					*  2 = new paint */


	/*
	*	Pixel / mesh data
	*/
	int index;			/* face index on domain derived mesh */
	int v1, v2, v3;		/* vertex indexes */

	int neighbour_pixel;	/* If this pixel isn't uv mapped to any face,
							but it's neighbouring pixel is */
	short quad;
	struct Vec3f *barycentricWeights;	/* b-weights for all pixel samples */
	float realCoord[3]; /* current pixel center world-space coordinates */
	float invNorm[3];  /*current pixel world-space inverted normal. depends on smooth/flat shading */

} PaintSurfacePoint;

typedef struct PaintSurface {
	
	struct PaintSurfacePoint *point;
	int w, h, active_points;
	short pixelSamples;
} PaintSurface;

void dynamicPaint_Modifier_do(struct DynamicPaintModifierData *pmd, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm);

void dynamicPaint_Modifier_free (struct DynamicPaintModifierData *pmd);
void dynamicPaint_Modifier_createType(struct DynamicPaintModifierData *pmd);
void dynamicPaint_Modifier_copy(struct DynamicPaintModifierData *pmd, struct DynamicPaintModifierData *tsmd);

#endif /* BKE_DYNAMIC_PAINT_H_ */
