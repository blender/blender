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

struct DRWPass;
struct Batch;
struct Object;

void DRW_mode_passes_setup(struct DRWPass **psl_wire_overlay,
                           struct DRWPass **psl_wire_overlay_hidden_wire,
                           struct DRWPass **psl_wire_outline,
                           struct DRWPass **psl_non_meshes,
                           struct DRWPass **psl_ob_center,
                           struct DRWPass **psl_bone_solid,
                           struct DRWPass **psl_bone_wire);

void DRW_shgroup_wire_overlay(struct Object *ob);
void DRW_shgroup_wire_outline(struct Object *ob, const bool do_front, const bool do_back, const bool do_outline);
void DRW_shgroup_lamp(struct Object *ob);
void DRW_shgroup_empty(struct Object *ob);
void DRW_shgroup_speaker(struct Object *ob);
void DRW_shgroup_relationship_lines(struct Object *ob);
void DRW_shgroup_object_center(struct Object *ob);

void DRW_shgroup_armature_object(struct Object *ob);
void DRW_shgroup_armature_edit(struct Object *ob);
void DRW_shgroup_armature_pose(struct Object *ob);

void DRW_shgroup_bone_octahedral_solid(const float (*arm_mat)[4], const float color[4]);
void DRW_shgroup_bone_octahedral_wire(const float (*arm_mat)[4], const float color[4]);
void DRW_shgroup_bone_point_solid(const float (*arm_mat)[4], const float color[4]);
void DRW_shgroup_bone_point_wire(const float (*arm_mat)[4], const float color[4]);
void DRW_shgroup_bone_relationship_lines(const float head[3], const float tail[3]);
void DRW_shgroup_bone_axes(const float (*arm_mat)[4], const float color[4]);

/* draw_armature.c */
void draw_armature_edit(struct Object *ob);
void draw_armature_pose(struct Object *ob, const float const_color[4]);

#endif /* __DRAW_MODE_PASS_H__ */