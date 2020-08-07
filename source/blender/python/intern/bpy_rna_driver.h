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

#pragma once

/** \file
 * \ingroup pythonintern
 */

struct ChannelDriver;
struct DriverTarget;
struct PathResolvedRNA;

#ifdef __cplusplus
extern "C" {
#endif

PyObject *pyrna_driver_get_variable_value(struct ChannelDriver *driver, struct DriverTarget *dtar);

PyObject *pyrna_driver_self_from_anim_rna(struct PathResolvedRNA *anim_rna);
bool pyrna_driver_is_equal_anim_rna(const struct PathResolvedRNA *anim_rna,
                                    const PyObject *py_anim_rna);

#ifdef __cplusplus
}
#endif
