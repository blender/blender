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

# <pep8-80 compliant>

import bpy
import bpy_extras
import bmesh
import os
from bpy_extras import object_utils


def save_bmesh(fw, bm,
               use_color, color_type, material_colors,
               use_uv, uv_image,
               path_mode, copy_set):

    base_src = os.path.dirname(bpy.data.filepath)
    base_dst = os.path.dirname(fw.__self__.name)

    fw('Shape {\n')
    fw('\tappearance Appearance {\n')
    if use_uv:
        fw('\t\ttexture ImageTexture {\n')
        filepath = uv_image.filepath
        filepath_full = os.path.normpath(bpy.path.abspath(filepath, library=uv_image.library))
        filepath_ref = bpy_extras.io_utils.path_reference(filepath_full, base_src, base_dst, path_mode, "textures", copy_set, uv_image.library)
        filepath_base = os.path.basename(filepath_full)

        images = [
            filepath_ref,
            filepath_base,
        ]
        if path_mode != 'RELATIVE':
            images.append(filepath_full)

        fw('\t\t\turl [ %s ]\n' % " ".join(['"%s"' % f for f in images]) )
        del images
        del filepath_ref, filepath_base, filepath_full, filepath
        fw('\t\t}\n')  # end 'ImageTexture'
    else:
        fw('\t\tmaterial Material {\n')
        fw('\t\t}\n')  # end 'Material'
    fw('\t}\n')  # end 'Appearance'

    fw('\tgeometry IndexedFaceSet {\n')
    fw('\t\tcoord Coordinate {\n')
    fw('\t\t\tpoint [ ')
    v = None
    for v in bm.verts:
        fw("%.6f %.6f %.6f " % v.co[:])
    del v
    fw(']\n')  # end 'point[]'
    fw('\t\t}\n')  # end 'Coordinate'

    if use_color:
        if color_type == 'MATERIAL':
            fw('\t\tcolorPerVertex FALSE\n')
            fw('\t\tcolor Color {\n')
            fw('\t\t\tcolor [ ')
            c = None
            for c in material_colors:
                fw(c)
            del c
            fw(']\n')  # end 'color[]'
            fw('\t\t}\n')  # end 'Color'
        elif color_type == 'VERTEX':
            fw('\t\tcolorPerVertex TRUE\n')
            fw('\t\tcolor Color {\n')
            fw('\t\t\tcolor [ ')
            v = None
            c_none = "0.00 0.00 0.00 "
            color_layer = bm.loops.layers.color.active
            assert(color_layer is not None)
            for v in bm.verts:
                # weak, use first loops color
                try:
                    l = v.link_loops[0]
                except:
                    l = None
                fw(c_none if l is None else ("%.2f %.2f %.2f " % l[color_layer][:]))

            del v
            fw(']\n')  # end 'color[]'
            fw('\t\t}\n')  # end 'Color'

        # ---

        if color_type == 'MATERIAL':
            fw('\t\tcolorIndex [ ')
            i = None
            for f in bm.faces:
                i = f.material_index
                if i >= len(material_colors):
                    i = 0
                fw("%d " % i)
            del i
            fw(']\n')  # end 'colorIndex[]'
        elif color_type == 'VERTEX':
            pass

    if use_uv:
        fw('\t\ttexCoord TextureCoordinate {\n')
        fw('\t\t\tpoint [ ')
        v = None
        uv_layer = bm.loops.layers.uv.active
        assert(uv_layer is not None)
        for f in bm.faces:
            for l in f.loops:
                fw("%.4f %.4f " % l[uv_layer].uv[:])

        del f
        fw(']\n')  # end 'point[]'
        fw('\t\t}\n')  # end 'TextureCoordinate'

        # ---

        fw('\t\ttexCoordIndex [ ')
        i = None
        for i in range(0, len(bm.faces) * 3, 3):
            fw("%d %d %d -1 " % (i, i + 1, i + 2))
        del i
        fw(']\n')  # end 'coordIndex[]'

    fw('\t\tcoordIndex [ ')
    f = fv = None
    for f in bm.faces:
        fv = f.verts[:]
        fw("%d %d %d -1 " % (fv[0].index, fv[1].index, fv[2].index))
    del f, fv
    fw(']\n')  # end 'coordIndex[]'

    fw('\t}\n')  # end 'IndexedFaceSet'
    fw('}\n')  # end 'Shape'


def save_object(fw, global_matrix,
                scene, obj,
                use_mesh_modifiers,
                use_color, color_type,
                use_uv,
                path_mode, copy_set):

    assert(obj.type == 'MESH')

    if use_mesh_modifiers:
        is_editmode = (obj.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.editmode_toggle()

        me = obj.to_mesh(scene, True, 'PREVIEW', calc_tessface=False)
        bm = bmesh.new()
        bm.from_mesh(me)

        if is_editmode:
            bpy.ops.object.editmode_toggle()
    else:
        me = obj.data
        if obj.mode == 'EDIT':
            bm_orig = bmesh.from_edit_mesh(me)
            bm = bm_orig.copy()
        else:
            bm = bmesh.new()
            bm.from_mesh(me)

    # triangulate first so tessellation matches the view-port.
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.transform(global_matrix * obj.matrix_world)

    # default empty
    material_colors = []
    uv_image = None

    if use_color:
        if color_type == 'VERTEX':
            if bm.loops.layers.color.active is None:
                # fallback to material
                color_type = 'MATERIAL'
        if color_type == 'MATERIAL':
            if not me.materials:
                use_color = False
            else:
                material_colors = [
                        "%.2f %.2f %.2f " % (m.diffuse_color[:] if m else (1.0, 1.0, 1.0))
                        for m in me.materials]
        assert(color_type in {'VERTEX', 'MATERIAL'})

    if use_uv:
        if bm.loops.layers.uv.active is None:
            use_uv = False
        uv_image = object_utils.object_image_guess(obj, bm=bm)
        if uv_image is None:
            use_uv = False

    save_bmesh(fw, bm,
               use_color, color_type, material_colors,
               use_uv, uv_image,
               path_mode, copy_set)

    bm.free()


def save(operator,
         context,
         filepath="",
         global_matrix=None,
         use_selection=False,
         use_mesh_modifiers=True,
         use_color=True,
         color_type='MATERIAL',
         use_uv=True,
         path_mode='AUTO'):

    scene = context.scene

    # store files to copy
    copy_set = set()

    file = open(filepath, 'w', encoding='utf-8')
    fw = file.write
    fw('#VRML V2.0 utf8\n')
    fw('#modeled using blender3d http://blender.org\n')

    if use_selection:
        objects = context.selected_objects
    else:
        objects = scene.objects

    for obj in objects:
        if obj.type == 'MESH':
            fw("\n# %r\n" % obj.name)
            save_object(fw, global_matrix,
                        scene, obj,
                        use_mesh_modifiers,
                        use_color, color_type,
                        use_uv,
                        path_mode, copy_set)

    file.close()

    # copy all collected files.
    bpy_extras.io_utils.path_reference_copy(copy_set)

    return {'FINISHED'}
