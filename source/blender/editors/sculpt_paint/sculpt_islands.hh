/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

namespace blender {

struct Object;
struct SculptSession;

namespace ed::sculpt_paint::islands {

/* Ensure vertex island keys exist and are valid. */
void ensure_cache(Object &object);

/** Mark vertex island keys as invalid. Call when adding or hiding geometry. */
void invalidate(SculptSession &ss);

/** Get vertex island key. */
int vert_id_get(const SculptSession &ss, int vert);

}  // namespace ed::sculpt_paint::islands

}  // namespace blender
