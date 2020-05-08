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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

#ifndef __BKE_FCURVE_DRIVER_H__
#define __BKE_FCURVE_DRIVER_H__

/** \file
 * \ingroup bke
 */

#include "DNA_curve_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ChannelDriver;
struct DriverTarget;
struct DriverVar;
struct FCurve;
struct PathResolvedRNA;
struct PointerRNA;
struct PropertyRNA;

/* ************** F-Curve Drivers ***************** */

/* With these iterators for convenience, the variables "tarIndex" and "dtar" can be
 * accessed directly from the code using them, but it is not recommended that their
 * values be changed to point at other slots...
 */

/* convenience looper over ALL driver targets for a given variable (even the unused ones) */
#define DRIVER_TARGETS_LOOPER_BEGIN(dvar) \
  { \
    DriverTarget *dtar = &dvar->targets[0]; \
    int tarIndex = 0; \
    for (; tarIndex < MAX_DRIVER_TARGETS; tarIndex++, dtar++)

/* convenience looper over USED driver targets only */
#define DRIVER_TARGETS_USED_LOOPER_BEGIN(dvar) \
  { \
    DriverTarget *dtar = &dvar->targets[0]; \
    int tarIndex = 0; \
    for (; tarIndex < dvar->num_targets; tarIndex++, dtar++)

/* tidy up for driver targets loopers */
#define DRIVER_TARGETS_LOOPER_END \
  } \
  ((void)0)

/* ---------------------- */

void fcurve_free_driver(struct FCurve *fcu);
struct ChannelDriver *fcurve_copy_driver(const struct ChannelDriver *driver);

void driver_variables_copy(struct ListBase *dst_vars, const struct ListBase *src_vars);

void BKE_driver_target_matrix_to_rot_channels(
    float mat[4][4], int auto_order, int rotation_mode, int channel, bool angles, float r_buf[4]);

void driver_free_variable(struct ListBase *variables, struct DriverVar *dvar);
void driver_free_variable_ex(struct ChannelDriver *driver, struct DriverVar *dvar);

void driver_change_variable_type(struct DriverVar *dvar, int type);
void driver_variable_name_validate(struct DriverVar *dvar);
struct DriverVar *driver_add_new_variable(struct ChannelDriver *driver);

float driver_get_variable_value(struct ChannelDriver *driver, struct DriverVar *dvar);
bool driver_get_variable_property(struct ChannelDriver *driver,
                                  struct DriverTarget *dtar,
                                  struct PointerRNA *r_ptr,
                                  struct PropertyRNA **r_prop,
                                  int *r_index);

bool BKE_driver_has_simple_expression(struct ChannelDriver *driver);
bool BKE_driver_expression_depends_on_time(struct ChannelDriver *driver);
void BKE_driver_invalidate_expression(struct ChannelDriver *driver,
                                      bool expr_changed,
                                      bool varname_changed);

float evaluate_driver(struct PathResolvedRNA *anim_rna,
                      struct ChannelDriver *driver,
                      struct ChannelDriver *driver_orig,
                      const float evaltime);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_FCURVE_DRIVER_H__*/
