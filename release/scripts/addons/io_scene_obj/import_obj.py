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

# Script copyright (C) Campbell Barton
# Contributors: Campbell Barton, Jiri Hnidek, Paolo Ciccone

"""
This script imports a Wavefront OBJ files to Blender.

Usage:
Run this script from "File->Import" menu and then load the desired OBJ file.
Note, This loads mesh objects and materials only, nurbs and curves are not supported.

http://wiki.blender.org/index.php/Scripts/Manual/Import/wavefront_obj
"""

import array
import os
import time
import bpy
import mathutils
from bpy_extras.io_utils import unpack_list
from bpy_extras.image_utils import load_image

from progress_report import ProgressReport, ProgressReportSubstep


def line_value(line_split):
    """
    Returns 1 string representing the value for this line
    None will be returned if theres only 1 word
    """
    length = len(line_split)
    if length == 1:
        return None

    elif length == 2:
        return line_split[1]

    elif length > 2:
        return b' '.join(line_split[1:])


def obj_image_load(context_imagepath_map, line, DIR, recursive, relpath):
    """
    Mainly uses comprehensiveImageLoad
    But we try all space-separated items from current line when file is not found with last one
    (users keep generating/using image files with spaces in a format that does not support them, sigh...)
    Also tries to replace '_' with ' ' for Max's exporter replaces spaces with underscores.
    """
    filepath_parts = line.split(b' ')
    image = None
    for i in range(-1, -len(filepath_parts), -1):
        imagepath = os.fsdecode(b" ".join(filepath_parts[i:]))
        image = context_imagepath_map.get(imagepath, ...)
        if image is ...:
            image = load_image(imagepath, DIR, recursive=recursive, relpath=relpath)
            if image is None and "_" in imagepath:
                image = load_image(imagepath.replace("_", " "), DIR, recursive=recursive, relpath=relpath)
            if image is not None:
                context_imagepath_map[imagepath] = image
                break;

    if image is None:
        imagepath = os.fsdecode(filepath_parts[-1])
        image = load_image(imagepath, DIR, recursive=recursive, place_holder=True, relpath=relpath)
        context_imagepath_map[imagepath] = image

    return image


def create_materials(filepath, relpath,
                     material_libs, unique_materials, unique_material_images,
                     use_image_search, use_cycles, float_func):
    """
    Create all the used materials in this obj,
    assign colors and images to the materials from all referenced material libs
    """
    DIR = os.path.dirname(filepath)
    context_material_vars = set()

    # Don't load the same image multiple times
    context_imagepath_map = {}

    cycles_material_wrap_map = {}

    def load_material_image(blender_material, mat_wrap, use_cycles, context_material_name, img_data, line, type):
        """
        Set textures defined in .mtl file.
        """
        map_options = {}

        curr_token = []
        for token in img_data[:-1]:
            if token.startswith(b'-'):
                if curr_token:
                    map_options[curr_token[0]] = curr_token[1:]
                curr_token[:] = []
            curr_token.append(token)
        if curr_token:
            map_options[curr_token[0]] = curr_token[1:]

        # Absolute path - c:\.. etc would work here
        image = obj_image_load(context_imagepath_map, line, DIR, use_image_search, relpath)

        texture = bpy.data.textures.new(name=type, type='IMAGE')
        if image is not None:
            texture.image = image

        map_offset = map_options.get(b'-o')
        map_scale = map_options.get(b'-s')

        # Adds textures for materials (rendering)
        if type == 'Kd':
            if use_cycles:
                mat_wrap.diffuse_image_set(image)
                mat_wrap.diffuse_mapping_set(coords='UV', translation=map_offset, scale=map_scale)

            mtex = blender_material.texture_slots.add()
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_color_diffuse = True

            # adds textures to faces (Textured/Alt-Z mode)
            # Only apply the diffuse texture to the face if the image has not been set with the inline usemat func.
            unique_material_images[context_material_name] = image  # set the texface image

        elif type == 'Ka':
            if use_cycles:
                # XXX Not supported?
                print("WARNING, currently unsupported ambient texture, skipped.")

            mtex = blender_material.texture_slots.add()
            mtex.use_map_color_diffuse = False
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_ambient = True

        elif type == 'Ks':
            if use_cycles:
                mat_wrap.specular_image_set(image)
                mat_wrap.specular_mapping_set(coords='UV', translation=map_offset, scale=map_scale)

            mtex = blender_material.texture_slots.add()
            mtex.use_map_color_diffuse = False
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_color_spec = True

        elif type == 'Ke':
            if use_cycles:
                # XXX Not supported?
                print("WARNING, currently unsupported emit texture, skipped.")

            mtex = blender_material.texture_slots.add()
            mtex.use_map_color_diffuse = False
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_emit = True

        elif type == 'Bump':
            bump_mult = map_options.get(b'-bm')
            bump_mult = float(bump_mult[0]) if (bump_mult is not None and len(bump_mult) > 1) else 1.0

            if use_cycles:
                mat_wrap.normal_image_set(image)
                mat_wrap.normal_mapping_set(coords='UV', translation=map_offset, scale=map_scale)
                if bump_mult:
                    mat_wrap.normal_factor_set(bump_mult)

            mtex = blender_material.texture_slots.add()
            mtex.use_map_color_diffuse = False
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_normal = True
            if bump_mult:
                mtex.normal_factor = bump_mult

        elif type == 'D':
            if use_cycles:
                mat_wrap.alpha_image_set(image)
                mat_wrap.alpha_mapping_set(coords='UV', translation=map_offset, scale=map_scale)

            mtex = blender_material.texture_slots.add()
            mtex.use_map_color_diffuse = False
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_alpha = True
            blender_material.use_transparency = True
            blender_material.transparency_method = 'Z_TRANSPARENCY'
            if "alpha" not in context_material_vars:
                blender_material.alpha = 0.0
            # Todo, unset diffuse material alpha if it has an alpha channel

        elif type == 'disp':
            if use_cycles:
                mat_wrap.bump_image_set(image)
                mat_wrap.bump_mapping_set(coords='UV', translation=map_offset, scale=map_scale)

            mtex = blender_material.texture_slots.add()
            mtex.use_map_color_diffuse = False
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_displacement = True

        elif type == 'refl':
            map_type = map_options.get(b'-type')
            if map_type and map_type != [b'sphere']:
                print("WARNING, unsupported reflection type '%s', defaulting to 'sphere'"
                      "" % ' '.join(i.decode() for i in map_type))

            if use_cycles:
                mat_wrap.diffuse_image_set(image, projection='SPHERE')
                mat_wrap.diffuse_mapping_set(coords='Reflection', translation=map_offset, scale=map_scale)

            mtex = blender_material.texture_slots.add()
            mtex.use_map_color_diffuse = False
            mtex.texture = texture
            mtex.texture_coords = 'REFLECTION'
            mtex.use_map_color_diffuse = True
            mtex.mapping = 'SPHERE'
        else:
            raise Exception("invalid type %r" % type)

        if map_offset:
            mtex.offset.x = float(map_offset[0])
            if len(map_offset) >= 2:
                mtex.offset.y = float(map_offset[1])
            if len(map_offset) >= 3:
                mtex.offset.z = float(map_offset[2])
        if map_scale:
            mtex.scale.x = float(map_scale[0])
            if len(map_scale) >= 2:
                mtex.scale.y = float(map_scale[1])
            if len(map_scale) >= 3:
                mtex.scale.z = float(map_scale[2])

    # Add an MTL with the same name as the obj if no MTLs are spesified.
    temp_mtl = os.path.splitext((os.path.basename(filepath)))[0] + ".mtl"

    if os.path.exists(os.path.join(DIR, temp_mtl)):
        material_libs.add(temp_mtl)
    del temp_mtl

    # Create new materials
    for name in unique_materials:  # .keys()
        if name is not None:
            ma = unique_materials[name] = bpy.data.materials.new(name.decode('utf-8', "replace"))
            unique_material_images[name] = None  # assign None to all material images to start with, add to later.
            if use_cycles:
                from modules import cycles_shader_compat
                ma_wrap = cycles_shader_compat.CyclesShaderWrapper(ma)
                cycles_material_wrap_map[ma] = ma_wrap


    # XXX Why was this needed? Cannot find any good reason, and adds stupid empty matslot in case we do not separate
    #     mesh (see T44947).
    #~ unique_materials[None] = None
    #~ unique_material_images[None] = None

    for libname in sorted(material_libs):
        # print(libname)
        mtlpath = os.path.join(DIR, libname)
        if not os.path.exists(mtlpath):
            print("\tMaterial not found MTL: %r" % mtlpath)
        else:
            do_ambient = True
            do_highlight = False
            do_reflection = False
            do_transparency = False
            do_glass = False
            do_fresnel = False
            do_raytrace = False
            emit_colors = [0.0, 0.0, 0.0]

            # print('\t\tloading mtl: %e' % mtlpath)
            context_material = None
            context_mat_wrap = None
            mtl = open(mtlpath, 'rb')
            for line in mtl:  # .readlines():
                line = line.strip()
                if not line or line.startswith(b'#'):
                    continue

                line_split = line.split()
                line_id = line_split[0].lower()

                if line_id == b'newmtl':
                    # Finalize previous mat, if any.
                    if context_material:
                        emit_value = sum(emit_colors) / 3.0
                        if emit_value > 1e-6:
                            if use_cycles:
                                print("WARNING, currently unsupported emit value, skipped.")
                            # We have to adapt it to diffuse color too...
                            emit_value /= sum(context_material.diffuse_color) / 3.0
                        context_material.emit = emit_value

                        if not do_ambient:
                            context_material.ambient = 0.0

                        if do_highlight:
                            if use_cycles:
                                context_mat_wrap.hardness_value_set(1.0)
                            # FIXME, how else to use this?
                            context_material.specular_intensity = 1.0
                        else:
                            if use_cycles:
                                context_mat_wrap.hardness_value_set(0.0)

                        if do_reflection:
                            if use_cycles:
                                context_mat_wrap.reflect_factor_set(1.0)
                            context_material.raytrace_mirror.use = True
                            context_material.raytrace_mirror.reflect_factor = 1.0

                        if do_transparency:
                            context_material.use_transparency = True
                            context_material.transparency_method = 'RAYTRACE' if do_raytrace else 'Z_TRANSPARENCY'
                            if "alpha" not in context_material_vars:
                                if use_cycles:
                                    context_mat_wrap.alpha_value_set(0.0)
                                context_material.alpha = 0.0

                        if do_glass:
                            if use_cycles:
                                print("WARNING, currently unsupported glass material, skipped.")
                            if "ior" not in context_material_vars:
                                context_material.raytrace_transparency.ior = 1.5

                        if do_fresnel:
                            if use_cycles:
                                print("WARNING, currently unsupported fresnel option, skipped.")
                            context_material.raytrace_mirror.fresnel = 1.0  # could be any value for 'ON'

                        """
                        if do_raytrace:
                            context_material.use_raytrace = True
                        else:
                            context_material.use_raytrace = False
                        """
                        # XXX, this is not following the OBJ spec, but this was
                        # written when raytracing wasnt default, annoying to disable for blender users.
                        context_material.use_raytrace = True

                    context_material_name = line_value(line_split)
                    context_material = unique_materials.get(context_material_name)
                    if use_cycles and context_material is not None:
                        context_mat_wrap = cycles_material_wrap_map[context_material]
                    context_material_vars.clear()

                    emit_colors[:] = [0.0, 0.0, 0.0]
                    do_ambient = True
                    do_highlight = False
                    do_reflection = False
                    do_transparency = False
                    do_glass = False
                    do_fresnel = False
                    do_raytrace = False


                elif context_material:
                    # we need to make a material to assign properties to it.
                    if line_id == b'ka':
                        col = (float_func(line_split[1]), float_func(line_split[2]), float_func(line_split[3]))
                        if use_cycles:
                            context_mat_wrap.reflect_color_set(col)
                        context_material.mirror_color = col
                        # This is highly approximated, but let's try to stick as close from exporter as possible... :/
                        context_material.ambient = sum(context_material.mirror_color) / 3
                    elif line_id == b'kd':
                        col = (float_func(line_split[1]), float_func(line_split[2]), float_func(line_split[3]))
                        if use_cycles:
                            context_mat_wrap.diffuse_color_set(col)
                        context_material.diffuse_color = col
                        context_material.diffuse_intensity = 1.0
                    elif line_id == b'ks':
                        col = (float_func(line_split[1]), float_func(line_split[2]), float_func(line_split[3]))
                        if use_cycles:
                            context_mat_wrap.specular_color_set(col)
                            context_mat_wrap.hardness_value_set(1.0)
                        context_material.specular_color = col
                        context_material.specular_intensity = 1.0
                    elif line_id == b'ke':
                        # We cannot set context_material.emit right now, we need final diffuse color as well for this.
                        emit_colors[:] = [
                            float_func(line_split[1]), float_func(line_split[2]), float_func(line_split[3])]
                    elif line_id == b'ns':
                        if use_cycles:
                            context_mat_wrap.hardness_value_set(((float_func(line_split[1]) + 3.0) / 50.0) - 0.65)
                        context_material.specular_hardness = int((float_func(line_split[1]) * 0.51) + 1)
                    elif line_id == b'ni':  # Refraction index (between 1 and 3).
                        if use_cycles:
                            print("WARNING, currently unsupported glass material, skipped.")
                        context_material.raytrace_transparency.ior = max(1, min(float_func(line_split[1]), 3))
                        context_material_vars.add("ior")
                    elif line_id == b'd':  # dissolve (transparency)
                        if use_cycles:
                            context_mat_wrap.alpha_value_set(float_func(line_split[1]))
                        context_material.alpha = float_func(line_split[1])
                        context_material.use_transparency = True
                        context_material.transparency_method = 'Z_TRANSPARENCY'
                        context_material_vars.add("alpha")
                    elif line_id == b'tr':  # translucency
                        if use_cycles:
                            print("WARNING, currently unsupported translucency option, skipped.")
                        context_material.translucency = float_func(line_split[1])
                    elif line_id == b'tf':
                        # rgb, filter color, blender has no support for this.
                        pass
                    elif line_id == b'illum':
                        illum = int(line_split[1])

                        # inline comments are from the spec, v4.2
                        if illum == 0:
                            # Color on and Ambient off
                            do_ambient = False
                        elif illum == 1:
                            # Color on and Ambient on
                            pass
                        elif illum == 2:
                            # Highlight on
                            do_highlight = True
                        elif illum == 3:
                            # Reflection on and Ray trace on
                            do_reflection = True
                            do_raytrace = True
                        elif illum == 4:
                            # Transparency: Glass on
                            # Reflection: Ray trace on
                            do_transparency = True
                            do_reflection = True
                            do_glass = True
                            do_raytrace = True
                        elif illum == 5:
                            # Reflection: Fresnel on and Ray trace on
                            do_reflection = True
                            do_fresnel = True
                            do_raytrace = True
                        elif illum == 6:
                            # Transparency: Refraction on
                            # Reflection: Fresnel off and Ray trace on
                            do_transparency = True
                            do_reflection = True
                            do_raytrace = True
                        elif illum == 7:
                            # Transparency: Refraction on
                            # Reflection: Fresnel on and Ray trace on
                            do_transparency = True
                            do_reflection = True
                            do_fresnel = True
                            do_raytrace = True
                        elif illum == 8:
                            # Reflection on and Ray trace off
                            do_reflection = True
                        elif illum == 9:
                            # Transparency: Glass on
                            # Reflection: Ray trace off
                            do_transparency = True
                            do_reflection = True
                            do_glass = True
                        elif illum == 10:
                            # Casts shadows onto invisible surfaces

                            # blender can't do this
                            pass

                    elif line_id == b'map_ka':
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'Ka')
                    elif line_id == b'map_ks':
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'Ks')
                    elif line_id == b'map_kd':
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'Kd')
                    elif line_id == b'map_ke':
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'Ke')
                    elif line_id in {b'map_bump', b'bump'}:  # 'bump' is incorrect but some files use it.
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'Bump')
                    elif line_id in {b'map_d', b'map_tr'}:  # Alpha map - Dissolve
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'D')

                    elif line_id in {b'map_disp', b'disp'}:  # displacementmap
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'disp')

                    elif line_id in {b'map_refl', b'refl'}:  # reflectionmap
                        img_data = line.split()[1:]
                        if img_data:
                            load_material_image(context_material, context_mat_wrap, use_cycles,
                                                context_material_name, img_data, line, 'refl')
                    else:
                        print("\t%r:%r (ignored)" % (filepath, line))
            mtl.close()


def split_mesh(verts_loc, faces, unique_materials, filepath, SPLIT_OB_OR_GROUP):
    """
    Takes vert_loc and faces, and separates into multiple sets of
    (verts_loc, faces, unique_materials, dataname)
    """

    filename = os.path.splitext((os.path.basename(filepath)))[0]

    if not SPLIT_OB_OR_GROUP or not faces:
        use_verts_nor = any((False if f[1] is ... else True) for f in faces)
        use_verts_tex = any((False if f[2] is ... else True) for f in faces)
        # use the filename for the object name since we aren't chopping up the mesh.
        return [(verts_loc, faces, unique_materials, filename, use_verts_nor, use_verts_tex)]

    def key_to_name(key):
        # if the key is a tuple, join it to make a string
        if not key:
            return filename  # assume its a string. make sure this is true if the splitting code is changed
        else:
            return key.decode('utf-8', 'replace')

    # Return a key that makes the faces unique.
    face_split_dict = {}

    oldkey = -1  # initialize to a value that will never match the key

    for face in faces:
        key = face[5]

        if oldkey != key:
            # Check the key has changed.
            (verts_split, faces_split, unique_materials_split, vert_remap,
             use_verts_nor, use_verts_tex) = face_split_dict.setdefault(key, ([], [], {}, {}, [], []))
            oldkey = key

        face_vert_loc_indices = face[0]

        if not use_verts_nor and face[1] is not ...:
            use_verts_nor.append(True)

        if not use_verts_tex and face[2] is not ...:
            use_verts_tex.append(True)

        # Remap verts to new vert list and add where needed
        for enum, i in enumerate(face_vert_loc_indices):
            map_index = vert_remap.get(i)
            if map_index is None:
                map_index = len(verts_split)
                vert_remap[i] = map_index  # set the new remapped index so we only add once and can reference next time.
                verts_split.append(verts_loc[i])  # add the vert to the local verts

            face_vert_loc_indices[enum] = map_index  # remap to the local index

            matname = face[3]
            if matname and matname not in unique_materials_split:
                unique_materials_split[matname] = unique_materials[matname]

        faces_split.append(face)

    # remove one of the items and reorder
    return [(verts_split, faces_split, unique_materials_split, key_to_name(key), bool(use_vnor), bool(use_vtex))
            for key, (verts_split, faces_split, unique_materials_split, _, use_vnor, use_vtex)
            in face_split_dict.items()]


def create_mesh(new_objects,
                use_edges,
                verts_loc,
                verts_nor,
                verts_tex,
                faces,
                unique_materials,
                unique_material_images,
                unique_smooth_groups,
                vertex_groups,
                dataname,
                ):
    """
    Takes all the data gathered and generates a mesh, adding the new object to new_objects
    deals with ngons, sharp edges and assigning materials
    """

    if unique_smooth_groups:
        sharp_edges = set()
        smooth_group_users = {context_smooth_group: {} for context_smooth_group in unique_smooth_groups.keys()}
        context_smooth_group_old = -1

    fgon_edges = set()  # Used for storing fgon keys when we need to tesselate/untesselate them (ngons with hole).
    edges = []
    tot_loops = 0

    context_object = None

    # reverse loop through face indices
    for f_idx in range(len(faces) - 1, -1, -1):
        (face_vert_loc_indices,
         face_vert_nor_indices,
         face_vert_tex_indices,
         context_material,
         context_smooth_group,
         context_object,
         face_invalid_blenpoly,
         ) = faces[f_idx]

        len_face_vert_loc_indices = len(face_vert_loc_indices)

        if len_face_vert_loc_indices == 1:
            faces.pop(f_idx)  # cant add single vert faces

        # Face with a single item in face_vert_nor_indices is actually a polyline!
        elif len(face_vert_nor_indices) == 1 or len_face_vert_loc_indices == 2:
            if use_edges:
                edges.extend((face_vert_loc_indices[i], face_vert_loc_indices[i + 1])
                             for i in range(len_face_vert_loc_indices - 1))
            faces.pop(f_idx)

        else:
            # Smooth Group
            if unique_smooth_groups and context_smooth_group:
                # Is a part of of a smooth group and is a face
                if context_smooth_group_old is not context_smooth_group:
                    edge_dict = smooth_group_users[context_smooth_group]
                    context_smooth_group_old = context_smooth_group

                prev_vidx = face_vert_loc_indices[-1]
                for vidx in face_vert_loc_indices:
                    edge_key = (prev_vidx, vidx) if (prev_vidx < vidx) else (vidx, prev_vidx)
                    prev_vidx = vidx
                    edge_dict[edge_key] = edge_dict.get(edge_key, 0) + 1

            # NGons into triangles
            if face_invalid_blenpoly:
                # ignore triangles with invalid indices
                if len(face_vert_loc_indices) > 3:
                    from bpy_extras.mesh_utils import ngon_tessellate
                    ngon_face_indices = ngon_tessellate(verts_loc, face_vert_loc_indices)
                    faces.extend([([face_vert_loc_indices[ngon[0]],
                                    face_vert_loc_indices[ngon[1]],
                                    face_vert_loc_indices[ngon[2]],
                                    ],
                                [face_vert_nor_indices[ngon[0]],
                                    face_vert_nor_indices[ngon[1]],
                                    face_vert_nor_indices[ngon[2]],
                                    ] if face_vert_nor_indices else [],
                                [face_vert_tex_indices[ngon[0]],
                                    face_vert_tex_indices[ngon[1]],
                                    face_vert_tex_indices[ngon[2]],
                                    ] if face_vert_tex_indices else [],
                                context_material,
                                context_smooth_group,
                                context_object,
                                [],
                                )
                                for ngon in ngon_face_indices]
                                )
                    tot_loops += 3 * len(ngon_face_indices)

                    # edges to make ngons
                    if len(ngon_face_indices) > 1:
                        edge_users = set()
                        for ngon in ngon_face_indices:
                            prev_vidx = face_vert_loc_indices[ngon[-1]]
                            for ngidx in ngon:
                                vidx = face_vert_loc_indices[ngidx]
                                if vidx == prev_vidx:
                                    continue  # broken OBJ... Just skip.
                                edge_key = (prev_vidx, vidx) if (prev_vidx < vidx) else (vidx, prev_vidx)
                                prev_vidx = vidx
                                if edge_key in edge_users:
                                    fgon_edges.add(edge_key)
                                else:
                                    edge_users.add(edge_key)

                faces.pop(f_idx)
            else:
                tot_loops += len_face_vert_loc_indices

    # Build sharp edges
    if unique_smooth_groups:
        for edge_dict in smooth_group_users.values():
            for key, users in edge_dict.items():
                if users == 1:  # This edge is on the boundry of a group
                    sharp_edges.add(key)

    # map the material names to an index
    material_mapping = {name: i for i, name in enumerate(unique_materials)}  # enumerate over unique_materials keys()

    materials = [None] * len(unique_materials)

    for name, index in material_mapping.items():
        materials[index] = unique_materials[name]

    me = bpy.data.meshes.new(dataname)

    # make sure the list isnt too big
    for material in materials:
        me.materials.append(material)

    me.vertices.add(len(verts_loc))
    me.loops.add(tot_loops)
    me.polygons.add(len(faces))

    # verts_loc is a list of (x, y, z) tuples
    me.vertices.foreach_set("co", unpack_list(verts_loc))

    loops_vert_idx = []
    faces_loop_start = []
    faces_loop_total = []
    lidx = 0
    for f in faces:
        vidx = f[0]
        nbr_vidx = len(vidx)
        loops_vert_idx.extend(vidx)
        faces_loop_start.append(lidx)
        faces_loop_total.append(nbr_vidx)
        lidx += nbr_vidx

    me.loops.foreach_set("vertex_index", loops_vert_idx)
    me.polygons.foreach_set("loop_start", faces_loop_start)
    me.polygons.foreach_set("loop_total", faces_loop_total)

    if verts_nor and me.loops:
        # Note: we store 'temp' normals in loops, since validate() may alter final mesh,
        #       we can only set custom lnors *after* calling it.
        me.create_normals_split()

    if verts_tex and me.polygons:
        me.uv_textures.new()

    context_material_old = -1  # avoid a dict lookup
    mat = 0  # rare case it may be un-initialized.

    for i, (face, blen_poly) in enumerate(zip(faces, me.polygons)):
        if len(face[0]) < 3:
            raise Exception("bad face")  # Shall not happen, we got rid of those earlier!

        (face_vert_loc_indices,
         face_vert_nor_indices,
         face_vert_tex_indices,
         context_material,
         context_smooth_group,
         context_object,
         face_invalid_blenpoly,
         ) = face

        if context_smooth_group:
            blen_poly.use_smooth = True

        if context_material:
            if context_material_old is not context_material:
                mat = material_mapping[context_material]
                context_material_old = context_material
            blen_poly.material_index = mat

        if verts_nor and face_vert_nor_indices:
            for face_noidx, lidx in zip(face_vert_nor_indices, blen_poly.loop_indices):
                me.loops[lidx].normal[:] = verts_nor[0 if (face_noidx is ...) else face_noidx]

        if verts_tex and face_vert_tex_indices:
            if context_material:
                image = unique_material_images[context_material]
                if image:  # Can be none if the material dosnt have an image.
                    me.uv_textures[0].data[i].image = image

            blen_uvs = me.uv_layers[0]
            for face_uvidx, lidx in zip(face_vert_tex_indices, blen_poly.loop_indices):
                blen_uvs.data[lidx].uv = verts_tex[0 if (face_uvidx is ...) else face_uvidx]

    use_edges = use_edges and bool(edges)
    if use_edges:
        me.edges.add(len(edges))
        # edges should be a list of (a, b) tuples
        me.edges.foreach_set("vertices", unpack_list(edges))

    me.validate(clean_customdata=False)  # *Very* important to not remove lnors here!
    me.update(calc_edges=use_edges)

    # Un-tessellate as much as possible, in case we had to triangulate some ngons...
    if fgon_edges:
        import bmesh
        bm = bmesh.new()
        bm.from_mesh(me)
        verts = bm.verts[:]
        get = bm.edges.get
        edges = [get((verts[vidx1], verts[vidx2])) for vidx1, vidx2 in fgon_edges]
        try:
            bmesh.ops.dissolve_edges(bm, edges=edges, use_verts=False)
        except:
            # Possible dissolve fails for some edges, but don't fail silently in case this is a real bug.
            import traceback
            traceback.print_exc()

        bm.to_mesh(me)
        bm.free()

    # XXX If validate changes the geometry, this is likely to be broken...
    if unique_smooth_groups and sharp_edges:
        for e in me.edges:
            if e.key in sharp_edges:
                e.use_edge_sharp = True
        me.show_edge_sharp = True

    if verts_nor:
        clnors = array.array('f', [0.0] * (len(me.loops) * 3))
        me.loops.foreach_get("normal", clnors)

        if not unique_smooth_groups:
            me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))

        me.normals_split_custom_set(tuple(zip(*(iter(clnors),) * 3)))
        me.use_auto_smooth = True
        me.show_edge_sharp = True

    ob = bpy.data.objects.new(me.name, me)
    new_objects.append(ob)

    # Create the vertex groups. No need to have the flag passed here since we test for the
    # content of the vertex_groups. If the user selects to NOT have vertex groups saved then
    # the following test will never run
    for group_name, group_indices in vertex_groups.items():
        group = ob.vertex_groups.new(group_name.decode('utf-8', "replace"))
        group.add(group_indices, 1.0, 'REPLACE')


def create_nurbs(context_nurbs, vert_loc, new_objects):
    """
    Add nurbs object to blender, only support one type at the moment
    """
    deg = context_nurbs.get(b'deg', (3,))
    curv_range = context_nurbs.get(b'curv_range')
    curv_idx = context_nurbs.get(b'curv_idx', [])
    parm_u = context_nurbs.get(b'parm_u', [])
    parm_v = context_nurbs.get(b'parm_v', [])
    name = context_nurbs.get(b'name', b'ObjNurb')
    cstype = context_nurbs.get(b'cstype')

    if cstype is None:
        print('\tWarning, cstype not found')
        return
    if cstype != b'bspline':
        print('\tWarning, cstype is not supported (only bspline)')
        return
    if not curv_idx:
        print('\tWarning, curv argument empty or not set')
        return
    if len(deg) > 1 or parm_v:
        print('\tWarning, surfaces not supported')
        return

    cu = bpy.data.curves.new(name.decode('utf-8', "replace"), 'CURVE')
    cu.dimensions = '3D'

    nu = cu.splines.new('NURBS')
    nu.points.add(len(curv_idx) - 1)  # a point is added to start with
    nu.points.foreach_set("co", [co_axis for vt_idx in curv_idx for co_axis in (vert_loc[vt_idx] + (1.0,))])

    nu.order_u = deg[0] + 1

    # get for endpoint flag from the weighting
    if curv_range and len(parm_u) > deg[0] + 1:
        do_endpoints = True
        for i in range(deg[0] + 1):

            if abs(parm_u[i] - curv_range[0]) > 0.0001:
                do_endpoints = False
                break

            if abs(parm_u[-(i + 1)] - curv_range[1]) > 0.0001:
                do_endpoints = False
                break

    else:
        do_endpoints = False

    if do_endpoints:
        nu.use_endpoint_u = True

    # close
    '''
    do_closed = False
    if len(parm_u) > deg[0]+1:
        for i in xrange(deg[0]+1):
            #print curv_idx[i], curv_idx[-(i+1)]

            if curv_idx[i]==curv_idx[-(i+1)]:
                do_closed = True
                break

    if do_closed:
        nu.use_cyclic_u = True
    '''

    ob = bpy.data.objects.new(name.decode('utf-8', "replace"), cu)

    new_objects.append(ob)


def strip_slash(line_split):
    if line_split[-1][-1] == 92:  # '\' char
        if len(line_split[-1]) == 1:
            line_split.pop()  # remove the \ item
        else:
            line_split[-1] = line_split[-1][:-1]  # remove the \ from the end last number
        return True
    return False


def get_float_func(filepath):
    """
    find the float function for this obj file
    - whether to replace commas or not
    """
    file = open(filepath, 'rb')
    for line in file:  # .readlines():
        line = line.lstrip()
        if line.startswith(b'v'):  # vn vt v
            if b',' in line:
                file.close()
                return lambda f: float(f.replace(b',', b'.'))
            elif b'.' in line:
                file.close()
                return float

    file.close()
    # in case all vert values were ints
    return float


def load(context,
         filepath,
         *,
         global_clamp_size=0.0,
         use_smooth_groups=True,
         use_edges=True,
         use_split_objects=True,
         use_split_groups=True,
         use_image_search=True,
         use_groups_as_vgroups=False,
         use_cycles=True,
         relpath=None,
         global_matrix=None
         ):
    """
    Called by the user interface or another script.
    load_obj(path) - should give acceptable results.
    This function passes the file and sends the data off
        to be split into objects and then converted into mesh objects
    """

    def handle_vec(line_start, context_multi_line, line_split, tag, data, vec, vec_len):
        ret_context_multi_line = tag if strip_slash(line_split) else b''
        if line_start == tag:
            vec[:] = [float_func(v) for v in line_split[1:]]
        elif context_multi_line == tag:
            vec += [float_func(v) for v in line_split]
        if not ret_context_multi_line:
            data.append(tuple(vec[:vec_len]))
        return ret_context_multi_line

    def create_face(context_material, context_smooth_group, context_object):
        face_vert_loc_indices = []
        face_vert_nor_indices = []
        face_vert_tex_indices = []
        return (
            face_vert_loc_indices,
            face_vert_nor_indices,
            face_vert_tex_indices,
            context_material,
            context_smooth_group,
            context_object,
            [],  # If non-empty, that face is a Blender-invalid ngon (holes...), need a mutable object for that...
        )

    with ProgressReport(context.window_manager) as progress:
        progress.enter_substeps(1, "Importing OBJ %r..." % filepath)

        if global_matrix is None:
            global_matrix = mathutils.Matrix()

        if use_split_objects or use_split_groups:
            use_groups_as_vgroups = False

        time_main = time.time()

        verts_loc = []
        verts_nor = []
        verts_tex = []
        faces = []  # tuples of the faces
        material_libs = set()  # filenames to material libs this OBJ uses
        vertex_groups = {}  # when use_groups_as_vgroups is true

        # Get the string to float conversion func for this file- is 'float' for almost all files.
        float_func = get_float_func(filepath)

        # Context variables
        context_material = None
        context_smooth_group = None
        context_object = None
        context_vgroup = None

        # Nurbs
        context_nurbs = {}
        nurbs = []
        context_parm = b''  # used by nurbs too but could be used elsewhere

        # Until we can use sets
        unique_materials = {}
        unique_material_images = {}
        unique_smooth_groups = {}
        # unique_obects= {} - no use for this variable since the objects are stored in the face.

        # when there are faces that end with \
        # it means they are multiline-
        # since we use xreadline we cant skip to the next line
        # so we need to know whether
        context_multi_line = b''

        # Per-face handling data.
        face_vert_loc_indices = None
        face_vert_nor_indices = None
        face_vert_tex_indices = None
        face_vert_nor_valid = face_vert_tex_valid = False
        face_items_usage = set()
        face_invalid_blenpoly = None
        prev_vidx = None
        face = None
        vec = []

        progress.enter_substeps(3, "Parsing OBJ file...")
        with open(filepath, 'rb') as f:
            for line in f:  # .readlines():
                line_split = line.split()

                if not line_split:
                    continue

                line_start = line_split[0]  # we compare with this a _lot_

                if line_start == b'v' or context_multi_line == b'v':
                    context_multi_line = handle_vec(line_start, context_multi_line, line_split, b'v', verts_loc, vec, 3)

                elif line_start == b'vn' or context_multi_line == b'vn':
                    context_multi_line = handle_vec(line_start, context_multi_line, line_split, b'vn', verts_nor, vec, 3)

                elif line_start == b'vt' or context_multi_line == b'vt':
                    context_multi_line = handle_vec(line_start, context_multi_line, line_split, b'vt', verts_tex, vec, 2)

                # Handle faces lines (as faces) and the second+ lines of fa multiline face here
                # use 'f' not 'f ' because some objs (very rare have 'fo ' for faces)
                elif line_start == b'f' or context_multi_line == b'f':
                    if not context_multi_line:
                        line_split = line_split[1:]
                        # Instantiate a face
                        face = create_face(context_material, context_smooth_group, context_object)
                        (face_vert_loc_indices, face_vert_nor_indices, face_vert_tex_indices,
                         _1, _2, _3, face_invalid_blenpoly) = face
                        faces.append(face)
                        face_items_usage.clear()
                    # Else, use face_vert_loc_indices and face_vert_tex_indices previously defined and used the obj_face

                    context_multi_line = b'f' if strip_slash(line_split) else b''

                    for v in line_split:
                        obj_vert = v.split(b'/')
                        idx = int(obj_vert[0]) - 1
                        vert_loc_index = (idx + len(verts_loc) + 1) if (idx < 0) else idx
                        # Add the vertex to the current group
                        # *warning*, this wont work for files that have groups defined around verts
                        if use_groups_as_vgroups and context_vgroup:
                            vertex_groups[context_vgroup].append(vert_loc_index)
                        # This a first round to quick-detect ngons that *may* use a same edge more than once.
                        # Potential candidate will be re-checked once we have done parsing the whole face.
                        if not face_invalid_blenpoly:
                            # If we use more than once a same vertex, invalid ngon is suspected.
                            if vert_loc_index in face_items_usage:
                                face_invalid_blenpoly.append(True)
                            else:
                                face_items_usage.add(vert_loc_index)
                        face_vert_loc_indices.append(vert_loc_index)

                        # formatting for faces with normals and textures is
                        # loc_index/tex_index/nor_index
                        if len(obj_vert) > 1 and obj_vert[1] and obj_vert[1] != b'0':
                            idx = int(obj_vert[1]) - 1
                            face_vert_tex_indices.append((idx + len(verts_tex) + 1) if (idx < 0) else idx)
                            face_vert_tex_valid = True
                        else:
                            face_vert_tex_indices.append(...)

                        if len(obj_vert) > 2 and obj_vert[2] and obj_vert[2] != b'0':
                            idx = int(obj_vert[2]) - 1
                            face_vert_nor_indices.append((idx + len(verts_nor) + 1) if (idx < 0) else idx)
                            face_vert_nor_valid = True
                        else:
                            face_vert_nor_indices.append(...)

                    if not context_multi_line:
                        # Clear nor/tex indices in case we had none defined for this face.
                        if not face_vert_nor_valid:
                            face_vert_nor_indices.clear()
                        if not face_vert_tex_valid:
                            face_vert_tex_indices.clear()
                        face_vert_nor_valid = face_vert_tex_valid = False

                        # Means we have finished a face, we have to do final check if ngon is suspected to be blender-invalid...
                        if face_invalid_blenpoly:
                            face_invalid_blenpoly.clear()
                            face_items_usage.clear()
                            prev_vidx = face_vert_loc_indices[-1]
                            for vidx in face_vert_loc_indices:
                                edge_key = (prev_vidx, vidx) if (prev_vidx < vidx) else (vidx, prev_vidx)
                                if edge_key in face_items_usage:
                                    face_invalid_blenpoly.append(True)
                                    break
                                face_items_usage.add(edge_key)
                                prev_vidx = vidx

                elif use_edges and (line_start == b'l' or context_multi_line == b'l'):
                    # very similar to the face load function above with some parts removed
                    if not context_multi_line:
                        line_split = line_split[1:]
                        # Instantiate a face
                        face = create_face(context_material, context_smooth_group, context_object)
                        face_vert_loc_indices = face[0]
                        # XXX A bit hackish, we use special 'value' of face_vert_nor_indices (a single True item) to tag this
                        #     as a polyline, and not a regular face...
                        face[1][:] = [True]
                        faces.append(face)
                    # Else, use face_vert_loc_indices previously defined and used the obj_face

                    context_multi_line = b'l' if strip_slash(line_split) else b''

                    for v in line_split:
                        obj_vert = v.split(b'/')
                        idx = int(obj_vert[0]) - 1
                        face_vert_loc_indices.append((idx + len(verts_loc) + 1) if (idx < 0) else idx)

                elif line_start == b's':
                    if use_smooth_groups:
                        context_smooth_group = line_value(line_split)
                        if context_smooth_group == b'off':
                            context_smooth_group = None
                        elif context_smooth_group:  # is not None
                            unique_smooth_groups[context_smooth_group] = None

                elif line_start == b'o':
                    if use_split_objects:
                        context_object = line_value(line_split)
                        # unique_obects[context_object]= None

                elif line_start == b'g':
                    if use_split_groups:
                        context_object = line_value(line.split())
                        # print 'context_object', context_object
                        # unique_obects[context_object]= None
                    elif use_groups_as_vgroups:
                        context_vgroup = line_value(line.split())
                        if context_vgroup and context_vgroup != b'(null)':
                            vertex_groups.setdefault(context_vgroup, [])
                        else:
                            context_vgroup = None  # dont assign a vgroup

                elif line_start == b'usemtl':
                    context_material = line_value(line.split())
                    unique_materials[context_material] = None
                elif line_start == b'mtllib':  # usemap or usemat
                    # can have multiple mtllib filenames per line, mtllib can appear more than once,
                    # so make sure only occurrence of material exists
                    material_libs |= {os.fsdecode(f) for f in line.split()[1:]}

                    # Nurbs support
                elif line_start == b'cstype':
                    context_nurbs[b'cstype'] = line_value(line.split())  # 'rat bspline' / 'bspline'
                elif line_start == b'curv' or context_multi_line == b'curv':
                    curv_idx = context_nurbs[b'curv_idx'] = context_nurbs.get(b'curv_idx', [])  # in case were multiline

                    if not context_multi_line:
                        context_nurbs[b'curv_range'] = float_func(line_split[1]), float_func(line_split[2])
                        line_split[0:3] = []  # remove first 3 items

                    if strip_slash(line_split):
                        context_multi_line = b'curv'
                    else:
                        context_multi_line = b''

                    for i in line_split:
                        vert_loc_index = int(i) - 1

                        if vert_loc_index < 0:
                            vert_loc_index = len(verts_loc) + vert_loc_index + 1

                        curv_idx.append(vert_loc_index)

                elif line_start == b'parm' or context_multi_line == b'parm':
                    if context_multi_line:
                        context_multi_line = b''
                    else:
                        context_parm = line_split[1]
                        line_split[0:2] = []  # remove first 2

                    if strip_slash(line_split):
                        context_multi_line = b'parm'
                    else:
                        context_multi_line = b''

                    if context_parm.lower() == b'u':
                        context_nurbs.setdefault(b'parm_u', []).extend([float_func(f) for f in line_split])
                    elif context_parm.lower() == b'v':  # surfaces not supported yet
                        context_nurbs.setdefault(b'parm_v', []).extend([float_func(f) for f in line_split])
                    # else: # may want to support other parm's ?

                elif line_start == b'deg':
                    context_nurbs[b'deg'] = [int(i) for i in line.split()[1:]]
                elif line_start == b'end':
                    # Add the nurbs curve
                    if context_object:
                        context_nurbs[b'name'] = context_object
                    nurbs.append(context_nurbs)
                    context_nurbs = {}
                    context_parm = b''

                ''' # How to use usemap? depricated?
                elif line_start == b'usema': # usemap or usemat
                    context_image= line_value(line_split)
                '''

        progress.step("Done, loading materials and images...")

        create_materials(filepath, relpath, material_libs, unique_materials,
                         unique_material_images, use_image_search, use_cycles, float_func)

        progress.step("Done, building geometries (verts:%i faces:%i materials: %i smoothgroups:%i) ..." %
                      (len(verts_loc), len(faces), len(unique_materials), len(unique_smooth_groups)))

        # deselect all
        if bpy.ops.object.select_all.poll():
            bpy.ops.object.select_all(action='DESELECT')

        scene = context.scene
        new_objects = []  # put new objects here

        # Split the mesh by objects/materials, may
        SPLIT_OB_OR_GROUP = bool(use_split_objects or use_split_groups)

        for data in split_mesh(verts_loc, faces, unique_materials, filepath, SPLIT_OB_OR_GROUP):
            verts_loc_split, faces_split, unique_materials_split, dataname, use_vnor, use_vtex = data
            # Create meshes from the data, warning 'vertex_groups' wont support splitting
            #~ print(dataname, use_vnor, use_vtex)
            create_mesh(new_objects,
                        use_edges,
                        verts_loc_split,
                        verts_nor if use_vnor else [],
                        verts_tex if use_vtex else [],
                        faces_split,
                        unique_materials_split,
                        unique_material_images,
                        unique_smooth_groups,
                        vertex_groups,
                        dataname,
                        )

        # nurbs support
        for context_nurbs in nurbs:
            create_nurbs(context_nurbs, verts_loc, new_objects)

        # Create new obj
        for obj in new_objects:
            base = scene.objects.link(obj)
            base.select = True

            # we could apply this anywhere before scaling.
            obj.matrix_world = global_matrix

        scene.update()

        axis_min = [1000000000] * 3
        axis_max = [-1000000000] * 3

        if global_clamp_size:
            # Get all object bounds
            for ob in new_objects:
                for v in ob.bound_box:
                    for axis, value in enumerate(v):
                        if axis_min[axis] > value:
                            axis_min[axis] = value
                        if axis_max[axis] < value:
                            axis_max[axis] = value

            # Scale objects
            max_axis = max(axis_max[0] - axis_min[0], axis_max[1] - axis_min[1], axis_max[2] - axis_min[2])
            scale = 1.0

            while global_clamp_size < max_axis * scale:
                scale = scale / 10.0

            for obj in new_objects:
                obj.scale = scale, scale, scale

        progress.leave_substeps("Done.")
        progress.leave_substeps("Finished importing: %r" % filepath)

    return {'FINISHED'}
