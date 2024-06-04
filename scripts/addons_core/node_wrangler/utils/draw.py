# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import gpu
from gpu_extras.batch import batch_for_shader
from math import cos, sin, pi

from .nodes import get_nodes_links, prefs_line_width, abs_node_location, dpi_fac


def draw_line(x1, y1, x2, y2, size, colour=(1.0, 1.0, 1.0, 0.7)):
    shader = gpu.shader.from_builtin('POLYLINE_SMOOTH_COLOR')
    shader.uniform_float("viewportSize", gpu.state.viewport_get()[2:])
    shader.uniform_float("lineWidth", size * prefs_line_width())

    vertices = ((x1, y1), (x2, y2))
    vertex_colors = ((colour[0] + (1.0 - colour[0]) / 4,
                      colour[1] + (1.0 - colour[1]) / 4,
                      colour[2] + (1.0 - colour[2]) / 4,
                      colour[3] + (1.0 - colour[3]) / 4),
                     colour)

    batch = batch_for_shader(shader, 'LINE_STRIP', {"pos": vertices, "color": vertex_colors})
    batch.draw(shader)


def draw_circle_2d_filled(mx, my, radius, colour=(1.0, 1.0, 1.0, 0.7)):
    radius = radius * prefs_line_width()
    sides = 12
    vertices = [(radius * cos(i * 2 * pi / sides) + mx,
                 radius * sin(i * 2 * pi / sides) + my)
                for i in range(sides + 1)]

    shader = gpu.shader.from_builtin('UNIFORM_COLOR')
    shader.uniform_float("color", colour)
    batch = batch_for_shader(shader, 'TRI_FAN', {"pos": vertices})
    batch.draw(shader)


def draw_rounded_node_border(node, radius=8, colour=(1.0, 1.0, 1.0, 0.7)):
    area_width = bpy.context.area.width
    sides = 16
    radius *= prefs_line_width()

    nlocx, nlocy = abs_node_location(node)

    nlocx = (nlocx + 1) * dpi_fac()
    nlocy = (nlocy + 1) * dpi_fac()
    ndimx = node.dimensions.x
    ndimy = node.dimensions.y

    if node.hide:
        nlocx += -1
        nlocy += 5
    if node.type == 'REROUTE':
        # nlocx += 1
        nlocy -= 1
        ndimx = 0
        ndimy = 0
        radius += 6

    shader = gpu.shader.from_builtin('UNIFORM_COLOR')
    shader.uniform_float("color", colour)

    # Top left corner
    mx, my = bpy.context.region.view2d.view_to_region(nlocx, nlocy, clip=False)
    vertices = [(mx, my)]
    for i in range(sides + 1):
        if (4 <= i <= 8):
            if mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                vertices.append((cosine, sine))

    batch = batch_for_shader(shader, 'TRI_FAN', {"pos": vertices})
    batch.draw(shader)

    # Top right corner
    mx, my = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy, clip=False)
    vertices = [(mx, my)]
    for i in range(sides + 1):
        if (0 <= i <= 4):
            if mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                vertices.append((cosine, sine))

    batch = batch_for_shader(shader, 'TRI_FAN', {"pos": vertices})
    batch.draw(shader)

    # Bottom left corner
    mx, my = bpy.context.region.view2d.view_to_region(nlocx, nlocy - ndimy, clip=False)
    vertices = [(mx, my)]
    for i in range(sides + 1):
        if (8 <= i <= 12):
            if mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                vertices.append((cosine, sine))

    batch = batch_for_shader(shader, 'TRI_FAN', {"pos": vertices})
    batch.draw(shader)

    # Bottom right corner
    mx, my = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy - ndimy, clip=False)
    vertices = [(mx, my)]
    for i in range(sides + 1):
        if (12 <= i <= 16):
            if mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                vertices.append((cosine, sine))

    batch = batch_for_shader(shader, 'TRI_FAN', {"pos": vertices})
    batch.draw(shader)

    # prepare drawing all edges in one batch
    vertices = []
    indices = []
    id_last = 0

    # Left edge
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx, nlocy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx, nlocy - ndimy, clip=False)
    if m1x < area_width and m2x < area_width:
        vertices.extend([(m2x - radius, m2y), (m2x, m2y),
                         (m1x, m1y), (m1x - radius, m1y)])
        indices.extend([(id_last, id_last + 1, id_last + 3),
                        (id_last + 3, id_last + 1, id_last + 2)])
        id_last += 4

    # Top edge
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx, nlocy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy, clip=False)
    m1x = min(m1x, area_width)
    m2x = min(m2x, area_width)
    vertices.extend([(m1x, m1y), (m2x, m1y),
                     (m2x, m1y + radius), (m1x, m1y + radius)])
    indices.extend([(id_last, id_last + 1, id_last + 3),
                    (id_last + 3, id_last + 1, id_last + 2)])
    id_last += 4

    # Right edge
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy - ndimy, clip=False)
    if m1x < area_width and m2x < area_width:
        vertices.extend([(m1x, m2y), (m1x + radius, m2y),
                         (m1x + radius, m1y), (m1x, m1y)])
        indices.extend([(id_last, id_last + 1, id_last + 3),
                        (id_last + 3, id_last + 1, id_last + 2)])
        id_last += 4

    # Bottom edge
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx, nlocy - ndimy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy - ndimy, clip=False)
    m1x = min(m1x, area_width)
    m2x = min(m2x, area_width)
    vertices.extend([(m1x, m2y), (m2x, m2y),
                     (m2x, m1y - radius), (m1x, m1y - radius)])
    indices.extend([(id_last, id_last + 1, id_last + 3),
                    (id_last + 3, id_last + 1, id_last + 2)])

    # now draw all edges in one batch
    if len(vertices) != 0:
        batch = batch_for_shader(shader, 'TRIS', {"pos": vertices}, indices=indices)
        batch.draw(shader)


def draw_callback_nodeoutline(self, context, mode):
    if self.mouse_path:
        gpu.state.blend_set('ALPHA')

        nodes, _links = get_nodes_links(context)

        if mode == "LINK":
            col_outer = (1.0, 0.2, 0.2, 0.4)
            col_inner = (0.0, 0.0, 0.0, 0.5)
            col_circle_inner = (0.3, 0.05, 0.05, 1.0)
        elif mode == "LINKMENU":
            col_outer = (0.4, 0.6, 1.0, 0.4)
            col_inner = (0.0, 0.0, 0.0, 0.5)
            col_circle_inner = (0.08, 0.15, .3, 1.0)
        elif mode == "MIX":
            col_outer = (0.2, 1.0, 0.2, 0.4)
            col_inner = (0.0, 0.0, 0.0, 0.5)
            col_circle_inner = (0.05, 0.3, 0.05, 1.0)

        m1x = self.mouse_path[0][0]
        m1y = self.mouse_path[0][1]
        m2x = self.mouse_path[-1][0]
        m2y = self.mouse_path[-1][1]

        n1 = nodes[context.scene.NWLazySource]
        n2 = nodes[context.scene.NWLazyTarget]

        if n1 == n2:
            col_outer = (0.4, 0.4, 0.4, 0.4)
            col_inner = (0.0, 0.0, 0.0, 0.5)
            col_circle_inner = (0.2, 0.2, 0.2, 1.0)

        draw_rounded_node_border(n1, radius=6, colour=col_outer)  # outline
        draw_rounded_node_border(n1, radius=5, colour=col_inner)  # inner
        draw_rounded_node_border(n2, radius=6, colour=col_outer)  # outline
        draw_rounded_node_border(n2, radius=5, colour=col_inner)  # inner

        draw_line(m1x, m1y, m2x, m2y, 5, col_outer)  # line outline
        draw_line(m1x, m1y, m2x, m2y, 2, col_inner)  # line inner

        # circle outline
        draw_circle_2d_filled(m1x, m1y, 7, col_outer)
        draw_circle_2d_filled(m2x, m2y, 7, col_outer)

        # circle inner
        draw_circle_2d_filled(m1x, m1y, 5, col_circle_inner)
        draw_circle_2d_filled(m2x, m2y, 5, col_circle_inner)

        gpu.state.blend_set('NONE')
