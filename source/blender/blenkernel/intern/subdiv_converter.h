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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __SUBDIV_CONVERTER_H__
#define __SUBDIV_CONVERTER_H__

#include "BKE_subdiv.h"

/* NOTE: Was initially used to get proper enumerator types, but this makes
 * it tricky to compile without OpenSubdiv.
 */
/* #include "opensubdiv_converter_capi.h" */

struct Mesh;
struct OpenSubdiv_Converter;
struct SubdivSettings;

void BKE_subdiv_converter_init_for_mesh(struct OpenSubdiv_Converter *converter,
                                        const struct SubdivSettings *settings,
                                        const struct Mesh *mesh);

/* NOTE: Frees converter data, but not converter itself. This means, that if
 * converter was allocated on heap, it is up to the user to free that memory.
 */
void BKE_subdiv_converter_free(struct OpenSubdiv_Converter *converter);

/* ============================ INTERNAL HELPERS ============================ */

/* TODO(sergey): Find a way to make it OpenSubdiv_FVarLinearInterpolation,
 * without breaking compilation without OpenSubdiv.
 */
int BKE_subdiv_converter_fvar_linear_from_settings(
        const SubdivSettings *settings);

#endif  /* __SUBDIV_CONVERTER_H__ */
