# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

def draw_circle_2d(position, color, radius, *, segments=None):
    """
    Draw a circle.

    :arg position: Position where the circle will be drawn.
    :type position: 2D Vector
    :arg color: Color of the circle. To use transparency GL_BLEND has to be enabled.
    :type color: tuple containing RGBA values
    :arg radius: Radius of the circle.
    :type radius: float
    :arg segments: How many segments will be used to draw the circle.
        Higher values give better results but the drawing will take longer.
        If None or not specified, an automatic value will be calculated.
    :type segments: int or None
    """
    from math import sin, cos, pi, ceil, acos
    import gpu
    from gpu.types import (
        GPUBatch,
        GPUVertBuf,
        GPUVertFormat,
    )

    if segments is None:
        max_pixel_error = 0.25  # TODO: multiply 0.5 by display dpi
        segments = int(ceil(pi / acos(1.0 - max_pixel_error / radius)))
        segments = max(segments, 8)
        segments = min(segments, 1000)

    if segments <= 0:
        raise ValueError("Amount of segments must be greater than 0.")

    with gpu.matrix.push_pop():
        gpu.matrix.translate(position)
        gpu.matrix.scale_uniform(radius)
        mul = (1.0 / (segments - 1)) * (pi * 2)
        verts = [(sin(i * mul), cos(i * mul)) for i in range(segments)]
        fmt = GPUVertFormat()
        pos_id = fmt.attr_add(id="pos", comp_type='F32', len=2, fetch_mode='FLOAT')
        vbo = GPUVertBuf(len=len(verts), format=fmt)
        vbo.attr_fill(id=pos_id, data=verts)
        batch = GPUBatch(type='LINE_STRIP', buf=vbo)
        shader = gpu.shader.from_builtin('UNIFORM_COLOR')
        batch.program_set(shader)
        shader.uniform_float("color", color)
        batch.draw()


def draw_texture_2d(texture, position, width, height):
    """
    Draw a 2d texture.

    :arg texture: GPUTexture to draw (e.g. gpu.texture.from_image(image) for :class:`bpy.types.Image`).
    :type texture: :class:`gpu.types.GPUTexture`
    :arg position: Position of the lower left corner.
    :type position: 2D Vector
    :arg width: Width of the image when drawn (not necessarily
        the original width of the texture).
    :type width: float
    :arg height: Height of the image when drawn.
    :type height: float
    """
    import gpu
    from . batch import batch_for_shader

    coords = ((0, 0), (1, 0), (1, 1), (0, 1))

    shader = gpu.shader.from_builtin('IMAGE')
    batch = batch_for_shader(
        shader, 'TRI_FAN',
        {"pos": coords, "texCoord": coords},
    )

    with gpu.matrix.push_pop():
        gpu.matrix.translate(position)
        gpu.matrix.scale((width, height))

        shader = gpu.shader.from_builtin('IMAGE')

        if isinstance(texture, int):
            # Call the legacy bgl to not break the existing API
            import bgl
            bgl.glActiveTexture(bgl.GL_TEXTURE0)
            bgl.glBindTexture(bgl.GL_TEXTURE_2D, texture)
            shader.uniform_int("image", 0)
        else:
            shader.uniform_sampler("image", texture)

        batch.draw(shader)
