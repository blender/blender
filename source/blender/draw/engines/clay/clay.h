/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file clay.h
 *  \ingroup DNA
 */

#ifndef __CLAY_H__
#define __CLAY_H__

extern RenderEngineType viewport_clay_type;

struct RenderEngineSettings *CLAY_render_settings_create(void);
struct MaterialEngineSettings *CLAY_material_settings_create(void);

void CLAY_engine_free(void);

#endif /* __CLAY_H__ */
