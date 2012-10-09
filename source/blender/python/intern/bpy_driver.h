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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_driver.h
 *  \ingroup pythonintern
 */

#ifndef __BPY_DRIVER_H__
#define __BPY_DRIVER_H__

struct ChannelDriver;

int bpy_pydriver_create_dict(void);
extern PyObject *bpy_pydriver_Dict;

/* externals */
float BPY_driver_exec(struct ChannelDriver *driver, const float evaltime);
void BPY_driver_reset(void);

#endif  /* __BPY_DRIVER_H__ */
