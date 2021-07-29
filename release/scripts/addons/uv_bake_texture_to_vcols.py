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

"""
Bake UV-Texture to Vertex Colors Addon

Contact:        p_boelens@msn.com
Information:    https://developer.blender.org/T28211

Contributor(s): Patrick Boelens, CoDEmanX.

All rights reserved.
"""

bl_info = {
    "name": "Bake UV-Texture to Vertex Colors",
    "description": "Bakes the colors of the active UV Texture "
        "to a Vertex Color layer.",
    "author": "Patrick Boelens, CoDEmanX",
    "version": (0, 6),
    "blender": (2, 63, 0),
    "location": "3D View > Vertex Paint > Toolshelf > Bake",
    "warning": "Requires image texture, generated textures aren't supported.",
    "wiki_url": "http://wiki.blender.org/index.php?title=Extensions:2.6/Py/"
                "Scripts/UV/Bake_Texture_to_Vertex_Colors",
    "category": "UV",
}

import bpy
from bpy.props import BoolProperty, EnumProperty, FloatVectorProperty
from math import fabs
from colorsys import rgb_to_hsv, hsv_to_rgb

class UV_OT_bake_texture_to_vcols(bpy.types.Operator):
    bl_idname = "uv.bake_texture_to_vcols"
    bl_label = "Bake UV-Texture to Vertex Colors"
    bl_description = "Bake active UV-Texture to new Vertex Color layer "\
        "(requires image texture)"
    bl_options = {'REGISTER', 'UNDO'}

    replace_active_layer = BoolProperty(
        name="Replace layer",
        description="Overwrite active Vertex Color layer",
        default=True)

    mappingModes = [
        ("CLIP", "Clip", "Don't affect vertices who's UV-coordinates are out of bounds."),
        ("REPEAT", "Repeat", "Tile the image so that each vertex is accounted for."),
        ("EXTEND", "Extend", "Extends the edges of the image to the UV-coordinates.")]

    mappingMode = EnumProperty(
       items=mappingModes,
       default="CLIP",
       name="Mapping",
       description="The mode to use for baking vertices who's UV-coordinates are out of bounds")

    blendingModes = [("MIX", "Mix", ""),
                     ("ADD", "Add", ""),
                     ("SUBTRACT", "Subtract", ""),
                     ("MULTIPLY", "Multiply", ""),
                     ("SCREEN", "Screen", ""),
                     ("OVERLAY", "Overlay", ""),
                     ("DIFFERENCE", "Difference", ""),
                     ("DIVIDE", "Divide", ""),
                     ("DARKEN", "Darken", ""),
                     ("LIGHTEN", "Lighten", ""),
                     ("HUE", "Hue", ""),
                     ("SATURATION", "Saturation", ""),
                     ("VALUE", "Value", ""),
                     ("COLOR", "Color", ""),
                     ("SOFT_LIGHT", "Soft Light", ""),
                     ("LINEAR_LIGHT", "Linear Light", "")
                    ]

    blendingMode = EnumProperty(
        items=blendingModes,
        default="MULTIPLY",
        name="Blend Type",
        description="The blending mode to use when baking")

    mirror_x = BoolProperty(name="Mirror X", description="Mirror the image on the X-axis")
    mirror_y = BoolProperty(name="Mirror Y", description="Mirror the image on the Y-axis")

    @classmethod
    def poll(self, context):
        return (context.object and
                context.object.type == 'MESH' and
                context.mode != 'EDIT_MESH' and
                context.object.data.uv_layers.active and
                context.object.data.uv_textures.active)

    def execute(self, context):
        obdata = context.object.data

        if self.replace_active_layer and obdata.vertex_colors.active:
            vertex_colors = obdata.vertex_colors.active
        else:
            vertex_colors = obdata.vertex_colors.new(name="Baked UV texture")

            if not vertex_colors:

                # Can't add more than 17 VCol layers
                self.report({'ERROR'},
                    "Couldn't add another Vertex Color layer,\n"
                    "Please remove an existing layer or replace active.")

                return {'CANCELLED'}

        obdata.vertex_colors.active = vertex_colors

        uv_images = {}
        for uv_tex in obdata.uv_textures.active.data:
            if (uv_tex.image and
                uv_tex.image.name not in uv_images and
                uv_tex.image.pixels):

                uv_images[uv_tex.image.name] = (
                    uv_tex.image.size[0],
                    uv_tex.image.size[1],
                    uv_tex.image.pixels[:]
                    # Accessing pixels directly is far too slow.
                    # Copied to new array for massive performance-gain.
                )

        for p in obdata.polygons:
            img = obdata.uv_textures.active.data[p.index].image
            if not img:
                continue

            image_size_x, image_size_y, uv_pixels = uv_images[img.name]

            for loop in p.loop_indices:

                co = obdata.uv_layers.active.data[loop].uv
                x_co = round(co[0] * (image_size_x - 1))
                y_co = round(co[1] * (image_size_y - 1))

                if x_co < 0 or x_co >= image_size_x or y_co < 0 or y_co >= image_size_y:
                    if self.mappingMode == 'CLIP':
                        continue

                    elif self.mappingMode == 'REPEAT':
                        x_co %= image_size_x
                        y_co %= image_size_y

                    elif self.mappingMode == 'EXTEND':
                        if x_co > image_size_x - 1:
                            x_co = image_size_x - 1
                        if x_co < 0:
                            x_co = 0
                        if y_co > image_size_y - 1:
                            y_co = image_size_y - 1
                        if y_co < 0:
                            y_co = 0

                if self.mirror_x:
                     x_co = image_size_x -1 - x_co

                if self.mirror_y:
                     y_co = image_size_y -1 - y_co

                col_out = vertex_colors.data[loop].color

                pixelNumber = (image_size_x * y_co) + x_co
                r = uv_pixels[pixelNumber*4]
                g = uv_pixels[pixelNumber*4 + 1]
                b = uv_pixels[pixelNumber*4 + 2]
                a = uv_pixels[pixelNumber*4 + 3]

                col_in = r, g, b # texture-color
                col_result = [r,g,b] # existing / 'base' color

                if self.blendingMode == 'MIX':
                    col_result = col_in

                elif self.blendingMode == 'ADD':
                    col_result[0] = col_in[0] + col_out[0]
                    col_result[1] = col_in[1] + col_out[1]
                    col_result[2] = col_in[2] + col_out[2]

                elif self.blendingMode == 'SUBTRACT':
                    col_result[0] = col_in[0] - col_out[0]
                    col_result[1] = col_in[1] - col_out[1]
                    col_result[2] = col_in[2] - col_out[2]

                elif self.blendingMode == 'MULTIPLY':
                    col_result[0] = col_in[0] * col_out[0]
                    col_result[1] = col_in[1] * col_out[1]
                    col_result[2] = col_in[2] * col_out[2]

                elif self.blendingMode == 'SCREEN':
                    col_result[0] = 1 - (1.0 - col_in[0]) * (1.0 - col_out[0])
                    col_result[1] = 1 - (1.0 - col_in[1]) * (1.0 - col_out[1])
                    col_result[2] = 1 - (1.0 - col_in[2]) * (1.0 - col_out[2])

                elif self.blendingMode == 'OVERLAY':
                    if col_out[0] < 0.5:
                        col_result[0] = col_out[0] * (2.0 * col_in[0])
                    else:
                        col_result[0] = 1.0 - (2.0 * (1.0 - col_in[0])) * (1.0 - col_out[0])
                    if col_out[1] < 0.5:
                        col_result[1] = col_out[1] * (2.0 * col_in[1])
                    else:
                        col_result[1] = 1.0 - (2.0 * (1.0 - col_in[1])) * (1.0 - col_out[1])
                    if col_out[2] < 0.5:
                        col_result[2] = col_out[2] * (2.0 * col_in[2])
                    else:
                        col_result[2] = 1.0 - (2.0 * (1.0 - col_in[2])) * (1.0 - col_out[2])

                elif self.blendingMode == 'DIFFERENCE':
                    col_result[0] = fabs(col_in[0] - col_out[0])
                    col_result[1] = fabs(col_in[1] - col_out[1])
                    col_result[2] = fabs(col_in[2] - col_out[2])

                elif self.blendingMode == 'DIVIDE':
                    if(col_in[0] != 0.0):
                        col_result[0] = col_out[0] / col_in[0]
                    if(col_in[1] != 0.0):
                        col_result[0] = col_out[1] / col_in[1]
                    if(col_in[2] != 0.0):
                        col_result[2] = col_out[2] / col_in[2]

                elif self.blendingMode == 'DARKEN':
                    if col_in[0] < col_out[0]:
                        col_result[0] = col_in[0]
                    else:
                        col_result[0] = col_out[0]
                    if col_in[1] < col_out[1]:
                        col_result[1] = col_in[1]
                    else:
                        col_result[1] = col_out[1]
                    if col_in[2] < col_out[2]:
                        col_result[2] = col_in[2]
                    else:
                        col_result[2] = col_out[2]


                elif self.blendingMode == 'LIGHTEN':
                    if col_in[0] > col_out[0]:
                        col_result[0] = col_in[0]
                    else:
                        col_result[0] = col_out[0]
                    if col_in[1] > col_out[1]:
                        col_result[1] = col_in[1]
                    else:
                        col_result[1] = col_out[1]
                    if col_in[2] > col_out[2]:
                        col_result[2] = col_in[2]
                    else:
                        col_result[2] = col_out[2]

                elif self.blendingMode == 'HUE':
                    hsv_in = rgb_to_hsv(col_in[0], col_in[1], col_in[2])
                    hsv_out = rgb_to_hsv(col_out[0], col_out[1], col_out[2])
                    hue = hsv_in[0]
                    col_result = hsv_to_rgb(hue, hsv_out[1], hsv_out[2])

                elif self.blendingMode == 'SATURATION':
                    hsv_in = rgb_to_hsv(col_in[0], col_in[1], col_in[2])
                    hsv_out = rgb_to_hsv(col_out[0], col_out[1], col_out[2])
                    sat = hsv_in[1]
                    col_result = hsv_to_rgb(hsv_out[0], sat, hsv_out[2])

                elif self.blendingMode == 'VALUE':
                    hsv_in = rgb_to_hsv(col_in[0], col_in[1], col_in[2])
                    hsv_out = rgb_to_hsv(col_out[0], col_out[1], col_out[2])
                    val = hsv_in[2]
                    col_result = hsv_to_rgb(hsv_out[0], hsv_out[1], val)

                elif self.blendingMode == 'COLOR':
                    hsv_in = rgb_to_hsv(col_in[0], col_in[1], col_in[2])
                    hsv_out = rgb_to_hsv(col_out[0], col_out[1], col_out[2])
                    hue = hsv_in[0]
                    sat = hsv_in[1]
                    col_result = hsv_to_rgb(hue, sat, hsv_out[2])

                elif self.blendingMode == 'SOFT_LIGHT':
                    scr = 1 - (1.0 - col_in[0]) * (1.0 - col_out[0])
                    scg = 1 - (1.0 - col_in[1]) * (1.0 - col_out[1])
                    scb = 1 - (1.0 - col_in[2]) * (1.0 - col_out[2])

                    col_result[0] = (1.0 - col_out[0]) * (col_in[0] * col_out[0]) + (col_out[0] * scr)
                    col_result[1] = (1.0 - col_out[1]) * (col_in[1] * col_out[1]) + (col_out[1] * scg)
                    col_result[2] = (1.0 - col_out[2]) * (col_in[2] * col_out[2]) + (col_out[2] * scb)


                elif self.blendingMode == 'LINEAR_LIGHT':
                    if col_in[0] > 0.5:
                        col_result[0] = col_out[0] + 2.0 * (col_in[0] - 0.5)
                    else:
                        col_result[0] = col_out[0] + 2.0 * (col_in[0] - 1.0)
                    if col_in[1] > 0.5:
                        col_result[1] = col_out[1] + 2.0 * (col_in[1] - 0.5)
                    else:
                        col_result[1] = col_out[1] + 2.0 * (col_in[1] - 1.0)
                    if col_in[2] > 0.5:
                        col_result[2] = col_out[2] + 2.0 * (col_in[2] - 0.5)
                    else:
                        col_result[2] = col_out[2] + 2.0 * (col_in[2] - 1.0)

                # Add alpha color
                a_inverted = 1 - a
                alpha_color = context.scene.uv_bake_alpha_color
                col_result = (col_result[0] * a + alpha_color[0] * a_inverted,
                              col_result[1] * a + alpha_color[1] * a_inverted,
                              col_result[2] * a + alpha_color[2] * a_inverted)

                vertex_colors.data[loop].color = col_result

        return {'FINISHED'}

class VIEW3D_PT_tools_uv_bake_texture_to_vcols(bpy.types.Panel):
    bl_label = "Bake"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(self, context):
        return(context.mode == 'PAINT_VERTEX')

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.prop(context.scene, "uv_bake_alpha_color")
        col.separator()
        col.operator("uv.bake_texture_to_vcols", text="UV Texture to VCols")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.Scene.uv_bake_alpha_color = FloatVectorProperty(
        name="Alpha Color",
        description="Color to be used for transparency",
        subtype='COLOR',
        min=0.0,
        max=1.0)

def unregister():
    bpy.utils.unregister_module(__name__)
    del bpy.types.Scene.uv_bake_alpha_color

if __name__ == "__main__":
    register()
