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

#include "DNA_dynamicpaint_types.h"

/* Actual surface point	*/
typedef struct PaintSurfaceData {
	/* surface format data */
	void *format_data;
	/* surface type data */
	void *type_data;

	unsigned int total_points;
	short samples;

} PaintSurfaceData;

/* Paint type surface point	*/
typedef struct PaintPoint {

	/* Wet paint is handled at effect layer only
	*  and mixed to surface when drying */
	float e_color[3];
	float e_alpha;
	float wetness;
	short state;	/* -1 = doesn't exist (On UV mapped image
					*	    there can be points that doesn't exist on mesh surface)
					*  0 = empty or dry
					*  1 = wet paint
					*  2 = new paint */
	float color[3];
	float alpha;
} PaintPoint;

/* iWave type surface point	*/
typedef struct PaintIWavePoint {		

	float source;
	float obstruction;
	float height, previousHeight;

	float foam;

	float verticalDerivative;

} PaintIWavePoint;

struct DerivedMesh *dynamicPaint_Modifier_do(struct DynamicPaintModifierData *pmd, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm);
void dynamicPaint_cacheUpdateFrames(struct DynamicPaintSurface *surface);
int dynamicPaint_resetSurface(struct DynamicPaintSurface *surface);
int dynamicPaint_surfaceHasPreview(DynamicPaintSurface *surface);
void dynamicPaintSurface_updateType(struct DynamicPaintSurface *surface);
void dynamicPaintSurface_setUniqueName(DynamicPaintSurface *surface, char *basename);
void dynamicPaint_Modifier_free (struct DynamicPaintModifierData *pmd);
void dynamicPaint_Modifier_createType(struct DynamicPaintModifierData *pmd);
void dynamicPaint_Modifier_copy(struct DynamicPaintModifierData *pmd, struct DynamicPaintModifierData *tsmd);

#endif /* BKE_DYNAMIC_PAINT_H_ */
