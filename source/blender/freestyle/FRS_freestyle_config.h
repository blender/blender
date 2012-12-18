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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FRS_FREESTYLE_CONFIG_H__
#define __FRS_FREESTYLE_CONFIG_H__

/** \file FRS_freestyle_config.h
 *  \ingroup freestyle
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_scene_types.h"

void FRS_add_freestyle_config(SceneRenderLayer* srl);
void FRS_free_freestyle_config(SceneRenderLayer* srl);

#ifdef __cplusplus
}
#endif

#endif // __FRS_FREESTYLE_CONFIG_H__
