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


def write(fw, mesh, image_width, image_height, opacity, face_iter_func):
    # for making an XML compatible string
    from xml.sax.saxutils import escape
    from os.path import basename

    fw('<?xml version="1.0" standalone="no"?>\n')
    fw('<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" \n')
    fw('  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">\n')
    fw('<svg width="%d" height="%d" viewBox="0 0 %d %d"\n' %
       (image_width, image_height, image_width, image_height))
    fw('     xmlns="http://www.w3.org/2000/svg" version="1.1">\n')
    desc = ("%r, %s, (Blender %s)" %
            (basename(bpy.data.filepath), mesh.name, bpy.app.version_string))
    fw('<desc>%s</desc>\n' % escape(desc))

    # svg colors
    fill_settings = []
    fill_default = 'fill="grey"'
    for mat in mesh.materials if mesh.materials else [None]:
        if mat:
            fill_settings.append('fill="rgb(%d, %d, %d)"' %
                                 tuple(int(c * 255)
                                 for c in mat.diffuse_color))
        else:
            fill_settings.append(fill_default)

    polys = mesh.polygons
    for i, uvs in face_iter_func():
        try:  # rare cases material index is invalid.
            fill = fill_settings[polys[i].material_index]
        except IndexError:
            fill = fill_default

        fw('<polygon stroke="black" stroke-width="1"')
        if opacity > 0.0:
            fw(' %s fill-opacity="%.2g"' % (fill, opacity))

        fw(' points="')

        for j, uv in enumerate(uvs):
            x, y = uv[0], 1.0 - uv[1]
            fw('%.3f,%.3f ' % (x * image_width, y * image_height))
        fw('" />\n')
    fw('\n')
    fw('</svg>\n')
