/*
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
 * The Original Code is Copyright (C) 2013 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/freestyle.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_freestyle_types.h"
#include "DNA_group_types.h"

#include "BKE_freestyle.h"
#include "BKE_linestyle.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

// function declarations
static FreestyleLineSet *alloc_lineset(void);
static void copy_lineset(FreestyleLineSet *new_lineset, FreestyleLineSet *lineset);
static FreestyleModuleConfig *alloc_module(void);
static void copy_module(FreestyleModuleConfig *new_module, FreestyleModuleConfig *module);

void BKE_freestyle_config_init(FreestyleConfig *config)
{
	config->mode = FREESTYLE_CONTROL_EDITOR_MODE;

	BLI_listbase_clear(&config->modules);
	config->flags = 0;
	config->sphere_radius = 0.1f;
	config->dkr_epsilon = 0.0f;
	config->crease_angle = DEG2RADF(134.43f);

	BLI_listbase_clear(&config->linesets);
}

void BKE_freestyle_config_free(FreestyleConfig *config)
{
	FreestyleLineSet *lineset;

	for (lineset = (FreestyleLineSet *)config->linesets.first; lineset; lineset = lineset->next) {
		if (lineset->group) {
			lineset->group->id.us--;
			lineset->group = NULL;
		}
		if (lineset->linestyle) {
			lineset->linestyle->id.us--;
			lineset->linestyle = NULL;
		}
	}
	BLI_freelistN(&config->linesets);
	BLI_freelistN(&config->modules);
}

void BKE_freestyle_config_copy(FreestyleConfig *new_config, FreestyleConfig *config)
{
	FreestyleLineSet *lineset, *new_lineset;
	FreestyleModuleConfig *module, *new_module;

	new_config->mode = config->mode;
	new_config->flags = config->flags;
	new_config->sphere_radius = config->sphere_radius;
	new_config->dkr_epsilon = config->dkr_epsilon;
	new_config->crease_angle = config->crease_angle;

	BLI_listbase_clear(&new_config->linesets);
	for (lineset = (FreestyleLineSet *)config->linesets.first; lineset; lineset = lineset->next) {
		new_lineset = alloc_lineset();
		copy_lineset(new_lineset, lineset);
		BLI_addtail(&new_config->linesets, (void *)new_lineset);
	}

	BLI_listbase_clear(&new_config->modules);
	for (module = (FreestyleModuleConfig *)config->modules.first; module; module = module->next) {
		new_module = alloc_module();
		copy_module(new_module, module);
		BLI_addtail(&new_config->modules, (void *)new_module);
	}
}

static void copy_lineset(FreestyleLineSet *new_lineset, FreestyleLineSet *lineset)
{
	new_lineset->linestyle = lineset->linestyle;
	if (new_lineset->linestyle)
		new_lineset->linestyle->id.us++;
	new_lineset->flags = lineset->flags;
	new_lineset->selection = lineset->selection;
	new_lineset->qi = lineset->qi;
	new_lineset->qi_start = lineset->qi_start;
	new_lineset->qi_end = lineset->qi_end;
	new_lineset->edge_types = lineset->edge_types;
	new_lineset->exclude_edge_types = lineset->exclude_edge_types;
	new_lineset->group = lineset->group;
	if (new_lineset->group) {
		new_lineset->group->id.us++;
	}
	strcpy(new_lineset->name, lineset->name);
}

static FreestyleModuleConfig *alloc_module(void)
{
	return (FreestyleModuleConfig *)MEM_callocN(sizeof(FreestyleModuleConfig), "style module configuration");
}

void BKE_freestyle_module_add(FreestyleConfig *config)
{
	FreestyleModuleConfig *module_conf = alloc_module();
	BLI_addtail(&config->modules, (void *)module_conf);
	module_conf->script = NULL;
	module_conf->is_displayed = 1;
}

static void copy_module(FreestyleModuleConfig *new_module, FreestyleModuleConfig *module)
{
	new_module->script = module->script;
	new_module->is_displayed = module->is_displayed;
}

void BKE_freestyle_module_delete(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
{
	BLI_freelinkN(&config->modules, module_conf);
}

void BKE_freestyle_module_move_up(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
{
	BLI_remlink(&config->modules, module_conf);
	BLI_insertlinkbefore(&config->modules, module_conf->prev, module_conf);
}

void BKE_freestyle_module_move_down(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
{
	BLI_remlink(&config->modules, module_conf);
	BLI_insertlinkafter(&config->modules, module_conf->next, module_conf);
}

void BKE_freestyle_lineset_unique_name(FreestyleConfig *config, FreestyleLineSet *lineset)
{
	BLI_uniquename(&config->linesets, lineset, "FreestyleLineSet", '.', offsetof(FreestyleLineSet, name),
	               sizeof(lineset->name));
}

static FreestyleLineSet *alloc_lineset(void)
{
	return (FreestyleLineSet *)MEM_callocN(sizeof(FreestyleLineSet), "Freestyle line set");
}

FreestyleLineSet *BKE_freestyle_lineset_add(FreestyleConfig *config)
{
	int lineset_index = BLI_countlist(&config->linesets);

	FreestyleLineSet *lineset = alloc_lineset();
	BLI_addtail(&config->linesets, (void *)lineset);
	BKE_freestyle_lineset_set_active_index(config, lineset_index);

	lineset->linestyle = BKE_new_linestyle("LineStyle", NULL);
	lineset->flags |= FREESTYLE_LINESET_ENABLED;
	lineset->selection = FREESTYLE_SEL_VISIBILITY | FREESTYLE_SEL_EDGE_TYPES | FREESTYLE_SEL_IMAGE_BORDER;
	lineset->qi = FREESTYLE_QI_VISIBLE;
	lineset->qi_start = 0;
	lineset->qi_end = 100;
	lineset->edge_types = FREESTYLE_FE_SILHOUETTE | FREESTYLE_FE_BORDER | FREESTYLE_FE_CREASE;
	lineset->exclude_edge_types = 0;
	lineset->group = NULL;
	if (lineset_index > 0)
		sprintf(lineset->name, "LineSet %i", lineset_index + 1);
	else
		strcpy(lineset->name, "LineSet");
	BKE_freestyle_lineset_unique_name(config, lineset);

	return lineset;
}

FreestyleLineSet *BKE_freestyle_lineset_get_active(FreestyleConfig *config)
{
	FreestyleLineSet *lineset;

	for (lineset = (FreestyleLineSet *)config->linesets.first; lineset; lineset = lineset->next) {
		if (lineset->flags & FREESTYLE_LINESET_CURRENT)
			return lineset;
	}
	return NULL;
}

short BKE_freestyle_lineset_get_active_index(FreestyleConfig *config)
{
	FreestyleLineSet *lineset;
	short i;

	for (lineset = (FreestyleLineSet *)config->linesets.first, i = 0; lineset; lineset = lineset->next, i++) {
		if (lineset->flags & FREESTYLE_LINESET_CURRENT)
			return i;
	}
	return 0;
}

void BKE_freestyle_lineset_set_active_index(FreestyleConfig *config, short index)
{
	FreestyleLineSet *lineset;
	short i;

	for (lineset = (FreestyleLineSet *)config->linesets.first, i = 0; lineset; lineset = lineset->next, i++) {
		if (i == index)
			lineset->flags |= FREESTYLE_LINESET_CURRENT;
		else
			lineset->flags &= ~FREESTYLE_LINESET_CURRENT;
	}
}
