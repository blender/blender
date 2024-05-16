# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy


def export(filepath, tile, face_data, colors, width, height, opacity):
    with open(filepath, 'w', encoding='utf-8') as file:
        for text in get_file_parts(tile, face_data, colors, width, height, opacity):
            file.write(text)


def get_file_parts(tile, face_data, colors, width, height, opacity):
    yield from header(width, height)
    if opacity > 0.0:
        name_by_color = {}
        yield from prepare_colors(colors, name_by_color)
        yield from draw_colored_polygons(tile, face_data, name_by_color, width, height)
    yield from draw_lines(tile, face_data, width, height)
    yield from footer()


def header(width, height):
    yield "%!PS-Adobe-3.0 EPSF-3.0\n"
    yield f"%%Creator: Blender {bpy.app.version_string}\n"
    yield "%%Pages: 1\n"
    yield "%%Orientation: Portrait\n"
    yield f"%%BoundingBox: 0 0 {width} {height}\n"
    yield f"%%HiResBoundingBox: 0.0 0.0 {width:.4f} {height:.4f}\n"
    yield "%%EndComments\n"
    yield "%%Page: 1 1\n"
    yield "0 0 translate\n"
    yield "1.0 1.0 scale\n"
    yield "0 0 0 setrgbcolor\n"
    yield "[] 0 setdash\n"
    yield "1 setlinewidth\n"
    yield "1 setlinejoin\n"
    yield "1 setlinecap\n"


def prepare_colors(colors, out_name_by_color):
    for i, color in enumerate(colors):
        name = f"COLOR_{i}"
        yield "/%s {" % name
        out_name_by_color[color] = name

        yield "gsave\n"
        yield "%.3g %.3g %.3g setrgbcolor\n" % color
        yield "fill\n"
        yield "grestore\n"
        yield "0 setgray\n"
        yield "} def\n"


def draw_colored_polygons(tile, face_data, name_by_color, width, height):
    for uvs, color in face_data:
        yield from draw_polygon_path(tile, uvs, width, height)
        yield "closepath\n"
        yield "%s\n" % name_by_color[color]


def draw_lines(tile, face_data, width, height):
    for uvs, _ in face_data:
        yield from draw_polygon_path(tile, uvs, width, height)
        yield "closepath\n"
        yield "stroke\n"


def draw_polygon_path(tile, uvs, width, height):
    yield "newpath\n"
    for j, uv in enumerate(uvs):
        uv_scale = ((uv[0] - tile[0]) * width, (uv[1] - tile[1]) * height)
        if j == 0:
            yield "%.5f %.5f moveto\n" % uv_scale
        else:
            yield "%.5f %.5f lineto\n" % uv_scale


def footer():
    yield "showpage\n"
    yield "%%EOF\n"
