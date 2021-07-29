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

bl_info = {
    "name": "Animated Render Baker",
    "author": "Janne Karhu (jahka)",
    "version": (2, 0),
    "blender": (2, 75, 0),
    "location": "Properties > Render > Bake Panel",
    "description": "Renderbakes a series of frames",
    "category": "Render",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Object/Animated_Render_Baker",
}


import bpy
from bpy.props import IntProperty

class OBJECT_OT_animrenderbake(bpy.types.Operator):
    bl_label = "Animated Render Bake"
    bl_description= "Bake animated image textures of selected objects"
    bl_idname = "object.anim_bake_image"
    bl_register = True

    def framefile(self, filepath, frame):
        """
        Set frame number to file name image.png -> image0013.png
        """
        import os
        fn, ext = os.path.splitext(filepath)
        return "%s%04d%s" % (fn, frame, ext)

    def invoke(self, context, event):
        import shutil
        is_cycles = (context.scene.render.engine == 'CYCLES')

        scene = context.scene

        start = scene.animrenderbake_start
        end = scene.animrenderbake_end

        # Check for errors before starting
        if start >= end:
            self.report({'ERROR'}, "Start frame must be smaller than end frame")
            return {'CANCELLED'}

        selected = context.selected_objects

        # Only single object baking for now
        if scene.render.use_bake_selected_to_active:
            if len(selected) > 2:
                self.report({'ERROR'}, "Select only two objects for animated baking")
                return {'CANCELLED'}
        elif len(selected) > 1:
            self.report({'ERROR'}, "Select only one object for animated baking")
            return {'CANCELLED'}

        if context.active_object.type != 'MESH':
            self.report({'ERROR'}, "The baked object must be a mesh object")
            return {'CANCELLED'}

        if context.active_object.mode == 'EDIT':
            self.report({'ERROR'}, "Can't bake in edit-mode")
            return {'CANCELLED'}

        img = None

        # find the image that's used for rendering
        # TODO: support multiple images per bake
        if is_cycles:
            # XXX This tries to mimic nodeGetActiveTexture(), but we have no access to 'texture_active' state from RNA...
            #     IMHO, this should be a func in RNA nodetree struct anyway?
            inactive = None
            selected = None
            for mat_slot in context.active_object.material_slots:
                mat = mat_slot.material
                if not mat or not mat.node_tree:
                    continue
                trees = [mat.node_tree]
                while trees and not img:
                    tree = trees.pop()
                    node = tree.nodes.active
                    if node.type in {'TEX_IMAGE', 'TEX_ENVIRONMENT'}:
                        img = node.image
                        break
                    for node in tree.nodes:
                        if node.type in {'TEX_IMAGE', 'TEX_ENVIRONMENT'} and node.image:
                            if node.select:
                                if not selected:
                                    selected = node
                            else:
                                if not inactive:
                                    inactive = node
                        elif node.type == 'GROUP':
                            trees.add(node.node_tree)
                if img:
                    break
            if not img:
                if selected:
                    img = selected.image
                elif inactive:
                    img = inactive.image
        else:
            for uvtex in context.active_object.data.uv_textures:
                if uvtex.active_render == True:
                    for uvdata in uvtex.data:
                        if uvdata.image is not None:
                            img = uvdata.image
                            break

        if img is None:
            self.report({'ERROR'}, "No valid image found to bake to")
            return {'CANCELLED'}

        if img.is_dirty:
            self.report({'ERROR'}, "Save the image that's used for baking before use")
            return {'CANCELLED'}

        if img.packed_file is not None:
            self.report({'ERROR'}, "Can't animation-bake packed file")
            return {'CANCELLED'}

        # make sure we have an absolute path so that copying works for sure
        img_filepath_abs = bpy.path.abspath(img.filepath, library=img.library)

        print("Animated baking for frames (%d - %d)" % (start, end))

        for cfra in range(start, end + 1):
            print("Baking frame %d" % cfra)

            # update scene to new frame and bake to template image
            scene.frame_set(cfra)
            if is_cycles:
                ret = bpy.ops.object.bake()
            else:
                ret = bpy.ops.object.bake_image()
            if 'CANCELLED' in ret:
                return {'CANCELLED'}

            # Currently the api doesn't allow img.save_as()
            # so just save the template image as usual for
            # every frame and copy to a file with frame specific filename
            img.save()
            img_filepath_new = self.framefile(img_filepath_abs, cfra)
            shutil.copyfile(img_filepath_abs, img_filepath_new)
            print("Saved %r" % img_filepath_new)

        print("Baking done!")

        return{'FINISHED'}


def draw(self, context):
    layout = self.layout

    scene = context.scene

    row = layout.row()
    row.operator("object.anim_bake_image", text="Animated Bake", icon="RENDER_ANIMATION")
    rowsub = row.row(align=True)
    rowsub.prop(scene, "animrenderbake_start")
    rowsub.prop(scene, "animrenderbake_end")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.animrenderbake_start = IntProperty(
            name="Start",
            description="Start frame of the animated bake",
            default=1)

    bpy.types.Scene.animrenderbake_end = IntProperty(
            name="End",
            description="End frame of the animated bake",
            default=250)

    bpy.types.RENDER_PT_bake.prepend(draw)
    cycles_panel = getattr(bpy.types, "CyclesRender_PT_bake", None)
    if cycles_panel:
        cycles_panel.prepend(draw)


def unregister():
    bpy.utils.unregister_module(__name__)

    # restore original panel draw function
    del bpy.types.Scene.animrenderbake_start
    del bpy.types.Scene.animrenderbake_end

    bpy.types.RENDER_PT_bake.remove(draw)
    cycles_panel = getattr(bpy.types, "CyclesRender_PT_bake", None)
    if cycles_panel:
        cycles_panel.remove(draw)


if __name__ == "__main__":
    register()
