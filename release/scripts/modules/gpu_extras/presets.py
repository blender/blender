# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

def draw_circle_2d(position, color, radius, segments):
    from math import sin, cos, pi
    import gpu
    from gpu.types import (
        GPUBatch,
        GPUVertBuf,
        GPUVertFormat,
    )

    with gpu.matrix.push_pop():
        gpu.matrix.translate(position)
        gpu.matrix.scale_uniform(radius)
        seg = 32
        mul = (1.0 / (seg - 1)) * (pi * 2)
        verts = [(sin(i * mul), cos(i * mul)) for i in range(seg)]
        fmt = GPUVertFormat()
        pos_id = fmt.attr_add(id="pos", comp_type='F32', len=2, fetch_mode='FLOAT')
        vbo = GPUVertBuf(len=len(verts), format=fmt)
        vbo.attr_fill(id=pos_id, data=verts)
        batch = GPUBatch(type='LINE_STRIP', buf=vbo)
        shader = gpu.shader.from_builtin('2D_UNIFORM_COLOR')
        batch.program_set(shader)
        shader.uniform_float("color", color)
        batch.draw()
