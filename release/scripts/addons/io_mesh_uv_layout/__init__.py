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

bl_info = {
    "name": "UV Layout",
    "author": "Campbell Barton, Matt Ebb",
    "version": (1, 1, 1),
    "blender": (2, 75, 0),
    "location": "Image-Window > UVs > Export UV Layout",
    "description": "Export the UV layout as a 2D graphic",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Import-Export/UV_Layout",
    "support": 'OFFICIAL',
    "category": "Import-Export",
}


# @todo write the wiki page

if "bpy" in locals():
    import importlib
    if "export_uv_eps" in locals():
        importlib.reload(export_uv_eps)
    if "export_uv_png" in locals():
        importlib.reload(export_uv_png)
    if "export_uv_svg" in locals():
        importlib.reload(export_uv_svg)

import bpy

from bpy.props import (
        StringProperty,
        BoolProperty,
        EnumProperty,
        IntVectorProperty,
        FloatProperty,
        )


class ExportUVLayout(bpy.types.Operator):
    """Export UV layout to file"""

    bl_idname = "uv.export_layout"
    bl_label = "Export UV Layout"
    bl_options = {'REGISTER', 'UNDO'}

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    check_existing = BoolProperty(
            name="Check Existing",
            description="Check and warn on overwriting existing files",
            default=True,
            options={'HIDDEN'},
            )
    export_all = BoolProperty(
            name="All UVs",
            description="Export all UVs in this mesh (not just visible ones)",
            default=False,
            )
    modified = BoolProperty(
            name="Modified",
            description="Exports UVs from the modified mesh",
            default=False,
            )
    mode = EnumProperty(
            items=(('SVG', "Scalable Vector Graphic (.svg)",
                    "Export the UV layout to a vector SVG file"),
                   ('EPS', "Encapsulate PostScript (.eps)",
                    "Export the UV layout to a vector EPS file"),
                   ('PNG', "PNG Image (.png)",
                    "Export the UV layout to a bitmap image"),
                   ),
            name="Format",
            description="File format to export the UV layout to",
            default='PNG',
            )
    size = IntVectorProperty(
            size=2,
            default=(1024, 1024),
            min=8, max=32768,
            description="Dimensions of the exported file",
            )
    opacity = FloatProperty(
            name="Fill Opacity",
            min=0.0, max=1.0,
            default=0.25,
            description="Set amount of opacity for exported UV layout"
            )
    tessellated = BoolProperty(
            name="Tessellated UVs",
            description="Export tessellated UVs instead of polygons ones",
            default=False,
            options={'HIDDEN'},  # As not working currently :/
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and obj.data.uv_textures)

    def _space_image(self, context):
        space_data = context.space_data
        if isinstance(space_data, bpy.types.SpaceImageEditor):
            return space_data
        else:
            return None

    def _image_size(self, context, default_width=1024, default_height=1024):
        # fallback if not in image context.
        image_width, image_height = default_width, default_height

        space_data = self._space_image(context)
        if space_data:
            image = space_data.image
            if image:
                width, height = tuple(context.space_data.image.size)
                # in case no data is found.
                if width and height:
                    image_width, image_height = width, height

        return image_width, image_height

    def _face_uv_iter(self, context, mesh, tessellated):
        uv_layer = mesh.uv_layers.active.data
        polys = mesh.polygons

        if not self.export_all:
            uv_tex = mesh.uv_textures.active.data
            local_image = Ellipsis

            if context.tool_settings.show_uv_local_view:
                space_data = self._space_image(context)
                if space_data:
                    local_image = space_data.image

            for i, p in enumerate(polys):
                # context checks
                if polys[i].select and local_image in {Ellipsis,
                                                       uv_tex[i].image}:
                    start = p.loop_start
                    end = start + p.loop_total
                    uvs = tuple((uv.uv[0], uv.uv[1])
                                for uv in uv_layer[start:end])

                    # just write what we see.
                    yield (i, uvs)
        else:
            # all, simple
            for i, p in enumerate(polys):
                start = p.loop_start
                end = start + p.loop_total
                uvs = tuple((uv.uv[0], uv.uv[1]) for uv in uv_layer[start:end])
                yield (i, uvs)

    def execute(self, context):

        obj = context.active_object
        is_editmode = (obj.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        mode = self.mode

        filepath = self.filepath
        filepath = bpy.path.ensure_ext(filepath, "." + mode.lower())
        file = open(filepath, "w")
        fw = file.write

        if mode == 'EPS':
            from . import export_uv_eps
            func = export_uv_eps.write
        elif mode == 'PNG':
            from . import export_uv_png
            func = export_uv_png.write
        elif mode == 'SVG':
            from . import export_uv_svg
            func = export_uv_svg.write

        if self.modified:
            mesh = obj.to_mesh(context.scene, True, 'PREVIEW')
        else:
            mesh = obj.data

        func(fw, mesh, self.size[0], self.size[1], self.opacity,
             lambda: self._face_uv_iter(context, mesh, self.tessellated))

        if self.modified:
            bpy.data.meshes.remove(mesh)

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

        file.close()

        return {'FINISHED'}

    def check(self, context):
        filepath = bpy.path.ensure_ext(self.filepath, "." + self.mode.lower())
        if filepath != self.filepath:
            self.filepath = filepath
            return True
        else:
            return False

    def invoke(self, context, event):
        import os
        self.size = self._image_size(context)
        self.filepath = os.path.splitext(bpy.data.filepath)[0]
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func(self, context):
    self.layout.operator(ExportUVLayout.bl_idname)


def register():
    bpy.utils.register_module(__name__)
    bpy.types.IMAGE_MT_uvs.append(menu_func)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.IMAGE_MT_uvs.remove(menu_func)

if __name__ == "__main__":
    register()
