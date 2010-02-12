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

import bpy
from bpy.props import *


class ExportUVLayout(bpy.types.Operator):
    '''Export the Mesh as SVG'''

    bl_idname = "uv.export_layout"
    bl_label = "Export UV Layout"
    bl_register = True
    bl_undo = True

    path = StringProperty(name="File Path", description="File path used for exporting the SVG file", maxlen=1024, default="")
    check_existing = BoolProperty(name="Check Existing", description="Check and warn on overwriting existing files", default=True, options={'HIDDEN'})
    export_all = BoolProperty(name="All UV's", description="Export all UVs in this mesh (not just the visible ones)", default=False)
    mode = EnumProperty(items=(
                        ('SVG', "Scalable Vector Graphic (.svg)", "Export the UV layout to a vector SVG file"),
                        ('EPS', "Encapsulate PostScript (.eps)", "Export the UV layout to a vector EPS file")),
                name="Format",
                description="File format to export the UV layout to",
                default='SVG')
    
    def poll(self, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def _space_image(self, context):
        space_data = context.space_data
        if type(space_data) == bpy.types.SpaceImageEditor:
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
                # incase no data is found.
                if width and height:
                    image_width, image_height = width, height

        return image_width, image_height

    def _face_uv_iter(self, context):
        obj = context.active_object
        mesh = obj.data
        uv_layer = mesh.active_uv_texture.data
        uv_layer_len = len(uv_layer)
        
        if not self.properties.export_all:
            
            local_image = Ellipsis

            if context.tool_settings.uv_local_view:
                space_data = self._space_image(context)
                if space_data:
                    local_image = space_data.image
            
            faces = mesh.faces
            
            for i in range(uv_layer_len):
                uv_elem = uv_layer[i]
                # context checks
                if faces[i].selected and (local_image is Ellipsis or local_image == uv_elem.image):
                    #~ uv = uv_elem.uv
                    #~ if False not in uv_elem.uv_selected[:len(uv)]:
                    #~     yield (i, uv)
                    
                    # just write what we see.
                    yield (i, uv_layer[i].uv)
        else:
            # all, simple
            for i in range(uv_layer_len):
                yield (i, uv_layer[i].uv)
        
        
        

    def execute(self, context):
        # for making an XML compatible string
        from xml.sax.saxutils import escape
        from os.path import basename

        obj = context.active_object
        is_editmode = (obj.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        image_width, image_height = self._image_size(context)
        mesh = obj.data
        faces = mesh.faces

        mode = self.properties.mode
        
        file = open(self.properties.path, "w")
        fw = file.write

        if mode == 'SVG':

            fw('<?xml version="1.0" standalone="no"?>\n')
            fw('<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" \n')
            fw('  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">\n')
            fw('<svg width="%dpx" height="%dpx" viewBox="0px 0px %dpx %dpx"\n' % (image_width, image_height, image_width, image_height))
            fw('     xmlns="http://www.w3.org/2000/svg" version="1.1">\n')
            desc = "%s, %s, %s (Blender %s)" % (basename(bpy.data.filename), obj.name, mesh.name, bpy.app.version_string)
            fw('<desc>%s</desc>\n' % escape(desc))
    
            # svg colors
            fill_settings = []
            fill_default = 'fill="grey"'
            for mat in mesh.materials if mesh.materials else [None]:
                if mat:
                    fill_settings.append('fill="rgb(%d, %d, %d)"' % tuple(int(c*255) for c in mat.diffuse_color))
                else:
                    fill_settings.append(fill_default)

            for i, uvs in self._face_uv_iter(context):
                try: # rare cases material index is invalid.
                    fill = fill_settings[faces[i].material_index]
                except IndexError:
                    fill = fill_default
    
                fw('<polygon %s fill-opacity="0.5" stroke="black" stroke-width="1px" \n' % fill)
                fw('  points="')
                
                for j, uv in enumerate(uvs):
                    x, y = uv[0], 1.0 - uv[1]
                    fw('%.3f,%.3f ' % (x * image_width, y * image_height))
                fw('" />\n')
            fw('\n')
            fw('</svg>\n')

        elif mode == 'EPS':
            fw('%!PS-Adobe-3.0 EPSF-3.0\n')
            fw("%%%%Creator: Blender %s\n" % bpy.app.version_string)
            fw('%%Pages: 1\n')
            fw('%%Orientation: Portrait\n')
            fw("%%%%BoundingBox: 0 0 %d %d\n" % (image_width, image_height))
            fw("%%%%HiResBoundingBox: 0.0 0.0 %.4f %.4f\n" % (image_width, image_height))
            fw('%%EndComments\n')
            fw('%%Page: 1 1\n')
            fw('0 0 translate\n')
            fw('1.0 1.0 scale\n')
            fw('0 0 0 setrgbcolor\n')
            fw('[] 0 setdash\n')
            fw('1 setlinewidth\n')
            fw('1 setlinejoin\n')
            fw('1 setlinecap\n')
            fw('newpath\n')
            
            for i, uvs in self._face_uv_iter(context):
                for j, uv in enumerate(uvs):
                    x, y = uv[0], uv[1]
                    if j==0:
                        fw('%.5f %.5f moveto\n' % (x * image_width, y * image_height))
                    else:
                        fw('%.5f %.5f lineto\n' % (x * image_width, y * image_height))
            
            fw('closepath\n')
            fw('stroke\n')
            fw('showpage\n')
            fw('%%EOF\n')

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}

# Register the operator
bpy.types.register(ExportUVLayout)


def menu_func(self, context):
    default_path = bpy.data.filename.replace(".blend", ".svg")
    self.layout.operator(ExportUVLayout.bl_idname).path = default_path

bpy.types.IMAGE_MT_uvs.append(menu_func)

#if __name__ == "__main__":
#    bpy.ops.uv.export_layout(path="/home/ideasman42/foo.svg")
