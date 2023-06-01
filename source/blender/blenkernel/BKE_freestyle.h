/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
void BKE_freestyle_config_free(struct FreestyleConfig *config, bool do_id_user);
void BKE_freestyle_config_copy(struct FreestyleConfig *new_config,
                               const struct FreestyleConfig *config,
                               int flag);

/* FreestyleConfig.modules */
struct FreestyleModuleConfig *BKE_freestyle_module_add(struct FreestyleConfig *config);
bool BKE_freestyle_module_delete(struct FreestyleConfig *config,
                                 struct FreestyleModuleConfig *module_conf);
/**
 * Reinsert \a module_conf offset by \a direction from current position.
 * \return if position of \a module_conf changed.
 */
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
