/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to modify FCurves.
 */

#include "RNA_types.hh"

struct AnimData;
struct FCurve;
struct bAction;

namespace blender::animrig {

/**
 * Get (or add relevant data to be able to do so) F-Curve from the given Action,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 */
FCurve *ED_action_fcurve_ensure(Main *bmain,
                                bAction *act,
                                const char group[],
                                PointerRNA *ptr,
                                const char rna_path[],
                                int array_index);

/**
 * Find the F-Curve from the given Action. This assumes that all the destinations are valid.
 */
FCurve *ED_action_fcurve_find(bAction *act, const char rna_path[], int array_index);

/**
 * \note The caller needs to run #BKE_nla_tweakedit_remap to get NLA relative frame.
 *       The caller should also check #BKE_fcurve_is_protected before keying.
 */
bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra);

}  // namespace blender::animrig
