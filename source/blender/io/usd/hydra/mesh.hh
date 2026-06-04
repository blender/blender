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

/** Emit a mesh Rprim per material-slot submesh. */
void emit_mesh_object(PopulateContext &ctx, const BObjectInfo &info, EmittedObject &emitted);

/** Emit prototype submeshes for use as instancer prototypes. */
void emit_mesh_proto(PopulateContext &ctx, const BObjectInfo &info);

}  // namespace io::hydra
}  // namespace blender
