/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

struct Object;

namespace io::hydra {

struct EmittedObject;
struct PopulateContext;

/** Emit a volume Rprim for either a volume object or fluid modifier. */
bool emit_volume_object(PopulateContext &ctx, const Object *object, EmittedObject &emitted);

/** Emit a per-dupli volume prim. Volumes aren't gprims and don't go
 * through the instancer prototype mechanism. */
void emit_volume_dupli(PopulateContext &ctx, const Object *source, const float dupli_mat[4][4]);

}  // namespace io::hydra
}  // namespace blender
