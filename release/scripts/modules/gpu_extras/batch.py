# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "batch_for_shader",
)


def batch_for_shader(shader, type, content, *, indices=None):
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
    from gpu.types import (
        GPUBatch,
        GPUIndexBuf,
        GPUVertBuf,
        GPUVertFormat,
    )

    def recommended_comp_type(attr_type):
        if attr_type in {'FLOAT', 'VEC2', 'VEC3', 'VEC4', 'MAT3', 'MAT4'}:
            return 'F32'
        if attr_type in {'UINT', 'UVEC2', 'UVEC3', 'UVEC4'}:
            return 'U32'
        # `attr_type` in {'INT', 'IVEC2', 'IVEC3', 'IVEC4', 'BOOL'}.
        return 'I32'

    def recommended_attr_len(attr_name):
        attr_len = 1
        try:
            item = content[attr_name][0]
            while True:
                attr_len *= len(item)
                item = item[0]
        except (TypeError, IndexError):
            pass
        return attr_len

    def recommended_fetch_mode(comp_type):
        if comp_type == 'F32':
            return 'FLOAT'
        return 'INT'

    for data in content.values():
        vbo_len = len(data)
        break
    else:
        raise ValueError("Empty 'content'")

    vbo_format = GPUVertFormat()
    attrs_info = shader.attrs_info_get()
    for name, attr_type in attrs_info:
        comp_type = recommended_comp_type(attr_type)
        attr_len = recommended_attr_len(name)
        vbo_format.attr_add(id=name, comp_type=comp_type, len=attr_len, fetch_mode=recommended_fetch_mode(comp_type))

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
