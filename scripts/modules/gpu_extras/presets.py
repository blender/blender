# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "draw_circle_2d",
    "draw_texture_2d",
)


def draw_circle_2d(position, color, radius, *, segments=None):
    """
    Draw a circle.

    :arg position: 2D position where the circle will be drawn.
    :type position: Sequence[float]
    :arg color: Color of the circle (RGBA).
       To use transparency blend must be set to ``ALPHA``, see: :func:`gpu.state.blend_set`.
    :type color: Sequence[float]
    :arg radius: Radius of the circle.
    :type radius: float
    :arg segments: How many segments will be used to draw the circle.
        Higher values give better results but the drawing will take longer.
        If None or not specified, an automatic value will be calculated.
    :type segments: int | None
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
        shader = gpu.shader.from_builtin('POLYLINE_UNIFORM_COLOR')
        shader.uniform_float("viewportSize", gpu.state.viewport_get()[2:])
        shader.uniform_float("lineWidth", gpu.state.line_width_get())
        shader.uniform_float("color", color)
        batch.draw(shader)


def draw_texture_2d(texture, position, width, height, is_scene_linear_with_rec709_srgb_target=False):
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
    :arg is_scene_linear_with_rec709_srgb_target:
        True if the `texture` is stored in scene linear color space and
        the destination framebuffer uses the Rec.709 sRGB color space
        (which is true when drawing textures acquired from :class:`bpy.types.Image` inside a
        'PRE_VIEW', 'POST_VIEW' or 'POST_PIXEL' draw handler).
        Otherwise the color space is assumed to match the one of the framebuffer. (default=False)
    :type is_scene_linear_with_rec709_srgb_target: bool
    """
    import gpu
    from . batch import batch_for_shader

    coords = ((0, 0), (1, 0), (1, 1), (0, 1))
    indices = ((0, 1, 2), (2, 3, 0))

    shader = gpu.shader.from_builtin(
        'IMAGE_SCENE_LINEAR_TO_REC709_SRGB' if is_scene_linear_with_rec709_srgb_target else 'IMAGE')
    batch = batch_for_shader(
        shader, 'TRIS',
        {"pos": coords, "texCoord": coords},
        indices=indices
    )

    with gpu.matrix.push_pop():
        gpu.matrix.translate(position)
        gpu.matrix.scale((width, height))

        shader.uniform_sampler("image", texture)

        batch.draw(shader)
