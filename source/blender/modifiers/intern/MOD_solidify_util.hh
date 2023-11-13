/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

/* MOD_solidify_extrude.cc */

Mesh *MOD_solidify_extrude_modifyMesh(ModifierData *md,
                                      const ModifierEvalContext *ctx,
                                      Mesh *mesh);

/* MOD_solidify_nonmanifold.cc */

Mesh *MOD_solidify_nonmanifold_modifyMesh(ModifierData *md,
                                          const ModifierEvalContext *ctx,
                                          Mesh *mesh);
