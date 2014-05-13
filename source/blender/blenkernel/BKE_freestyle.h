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

#ifndef __BKE_FREESTYLE_H__
#define __BKE_FREESTYLE_H__

/** \file BKE_freestyle.h
 *  \ingroup bke
 */

#include "DNA_scene_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FreestyleConfig;
struct FreestyleLineSet;
struct FreestyleModuleConfig;

/* RNA aliases */
typedef struct FreestyleSettings FreestyleSettings;
typedef struct FreestyleModuleSettings FreestyleModuleSettings;

/* FreestyleConfig */
void BKE_freestyle_config_init(FreestyleConfig *config);
void BKE_freestyle_config_free(FreestyleConfig *config);
void BKE_freestyle_config_copy(FreestyleConfig *new_config, FreestyleConfig *config);

/* FreestyleConfig.modules */
FreestyleModuleConfig *BKE_freestyle_module_add(FreestyleConfig *config);
bool BKE_freestyle_module_delete(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
bool BKE_freestyle_module_move_up(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
bool BKE_freestyle_module_move_down(FreestyleConfig *config, FreestyleModuleConfig *module_conf);

/* FreestyleConfig.linesets */
FreestyleLineSet *BKE_freestyle_lineset_add(FreestyleConfig *config, const char *name);
bool BKE_freestyle_lineset_delete(FreestyleConfig *config, FreestyleLineSet *lineset);
FreestyleLineSet *BKE_freestyle_lineset_get_active(FreestyleConfig *config);
short BKE_freestyle_lineset_get_active_index(FreestyleConfig *config);
void BKE_freestyle_lineset_set_active_index(FreestyleConfig *config, short index);
void BKE_freestyle_lineset_unique_name(FreestyleConfig *config, FreestyleLineSet *lineset);

#ifdef __cplusplus
}
#endif

#endif

