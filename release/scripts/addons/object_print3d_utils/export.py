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

# Export wrappers and integration with external tools.

import bpy
import os


def image_copy_guess(filepath, objects):
    # 'filepath' is the path we are writing to.
    import shutil
    from bpy_extras import object_utils
    image = None
    for obj in objects:
        image = object_utils.object_image_guess(obj)
        if image is not None:
            break

    if image is not None:
        imagepath = bpy.path.abspath(image.filepath, library=image.library)
        if os.path.exists(imagepath):
            filepath_noext = os.path.splitext(filepath)[0]
            ext = os.path.splitext(imagepath)[1]

            imagepath_dst = filepath_noext + ext
            print("copying texture: %r -> %r" % (imagepath, imagepath_dst))
            try:
                shutil.copy(imagepath, imagepath_dst)
            except:
                import traceback
                traceback.print_exc()


def write_mesh(context, info, report_cb):
    scene = context.scene
    unit = scene.unit_settings
    print_3d = scene.print_3d

    obj_base = scene.object_bases.active
    obj = obj_base.object

    export_format = print_3d.export_format
    global_scale = unit.scale_length if (unit.system != 'NONE' and print_3d.use_apply_scale) else 1.0
    path_mode = 'COPY' if print_3d.use_export_texture else 'AUTO'

    context_override = context.copy()

    obj_base_tmp = None

    # PLY can only export single mesh objects!
    if export_format == 'PLY':
        context_backup = context.copy()
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        from . import mesh_helpers
        obj_base_tmp = mesh_helpers.object_merge(context, context_override["selected_objects"])
        context_override["active_object"] = obj_base_tmp.object
        context_override["selected_bases"] = [obj_base_tmp]
        context_override["selected_objects"] = [obj_base_tmp.object]
    else:
        if obj_base not in context_override["selected_bases"]:
            context_override["selected_bases"].append(obj_base)
        if obj not in context_override["selected_objects"]:
            context_override["selected_objects"].append(obj)

    export_path = bpy.path.abspath(print_3d.export_path)

    # Create name 'export_path/blendname-objname'
    # add the filename component
    if bpy.data.is_saved:
        name = os.path.basename(bpy.data.filepath)
        name = os.path.splitext(name)[0]
    else:
        name = "untitled"
    # add object name
    name += "-%s" % bpy.path.clean_name(obj.name)

    # first ensure the path is created
    if export_path:
        # this can fail with strange errors,
        # if the dir cant be made then we get an error later.
        try:
            os.makedirs(export_path, exist_ok=True)
        except:
            import traceback
            traceback.print_exc()

    filepath = os.path.join(export_path, name)

    # ensure addon is enabled
    import addon_utils

    def addon_ensure(addon_id):
        # Enable the addon, dont change preferences.
        default_state, loaded_state = addon_utils.check(addon_id)
        if not loaded_state:
            addon_utils.enable(addon_id, default_set=False)

    if export_format == 'STL':
        addon_ensure("io_mesh_stl")
        filepath = bpy.path.ensure_ext(filepath, ".stl")
        ret = bpy.ops.export_mesh.stl(
                context_override,
                filepath=filepath,
                ascii=False,
                use_mesh_modifiers=True,
                use_selection=True,
                global_scale=global_scale,
                )
    elif export_format == 'PLY':
        addon_ensure("io_mesh_ply")
        filepath = bpy.path.ensure_ext(filepath, ".ply")
        ret = bpy.ops.export_mesh.ply(
                context_override,
                filepath=filepath,
                use_mesh_modifiers=True,
                global_scale=global_scale,
                )
    elif export_format == 'X3D':
        addon_ensure("io_scene_x3d")
        filepath = bpy.path.ensure_ext(filepath, ".x3d")
        ret = bpy.ops.export_scene.x3d(
                context_override,
                filepath=filepath,
                use_mesh_modifiers=True,
                use_selection=True,
                path_mode=path_mode,
                global_scale=global_scale,
                )
    elif export_format == 'WRL':
        addon_ensure("io_scene_vrml2")
        filepath = bpy.path.ensure_ext(filepath, ".wrl")
        ret = bpy.ops.export_scene.vrml2(
                context_override,
                filepath=filepath,
                use_mesh_modifiers=True,
                use_selection=True,
                path_mode=path_mode,
                global_scale=global_scale,
                )
    elif export_format == 'OBJ':
        addon_ensure("io_scene_obj")
        filepath = bpy.path.ensure_ext(filepath, ".obj")
        ret = bpy.ops.export_scene.obj(
                context_override,
                filepath=filepath,
                use_mesh_modifiers=True,
                use_selection=True,
                path_mode=path_mode,
                global_scale=global_scale,
                )
    else:
        assert(0)

    # for formats that don't support images
    if export_format in {'STL', 'PLY'}:
        if path_mode == 'COPY':
            image_copy_guess(filepath, context_override["selected_objects"])

    if obj_base_tmp is not None:
        obj = obj_base_tmp.object
        mesh = obj.data
        scene.objects.unlink(obj)
        bpy.data.objects.remove(obj)
        bpy.data.meshes.remove(mesh)
        del obj_base_tmp, obj, mesh

        # restore context
        base = None
        for base in context_backup["selected_bases"]:
            base.select = True
        del base
        scene.objects.active = context_backup["active_object"]

    if 'FINISHED' in ret:
        info.append(("%r ok" % os.path.basename(filepath), None))

        if report_cb is not None:
            report_cb({'INFO'}, "Exported: %r" % filepath)
        return True
    else:
        info.append(("%r fail" % os.path.basename(filepath), None))
        return False
