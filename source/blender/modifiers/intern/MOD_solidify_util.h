/*
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
 */

/** \file
 * \ingroup modifiers
 */

#ifndef __MOD_SOLIDIFY_UTIL_H__
#define __MOD_SOLIDIFY_UTIL_H__

/* MOD_solidify_extrude.c */
Mesh *MOD_solidify_extrude_modifyMesh(ModifierData *md,
                                      const ModifierEvalContext *ctx,
                                      Mesh *mesh);

/* MOD_solidify_nonmanifold.c */
Mesh *MOD_solidify_nonmanifold_modifyMesh(ModifierData *md,
                                          const ModifierEvalContext *ctx,
                                          Mesh *mesh);

#endif /* __MOD_SOLIDIFY_UTIL_H__ */
