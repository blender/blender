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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

import bpy
from bpy.props import *

class ExportUVLayout(bpy.types.Operator):
    '''Export the Mesh as SVG.'''

    bl_idname = "uv.export_layout"
    bl_label = "Export UV Layout"
    bl_register = True
    bl_undo = True
    
    path = StringProperty(name="File Path", description="File path used for exporting the SVG file", maxlen=1024, default="")
    check_existing = BoolProperty(name="Check Existing", description="Check and warn on overwriting existing files", default=True, hidden=True)
    only_selected = BoolProperty(name="Only Selected", description="Export Only the selected UVs", default=False)
    
    def poll(self, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')
    
    def _image_size(self, context, default_width=1024, default_height=1024):
        # fallback if not in image context.
        image_width, image_height = default_width, default_height

        space_data = context.space_data
        if type(space_data) == bpy.types.SpaceImageEditor:
            image = space_data.image
            if image:
                width, height = tuple(context.space_data.image.size)
                # incase no data is found.
                if width and height:
                    image_width, image_height = width, height
        
        return image_width, image_height

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
        
        active_uv_layer = None
        for lay in mesh.uv_textures:
            if lay.active:
                active_uv_layer = lay.data
                break

        fuvs = [(uv.uv1, uv.uv2, uv.uv3, uv.uv4) for uv in active_uv_layer]
        fuvs_cpy = [(uv[0].copy(), uv[1].copy(), uv[2].copy(), uv[3].copy()) for uv in fuvs]
        
        # as a list
        faces = mesh.faces[:]
        
        fuvsel = [(False not in uv.uv_selected) for uv in active_uv_layer]
        
        file = open(self.properties.path, "w")
        fw = file.write
        
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
        
        only_selected = self.properties.only_selected
        
        for i, uv in enumerate(active_uv_layer):
            
            if only_selected and False in uv.uv_selected:
                continue

            if len(faces[i].verts) == 3:
                uvs = uv.uv1, uv.uv2, uv.uv3
            else:
                uvs = uv.uv1, uv.uv2, uv.uv3, uv.uv4
            
            try: # rare cases material index is invalid.
                fill = fill_settings[faces[i].material_index]
            except IndexError:
                fill = fill_default

            fw('<polygon %s fill-opacity="0.5" stroke="black" stroke-width="1px" \n' % fill)
            fw('  points="')
            
            for j, uv in enumerate(uvs):
                x, y = uv.x, 1.0 - uv.y
                fw('%.3f,%.3f ' % (x * image_width, y * image_height))
            fw('" />\n')
        fw('\n')
        fw('</svg>\n')
        
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
