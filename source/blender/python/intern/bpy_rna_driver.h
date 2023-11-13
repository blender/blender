/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pythonintern
 */

struct ChannelDriver;
struct DriverTarget;
struct PathResolvedRNA;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A version of #driver_get_variable_value which returns a #PyObject.
 */
PyObject *pyrna_driver_get_variable_value(const struct AnimationEvalContext *anim_eval_context,
                                          struct ChannelDriver *driver,
                                          struct DriverVar *dvar,
                                          struct DriverTarget *dtar);

PyObject *pyrna_driver_self_from_anim_rna(struct PathResolvedRNA *anim_rna);
bool pyrna_driver_is_equal_anim_rna(const struct PathResolvedRNA *anim_rna,
                                    const PyObject *py_anim_rna);

#ifdef __cplusplus
}
#endif
