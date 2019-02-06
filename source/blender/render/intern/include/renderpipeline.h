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
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup render
 */


#ifndef __RENDERPIPELINE_H__
#define __RENDERPIPELINE_H__

struct ListBase;
struct Render;
struct RenderData;
struct RenderLayer;
struct RenderResult;

struct RenderLayer *render_get_active_layer(struct Render *re, struct RenderResult *rr);
void render_update_anim_renderdata(struct Render *re, struct RenderData *rd, struct ListBase *render_layers);
void render_copy_renderdata(struct RenderData *to, struct RenderData *from);

#endif /* __RENDERPIPELINE_H__ */
