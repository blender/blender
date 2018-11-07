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

__all__ = (
    "batch_for_shader",
)


def batch_for_shader(shader, type, content, indices=None):
    """
    Return a batch already configured and compatible with the shader.

    :arg shader: shader for which a compatible format will be computed.
    :type shader: :class:`gpu.types.GPUShader`
    :arg type: "'POINTS', 'LINES', 'TRIS' or 'LINES_ADJ'".
    :type type: str
    :arg content: Maps the name of the shader attribute with the data to fill the vertex buffer.
    :type content: dict
    :return: compatible batch
    :rtype: :class:`gpu.types.Batch`
    """
    import gpu
    from gpu.types import (
        GPUBatch,
        GPUIndexBuf,
        GPUVertBuf,
    )

    for data in content.values():
        vbo_len = len(data)
        break
    else:
        raise ValueError("Empty 'content'")

    vbo_format = shader.format_calc()
    vbo = GPUVertBuf(vbo_format, vbo_len)

    for id, data in content.items():
        if len(data) != vbo_len:
            raise ValueError("Length mismatch for 'content' values")
        vbo.attr_fill(id, data)

    if indices is None:
        return GPUBatch(type=type, buf=vbo)
    else:
        ibo = GPUIndexBuf(type=type, seq=indices)
        return GPUBatch(type=type, buf=vbo, elem=ibo)
