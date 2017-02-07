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

/** \file draw_mode_pass.h
 *  \ingroup draw
 */

#ifndef __DRAW_MODE_PASS_H__
#define __DRAW_MODE_PASS_H__

#include "DRW_render.h"

struct DRWPass;
struct Batch;
struct Object;

void DRW_pass_setup_common(struct DRWPass **wire_overlay, struct DRWPass **wire_outline, struct DRWPass **non_meshes, struct DRWPass **ob_center);

void DRW_shgroup_wire_overlay(struct DRWPass *wire_overlay, struct Object *ob);
void DRW_shgroup_wire_outline(
        struct DRWPass *wire_outline, struct Object *ob, const bool do_front, const bool do_back, const bool do_outline);

void DRW_shgroup_non_meshes(struct DRWPass *non_meshes, struct Object *ob);
void DRW_shgroup_relationship_lines(struct DRWPass *non_meshes, struct Object *ob);
void DRW_shgroup_object_center(struct DRWPass *ob_center, struct Object *ob);

#endif /* __DRAW_MODE_PASS_H__ */