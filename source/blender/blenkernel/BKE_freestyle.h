/*
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
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct FreestyleConfig;
struct FreestyleLineSet;
struct FreestyleModuleConfig;
struct Main;

/* RNA aliases */
typedef struct FreestyleModuleSettings FreestyleModuleSettings;
typedef struct FreestyleSettings FreestyleSettings;

/* FreestyleConfig */
void BKE_freestyle_config_init(struct FreestyleConfig *config);
void BKE_freestyle_config_free(struct FreestyleConfig *config, const bool do_id_user);
void BKE_freestyle_config_copy(struct FreestyleConfig *new_config,
                               const struct FreestyleConfig *config,
                               const int flag);

/* FreestyleConfig.modules */
struct FreestyleModuleConfig *BKE_freestyle_module_add(struct FreestyleConfig *config);
bool BKE_freestyle_module_delete(struct FreestyleConfig *config,
                                 struct FreestyleModuleConfig *module_conf);
bool BKE_freestyle_module_move(struct FreestyleConfig *config,
                               struct FreestyleModuleConfig *module_conf,
                               int direction);

/* FreestyleConfig.linesets */
struct FreestyleLineSet *BKE_freestyle_lineset_add(struct Main *bmain,
                                                   struct FreestyleConfig *config,
                                                   const char *name);
bool BKE_freestyle_lineset_delete(struct FreestyleConfig *config,
                                  struct FreestyleLineSet *lineset);
struct FreestyleLineSet *BKE_freestyle_lineset_get_active(struct FreestyleConfig *config);
short BKE_freestyle_lineset_get_active_index(struct FreestyleConfig *config);
void BKE_freestyle_lineset_set_active_index(struct FreestyleConfig *config, short index);
void BKE_freestyle_lineset_unique_name(struct FreestyleConfig *config,
                                       struct FreestyleLineSet *lineset);

#ifdef __cplusplus
}
#endif
