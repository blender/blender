/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_curve_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimationEvalContext;
struct ChannelDriver;
struct DriverTarget;
struct DriverVar;
struct FCurve;
struct PathResolvedRNA;
struct PointerRNA;
struct PropertyRNA;
struct Scene;
struct ViewLayer;

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

/**
 * This frees the driver itself.
 */
void fcurve_free_driver(struct FCurve *fcu);
/**
 * This makes a copy of the given driver.
 */
struct ChannelDriver *fcurve_copy_driver(const struct ChannelDriver *driver);

/**
 * Get property from which the specific property can be found from.
 *
 * This depends on the type of `dvar`:
 *
 *   - For the Single Property the `r_prop` is a pointer to an ID, which is used to resolve the
 *     target rna_path.
 *
 *   - For Transform Channel, Rotational Difference, Distance the `r_prop` is a pointer to an
 *     object from which transformation is read.
 *
 *   - For Context Property the `r_prop` points to a resolved data corresponding to the
 *     dtar->context_property accessed from the given evaluated context. This could either be an ID
 *     property for Active Scene, or a data property for Active View Layer.
 *
 * If the target property can not be resolved false is returned.
 */
typedef struct DriverTargetContext {
  struct Scene *scene;
  struct ViewLayer *view_layer;
} DriverTargetContext;
bool driver_get_target_property(const DriverTargetContext *driver_target_context,
                                struct DriverVar *dvar,
                                struct DriverTarget *dtar,
                                struct PointerRNA *r_prop);

/**
 * Copy driver variables from src_vars list to dst_vars list.
 */
void driver_variables_copy(struct ListBase *dst_vars, const struct ListBase *src_vars);

/**
 * Compute channel values for a rotational Transform Channel driver variable.
 */
void BKE_driver_target_matrix_to_rot_channels(
    float mat[4][4], int auto_order, int rotation_mode, int channel, bool angles, float r_buf[4]);

/**
 * Perform actual freeing driver variable and remove it from the given list.
 */
void driver_free_variable(struct ListBase *variables, struct DriverVar *dvar);
/**
 * Free the driver variable and do extra updates.
 */
void driver_free_variable_ex(struct ChannelDriver *driver, struct DriverVar *dvar);

/**
 * Change the type of driver variable.
 */
void driver_change_variable_type(struct DriverVar *dvar, int type);
/**
 * Validate driver variable name (after being renamed).
 */
void driver_variable_name_validate(struct DriverVar *dvar);
/**
 * Ensure the driver variable's name is unique.
 *
 * Assumes the driver variable has already been assigned to the driver, so that
 * the `prev/next` pointers can be used to find the other variables.
 */
void driver_variable_unique_name(struct DriverVar *dvar);
/**
 * Add a new driver variable.
 */
struct DriverVar *driver_add_new_variable(struct ChannelDriver *driver);

/**
 * Evaluate a Driver Variable to get a value that contributes to the final.
 */
float driver_get_variable_value(const struct AnimationEvalContext *anim_eval_context,
                                struct ChannelDriver *driver,
                                struct DriverVar *dvar);

typedef enum eDriverVariablePropertyResult {
  /** The property reference has been successfully resolved and can be accessed. */
  DRIVER_VAR_PROPERTY_SUCCESS,
  /** Evaluation should use the fallback value. */
  DRIVER_VAR_PROPERTY_FALLBACK,
  /** The target property could not be resolved. */
  DRIVER_VAR_PROPERTY_INVALID,
  /** The property was resolved (output parameters are set),
   *  but the array index is out of bounds. */
  DRIVER_VAR_PROPERTY_INVALID_INDEX
} eDriverVariablePropertyResult;

/**
 * Same as 'dtar_get_prop_val'. but get the RNA property.
 */
eDriverVariablePropertyResult driver_get_variable_property(
    const struct AnimationEvalContext *anim_eval_context,
    struct ChannelDriver *driver,
    struct DriverVar *dvar,
    struct DriverTarget *dtar,
    bool allow_no_index,
    struct PointerRNA *r_ptr,
    struct PropertyRNA **r_prop,
    int *r_index);

/**
 * Check if the expression in the driver conforms to the simple subset.
 */
bool BKE_driver_has_simple_expression(struct ChannelDriver *driver);
/**
 * Check if the expression in the driver may depend on the current frame.
 */
bool BKE_driver_expression_depends_on_time(struct ChannelDriver *driver);
/**
 * Reset cached compiled expression data.
 */
void BKE_driver_invalidate_expression(struct ChannelDriver *driver,
                                      bool expr_changed,
                                      bool varname_changed);

/**
 * Evaluate an Channel-Driver to get a 'time' value to use
 * instead of `anim_eval_context->eval_time`.
 *
 * - `anim_eval_context->eval_time` is the frame at which F-Curve is being evaluated.
 * - Has to return a float value.
 * - \a driver_orig is where we cache Python expressions, in case of copy-on-eval
 */
float evaluate_driver(struct PathResolvedRNA *anim_rna,
                      struct ChannelDriver *driver,
                      struct ChannelDriver *driver_orig,
                      const struct AnimationEvalContext *anim_eval_context);

#ifdef __cplusplus
}
#endif
