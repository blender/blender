# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

###############################################################################
#234567890123456789012345678901234567890123456789012345678901234567890123456789
#--------1---------2---------3---------4---------5---------6---------7---------


# ##### BEGIN COPYRIGHT BLOCK #####
#
# initial script copyright (c)2011-2013 Alexander Nussbaumer
#
# ##### END COPYRIGHT BLOCK #####


#import python stuff
import io
from math import (
        pi,
        )
from mathutils import (
        Matrix,
        )
from os import (
        path,
        )
from sys import (
        exc_info,
        )
from time import (
        time,
        )


# import io_scene_ms3d stuff
from io_scene_ms3d.ms3d_strings import (
        ms3d_str,
        )
from io_scene_ms3d.ms3d_spec import (
        Ms3dSpec,
        Ms3dModel,
        Ms3dVertex,
        Ms3dTriangle,
        Ms3dGroup,
        Ms3dMaterial,
        Ms3dJoint,
        Ms3dRotationKeyframe,
        Ms3dTranslationKeyframe,
        Ms3dCommentEx,
        Ms3dComment,
        )
from io_scene_ms3d.ms3d_utils import (
        select_all,
        enable_edit_mode,
        pre_setup_environment,
        post_setup_environment,
        matrix_difference,
        )
from io_scene_ms3d.ms3d_ui import (
        Ms3dUi,
        Ms3dMaterialProperties,
        Ms3dMaterialHelper,
        )


#import blender stuff
from bpy import (
        ops,
        )
import bmesh


###############################################################################
class Ms3dExporter():
    """
    Load a MilkShape3D MS3D File
    """
    def __init__(self,
            report,
            verbose='NONE',
            use_blender_names=True,
            use_blender_materials=False,
            apply_transform=True,
            apply_modifiers=True,
            apply_modifiers_mode='PREVIEW',
            use_animation=True,
            normalize_weights=True,
            shrink_to_keys=False,
            bake_each_frame=True,
            ):
        self.report = report
        self.options_verbose = verbose
        self.options_use_blender_names = use_blender_names
        self.options_use_blender_materials = use_blender_materials
        self.options_apply_transform = apply_transform
        self.options_apply_modifiers = apply_modifiers
        self.options_apply_modifiers_mode = apply_modifiers_mode
        self.options_use_animation = use_animation
        self.options_normalize_weights = normalize_weights
        self.options_shrink_to_keys = shrink_to_keys
        self.options_bake_each_frame = bake_each_frame
        pass

    # create a empty ms3d ms3d_model
    # fill ms3d_model with blender content
    # writer ms3d file
    def write(self, blender_context, filepath):
        """convert bender content to ms3d content and write it to file"""

        t1 = time()
        t2 = None

        try:
            # setup environment
            pre_setup_environment(self, blender_context)

            # create an empty ms3d template
            ms3d_model = Ms3dModel()

            # inject blender data to ms3d file
            self.from_blender(blender_context, ms3d_model)

            t2 = time()

            try:
                # write ms3d file to disk
                with io.FileIO(filepath, "wb") as raw_io:
                    debug_out = ms3d_model.write(raw_io)
                    raw_io.flush()
                    raw_io.close()

                    if self.options_verbose in Ms3dUi.VERBOSE_MAXIMAL:
                        print(debug_out)
            finally:
                pass

            # if option is set, this time will enlargs the io time
            if self.options_verbose in Ms3dUi.VERBOSE_MAXIMAL:
                ms3d_model.print_internal()

            post_setup_environment(self, blender_context)
            # restore active object
            blender_context.scene.objects.active = self.active_object

            if ((not blender_context.scene.objects.active)
                    and (blender_context.selected_objects)):
                blender_context.scene.objects.active \
                        = blender_context.selected_objects[0]

            # restore pre operator undo state
            blender_context.user_preferences.edit.use_global_undo = self.undo

            is_valid, statistics = ms3d_model.is_valid()
            if self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                print()
                print("##########################################################")
                print("Export from Blender to MS3D")
                print(statistics)
                print("##########################################################")

        except Exception:
            type, value, traceback = exc_info()
            if self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                print("write - exception in try block\n  type: '{0}'\n"
                        "  value: '{1}'".format(type, value, traceback))
                if self.report:
                    self.report({'WARNING', 'ERROR', }, "write - exception.")

            if t2 is None:
                t2 = time()

            return False

        else:
            pass

        t3 = time()
        if self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
            print(ms3d_str['SUMMARY_EXPORT'].format(
                    (t3 - t1), (t2 - t1), (t3 - t2)))

        return True


    ###########################################################################
    def from_blender(self, blender_context, ms3d_model):
        blender_mesh_objects = []

        source = (blender_context.active_object, )

        for blender_object in source:
            if blender_object and blender_object.type == 'MESH' \
                    and blender_object.is_visible(blender_context.scene):
                blender_mesh_objects.append(blender_object)

        blender_to_ms3d_bones = {}

        self.create_animation(blender_context, ms3d_model, blender_mesh_objects, blender_to_ms3d_bones)
        self.create_geometry(blender_context, ms3d_model, blender_mesh_objects,
                blender_to_ms3d_bones)


    ###########################################################################
    def create_geometry(self, blender_context, ms3d_model, blender_mesh_objects, blender_to_ms3d_bones):
        blender_scene = blender_context.scene

        blender_to_ms3d_vertices = {}
        blender_to_ms3d_triangles = {}
        blender_to_ms3d_groups = {}
        blender_to_ms3d_materials = {}

        for blender_mesh_object in blender_mesh_objects:
            blender_mesh = blender_mesh_object.data

            ms3d_model._model_ex_object.joint_size = \
                    blender_mesh.ms3d.joint_size
            ms3d_model._model_ex_object.alpha_ref = blender_mesh.ms3d.alpha_ref
            ms3d_model._model_ex_object.transparency_mode = \
                    Ms3dUi.transparency_mode_to_ms3d(
                    blender_mesh.ms3d.transparency_mode)

            if blender_mesh.ms3d.comment:
                ms3d_model._comment_object = Ms3dComment(blender_mesh.ms3d.comment)

            ##########################
            # prepare ms3d groups if available
            # works only for exporting active object
            ##EXPORT_ACTIVE_ONLY:
            for ms3d_local_group_index, blender_ms3d_group in enumerate(
                    blender_mesh.ms3d.groups):
                ms3d_group = Ms3dGroup()
                ms3d_group.__index = len(ms3d_model._groups)
                ms3d_group.name = blender_ms3d_group.name
                ms3d_group.flags = Ms3dUi.flags_to_ms3d(blender_ms3d_group.flags)
                if blender_ms3d_group.comment:
                    ms3d_group._comment_object = Ms3dCommentEx()
                    ms3d_group._comment_object.comment = \
                            blender_ms3d_group.comment
                    ms3d_group._comment_object.index = len(ms3d_model._groups)
                ms3d_group.material_index = None # to mark as not setted
                ms3d_model._groups.append(ms3d_group)
                blender_to_ms3d_groups[blender_ms3d_group.id] = ms3d_group

            ##########################
            # i have to use BMesh, because there are several custom data stored.
            # BMesh doesn't support quads_convert_to_tris()
            # so, i use that very ugly way:
            # create a complete copy of mesh and bend object data
            # to be able to apply operations to it.

            # temporary, create a full heavy copy of the model
            # (object, mesh, modifiers)
            blender_mesh_temp = blender_mesh_object.data.copy()
            blender_mesh_object_temp = blender_mesh_object.copy()
            blender_mesh_object_temp.data = blender_mesh_temp
            blender_scene.objects.link(blender_mesh_object_temp)
            blender_scene.objects.active = blender_mesh_object_temp

            # apply transform
            if self.options_apply_transform:
                matrix_transform = blender_mesh_object_temp.matrix_basis
            else:
                matrix_transform = 1

            # apply modifiers
            for modifier in blender_mesh_object_temp.modifiers:
                if self.options_apply_modifiers:
                    # disable only armature modifiers and only,
                    # when use_animation is enabled
                    if  self.options_use_animation \
                            and modifier.type in {'ARMATURE', }:
                        modifier.show_viewport = False
                        modifier.show_render = False
                else:
                    # disable all modifiers,
                    # to be able to add and apply triangulate modifier later
                    modifier.show_viewport = False
                    modifier.show_render = False

            # convert to tris by using the triangulate modifier
            blender_mesh_object_temp.modifiers.new("temp", 'TRIANGULATE')
            blender_mesh_temp = blender_mesh_object_temp.to_mesh(
                    blender_scene,
                    True,
                    self.options_apply_modifiers_mode)

            enable_edit_mode(True, blender_context)
            bm = bmesh.new()
            bm.from_mesh(blender_mesh_temp)

            layer_texture = bm.faces.layers.tex.get(
                    ms3d_str['OBJECT_LAYER_TEXTURE'])
            if layer_texture is None:
                layer_texture = bm.faces.layers.tex.new(
                        ms3d_str['OBJECT_LAYER_TEXTURE'])

            layer_smoothing_group = bm.faces.layers.int.get(
                    ms3d_str['OBJECT_LAYER_SMOOTHING_GROUP'])
            if layer_smoothing_group is None:
                layer_smoothing_group = bm.faces.layers.int.new(
                        ms3d_str['OBJECT_LAYER_SMOOTHING_GROUP'])

            layer_group = bm.faces.layers.int.get(
                    ms3d_str['OBJECT_LAYER_GROUP'])
            if layer_group is None:
                layer_group = bm.faces.layers.int.new(
                        ms3d_str['OBJECT_LAYER_GROUP'])

            layer_uv = bm.loops.layers.uv.get(ms3d_str['OBJECT_LAYER_UV'])
            if layer_uv is None:
                if bm.loops.layers.uv:
                    layer_uv = bm.loops.layers.uv[0]
                else:
                    layer_uv = bm.loops.layers.uv.new(
                            ms3d_str['OBJECT_LAYER_UV'])

            layer_deform = bm.verts.layers.deform.active

            layer_extra = bm.verts.layers.int.get(ms3d_str['OBJECT_LAYER_EXTRA'])
            if layer_extra is None:
                layer_extra = bm.verts.layers.int.new(
                        ms3d_str['OBJECT_LAYER_EXTRA'])


            ##########################
            # handle vertices
            for bmv in bm.verts:
                item = blender_to_ms3d_vertices.get(bmv)
                if item is None:
                    index = len(ms3d_model._vertices)
                    ms3d_vertex = Ms3dVertex()
                    ms3d_vertex.__index = index

                    ms3d_vertex._vertex = self.geometry_correction(
                            matrix_transform * bmv.co)

                    if self.options_use_animation and layer_deform:
                        blender_vertex_group_ids = bmv[layer_deform]
                        if blender_vertex_group_ids:
                            bone_weights = {}
                            for blender_index, blender_weight \
                                    in blender_vertex_group_ids.items():
                                ms3d_joint = blender_to_ms3d_bones.get(
                                        blender_mesh_object_temp.vertex_groups[\
                                                blender_index].name)
                                if ms3d_joint:
                                    weight = bone_weights.get(ms3d_joint.__index)
                                    if not weight:
                                        weight = 0
                                    bone_weights[ms3d_joint.__index] = weight + blender_weight

                            # sort (bone_id: weight) according its weights
                            # to skip only less important weights in the next pass
                            bone_weights_sorted = sorted(bone_weights.items(), key=lambda item: item[1], reverse=True)

                            count = 0
                            bone_ids = []
                            weights = []
                            for ms3d_index, blender_weight \
                                    in bone_weights_sorted:

                                if count == 0:
                                    ms3d_vertex.bone_id = ms3d_index
                                    weights.append(blender_weight)
                                elif count == 1:
                                    bone_ids.append(ms3d_index)
                                    weights.append(blender_weight)
                                elif count == 2:
                                    bone_ids.append(ms3d_index)
                                    weights.append(blender_weight)
                                elif count == 3:
                                    bone_ids.append(ms3d_index)
                                    if self.report and self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                                        self.report(
                                                {'WARNING', 'INFO'},
                                                ms3d_str['WARNING_EXPORT_SKIP_WEIGHT'])
                                else:
                                    # only first three weights will be supported / four bones
                                    if self.report and self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                                        self.report(
                                                {'WARNING', 'INFO'},
                                                ms3d_str['WARNING_EXPORT_SKIP_WEIGHT_EX'])
                                    break
                                count += 1

                            # normalize weights to 100%
                            if self.options_normalize_weights:
                                weight_sum = 0.0
                                for weight in weights:
                                    weight_sum += weight

                                if weight_sum > 0.0:
                                    weight_normalize = 1.0 / weight_sum
                                else:
                                    weight_normalize = 1.0

                                weight_sum = 100
                                for index, weight in enumerate(weights):
                                    if index >= count-1 or index >= 2:
                                        # take the full rest instead of calculate,
                                        # that should fill up to exactly 100%
                                        # (in some cases it is only 99% bacaus of roulding errors)
                                        weights[index] = int(weight_sum)
                                        break
                                    normalized_weight = int(weight * weight_normalize * 100)
                                    weights[index] = normalized_weight
                                    weight_sum -= normalized_weight

                            # fill up missing values
                            while len(bone_ids) < 3:
                                bone_ids.append(Ms3dSpec.DEFAULT_VERTEX_BONE_ID)
                            while len(weights) < 3:
                                weights.append(0)

                            ms3d_vertex._vertex_ex_object._bone_ids = \
                                    tuple(bone_ids)
                            ms3d_vertex._vertex_ex_object._weights = \
                                    tuple(weights)

                    if layer_extra:
                        #ms3d_vertex._vertex_ex_object.extra = bmv[layer_extra]
                        # bm.verts.layers.int does only support signed int32
                        # convert signed int32 to unsigned int32 (little-endian)
                        signed_int32 = bmv[layer_extra]
                        bytes_int32 = signed_int32.to_bytes(
                                4, byteorder='little', signed=True)
                        unsigned_int32 = int.from_bytes(
                                bytes_int32, byteorder='little', signed=False)
                        ms3d_vertex._vertex_ex_object.extra = unsigned_int32

                    ms3d_model._vertices.append(ms3d_vertex)
                    blender_to_ms3d_vertices[bmv] = ms3d_vertex

            ##########################
            # handle faces / tris
            for bmf in bm.faces:
                item = blender_to_ms3d_triangles.get(bmf)
                if item is None:
                    index = len(ms3d_model._triangles)
                    ms3d_triangle = Ms3dTriangle()
                    ms3d_triangle.__index = index
                    bmv0 = bmf.verts[0]
                    bmv1 = bmf.verts[1]
                    bmv2 = bmf.verts[2]
                    ms3d_vertex0 = blender_to_ms3d_vertices[bmv0]
                    ms3d_vertex1 = blender_to_ms3d_vertices[bmv1]
                    ms3d_vertex2 = blender_to_ms3d_vertices[bmv2]
                    ms3d_vertex0.reference_count += 1
                    ms3d_vertex1.reference_count += 1
                    ms3d_vertex2.reference_count += 1
                    ms3d_triangle._vertex_indices = (
                            ms3d_vertex0.__index,
                            ms3d_vertex1.__index,
                            ms3d_vertex2.__index,
                            )
                    ms3d_triangle._vertex_normals = (
                            self.geometry_correction(bmv0.normal),
                            self.geometry_correction(bmv1.normal),
                            self.geometry_correction(bmv2.normal),
                            )
                    ms3d_triangle._s = (
                            bmf.loops[0][layer_uv].uv.x,
                            bmf.loops[1][layer_uv].uv.x,
                            bmf.loops[2][layer_uv].uv.x,
                            )
                    ms3d_triangle._t = (
                            1.0 - bmf.loops[0][layer_uv].uv.y,
                            1.0 - bmf.loops[1][layer_uv].uv.y,
                            1.0 - bmf.loops[2][layer_uv].uv.y,
                            )

                    ms3d_triangle.smoothing_group = bmf[layer_smoothing_group]
                    ms3d_model._triangles.append(ms3d_triangle)

                    ms3d_material = self.get_ms3d_material_add_if(
                            blender_mesh, ms3d_model,
                            blender_to_ms3d_materials, bmf.material_index)
                    ms3d_group = blender_to_ms3d_groups.get(bmf[layer_group])

                    ##EXPORT_ACTIVE_ONLY:
                    if ms3d_group is not None:
                        if ms3d_material is None:
                            ms3d_group.material_index = \
                                    Ms3dSpec.DEFAULT_GROUP_MATERIAL_INDEX
                        else:
                            if ms3d_group.material_index is None:
                                ms3d_group.material_index = \
                                        ms3d_material.__index
                            else:
                                if ms3d_group.material_index != \
                                        ms3d_material.__index:
                                    ms3d_group = \
                                            self.get_ms3d_group_by_material_add_if(
                                            ms3d_model, ms3d_material)
                    else:
                        if ms3d_material is not None:
                            ms3d_group = self.get_ms3d_group_by_material_add_if(
                                    ms3d_model, ms3d_material)
                        else:
                            ms3d_group = self.get_ms3d_group_default_material_add_if(
                                    ms3d_model)

                    if ms3d_group is not None:
                        ms3d_group._triangle_indices.append(
                                ms3d_triangle.__index)
                        ms3d_triangle.group_index = ms3d_group.__index

                    blender_to_ms3d_triangles[bmf] = ms3d_triangle

            if bm is not None:
                bm.free()

            enable_edit_mode(False, blender_context)

            ##########################
            # remove the temporary data
            blender_scene.objects.unlink(blender_mesh_object_temp)
            if blender_mesh_temp is not None:
                blender_mesh_temp.user_clear()
                blender_context.blend_data.meshes.remove(blender_mesh_temp)
            blender_mesh_temp = None
            if blender_mesh_object_temp is not None:
                blender_mesh_temp = blender_mesh_object_temp.data.user_clear()
                blender_mesh_object_temp.user_clear()
                blender_context.blend_data.objects.remove(
                        blender_mesh_object_temp)
            if blender_mesh_temp is not None:
                blender_mesh_temp.user_clear()
                blender_context.blend_data.meshes.remove(blender_mesh_temp)


    ###########################################################################
    def create_animation(self, blender_context, ms3d_model,
            blender_mesh_objects, blender_to_ms3d_bones):
        ##########################
        # setup scene
        blender_scene = blender_context.scene

        if not self.options_use_animation:
            ms3d_model.animation_fps = 24
            ms3d_model.number_total_frames = 1
            ms3d_model.current_time = 0
            return

        frame_start = blender_scene.frame_start
        frame_end = blender_scene.frame_end
        frame_total = (frame_end - frame_start) + 1
        frame_step = blender_scene.frame_step
        frame_offset = 0

        fps = blender_scene.render.fps * blender_scene.render.fps_base
        time_base = 1.0 / fps

        base_bone_correction = Matrix.Rotation(pi / 2, 4, 'Z')

        for blender_mesh_object in blender_mesh_objects:
            blender_bones = None
            blender_action = None
            blender_nla_tracks = None

            # note: only one armature modifier/parent will be handled.
            #   if the parent is an armature, it will be handled irrespective
            #   of existence of any armature modifier

            # question: maybe it is better to handle
            #   all existing armature sources (parent / modifier)
            #   as a merged animation...
            #   what is best practice in case of multiple animation sources?

            # take parent to account if it is an armature
            if blender_mesh_object.parent and \
                    blender_mesh_object.parent_type == 'ARMATURE' and \
                    blender_mesh_object.parent.pose:
                blender_bones = blender_mesh_object.parent.data.bones
                blender_pose_bones = blender_mesh_object.parent.pose.bones
                if blender_mesh_object.parent.animation_data:
                    blender_action = \
                            blender_mesh_object.parent.animation_data.action
                    blender_nla_tracks = \
                            blender_mesh_object.parent.animation_data.nla_tracks

                # apply transform
                if self.options_apply_transform:
                    matrix_transform = blender_mesh_object.parent.matrix_basis
                else:
                    matrix_transform = 1

            # search for animation modifier
            else:
                for blender_modifier in blender_mesh_object.modifiers:
                    if blender_modifier.type == 'ARMATURE' \
                            and blender_modifier.object.pose:
                        blender_bones = blender_modifier.object.data.bones
                        blender_pose_bones = blender_modifier.object.pose.bones
                        if blender_modifier.object.animation_data:
                            blender_action = \
                                    blender_modifier.object.animation_data.action
                            blender_nla_tracks = \
                                    blender_modifier.object.animation_data.nla_tracks

                        # apply transform
                        if self.options_apply_transform:
                            matrix_transform = blender_modifier.object.matrix_basis
                        else:
                            matrix_transform = 1

                        break

            # skip animation/bone handling, if no animation data is available
            if blender_bones is None \
                    and (blender_action is None and blender_nla_tracks is None):
                continue

            ##########################
            # bones
            blender_bones_ordered = []
            self.build_blender_bone_dependency_order(
                    blender_bones, blender_bones_ordered)
            for blender_bone_name in blender_bones_ordered:
                blender_bone_oject = blender_bones[blender_bone_name]
                ms3d_joint = Ms3dJoint()
                ms3d_joint.__index = len(ms3d_model._joints)

                blender_bone_ms3d = blender_bone_oject.ms3d
                blender_bone = blender_bone_oject

                ms3d_joint.flags = Ms3dUi.flags_to_ms3d(blender_bone_ms3d.flags)
                if blender_bone_ms3d.comment:
                    ms3d_joint._comment_object = Ms3dCommentEx()
                    ms3d_joint._comment_object.comment = \
                            blender_bone_ms3d.comment
                    ms3d_joint._comment_object.index = ms3d_joint.__index

                ms3d_joint.joint_ex_object._color = blender_bone_ms3d.color[:]

                ms3d_joint.name = blender_bone.name

                if blender_bone.parent:
                    ms3d_joint.parent_name = blender_bone.parent.name
                    ms3d_joint.__matrix = matrix_difference(
                            matrix_transform * blender_bone.matrix_local,
                            matrix_transform * blender_bone.parent.matrix_local)
                else:
                    ms3d_joint.__matrix = base_bone_correction \
                            * matrix_transform * blender_bone.matrix_local

                mat = ms3d_joint.__matrix
                loc = mat.to_translation()
                rot = mat.to_euler('XZY')
                ms3d_joint._position = self.joint_correction(loc)
                ms3d_joint._rotation = self.joint_correction(rot)

                ms3d_model._joints.append(ms3d_joint)
                blender_to_ms3d_bones[blender_bone.name] = ms3d_joint

            ##########################
            # animation
            frames = None
            frames_location = set()
            frames_rotation = set()
            frames_scale = set()

            if blender_action:
                self.fill_keyframe_sets(
                        blender_action.fcurves,
                        frames_location, frames_rotation, frames_scale,
                        0)

            if blender_nla_tracks:
                for nla_track in blender_nla_tracks:
                    if nla_track.mute:
                        continue
                    for strip in nla_track.strips:
                        if strip.mute:
                            continue
                        frame_correction = strip.frame_start \
                                - strip.action_frame_start
                        self.fill_keyframe_sets(
                                strip.action.fcurves,
                                frames_location, frames_rotation, frames_scale,
                                frame_correction)

            frames = set(frames_location)
            frames = frames.union(frames_rotation)
            frames = frames.union(frames_scale)

            if not self.options_shrink_to_keys:
                frames = frames.intersection(range(
                        blender_scene.frame_start, blender_scene.frame_end + 1))

            frames_sorted = list(frames)
            frames_sorted.sort()

            if self.options_shrink_to_keys and len(frames_sorted) >= 2:
                frame_start = frames_sorted[0]
                frame_end = frames_sorted[len(frames_sorted)-1]
                frame_total = (frame_end - frame_start) + 1
                frame_offset = frame_start - 1

            if self.options_bake_each_frame:
                frames_sorted = range(int(frame_start), int(frame_end + 1),
                        int(frame_step))

            frame_temp = blender_scene.frame_current

            for current_frame in frames_sorted:
                blender_scene.frame_set(current_frame)

                current_time = (current_frame - frame_offset) * time_base
                for blender_bone_name in blender_bones_ordered:
                    blender_bone = blender_bones[blender_bone_name]
                    blender_pose_bone = blender_pose_bones[blender_bone_name]
                    ms3d_joint = blender_to_ms3d_bones[blender_bone_name]

                    m1 = blender_bone.matrix_local.inverted()
                    if blender_pose_bone.parent:
                        m2 = blender_pose_bone.parent.matrix_channel.inverted()
                    else:
                        m2 = 1
                    m3 = blender_pose_bone.matrix.copy()
                    m = ((m1 * m2) * m3)
                    loc = m.to_translation()
                    rot = m.to_euler('XZY')

                    ms3d_joint.translation_key_frames.append(
                            Ms3dTranslationKeyframe(
                                    current_time, self.joint_correction(loc)
                                    )
                            )
                    ms3d_joint.rotation_key_frames.append(
                            Ms3dRotationKeyframe(
                                    current_time, self.joint_correction(rot)
                                    )
                            )

            blender_scene.frame_set(frame_temp)

        ms3d_model.animation_fps = fps
        if ms3d_model.number_joints > 0:
            ms3d_model.number_total_frames = int(frame_total)
            ms3d_model.current_time = ((blender_scene.frame_current \
                    - blender_scene.frame_start) + 1) * time_base
        else:
            ms3d_model.number_total_frames = 1
            ms3d_model.current_time = 0


    ###########################################################################
    def get_ms3d_group_default_material_add_if(self, ms3d_model):
        markerName = "MaterialGroupDefault"
        markerComment = "group without material"

        for ms3d_group in ms3d_model._groups:
            if ms3d_group.material_index == \
                    Ms3dSpec.DEFAULT_GROUP_MATERIAL_INDEX \
                    and ms3d_group.name == markerName \
                    and ms3d_group._comment_object \
                    and ms3d_group._comment_object.comment == markerComment:
                return ms3d_group

        ms3d_group = Ms3dGroup()
        ms3d_group.__index = len(ms3d_model._groups)
        ms3d_group.name = markerName
        ms3d_group._comment_object = Ms3dCommentEx()
        ms3d_group._comment_object.comment = markerComment
        ms3d_group._comment_object.index = len(ms3d_model._groups)
        ms3d_group.material_index = Ms3dSpec.DEFAULT_GROUP_MATERIAL_INDEX

        ms3d_model._groups.append(ms3d_group)

        return ms3d_group


    ###########################################################################
    def get_ms3d_group_by_material_add_if(self, ms3d_model, ms3d_material):
        if ms3d_material.__index < 0 \
                or ms3d_material.__index >= len(ms3d_model.materials):
            return None

        markerName = "MaterialGroup.{}".format(ms3d_material.__index)
        markerComment = "MaterialGroup({})".format(ms3d_material.name)

        for ms3d_group in ms3d_model._groups:
            if ms3d_group.name == markerName \
                    and ms3d_group._comment_object \
                    and ms3d_group._comment_object.comment == markerComment:
                return ms3d_group

        ms3d_group = Ms3dGroup()
        ms3d_group.__index = len(ms3d_model._groups)
        ms3d_group.name = markerName
        ms3d_group._comment_object = Ms3dCommentEx()
        ms3d_group._comment_object.comment = markerComment
        ms3d_group._comment_object.index = len(ms3d_model._groups)
        ms3d_group.material_index = ms3d_material.__index

        ms3d_model._groups.append(ms3d_group)

        return ms3d_group


    ###########################################################################
    def get_ms3d_material_add_if(self, blender_mesh, ms3d_model,
            blender_to_ms3d_materials, blender_index):
        if blender_index < 0 or blender_index >= len(blender_mesh.materials):
            return None

        blender_material = blender_mesh.materials[blender_index]
        ms3d_material = blender_to_ms3d_materials.get(blender_material)
        if ms3d_material is None:
            ms3d_material = Ms3dMaterial()
            ms3d_material.__index = len(ms3d_model.materials)

            blender_ms3d_material = blender_material.ms3d

            if not self.options_use_blender_names \
                    and not self.options_use_blender_materials \
                    and blender_ms3d_material.name:
                ms3d_material.name = blender_ms3d_material.name
            else:
                ms3d_material.name = blender_material.name

            temp_material = None
            if self.options_use_blender_materials:
                temp_material = Ms3dMaterial()
                Ms3dMaterialHelper.copy_from_blender(
                        None, None, temp_material, blender_material)
            else:
                temp_material = blender_ms3d_material

            ms3d_material._ambient = temp_material.ambient[:]
            ms3d_material._diffuse = temp_material.diffuse[:]
            ms3d_material._specular = temp_material.specular[:]
            ms3d_material._emissive = temp_material.emissive[:]
            ms3d_material.shininess = temp_material.shininess
            ms3d_material.transparency = temp_material.transparency
            ms3d_material.texture = temp_material.texture
            ms3d_material.alphamap = temp_material.alphamap

            ms3d_material.mode = Ms3dUi.texture_mode_to_ms3d(
                    blender_ms3d_material.mode)
            if blender_ms3d_material.comment:
                ms3d_material._comment_object = Ms3dCommentEx()
                ms3d_material._comment_object.comment = \
                        blender_ms3d_material.comment
                ms3d_material._comment_object.index = ms3d_material.__index

            ms3d_model.materials.append(ms3d_material)

            blender_to_ms3d_materials[blender_material] = ms3d_material

        return ms3d_material


    ###########################################################################
    def geometry_correction(self, value):
        return (value[1], value[2], value[0])


    ###########################################################################
    def joint_correction(self, value):
        return (-value[0], value[2], value[1])


    ###########################################################################
    def build_blender_bone_dependency_order(self, blender_bones,
            blender_bones_ordered):
        if not blender_bones:
            return blender_bones_ordered

        blender_bones_children = {None: []}
        for blender_bone in blender_bones:
            if blender_bone.parent:
                blender_bone_children = blender_bones_children.get(
                        blender_bone.parent.name)
                if blender_bone_children is None:
                    blender_bone_children = blender_bones_children[
                            blender_bone.parent.name] = []
            else:
                blender_bone_children = blender_bones_children[None]

            blender_bone_children.append(blender_bone.name)

        self.traverse_dependencies(
                blender_bones_ordered,
                blender_bones_children,
                None)

        return blender_bones_ordered


    ###########################################################################
    def traverse_dependencies(self, blender_bones_ordered,
            blender_bones_children, key):
        blender_bone_children = blender_bones_children.get(key)
        if blender_bone_children:
            for blender_bone_name in blender_bone_children:
                blender_bones_ordered.append(blender_bone_name)
                self.traverse_dependencies(
                        blender_bones_ordered,
                        blender_bones_children,
                        blender_bone_name)

    ###########################################################################
    def fill_keyframe_sets(self,
            fcurves,
            frames_location, frames_rotation, frames_scale,
            frame_correction):
        for fcurve in fcurves:
            if fcurve.data_path.endswith(".location"):
                frames = frames_location
            elif fcurve.data_path.endswith(".rotation_euler"):
                frames = frames_rotation
            elif fcurve.data_path.endswith(".rotation_quaternion"):
                frames = frames_rotation
            elif fcurve.data_path.endswith(".scale"):
                frames = frames_scale
            else:
                pass

            for keyframe_point in fcurve.keyframe_points:
                frames.add(int(keyframe_point.co[0] + frame_correction))


###############################################################################
#234567890123456789012345678901234567890123456789012345678901234567890123456789
#--------1---------2---------3---------4---------5---------6---------7---------
# ##### END OF FILE #####
