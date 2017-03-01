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

/* Used as ubo but colors can be directly
 * referenced as well */
/* Keep in sync with globalsBlock in shaders */
typedef struct GlobalsUboStorage {
	/* UBOs data needs to be 16 byte aligned (size of vec4) */
	float colorWire[4];
	float colorWireEdit[4];
	float colorActive[4];
	float colorSelect[4];
	float colorTransform[4];
	float colorGroupActive[4];
	float colorGroup[4];
	float colorLamp[4];
	float colorSpeaker[4];
	float colorCamera[4];
	float colorEmpty[4];
	float colorVertex[4];
	float colorVertexSelect[4];
	float colorEditMeshActive[4];
	float colorEdgeSelect[4];
	float colorEdgeSeam[4];
	float colorEdgeSharp[4];
	float colorEdgeCrease[4];
	float colorEdgeBWeight[4];
	float colorEdgeFaceSelect[4];
	float colorFace[4];
	float colorFaceSelect[4];
	float colorNormal[4];
	float colorVNormal[4];
	float colorLNormal[4];
	float colorFaceDot[4];

	float colorDeselect[4];
	float colorOutline[4];
	float colorLampNoAlpha[4];

	/* Pack individual float at the end of the buffer to avoid alignement errors */
	float sizeLampCenter, sizeLampCircle, sizeLampCircleShadow;
	float sizeVertex, sizeEdge, sizeEdgeFix, sizeNormal, sizeFaceDot;
} GlobalsUboStorage;
/* Keep in sync with globalsBlock in shaders */

void DRW_update_global_values(void);

void DRW_mode_passes_setup(struct DRWPass **psl_wire_overlay,
                           struct DRWPass **psl_wire_overlay_hidden_wire,
                           struct DRWPass **psl_wire_outline,
                           struct DRWPass **psl_non_meshes,
                           struct DRWPass **psl_ob_center,
                           struct DRWPass **psl_bone_solid,
                           struct DRWPass **psl_bone_wire);

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