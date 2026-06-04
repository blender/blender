/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

struct Object;

namespace io::hydra {

struct BObjectInfo;
struct EmittedObject;
struct PopulateContext;

/* Emit a curves object as an Rprim. */
void emit_curves_object(PopulateContext &ctx, const BObjectInfo &info, EmittedObject &emitted);
/* Emit a curves instancer prototype. */
void emit_curves_proto(PopulateContext &ctx, const BObjectInfo &info);

/* Emit particular hair as an Rprim. */
void emit_hair_for_object(PopulateContext &ctx, Object *object, EmittedObject &emitted);
/* Emit a hair instancer prototype. */
void emit_hair_proto(PopulateContext &ctx, Object *source);

}  // namespace io::hydra
}  // namespace blender
