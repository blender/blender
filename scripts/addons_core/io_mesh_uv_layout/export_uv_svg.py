# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from os.path import basename
from xml.sax.saxutils import escape


def export(filepath, tile, face_data, colors, width, height, opacity):
    with open(filepath, 'w', encoding='utf-8') as file:
        for text in get_file_parts(tile, face_data, colors, width, height, opacity):
            file.write(text)


def get_file_parts(tile, face_data, colors, width, height, opacity):
    yield from header(width, height)
    yield from draw_polygons(tile, face_data, width, height, opacity)
    yield from footer()


def header(width, height):
    yield '<?xml version="1.0" standalone="no"?>\n'
    yield '<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" \n'
    yield '  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">\n'
    yield f'<svg width="{width}" height="{height}" viewBox="0 0 {width} {height}"\n'
    yield '     xmlns="http://www.w3.org/2000/svg" version="1.1">\n'
    desc = f"{basename(bpy.data.filepath)}, (Blender {bpy.app.version_string})"
    yield f'<desc>{escape(desc)}</desc>\n'


def draw_polygons(tile, face_data, width, height, opacity):
    for uvs, color in face_data:
        fill = f'fill="{get_color_string(color)}"'

        yield '<polygon stroke="black" stroke-width="1"'
        yield f' {fill} fill-opacity="{opacity:.2g}"'

        yield ' points="'

        for uv in uvs:
            x, y = uv[0] - tile[0], 1.0 - uv[1] + tile[1]
            yield f'{x*width:.3f},{y*height:.3f} '
        yield '" />\n'


def get_color_string(color):
    r, g, b = color
    return f"rgb({round(r*255)}, {round(g*255)}, {round(b*255)})"


def footer():
    yield '\n'
    yield '</svg>\n'
