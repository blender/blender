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
from mathutils import (
        Vector,
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
        Ms3dVertexEx2,
        Ms3dVertexEx3,
        Ms3dHeader,
        )
from io_scene_ms3d.ms3d_utils import (
        select_all,
        enable_pose_mode,
        enable_edit_mode,
        pre_setup_environment,
        post_setup_environment,
        get_edge_split_modifier_add_if,
        )
from io_scene_ms3d.ms3d_ui import (
        Ms3dUi,
        )


#import blender stuff
from bpy import (
        ops,
        )
import bmesh
from bpy_extras.image_utils import (
        load_image,
        )


###############################################################################
FORMAT_GROUP = "{}.g"
FORMAT_IMAGE = "{}.i"
FORMAT_TEXTURE = "{}.tex"
# keep material name like it is (prevent name "snakes" on re-import)
#FORMAT_MATERIAL = "{}.mat"
FORMAT_MATERIAL = "{}"
FORMAT_ACTION = "{}.act"
FORMAT_MESH = "{}.m"
FORMAT_MESH_OBJECT = "{}.mo"
FORMAT_EMPTY_OBJECT = "{}.eo"
FORMAT_ARMATURE = "{}.a"
FORMAT_ARMATURE_OBJECT = "{}.ao"
FORMAT_ARMATURE_NLA = "{}.an"

###############################################################################
class Ms3dImporter():
    """
    Load a MilkShape3D MS3D File
    """
    def __init__(self,
            report,
            verbose='NONE',
            use_extended_normal_handling=False,
            use_animation=True,
            use_quaternion_rotation=False,
            use_joint_size=False,
            joint_size=1.0,
            use_joint_to_bones=False,
            ):
        self.report = report
        self.options_verbose = verbose
        self.options_use_extended_normal_handling = use_extended_normal_handling
        self.options_use_animation = use_animation
        self.options_use_quaternion_rotation = use_quaternion_rotation
        self.options_use_joint_size = use_joint_size
        self.options_joint_size = joint_size
        self.options_use_joint_to_bones = use_joint_to_bones
        self.directory_name = ""
        self.file_name = ""
        pass

    ###########################################################################
    # create empty blender ms3d_model
    # read ms3d file
    # fill blender with ms3d_model content
    def read(self, blender_context, filepath):
        """ read ms3d file and convert ms3d content to bender content """
        t1 = time()
        t2 = None
        self.has_textures = False

        try:
            # setup environment
            pre_setup_environment(self, blender_context)

            # inject splitted filepath
            self.directory_name, self.file_name = path.split(filepath)

            # create an empty ms3d template
            ms3d_model = Ms3dModel(self.file_name)

            try:
                # open ms3d file
                with io.FileIO(filepath, 'rb') as raw_io:
                    # read and inject ms3d data from disk to internal structure
                    debug_out = ms3d_model.read(raw_io)
                    raw_io.close()

                    if self.options_verbose in Ms3dUi.VERBOSE_MAXIMAL:
                        print(debug_out)
            finally:
                pass

            # if option is set, this time will enlarges the io time
            if self.options_verbose in Ms3dUi.VERBOSE_MAXIMAL:
                ms3d_model.print_internal()

            t2 = time()

            is_valid, statistics = ms3d_model.is_valid()

            if is_valid:
                # inject ms3d data to blender
                self.to_blender(blender_context, ms3d_model)

                post_setup_environment(self, blender_context)

            if self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                print()
                print("##########################################################")
                print("Import from MS3D to Blender")
                print(statistics)
                print("##########################################################")

        except Ms3dHeader.HeaderError:
            msg = "read - invalid file format."
            if self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                print(msg)
                if self.report:
                    self.report({'WARNING', 'ERROR', }, msg)

            return False

        except Exception:
            type, value, traceback = exc_info()
            if self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                print("read - exception in try block\n  type: '{0}'\n"
                        "  value: '{1}'".format(type, value, traceback))
                if self.report:
                    self.report({'WARNING', 'ERROR', }, "read - exception.")

            if t2 is None:
                t2 = time()

            return False

        else:
            pass

        t3 = time()

        if self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
            print(ms3d_str['SUMMARY_IMPORT'].format(
                    (t3 - t1), (t2 - t1), (t3 - t2)))

        return True


    ###########################################################################
    def to_blender(self, blender_context, ms3d_model):
        blender_mesh_object = self.create_geometry(blender_context, ms3d_model)
        blender_armature_object = self.create_animation(
                blender_context, ms3d_model, blender_mesh_object)

        blender_empty_object = self.organize_objects(
                blender_context, ms3d_model,
                [blender_mesh_object, blender_armature_object])

        return blender_empty_object, blender_mesh_object


    ###########################################################################
    def organize_objects(self, blender_context, ms3d_model, blender_objects):
        ##########################
        # blender_armature_object to blender_mesh_object
        # that has bad side effects to the armature
        # and causes cyclic dependencies
        ###blender_armature_object.parent = blender_mesh_object
        ###blender_mesh_object.parent = blender_armature_object

        blender_scene = blender_context.scene

        blender_group = blender_context.blend_data.groups.new(
                FORMAT_GROUP.format(ms3d_model.name))
        blender_empty_object = blender_context.blend_data.objects.new(
                FORMAT_EMPTY_OBJECT.format(ms3d_model.name), None)
        blender_empty_object.location = blender_scene.cursor_location
        blender_scene.objects.link(blender_empty_object)
        blender_group.objects.link(blender_empty_object)

        for blender_object in blender_objects:
            if blender_object is not None:
                blender_group.objects.link(blender_object)
                blender_object.parent = blender_empty_object

        return blender_empty_object


    ###########################################################################
    def create_geometry(self, blender_context, ms3d_model):
        ##########################
        # blender stuff:
        # create a blender Mesh
        blender_mesh = blender_context.blend_data.meshes.new(
                FORMAT_MESH.format(ms3d_model.name))
        blender_mesh.ms3d.name = ms3d_model.name

        ms3d_comment = ms3d_model.comment_object
        if ms3d_comment is not None:
            blender_mesh.ms3d.comment = ms3d_comment.comment
        ms3d_model_ex = ms3d_model.model_ex_object
        if ms3d_model_ex is not None:
            blender_mesh.ms3d.joint_size = ms3d_model_ex.joint_size
            blender_mesh.ms3d.alpha_ref = ms3d_model_ex.alpha_ref
            blender_mesh.ms3d.transparency_mode \
                    = Ms3dUi.transparency_mode_from_ms3d(
                            ms3d_model_ex.transparency_mode)

        ##########################
        # blender stuff:
        # link to blender object
        blender_mesh_object = blender_context.blend_data.objects.new(
                FORMAT_MESH_OBJECT.format(ms3d_model.name), blender_mesh)

        ##########################
        # blender stuff:
        # create edge split modifier, to make sharp edges visible
        blender_modifier = get_edge_split_modifier_add_if(blender_mesh_object)

        ##########################
        # blender stuff:
        # link to blender scene
        blender_scene = blender_context.scene
        blender_scene.objects.link(blender_mesh_object)
        #blender_mesh_object.location = blender_scene.cursor_location
        enable_edit_mode(False, blender_context)
        select_all(False)
        blender_mesh_object.select = True
        blender_scene.objects.active = blender_mesh_object

        ##########################
        # take this as active object after import
        self.active_object = blender_mesh_object

        ##########################
        # blender stuff:
        # create all (ms3d) groups
        ms3d_to_blender_group_index = {}
        blender_group_manager = blender_mesh.ms3d
        for ms3d_group_index, ms3d_group in enumerate(ms3d_model.groups):
            blender_group = blender_group_manager.create_group()
            blender_group.name = ms3d_group.name
            blender_group.flags = Ms3dUi.flags_from_ms3d(ms3d_group.flags)
            blender_group.material_index = ms3d_group.material_index

            ms3d_comment = ms3d_group.comment_object
            if ms3d_comment is not None:
                blender_group.comment = ms3d_comment.comment

            # translation dictionary
            ms3d_to_blender_group_index[ms3d_group_index] = blender_group.id

        ####################################################
        # begin BMesh stuff
        #

        ##########################
        # BMesh stuff:
        # create an empty BMesh
        bm = bmesh.new()

        ##########################
        # BMesh stuff:
        # create new Layers for custom data per "mesh face"
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

        ##########################
        # BMesh stuff:
        # create new Layers for custom data per "face vertex"
        layer_uv = bm.loops.layers.uv.get(ms3d_str['OBJECT_LAYER_UV'])
        if layer_uv is None:
            layer_uv = bm.loops.layers.uv.new(ms3d_str['OBJECT_LAYER_UV'])

        ##########################
        # BMesh stuff:
        # create new Layers for custom data per "vertex"
        layer_extra = bm.verts.layers.int.get(ms3d_str['OBJECT_LAYER_EXTRA'])
        if layer_extra is None:
            layer_extra = bm.verts.layers.int.new(ms3d_str['OBJECT_LAYER_EXTRA'])

        ##########################
        # BMesh stuff:
        # create all vertices
        for ms3d_vertex_index, ms3d_vertex in enumerate(ms3d_model.vertices):
            bmv = bm.verts.new(self.geometry_correction(ms3d_vertex.vertex))
            bmv.index = ms3d_vertex_index

            if layer_extra and ms3d_vertex.vertex_ex_object and \
                    (isinstance(ms3d_vertex.vertex_ex_object, Ms3dVertexEx2) \
                    or isinstance(ms3d_vertex.vertex_ex_object, Ms3dVertexEx3)):

                #bmv[layer_extra] = ms3d_vertex.vertex_ex_object.extra
                # bm.verts.layers.int does only support signed int32
                # convert unsigned int32 to signed int32 (little-endian)
                unsigned_int32 = ms3d_vertex.vertex_ex_object.extra
                bytes_int32 = unsigned_int32.to_bytes(
                        4, byteorder='little', signed=False)
                signed_int32 = int.from_bytes(
                        bytes_int32, byteorder='little', signed=True)
                bmv[layer_extra] = signed_int32
        bm.verts.ensure_lookup_table()

        ##########################
        # blender stuff (uses BMesh stuff):
        # create all materials / image textures
        ms3d_to_blender_material = {}
        for ms3d_material_index, ms3d_material in enumerate(
                ms3d_model.materials):
            blender_material = blender_context.blend_data.materials.new(
                    FORMAT_MATERIAL.format(ms3d_material.name))

            # custom data
            blender_material.ms3d.name = ms3d_material.name
            blender_material.ms3d.ambient = ms3d_material.ambient
            blender_material.ms3d.diffuse = ms3d_material.diffuse
            blender_material.ms3d.specular = ms3d_material.specular
            blender_material.ms3d.emissive = ms3d_material.emissive
            blender_material.ms3d.shininess = ms3d_material.shininess
            blender_material.ms3d.transparency = ms3d_material.transparency
            blender_material.ms3d.mode = Ms3dUi.texture_mode_from_ms3d(
                    ms3d_material.mode)

            if ms3d_material.texture:
                blender_material.ms3d.texture = ms3d_material.texture

            if ms3d_material.alphamap:
                blender_material.ms3d.alphamap = ms3d_material.alphamap

            ms3d_comment = ms3d_material.comment_object
            if ms3d_comment is not None:
                blender_material.ms3d.comment = ms3d_comment.comment

            # blender data
            blender_material.ambient = (
                    (ms3d_material.ambient[0]
                    + ms3d_material.ambient[1]
                    + ms3d_material.ambient[2]) / 3.0)

            blender_material.diffuse_color[0] = ms3d_material.diffuse[0]
            blender_material.diffuse_color[1] = ms3d_material.diffuse[1]
            blender_material.diffuse_color[2] = ms3d_material.diffuse[2]

            blender_material.specular_color[0] = ms3d_material.specular[0]
            blender_material.specular_color[1] = ms3d_material.specular[1]
            blender_material.specular_color[2] = ms3d_material.specular[2]

            blender_material.emit = (
                    (ms3d_material.emissive[0]
                    + ms3d_material.emissive[1]
                    + ms3d_material.emissive[2]) / 3.0)

            blender_material.specular_hardness = ms3d_material.shininess * 4.0
            blender_material.alpha = 1.0 - ms3d_material.transparency

            # diffuse texture
            if ms3d_material.texture:
                dir_name_diffuse = self.directory_name
                file_name_diffuse = path.split(ms3d_material.texture)[1]
                blender_image_diffuse = load_image(
                        file_name_diffuse, dir_name_diffuse)
                name_diffuse = path.splitext(file_name_diffuse)[0]
                if blender_image_diffuse:
                    blender_image_diffuse.name = FORMAT_IMAGE.format(name_diffuse)
                blender_texture_diffuse = \
                        blender_context.blend_data.textures.new(
                        name=FORMAT_TEXTURE.format(name_diffuse),
                        type='IMAGE')
                blender_texture_diffuse.image = blender_image_diffuse
                blender_texture_slot_diffuse \
                        = blender_material.texture_slots.add()
                blender_texture_slot_diffuse.texture = blender_texture_diffuse
                blender_texture_slot_diffuse.texture_coords = 'UV'
                blender_texture_slot_diffuse.uv_layer = layer_uv.name
                blender_texture_slot_diffuse.use_map_color_diffuse = True
                blender_texture_slot_diffuse.use_map_alpha = False
                if blender_image_diffuse is not None:
                    self.has_textures = True
            else:
                blender_image_diffuse = None

            # alpha texture
            if ms3d_material.alphamap:
                dir_name_alpha = self.directory_name
                file_name_alpha = path.split(ms3d_material.alphamap)[1]
                blender_image_alpha = load_image(
                        file_name_alpha, dir_name_alpha)
                name_alpha = path.splitext(file_name_alpha)[0]
                if blender_image_alpha:
                    blender_image_alpha.name = FORMAT_IMAGE.format(name_alpha)
                blender_texture_alpha = blender_context.blend_data.textures.new(
                        name=FORMAT_TEXTURE.format(file_name_alpha),
                        type='IMAGE')
                blender_texture_alpha.image = blender_image_alpha
                blender_texture_slot_alpha \
                        = blender_material.texture_slots.add()
                blender_texture_slot_alpha.texture = blender_texture_alpha
                blender_texture_slot_alpha.texture_coords = 'UV'
                blender_texture_slot_alpha.uv_layer = layer_uv.name
                blender_texture_slot_alpha.use_map_color_diffuse = False
                blender_texture_slot_alpha.use_map_alpha = True
                blender_texture_slot_alpha.use_rgb_to_intensity = True
                blender_material.alpha = 0
                blender_material.specular_alpha = 0

            # append blender material to blender mesh, to be linked to
            blender_mesh.materials.append(blender_material)

            # translation dictionary
            ms3d_to_blender_material[ms3d_material_index] \
                    = blender_image_diffuse

        ##########################
        # BMesh stuff:
        # create all triangles
        length_verts = len(bm.verts)
        vertex_extra_index = length_verts
        blender_invalide_normal = Vector()
        smoothing_group_blender_faces = {}
        for ms3d_triangle_index, ms3d_triangle in enumerate(
                ms3d_model.triangles):
            bmv_list = []
            bmf_normal = Vector()

            for index, vert_index in enumerate(ms3d_triangle.vertex_indices):
                if vert_index < 0 or vert_index >= length_verts:
                    continue
                bmv = bm.verts[vert_index]

                blender_normal = self.geometry_correction(
                        ms3d_triangle.vertex_normals[index])
                if bmv.normal == blender_invalide_normal:
                    bmv.normal = blender_normal
                elif bmv.normal != blender_normal \
                        and self.options_use_extended_normal_handling:
                    ## search for an already created extra vertex
                    bmv_new = None
                    for vert_index_candidat in range(
                            vertex_extra_index, length_verts):
                        bmv_candidat = bm.verts[vert_index_candidat]
                        if bmv_candidat.co == bmv.co \
                                and bmv_candidat.normal == blender_normal:
                            bmv_new = bmv_candidat
                            vert_index = vert_index_candidat
                            break

                    ## if not exists, create one in blender and ms3d as well
                    if bmv_new is None:
                        ms3d_model.vertices.append(
                                ms3d_model.vertices[vert_index])
                        bmv_new = bm.verts.new(bmv.co)
                        bm.verts.ensure_lookup_table()
                        bmv_new.index = -vert_index
                        bmv_new.normal = blender_normal
                        bmv_new[layer_extra] = bmv[layer_extra]
                        vert_index = length_verts
                        length_verts += 1
                        if self.report and self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                            self.report({'WARNING', 'INFO'},
                                    ms3d_str['WARNING_IMPORT_EXTRA_VERTEX_NORMAL'].format(
                                    bmv.normal, blender_normal))
                    bmv = bmv_new

                if [[x] for x in bmv_list if x == bmv]:
                    if self.report and self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                        self.report(
                                {'WARNING', 'INFO'},
                                ms3d_str['WARNING_IMPORT_SKIP_VERTEX_DOUBLE'].format(
                                        ms3d_triangle_index))
                    continue
                bmv_list.append(bmv)
                bmf_normal += bmv.normal

            if len(bmv_list) < 3:
                if self.report and self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                    self.report(
                            {'WARNING', 'INFO'},
                            ms3d_str['WARNING_IMPORT_SKIP_LESS_VERTICES'].format(
                                    ms3d_triangle_index))
                continue

            # create edges for the face
            # (not really needed, because bm.faces.new() will create its edges,
            # if not exist, but good if we have already in case we need full control
            # of bmesh stuff maybe in the future.
            bme = bm.edges.get((bmv_list[0], bmv_list[1]))
            if bme is None:
                bme = bm.edges.new((bmv_list[0], bmv_list[1]))
                ##bm.edges.ensure_lookup_table()
                bme.index = len(bm.edges) - 1
            bme = bm.edges.get((bmv_list[1], bmv_list[2]))
            if bme is None:
                bme = bm.edges.new((bmv_list[1], bmv_list[2]))
                ##bm.edges.ensure_lookup_table()
                bme.index = len(bm.edges) - 1
            bme = bm.edges.get((bmv_list[2], bmv_list[0]))
            if bme is None:
                bme = bm.edges.new((bmv_list[2], bmv_list[0]))
                ##bm.edges.ensure_lookup_table()
                bme.index = len(bm.edges) - 1

            bmf = bm.faces.get(bmv_list)
            if bmf is not None:
                if self.report and self.options_verbose in Ms3dUi.VERBOSE_NORMAL:
                    self.report(
                            {'WARNING', 'INFO'},
                            ms3d_str['WARNING_IMPORT_SKIP_FACE_DOUBLE'].format(
                                    ms3d_triangle_index))
                continue

            bmf = bm.faces.new(bmv_list)
            bmf.index = ms3d_triangle_index
            bmf_normal.normalize()
            bmf.normal = bmf_normal

            # blender uv custom data per "face vertex"
            bmf.loops[0][layer_uv].uv = Vector(
                    (ms3d_triangle.s[0], 1.0 - ms3d_triangle.t[0]))
            bmf.loops[1][layer_uv].uv = Vector(
                    (ms3d_triangle.s[1], 1.0 - ms3d_triangle.t[1]))
            bmf.loops[2][layer_uv].uv = Vector(
                    (ms3d_triangle.s[2], 1.0 - ms3d_triangle.t[2]))

            # ms3d custom data per "mesh face"
            bmf[layer_smoothing_group] = ms3d_triangle.smoothing_group

            blender_group_id = ms3d_to_blender_group_index.get(
                    ms3d_triangle.group_index)
            if blender_group_id is not None:
                bmf[layer_group] = blender_group_id

            if ms3d_triangle.group_index >= 0 \
                    and ms3d_triangle.group_index < len(ms3d_model.groups):
                ms3d_material_index \
                        = ms3d_model.groups[ms3d_triangle.group_index].material_index
                if ms3d_material_index != Ms3dSpec.NONE_GROUP_MATERIAL_INDEX:
                    bmf.material_index = ms3d_material_index
                    # apply diffuse texture image to face, to be visible in 3d view
                    bmf[layer_texture].image = ms3d_to_blender_material.get(
                            ms3d_material_index)

            # helper dictionary for post-processing smoothing_groups
            smoothing_group_blender_face = smoothing_group_blender_faces.get(
                    ms3d_triangle.smoothing_group)
            if smoothing_group_blender_face is None:
                smoothing_group_blender_face = []
                smoothing_group_blender_faces[ms3d_triangle.smoothing_group] \
                        = smoothing_group_blender_face
            smoothing_group_blender_face.append(bmf)
        ##bm.faces.ensure_lookup_table()

        ##########################
        # BMesh stuff:
        # create all sharp edges for blender to make smoothing_groups visible
        for ms3d_smoothing_group_index, blender_face_list \
                in smoothing_group_blender_faces.items():
            edge_dict = {}
            for bmf in blender_face_list:
                bmf.smooth = True
                for bme in bmf.edges:
                    if edge_dict.get(bme) is None:
                        edge_dict[bme] = 0
                    else:
                        edge_dict[bme] += 1
                    bme.seam = (edge_dict[bme] == 0)
                    bme.smooth = (edge_dict[bme] != 0)

        ##########################
        # BMesh stuff:
        # finally transfer BMesh to Mesh
        bm.to_mesh(blender_mesh)
        bm.free()


        #
        # end BMesh stuff
        ####################################################

        blender_mesh.validate(self.options_verbose in Ms3dUi.VERBOSE_MAXIMAL)

        return blender_mesh_object


    ###########################################################################
    def create_animation(self, blender_context, ms3d_model, blender_mesh_object):
        ##########################
        # setup scene
        blender_scene = blender_context.scene
        blender_scene.render.fps = ms3d_model.animation_fps
        if ms3d_model.animation_fps:
            blender_scene.render.fps_base = (blender_scene.render.fps /
                    ms3d_model.animation_fps)

        blender_scene.frame_start = 1
        blender_scene.frame_end = (ms3d_model.number_total_frames
                + blender_scene.frame_start) - 1
        blender_scene.frame_current = (ms3d_model.current_time
                * ms3d_model.animation_fps)

        ##########################
        if not ms3d_model.joints:
            return

        ##########################
        ms3d_armature_name = FORMAT_ARMATURE.format(ms3d_model.name)
        ms3d_armature_object_name = FORMAT_ARMATURE_OBJECT.format(ms3d_model.name)
        ms3d_action_name = FORMAT_ACTION.format(ms3d_model.name)

        ##########################
        # create new blender_armature_object
        blender_armature = blender_context.blend_data.armatures.new(
                ms3d_armature_name)
        blender_armature.ms3d.name = ms3d_model.name
        blender_armature.draw_type = 'STICK'
        blender_armature.show_axes = True
        blender_armature.use_auto_ik = True
        blender_armature_object = blender_context.blend_data.objects.new(
                ms3d_armature_object_name, blender_armature)
        blender_scene.objects.link(blender_armature_object)
        #blender_armature_object.location = blender_scene.cursor_location
        blender_armature_object.show_x_ray = True

        ##########################
        # create new modifier
        blender_modifier = blender_mesh_object.modifiers.new(
                blender_armature.name, type='ARMATURE')
        blender_modifier.show_expanded = False
        blender_modifier.use_vertex_groups = True
        blender_modifier.use_bone_envelopes = False
        blender_modifier.object = blender_armature_object

        ##########################
        # prepare for vertex groups
        ms3d_to_blender_vertex_groups = {}
        for ms3d_vertex_index, ms3d_vertex in enumerate(ms3d_model.vertices):
            # prepare for later use for blender vertex group
            if ms3d_vertex.bone_id != Ms3dSpec.NONE_VERTEX_BONE_ID:
                if ms3d_vertex.vertex_ex_object \
                        and ( \
                        ms3d_vertex.vertex_ex_object.bone_ids[0] != \
                                Ms3dSpec.NONE_VERTEX_BONE_ID \
                        or ms3d_vertex.vertex_ex_object.bone_ids[1] != \
                                Ms3dSpec.NONE_VERTEX_BONE_ID \
                        or ms3d_vertex.vertex_ex_object.bone_ids[2] != \
                                Ms3dSpec.NONE_VERTEX_BONE_ID \
                        ):
                    ms3d_vertex_group_ids_weights = []
                    ms3d_vertex_group_ids_weights.append(
                            (ms3d_vertex.bone_id,
                            float(ms3d_vertex.vertex_ex_object.weights[0] % 101) / 100.0,
                            ))
                    if ms3d_vertex.vertex_ex_object.bone_ids[0] != \
                            Ms3dSpec.NONE_VERTEX_BONE_ID:
                        ms3d_vertex_group_ids_weights.append(
                                (ms3d_vertex.vertex_ex_object.bone_ids[0],
                                float(ms3d_vertex.vertex_ex_object.weights[1] % 101) / 100.0
                                ))
                    if ms3d_vertex.vertex_ex_object.bone_ids[1] != \
                            Ms3dSpec.NONE_VERTEX_BONE_ID:
                        ms3d_vertex_group_ids_weights.append(
                                (ms3d_vertex.vertex_ex_object.bone_ids[1],
                                float(ms3d_vertex.vertex_ex_object.weights[2] % 101) / 100.0
                                ))
                    if ms3d_vertex.vertex_ex_object.bone_ids[2] != \
                            Ms3dSpec.NONE_VERTEX_BONE_ID:
                        ms3d_vertex_group_ids_weights.append(
                                (ms3d_vertex.vertex_ex_object.bone_ids[2],
                                1.0 -
                                float((ms3d_vertex.vertex_ex_object.weights[0] % 101)
                                + (ms3d_vertex.vertex_ex_object.weights[1] % 101)
                                + (ms3d_vertex.vertex_ex_object.weights[2] % 101)) / 100.0
                                ))

                else:
                    ms3d_vertex_group_ids_weights = [(ms3d_vertex.bone_id, 1.0), ]

                for ms3d_vertex_group_id_weight in ms3d_vertex_group_ids_weights:
                    ms3d_vertex_group_id = ms3d_vertex_group_id_weight[0]
                    blender_vertex_weight = ms3d_vertex_group_id_weight[1]
                    blender_vertex_group = ms3d_to_blender_vertex_groups.get(
                            ms3d_vertex_group_id)
                    if blender_vertex_group is None:
                        ms3d_to_blender_vertex_groups[ms3d_vertex_group_id] \
                                = blender_vertex_group = []
                    blender_vertex_group.append((ms3d_vertex_index,
                            blender_vertex_weight))

        ##########################
        # blender stuff:
        # create all vertex groups to be used for bones
        for ms3d_bone_id, blender_vertex_index_weight_list \
                in ms3d_to_blender_vertex_groups.items():
            ms3d_name = ms3d_model.joints[ms3d_bone_id].name
            blender_vertex_group = blender_mesh_object.vertex_groups.new(
                    ms3d_name)
            for blender_vertex_id_weight in blender_vertex_index_weight_list:
                blender_vertex_index = blender_vertex_id_weight[0]
                blender_vertex_weight = blender_vertex_id_weight[1]
                blender_vertex_group.add((blender_vertex_index, ),
                        blender_vertex_weight, 'ADD')

        ##########################
        # bring joints in the correct order
        ms3d_joints_ordered = []
        self.build_ms3d_joint_dependency_order(ms3d_model.joints,
                ms3d_joints_ordered)

        ##########################
        # prepare joint data for later use
        ms3d_joint_by_name = {}
        for ms3d_joint in ms3d_joints_ordered:
            item = ms3d_joint_by_name.get(ms3d_joint.name)
            if item is None:
                ms3d_joint.__children = []
                ms3d_joint_by_name[ms3d_joint.name] = ms3d_joint

            matrix_local_rot = (Matrix.Rotation(ms3d_joint.rotation[2], 4, 'Z')
                    * Matrix.Rotation(ms3d_joint.rotation[1], 4, 'Y')
                    ) * Matrix.Rotation(ms3d_joint.rotation[0], 4, 'X')
            matrix_local = Matrix.Translation(Vector(ms3d_joint.position)
                    ) * matrix_local_rot

            ms3d_joint.__matrix_local_rot = matrix_local_rot
            ms3d_joint.__matrix_global_rot = matrix_local_rot
            ms3d_joint.__matrix_local = matrix_local
            ms3d_joint.__matrix_global = matrix_local

            if ms3d_joint.parent_name:
                ms3d_joint_parent = ms3d_joint_by_name.get(
                        ms3d_joint.parent_name)
                if ms3d_joint_parent is not None:
                    ms3d_joint_parent.__children.append(ms3d_joint)

                    matrix_global = ms3d_joint_parent.__matrix_global \
                            * matrix_local
                    ms3d_joint.__matrix_global = matrix_global

                    matrix_global_rot = ms3d_joint_parent.__matrix_global_rot \
                            * matrix_local_rot
                    ms3d_joint.__matrix_global_rot = matrix_global_rot

        ##########################
        # ms3d_joint to blender_edit_bone
        if ms3d_model.model_ex_object and not self.options_use_joint_size:
            joint_length = ms3d_model.model_ex_object.joint_size
        else:
            joint_length = self.options_joint_size
        if joint_length < 0.01:
            joint_length = 0.01

        blender_scene.objects.active = blender_armature_object
        enable_edit_mode(True, blender_context)
        for ms3d_joint in ms3d_joints_ordered:
            blender_edit_bone = blender_armature.edit_bones.new(ms3d_joint.name)
            blender_edit_bone.use_connect = False
            blender_edit_bone.use_inherit_rotation = True
            blender_edit_bone.use_inherit_scale = True
            blender_edit_bone.use_local_location = True
            blender_armature.edit_bones.active = blender_edit_bone

            ms3d_joint = ms3d_joint_by_name[ms3d_joint.name]
            ms3d_joint_vector = ms3d_joint.__matrix_global * Vector()

            blender_edit_bone.head \
                    = self.geometry_correction(ms3d_joint_vector)

            vector_tail_end_up = ms3d_joint.__matrix_global_rot * Vector((0,1,0))
            vector_tail_end_dir = ms3d_joint.__matrix_global_rot * Vector((0,0,1))
            vector_tail_end_up.normalize()
            vector_tail_end_dir.normalize()
            blender_edit_bone.tail = blender_edit_bone.head \
                    + self.geometry_correction(
                    vector_tail_end_dir * joint_length)
            blender_edit_bone.align_roll(self.geometry_correction(
                    vector_tail_end_up))

            if ms3d_joint.parent_name:
                ms3d_joint_parent = ms3d_joint_by_name[ms3d_joint.parent_name]
                blender_edit_bone_parent = ms3d_joint_parent.blender_edit_bone
                blender_edit_bone.parent = blender_edit_bone_parent

            ms3d_joint.blender_bone_name = blender_edit_bone.name
            ms3d_joint.blender_edit_bone = blender_edit_bone
        enable_edit_mode(False, blender_context)

        if self.options_use_joint_to_bones:
            enable_edit_mode(True, blender_context)
            for ms3d_joint in ms3d_joints_ordered:
                blender_edit_bone = blender_armature.edit_bones[ms3d_joint.name]
                if blender_edit_bone.children:
                    new_length = 0.0
                    for child_bone in blender_edit_bone.children:
                        length = (child_bone.head - blender_edit_bone.head).length
                        if new_length <= 0 or length < new_length:
                            new_length = length
                    if new_length >= 0.01:
                        direction = blender_edit_bone.tail - blender_edit_bone.head
                        direction.normalize()
                        blender_edit_bone.tail = blender_edit_bone.head + (direction * new_length)
            enable_edit_mode(False, blender_context)

        ##########################
        # post process bones
        enable_edit_mode(False, blender_context)
        for ms3d_joint_name, ms3d_joint in ms3d_joint_by_name.items():
            blender_bone = blender_armature.bones.get(
                    ms3d_joint.blender_bone_name)
            if blender_bone is None:
                continue

            blender_bone.ms3d.name = ms3d_joint.name
            blender_bone.ms3d.flags = Ms3dUi.flags_from_ms3d(ms3d_joint.flags)

            ms3d_joint_ex = ms3d_joint.joint_ex_object
            if ms3d_joint_ex is not None:
                blender_bone.ms3d.color = ms3d_joint_ex.color

            ms3d_comment = ms3d_joint.comment_object
            if ms3d_comment is not None:
                blender_bone.ms3d.comment = ms3d_comment.comment

        ##########################
        if not self.options_use_animation:
            return blender_armature_object


        ##########################
        # process pose bones
        enable_pose_mode(True, blender_context)

        blender_action = blender_context.blend_data.actions.new(ms3d_action_name)
        if blender_armature_object.animation_data is None:
            blender_armature_object.animation_data_create()
        blender_armature_object.animation_data.action = blender_action

        ##########################
        # transition between keys may be incorrect
        # because of the gimbal-lock problem!
        # http://www.youtube.com/watch?v=zc8b2Jo7mno
        # http://www.youtube.com/watch?v=rrUCBOlJdt4
        # you can fix it manually by selecting the affected keyframes
        # and apply the following option to it:
        # "Graph Editor -> Key -> Discontinuity (Euler) Filter"
        # ==> "bpy.ops.graph.euler_filter()"
        # but this option is only available for Euler rotation f-curves!
        #
        for ms3d_joint_name, ms3d_joint in ms3d_joint_by_name.items():
            blender_pose_bone = blender_armature_object.pose.bones.get(
                    ms3d_joint.blender_bone_name)
            if blender_pose_bone is None:
                continue

            data_path = blender_pose_bone.path_from_id('location')
            fcurve_location_x = blender_action.fcurves.new(data_path, index=0)
            fcurve_location_y = blender_action.fcurves.new(data_path, index=1)
            fcurve_location_z = blender_action.fcurves.new(data_path, index=2)
            for translation_key_frames in ms3d_joint.translation_key_frames:
                frame = (translation_key_frames.time * ms3d_model.animation_fps)
                matrix_local = Matrix.Translation(
                        Vector(translation_key_frames.position))
                v = (matrix_local) * Vector()
                fcurve_location_x.keyframe_points.insert(frame, -v[0])
                fcurve_location_y.keyframe_points.insert(frame, v[2])
                fcurve_location_z.keyframe_points.insert(frame, v[1])

            if self.options_use_quaternion_rotation:
                blender_pose_bone.rotation_mode = 'QUATERNION'
                data_path = blender_pose_bone.path_from_id("rotation_quaternion")
                fcurve_rotation_w = blender_action.fcurves.new(data_path, index=0)
                fcurve_rotation_x = blender_action.fcurves.new(data_path, index=1)
                fcurve_rotation_y = blender_action.fcurves.new(data_path, index=2)
                fcurve_rotation_z = blender_action.fcurves.new(data_path, index=3)
                for rotation_key_frames in ms3d_joint.rotation_key_frames:
                    frame = (rotation_key_frames.time * ms3d_model.animation_fps)
                    matrix_local_rot = (
                            Matrix.Rotation(
                                    rotation_key_frames.rotation[2], 4, 'Y')
                            * Matrix.Rotation(
                                    rotation_key_frames.rotation[1], 4, 'Z')
                            ) * Matrix.Rotation(
                                    -rotation_key_frames.rotation[0], 4, 'X')
                    q = (matrix_local_rot).to_quaternion()
                    fcurve_rotation_w.keyframe_points.insert(frame, q.w)
                    fcurve_rotation_x.keyframe_points.insert(frame, q.x)
                    fcurve_rotation_y.keyframe_points.insert(frame, q.y)
                    fcurve_rotation_z.keyframe_points.insert(frame, q.z)
            else:
                blender_pose_bone.rotation_mode = 'XZY'
                data_path = blender_pose_bone.path_from_id("rotation_euler")
                fcurve_rotation_x = blender_action.fcurves.new(data_path, index=0)
                fcurve_rotation_y = blender_action.fcurves.new(data_path, index=1)
                fcurve_rotation_z = blender_action.fcurves.new(data_path, index=2)
                for rotation_key_frames in ms3d_joint.rotation_key_frames:
                    frame = (rotation_key_frames.time * ms3d_model.animation_fps)
                    fcurve_rotation_x.keyframe_points.insert(
                            frame, -rotation_key_frames.rotation[0])
                    fcurve_rotation_y.keyframe_points.insert(
                            frame, rotation_key_frames.rotation[2])
                    fcurve_rotation_z.keyframe_points.insert(
                            frame, rotation_key_frames.rotation[1])

        enable_pose_mode(False, blender_context)

        return blender_armature_object


    ###########################################################################
    def geometry_correction(self, value):
        return Vector((value[2], value[0], value[1]))


    ###########################################################################
    def build_ms3d_joint_dependency_order(self, ms3d_joints, ms3d_joints_ordered):
        ms3d_joints_children = {"": {}}
        for ms3d_joint in ms3d_joints:
            if ms3d_joint.parent_name:
                ms3d_joint_children = ms3d_joints_children.get(
                        ms3d_joint.parent_name)
                if ms3d_joint_children is None:
                    ms3d_joint_children = ms3d_joints_children[
                            ms3d_joint.parent_name] = {}
            else:
                ms3d_joint_children = ms3d_joints_children[""]

            ms3d_joint_children[ms3d_joint.name] = ms3d_joint

        self.traverse_dependencies(
                ms3d_joints_ordered,
                ms3d_joints_children,
                "")


        return ms3d_joints_ordered


    ###########################################################################
    def traverse_dependencies(self, ms3d_joints_ordered, ms3d_joints_children,
            key):
        ms3d_joint_children = ms3d_joints_children.get(key)
        if ms3d_joint_children:
            for item in ms3d_joint_children.items():
                ms3d_joint_name = item[0]
                ms3d_joint = item[1]
                ms3d_joints_ordered.append(ms3d_joint)
                self.traverse_dependencies(
                        ms3d_joints_ordered,
                        ms3d_joints_children,
                        ms3d_joint_name)


###############################################################################
#234567890123456789012345678901234567890123456789012345678901234567890123456789
#--------1---------2---------3---------4---------5---------6---------7---------
# ##### END OF FILE #####
