/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with Actions.
 */

#include "RNA_types.hh"

struct FCurve;
struct bAction;

namespace blender::animrig {
/**
 * Get (or add relevant data to be able to do so) F-Curve from the given Action,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 */
FCurve *action_fcurve_ensure(Main *bmain,
                             bAction *act,
                             const char group[],
                             PointerRNA *ptr,
                             const char rna_path[],
                             int array_index);

/**
 * Find the F-Curve from the given Action. This assumes that all the destinations are valid.
 */
FCurve *action_fcurve_find(bAction *act, const char rna_path[], int array_index);

}  // namespace blender::animrig
