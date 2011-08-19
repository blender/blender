/* linestyle.c
 *
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h" /* for ramp blend */
#include "DNA_texture_types.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_texture.h"
#include "BKE_colortools.h"
#include "BKE_animsys.h"

#include "BLI_blenlib.h"

static char *modifier_name[LS_MODIFIER_NUM] = {
	NULL,
	"Along Stroke",
	"Distance from Camera",
	"Distance from Object",
	"Material",
	"Sampling",
	"Bezier Curve",
	"Sinus Displacement",
	"Spatial Noise",
	"Perlin Noise 1D",
	"Perlin Noise 2D",
	"Backbone Stretcher",
	"Tip Remover"};

static void default_linestyle_settings(FreestyleLineStyle *linestyle)
{
	linestyle->panel = LS_PANEL_STROKES;
	linestyle->r = linestyle->g = linestyle->b = 0.0;
	linestyle->alpha = 1.0;
	linestyle->thickness = 1.0;

	linestyle->color_modifiers.first = linestyle->color_modifiers.last = NULL;
	linestyle->alpha_modifiers.first = linestyle->alpha_modifiers.last = NULL;
	linestyle->thickness_modifiers.first = linestyle->thickness_modifiers.last = NULL;
	linestyle->geometry_modifiers.first = linestyle->geometry_modifiers.last = NULL;

	FRS_add_linestyle_geometry_modifier(linestyle, LS_MODIFIER_SAMPLING);

	linestyle->caps = LS_CAPS_BUTT;
}

FreestyleLineStyle *FRS_new_linestyle(char *name, struct Main *main)
{
	FreestyleLineStyle *linestyle;

	if (!main)
		main = G.main;

	linestyle = (FreestyleLineStyle *)alloc_libblock(&main->linestyle, ID_LS, name);
	
	default_linestyle_settings(linestyle);

	return linestyle;
}

void FRS_free_linestyle(FreestyleLineStyle *linestyle)
{
	LineStyleModifier *m;

	BKE_free_animdata(&linestyle->id);
	while ((m = (LineStyleModifier *)linestyle->color_modifiers.first))
		FRS_remove_linestyle_color_modifier(linestyle, m);
	while ((m = (LineStyleModifier *)linestyle->alpha_modifiers.first))
		FRS_remove_linestyle_alpha_modifier(linestyle, m);
	while ((m = (LineStyleModifier *)linestyle->thickness_modifiers.first))
		FRS_remove_linestyle_thickness_modifier(linestyle, m);
	while ((m = (LineStyleModifier *)linestyle->geometry_modifiers.first))
		FRS_remove_linestyle_geometry_modifier(linestyle, m);
}

static LineStyleModifier *new_modifier(int type, size_t size)
{
	LineStyleModifier *m;

	m = (LineStyleModifier *)MEM_callocN(size, "line style modifier");
	if (m) {
		m->type = type;
		strcpy(m->name, modifier_name[type]);
		m->influence = 1.0f;
		m->flags = LS_MODIFIER_ENABLED | LS_MODIFIER_EXPANDED;
	}
	return m;
}

static void add_to_modifier_list(ListBase *lb, LineStyleModifier *m)
{
	BLI_addtail(lb, (void *)m);
	BLI_uniquename(lb, m, modifier_name[m->type], '.', BLI_STRUCT_OFFSET(LineStyleModifier, name), sizeof(m->name));
}

int FRS_add_linestyle_color_modifier(FreestyleLineStyle *linestyle, int type)
{
	size_t size;
	LineStyleModifier *m;

	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		size = sizeof(LineStyleColorModifier_AlongStroke);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		size = sizeof(LineStyleColorModifier_DistanceFromCamera);
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		size = sizeof(LineStyleColorModifier_DistanceFromObject);
		break;
	case LS_MODIFIER_MATERIAL:
		size = sizeof(LineStyleColorModifier_Material);
		break;
	default:
		return -1; /* unknown modifier type */
	}
	m = new_modifier(type, size);
	if (!m)
		return -1;
	m->blend = MA_RAMP_BLEND;
	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		((LineStyleColorModifier_AlongStroke *)m)->color_ramp = add_colorband(1);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp = add_colorband(1);
		((LineStyleColorModifier_DistanceFromCamera *)m)->range_min = 0.0f;
		((LineStyleColorModifier_DistanceFromCamera *)m)->range_max = 10000.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		((LineStyleColorModifier_DistanceFromObject *)m)->target = NULL;
		((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp = add_colorband(1);
		((LineStyleColorModifier_DistanceFromObject *)m)->range_min = 0.0f;
		((LineStyleColorModifier_DistanceFromObject *)m)->range_max = 10000.0f;
		break;
	case LS_MODIFIER_MATERIAL:
		((LineStyleColorModifier_Material *)m)->color_ramp = add_colorband(1);
		((LineStyleColorModifier_Material *)m)->mat_attr = LS_MODIFIER_MATERIAL_DIFF;
		break;
	default:
		return -1; /* unknown modifier type */
	}
	add_to_modifier_list(&linestyle->color_modifiers, m);

	return 0;
}

void FRS_remove_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
	switch (m->type) {
	case LS_MODIFIER_ALONG_STROKE:
		MEM_freeN(((LineStyleColorModifier_AlongStroke *)m)->color_ramp);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		MEM_freeN(((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp);
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		MEM_freeN(((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp);
		break;
	case LS_MODIFIER_MATERIAL:
		MEM_freeN(((LineStyleColorModifier_Material *)m)->color_ramp);
		break;
	}
	BLI_freelinkN(&linestyle->color_modifiers, m);
}

int FRS_add_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, int type)
{
	size_t size;
	LineStyleModifier *m;

	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		size = sizeof(LineStyleAlphaModifier_AlongStroke);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		size = sizeof(LineStyleAlphaModifier_DistanceFromCamera);
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		size = sizeof(LineStyleAlphaModifier_DistanceFromObject);
		break;
	case LS_MODIFIER_MATERIAL:
		size = sizeof(LineStyleAlphaModifier_Material);
		break;
	default:
		return -1; /* unknown modifier type */
	}
	m = new_modifier(type, size);
	if (!m)
		return -1;
	m->blend = LS_VALUE_BLEND;
	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		((LineStyleAlphaModifier_AlongStroke *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		((LineStyleAlphaModifier_DistanceFromCamera *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleAlphaModifier_DistanceFromCamera *)m)->range_min = 0.0f;
		((LineStyleAlphaModifier_DistanceFromCamera *)m)->range_max = 10000.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		((LineStyleAlphaModifier_DistanceFromObject *)m)->target = NULL;
		((LineStyleAlphaModifier_DistanceFromObject *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleAlphaModifier_DistanceFromObject *)m)->range_min = 0.0f;
		((LineStyleAlphaModifier_DistanceFromObject *)m)->range_max = 10000.0f;
		break;
	case LS_MODIFIER_MATERIAL:
		((LineStyleAlphaModifier_Material *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleAlphaModifier_Material *)m)->mat_attr = LS_MODIFIER_MATERIAL_DIFF;
		break;
	}
	add_to_modifier_list(&linestyle->alpha_modifiers, m);

	return 0;
}

void FRS_remove_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
	switch (m->type) {
	case LS_MODIFIER_ALONG_STROKE:
		curvemapping_free(((LineStyleAlphaModifier_AlongStroke *)m)->curve);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		curvemapping_free(((LineStyleAlphaModifier_DistanceFromCamera *)m)->curve);
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		curvemapping_free(((LineStyleAlphaModifier_DistanceFromObject *)m)->curve);
		break;
	case LS_MODIFIER_MATERIAL:
		curvemapping_free(((LineStyleAlphaModifier_Material *)m)->curve);
		break;
	}
	BLI_freelinkN(&linestyle->alpha_modifiers, m);
}

int FRS_add_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, int type)
{
	size_t size;
	LineStyleModifier *m;

	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		size = sizeof(LineStyleThicknessModifier_AlongStroke);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		size = sizeof(LineStyleThicknessModifier_DistanceFromCamera);
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		size = sizeof(LineStyleThicknessModifier_DistanceFromObject);
		break;
	case LS_MODIFIER_MATERIAL:
		size = sizeof(LineStyleThicknessModifier_Material);
		break;
	default:
		return -1; /* unknown modifier type */
	}
	m = new_modifier(type, size);
	if (!m)
		return -1;
	m->blend = LS_VALUE_BLEND;
	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		((LineStyleThicknessModifier_AlongStroke *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleThicknessModifier_AlongStroke *)m)->value_min = 0.0f;
		((LineStyleThicknessModifier_AlongStroke *)m)->value_max = 1.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->range_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->range_max = 1000.0f;
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->value_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->value_max = 1.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		((LineStyleThicknessModifier_DistanceFromObject *)m)->target = NULL;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleThicknessModifier_DistanceFromObject *)m)->range_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->range_max = 1000.0f;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->value_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->value_max = 1.0f;
		break;
	case LS_MODIFIER_MATERIAL:
		((LineStyleThicknessModifier_Material *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleThicknessModifier_Material *)m)->mat_attr = LS_MODIFIER_MATERIAL_DIFF;
		((LineStyleThicknessModifier_Material *)m)->value_min = 0.0f;
		((LineStyleThicknessModifier_Material *)m)->value_max = 1.0f;
		break;
	}
	add_to_modifier_list(&linestyle->thickness_modifiers, m);

	return 0;
}

void FRS_remove_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
	switch (m->type) {
	case LS_MODIFIER_ALONG_STROKE:
		curvemapping_free(((LineStyleThicknessModifier_AlongStroke *)m)->curve);
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		curvemapping_free(((LineStyleThicknessModifier_DistanceFromCamera *)m)->curve);
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		curvemapping_free(((LineStyleThicknessModifier_DistanceFromObject *)m)->curve);
		break;
	case LS_MODIFIER_MATERIAL:
		curvemapping_free(((LineStyleThicknessModifier_Material *)m)->curve);
		break;
	}
	BLI_freelinkN(&linestyle->thickness_modifiers, m);
}

int FRS_add_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, int type)
{
	size_t size;
	LineStyleModifier *m;

	switch (type) {
	case LS_MODIFIER_SAMPLING:
		size = sizeof(LineStyleGeometryModifier_Sampling);
		break;
	case LS_MODIFIER_BEZIER_CURVE:
		size = sizeof(LineStyleGeometryModifier_BezierCurve);
		break;
	case LS_MODIFIER_SINUS_DISPLACEMENT:
		size = sizeof(LineStyleGeometryModifier_SinusDisplacement);
		break;
	case LS_MODIFIER_SPATIAL_NOISE:
		size = sizeof(LineStyleGeometryModifier_SpatialNoise);
		break;
	case LS_MODIFIER_PERLIN_NOISE_1D:
		size = sizeof(LineStyleGeometryModifier_PerlinNoise1D);
		break;
	case LS_MODIFIER_PERLIN_NOISE_2D:
		size = sizeof(LineStyleGeometryModifier_PerlinNoise2D);
		break;
	case LS_MODIFIER_BACKBONE_STRETCHER:
		size = sizeof(LineStyleGeometryModifier_BackboneStretcher);
		break;
	case LS_MODIFIER_TIP_REMOVER:
		size = sizeof(LineStyleGeometryModifier_TipRemover);
		break;
	default:
		return -1; /* unknown modifier type */
	}
	m = new_modifier(type, size);
	if (!m)
		return -1;
	switch (type) {
	case LS_MODIFIER_SAMPLING:
		((LineStyleGeometryModifier_Sampling *)m)->sampling = 10.0;
		break;
	case LS_MODIFIER_BEZIER_CURVE:
		((LineStyleGeometryModifier_BezierCurve *)m)->error = 10.0;
		break;
	case LS_MODIFIER_SINUS_DISPLACEMENT:
		((LineStyleGeometryModifier_SinusDisplacement *)m)->wavelength = 20.0;
		((LineStyleGeometryModifier_SinusDisplacement *)m)->amplitude = 5.0;
		((LineStyleGeometryModifier_SinusDisplacement *)m)->phase = 0.0;
		break;
	case LS_MODIFIER_SPATIAL_NOISE:
		((LineStyleGeometryModifier_SpatialNoise *)m)->amplitude = 5.0;
		((LineStyleGeometryModifier_SpatialNoise *)m)->scale = 20.0;
		((LineStyleGeometryModifier_SpatialNoise *)m)->octaves = 4;
		((LineStyleGeometryModifier_SpatialNoise *)m)->flags = LS_MODIFIER_SPATIAL_NOISE_SMOOTH | LS_MODIFIER_SPATIAL_NOISE_PURERANDOM;
		break;
	case LS_MODIFIER_PERLIN_NOISE_1D:
		((LineStyleGeometryModifier_PerlinNoise1D *)m)->frequency = 10.0;
		((LineStyleGeometryModifier_PerlinNoise1D *)m)->amplitude = 10.0;
		((LineStyleGeometryModifier_PerlinNoise1D *)m)->octaves = 4;
		break;
	case LS_MODIFIER_PERLIN_NOISE_2D:
		((LineStyleGeometryModifier_PerlinNoise2D *)m)->frequency = 10.0;
		((LineStyleGeometryModifier_PerlinNoise2D *)m)->amplitude = 10.0;
		((LineStyleGeometryModifier_PerlinNoise2D *)m)->octaves = 4;
		break;
	case LS_MODIFIER_BACKBONE_STRETCHER:
		((LineStyleGeometryModifier_BackboneStretcher *)m)->amount = 10.0;
		break;
	case LS_MODIFIER_TIP_REMOVER:
		((LineStyleGeometryModifier_TipRemover *)m)->tip_length = 10.0;
		break;
	}
	add_to_modifier_list(&linestyle->geometry_modifiers, m);
	return 0;
}

void FRS_remove_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
	switch (m->type) {
	case LS_MODIFIER_SAMPLING:
		break;
	case LS_MODIFIER_BEZIER_CURVE:
		break;
	case LS_MODIFIER_SINUS_DISPLACEMENT:
		break;
	case LS_MODIFIER_SPATIAL_NOISE:
		break;
	case LS_MODIFIER_PERLIN_NOISE_1D:
		break;
	case LS_MODIFIER_PERLIN_NOISE_2D:
		break;
	case LS_MODIFIER_BACKBONE_STRETCHER:
		break;
	case LS_MODIFIER_TIP_REMOVER:
		break;
	}
	BLI_freelinkN(&linestyle->geometry_modifiers, m);
}

static void move_modifier(ListBase *lb, LineStyleModifier *modifier, int direction)
{
	BLI_remlink(lb, modifier);
	if (direction > 0)
		BLI_insertlinkbefore(lb, modifier->prev, modifier);
	else
		BLI_insertlinkafter(lb, modifier->next, modifier);
}

void FRS_move_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction)
{
	move_modifier(&linestyle->color_modifiers, modifier, direction);
}

void FRS_move_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction)
{
	move_modifier(&linestyle->alpha_modifiers, modifier, direction);
}

void FRS_move_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction)
{
	move_modifier(&linestyle->thickness_modifiers, modifier, direction);
}

void FRS_move_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction)
{
	move_modifier(&linestyle->geometry_modifiers, modifier, direction);
}

void FRS_list_modifier_color_ramps(FreestyleLineStyle *linestyle, ListBase *listbase)
{
	LineStyleModifier *m;
	ColorBand *color_ramp;
	LinkData *link;

	listbase->first = listbase->last = NULL;
	for (m = (LineStyleModifier *)linestyle->color_modifiers.first; m; m = m->next) {
		switch (m->type) {
		case LS_MODIFIER_ALONG_STROKE:
			color_ramp = ((LineStyleColorModifier_AlongStroke *)m)->color_ramp;
			break;
		case LS_MODIFIER_DISTANCE_FROM_CAMERA:
			color_ramp = ((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp;
			break;
		case LS_MODIFIER_DISTANCE_FROM_OBJECT:
			color_ramp = ((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp;
			break;
		case LS_MODIFIER_MATERIAL:
			color_ramp = ((LineStyleColorModifier_Material *)m)->color_ramp;
			break;
		default:
			continue;
		}
		link = (LinkData *) MEM_callocN( sizeof(LinkData), "link to color ramp");
		link->data = color_ramp;
		BLI_addtail(listbase, link);
	}
}

char *FRS_path_from_ID_to_color_ramp(FreestyleLineStyle *linestyle, ColorBand *color_ramp)
{
	LineStyleModifier *m;

	for (m = (LineStyleModifier *)linestyle->color_modifiers.first; m; m = m->next) {
		switch (m->type) {
		case LS_MODIFIER_ALONG_STROKE:
			if (color_ramp == ((LineStyleColorModifier_AlongStroke *)m)->color_ramp)
				goto found;
			break;
		case LS_MODIFIER_DISTANCE_FROM_CAMERA:
			if (color_ramp == ((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp)
				goto found;
			break;
		case LS_MODIFIER_DISTANCE_FROM_OBJECT:
			if (color_ramp == ((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp)
				goto found;
			break;
		case LS_MODIFIER_MATERIAL:
			if (color_ramp == ((LineStyleColorModifier_Material *)m)->color_ramp)
				goto found;
			break;
		}
	}
	printf("FRS_path_from_ID_to_color_ramp: No color ramps correspond to the given pointer.\n");
	return NULL;

found:
	return BLI_sprintfN("color_modifiers[\"%s\"].color_ramp", m->name);
}

void FRS_unlink_linestyle_target_object(FreestyleLineStyle *linestyle, struct Object *ob)
{
	LineStyleModifier *m;

	for (m = (LineStyleModifier *)linestyle->color_modifiers.first; m; m = m->next) {
		if (m->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
			if (((LineStyleColorModifier_DistanceFromObject *)m)->target == ob) {
				((LineStyleColorModifier_DistanceFromObject *)m)->target = NULL;
			}
		}
	}
	for (m = (LineStyleModifier *)linestyle->alpha_modifiers.first; m; m = m->next) {
		if (m->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
			if (((LineStyleAlphaModifier_DistanceFromObject *)m)->target == ob) {
				((LineStyleAlphaModifier_DistanceFromObject *)m)->target = NULL;
			}
		}
	}
	for (m = (LineStyleModifier *)linestyle->thickness_modifiers.first; m; m = m->next) {
		if (m->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
			if (((LineStyleThicknessModifier_DistanceFromObject *)m)->target == ob) {
				((LineStyleThicknessModifier_DistanceFromObject *)m)->target = NULL;
			}
		}
	}
}
