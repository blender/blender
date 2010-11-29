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
	"Distance from Object"};

static void default_linestyle_settings(FreestyleLineStyle *linestyle)
{
	linestyle->panel = LS_PANEL_COLOR;
	linestyle->r = linestyle->g = linestyle->b = 0.0;
	linestyle->alpha = 1.0;
	linestyle->thickness = 1.0;

	linestyle->color_modifiers.first = linestyle->color_modifiers.last = NULL;
	linestyle->alpha_modifiers.first = linestyle->alpha_modifiers.last = NULL;
	linestyle->thickness_modifiers.first = linestyle->thickness_modifiers.last = NULL;

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
	static size_t modifier_size[LS_MODIFIER_NUM] = {
		0,
		sizeof(LineStyleColorModifier_AlongStroke),
		sizeof(LineStyleColorModifier_DistanceFromCamera),
		sizeof(LineStyleColorModifier_DistanceFromObject)
	};
	LineStyleModifier *m;

	if (type <= 0 || type >= LS_MODIFIER_NUM || modifier_size[type] == 0)
		return -1;
	m = new_modifier(type, modifier_size[type]);
	if (!m)
		return -1;
	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		((LineStyleColorModifier_AlongStroke *)m)->color_ramp = add_colorband(1);
		((LineStyleColorModifier_AlongStroke *)m)->blend = MA_RAMP_BLEND;
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp = add_colorband(1);
		((LineStyleColorModifier_DistanceFromCamera *)m)->blend = MA_RAMP_BLEND;
		((LineStyleColorModifier_DistanceFromCamera *)m)->range_min = 0.0f;
		((LineStyleColorModifier_DistanceFromCamera *)m)->range_max = 10000.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		((LineStyleColorModifier_DistanceFromObject *)m)->target = NULL;
		((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp = add_colorband(1);
		((LineStyleColorModifier_DistanceFromObject *)m)->blend = MA_RAMP_BLEND;
		((LineStyleColorModifier_DistanceFromObject *)m)->range_min = 0.0f;
		((LineStyleColorModifier_DistanceFromObject *)m)->range_max = 10000.0f;
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
	}
	BLI_freelinkN(&linestyle->color_modifiers, m);
}

int FRS_add_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, int type)
{
	static size_t modifier_size[LS_MODIFIER_NUM] = {
		0,
		sizeof(LineStyleAlphaModifier_AlongStroke),
		sizeof(LineStyleAlphaModifier_DistanceFromCamera),
		sizeof(LineStyleAlphaModifier_DistanceFromObject)
	};
	LineStyleModifier *m;

	if (type <= 0 || type >= LS_MODIFIER_NUM || modifier_size[type] == 0)
		return -1;
	m = new_modifier(type, modifier_size[type]);
	if (!m)
		return -1;
	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		((LineStyleAlphaModifier_AlongStroke *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleAlphaModifier_AlongStroke *)m)->blend = LS_VALUE_BLEND;
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		((LineStyleAlphaModifier_DistanceFromCamera *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleAlphaModifier_DistanceFromCamera *)m)->blend = LS_VALUE_BLEND;
		((LineStyleAlphaModifier_DistanceFromCamera *)m)->range_min = 0.0f;
		((LineStyleAlphaModifier_DistanceFromCamera *)m)->range_max = 10000.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		((LineStyleAlphaModifier_DistanceFromObject *)m)->target = NULL;
		((LineStyleAlphaModifier_DistanceFromObject *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleAlphaModifier_DistanceFromObject *)m)->blend = LS_VALUE_BLEND;
		((LineStyleAlphaModifier_DistanceFromObject *)m)->range_min = 0.0f;
		((LineStyleAlphaModifier_DistanceFromObject *)m)->range_max = 10000.0f;
		break;
	default:
		return -1; /* unknown modifier type */
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
	}
	BLI_freelinkN(&linestyle->alpha_modifiers, m);
}

int FRS_add_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, int type)
{
	static size_t modifier_size[LS_MODIFIER_NUM] = {
		0,
		sizeof(LineStyleThicknessModifier_AlongStroke),
		sizeof(LineStyleThicknessModifier_DistanceFromCamera),
		sizeof(LineStyleThicknessModifier_DistanceFromObject)
	};
	LineStyleModifier *m;

	if (type <= 0 || type >= LS_MODIFIER_NUM || modifier_size[type] == 0)
		return -1;
	m = new_modifier(type, modifier_size[type]);
	if (!m)
		return -1;
	switch (type) {
	case LS_MODIFIER_ALONG_STROKE:
		((LineStyleThicknessModifier_AlongStroke *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleThicknessModifier_AlongStroke *)m)->blend = LS_VALUE_BLEND;
		((LineStyleThicknessModifier_AlongStroke *)m)->value_min = 0.0f;
		((LineStyleThicknessModifier_AlongStroke *)m)->value_max = 1.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->blend = LS_VALUE_BLEND;
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->range_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->range_max = 1000.0f;
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->value_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromCamera *)m)->value_max = 1.0f;
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		((LineStyleThicknessModifier_DistanceFromObject *)m)->target = NULL;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->curve = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		((LineStyleThicknessModifier_DistanceFromObject *)m)->blend = LS_VALUE_BLEND;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->range_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->range_max = 1000.0f;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->value_min = 0.0f;
		((LineStyleThicknessModifier_DistanceFromObject *)m)->value_max = 1.0f;
		break;
	default:
		return -1; /* unknown modifier type */
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
	}
	BLI_freelinkN(&linestyle->thickness_modifiers, m);
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
