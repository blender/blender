# SPDX-FileCopyrightText: 2011-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import gpu
from mathutils import Matrix
from mathutils.geometry import tessellate_polygon
from gpu_extras.batch import batch_for_shader

# Use OIIO if available, else Blender for writing the image.
try:
    import OpenImageIO as oiio
except ImportError:
    oiio = None


def export(filepath, tile, face_data, colors, width, height, opacity):
    offscreen = gpu.types.GPUOffScreen(width, height)
    offscreen.bind()

    try:
        fb = gpu.state.active_framebuffer_get()
        fb.clear(color=(0.0, 0.0, 0.0, 0.0))
        draw_image(tile, face_data, opacity)

        pixel_data = fb.read_color(0, 0, width, height, 4, 0, 'UBYTE')
        pixel_data.dimensions = width * height * 4
        save_pixels(filepath, pixel_data, width, height)
    finally:
        offscreen.unbind()
        offscreen.free()


def draw_image(tile, face_data, opacity):
    gpu.state.blend_set('ALPHA')

    with gpu.matrix.push_pop():
        gpu.matrix.load_matrix(get_normalize_uvs_matrix(tile))
        gpu.matrix.load_projection_matrix(Matrix.Identity(4))

        draw_background_colors(face_data, opacity)
        draw_lines(face_data)

    gpu.state.blend_set('NONE')


def get_normalize_uvs_matrix(tile):
    '''matrix maps x and y coordinates from [0, 1] to [-1, 1]'''
    matrix = Matrix.Identity(4)
    matrix.col[3][0] = -1 - (tile[0] * 2)
    matrix.col[3][1] = -1 - (tile[1] * 2)
    matrix[0][0] = 2
    matrix[1][1] = 2

    # OIIO writes arrays from the left-upper corner.
    if oiio:
        matrix.col[3][1] *= -1.0
        matrix[1][1] *= -1.0

    return matrix


def draw_background_colors(face_data, opacity):
    coords = [uv for uvs, _ in face_data for uv in uvs]
    colors = [(*color, opacity) for uvs, color in face_data for _ in range(len(uvs))]

    indices = []
    offset = 0
    for uvs, _ in face_data:
        triangles = tessellate_uvs(uvs)
        indices.extend([index + offset for index in triangle] for triangle in triangles)
        offset += len(uvs)

    shader = gpu.shader.from_builtin('FLAT_COLOR')
    batch = batch_for_shader(
        shader, 'TRIS',
        {"pos": coords, "color": colors},
        indices=indices,
    )
    batch.draw(shader)


def tessellate_uvs(uvs):
    return tessellate_polygon([uvs])


def draw_lines(face_data):
    coords = []
    for uvs, _ in face_data:
        for i in range(len(uvs)):
            start = uvs[i]
            end = uvs[(i + 1) % len(uvs)]
            coords.append((start[0], start[1]))
            coords.append((end[0], end[1]))

    shader = gpu.shader.from_builtin('POLYLINE_UNIFORM_COLOR')
    shader.uniform_float("viewportSize", gpu.state.viewport_get()[2:])
    shader.uniform_float("lineWidth", 1.0)
    shader.uniform_float("color", (0.0, 0.0, 0.0, 1.0))

    batch = batch_for_shader(shader, 'LINES', {"pos": coords})
    batch.draw(shader)


def save_pixels(filepath, pixel_data, width, height):
    if oiio:
        spec = oiio.ImageSpec(width, height, 4, "uint8")
        image = oiio.ImageOutput.create(filepath)
        image.open(filepath, spec)
        image.write_image(pixel_data)
        image.close()
        return

    image = bpy.data.images.new("temp", width, height, alpha=True)
    image.filepath = filepath
    image.pixels = [v / 255 for v in pixel_data]
    image.save()
    bpy.data.images.remove(image)
