/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_LAMP_H__
#define __BKE_LAMP_H__

/** \file BKE_lamp.h
 *  \ingroup bke
 *  \brief General operations, lookup, etc. for blender lamps.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct Lamp;
struct Main;
struct Scene;

void BKE_lamp_init(struct Lamp *la);
struct Lamp *BKE_lamp_add(struct Main *bmain, const char *name) ATTR_WARN_UNUSED_RESULT;
struct Lamp *BKE_lamp_copy(struct Main *bmain, const struct Lamp *la) ATTR_WARN_UNUSED_RESULT;
struct Lamp *localize_lamp(struct Lamp *la) ATTR_WARN_UNUSED_RESULT;
void BKE_lamp_make_local(struct Main *bmain, struct Lamp *la, const bool lib_local);
void BKE_lamp_free(struct Lamp *la);

void lamp_drivers_update(struct Scene *scene, struct Lamp *la, float ctime);

#ifdef __cplusplus
}
#endif

#endif
