# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

# NOTE: Generated from latest glTF 2.0 JSON Scheme specs using quicktype (https://github.com/quicktype/quicktype)
# command used:
# quicktype --src glTF.schema.json --src-lang schema -t gltf --lang python --python-version 3.5

# TODO: add __slots__ to all classes by extending the generator

# TODO: REMOVE traceback import

# NOTE: this file is modified for addonExtension use. See
# https://github.com/KhronosGroup/glTF-Blender-IO/commit/62ff119d8ffeab48f66e9d2699741407d532fe0f

import sys
import traceback

from ...io.com import gltf2_io_debug


def from_int(x):
    assert isinstance(x, int) and not isinstance(x, bool)
    return x


def from_none(x):
    assert x is None
    return x


def from_union(fs, x):
    tracebacks = []
    for f in fs:
        try:
            return f(x)
        except AssertionError:
            _, _, tb = sys.exc_info()
            tracebacks.append(tb)
    for tb in tracebacks:
        traceback.print_tb(tb)  # Fixed format
        tb_info = traceback.extract_tb(tb)
        for tbi in tb_info:
            filename, line, func, text = tbi
            print('ERROR', 'An error occurred on line {} in statement {}'.format(line, text))
    assert False


def from_dict(f, x):
    assert isinstance(x, dict)
    return {k: f(v) for (k, v) in x.items()}


def to_class(c, x):
    assert isinstance(x, c)
    return x.to_dict()


def from_list(f, x):
    assert isinstance(x, list)
    return [f(y) for y in x]


def from_float(x):
    assert isinstance(x, (float, int)) and not isinstance(x, bool)
    return float(x)


def from_str(x):
    assert isinstance(x, str)
    return x


def from_bool(x):
    assert isinstance(x, bool)
    return x


def to_float(x):
    assert isinstance(x, float)
    return x


def extension_to_dict(obj):
    if hasattr(obj, 'to_list'):
        obj = obj.to_list()
    if hasattr(obj, 'to_dict'):
        obj = obj.to_dict()
    if isinstance(obj, list):
        return [extension_to_dict(x) for x in obj]
    elif isinstance(obj, dict):
        return {k: extension_to_dict(v) for (k, v) in obj.items()}
    return obj


def from_extension(x):
    x = extension_to_dict(x)
    assert isinstance(x, dict)
    return x


def from_extra(x):
    return extension_to_dict(x)


class AccessorSparseIndices:
    """Index array of size `count` that points to those accessor attributes that deviate from
    their initialization value. Indices must strictly increase.

    Indices of those attributes that deviate from their initialization value.
    """

    def __init__(self, buffer_view, byte_offset, component_type, extensions, extras):
        self.buffer_view = buffer_view
        self.byte_offset = byte_offset
        self.component_type = component_type
        self.extensions = extensions
        self.extras = extras

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        buffer_view = from_int(obj.get("bufferView"))
        byte_offset = from_union([from_int, from_none], obj.get("byteOffset"))
        component_type = from_int(obj.get("componentType"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        return AccessorSparseIndices(buffer_view, byte_offset, component_type, extensions, extras)

    def to_dict(self):
        result = {}
        result["bufferView"] = from_int(self.buffer_view)
        result["byteOffset"] = from_union([from_int, from_none], self.byte_offset)
        result["componentType"] = from_int(self.component_type)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        return result


class AccessorSparseValues:
    """Array of size `count` times number of components, storing the displaced accessor
    attributes pointed by `indices`. Substituted values must have the same `componentType`
    and number of components as the base accessor.

    Array of size `accessor.sparse.count` times number of components storing the displaced
    accessor attributes pointed by `accessor.sparse.indices`.
    """

    def __init__(self, buffer_view, byte_offset, extensions, extras):
        self.buffer_view = buffer_view
        self.byte_offset = byte_offset
        self.extensions = extensions
        self.extras = extras

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        buffer_view = from_int(obj.get("bufferView"))
        byte_offset = from_union([from_int, from_none], obj.get("byteOffset"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        return AccessorSparseValues(buffer_view, byte_offset, extensions, extras)

    def to_dict(self):
        result = {}
        result["bufferView"] = from_int(self.buffer_view)
        result["byteOffset"] = from_union([from_int, from_none], self.byte_offset)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        return result


class AccessorSparse:
    """Sparse storage of attributes that deviate from their initialization value."""

    def __init__(self, count, extensions, extras, indices, values):
        self.count = count
        self.extensions = extensions
        self.extras = extras
        self.indices = indices
        self.values = values

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        count = from_int(obj.get("count"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        indices = AccessorSparseIndices.from_dict(obj.get("indices"))
        values = AccessorSparseValues.from_dict(obj.get("values"))
        return AccessorSparse(count, extensions, extras, indices, values)

    def to_dict(self):
        result = {}
        result["count"] = from_int(self.count)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["indices"] = to_class(AccessorSparseIndices, self.indices)
        result["values"] = to_class(AccessorSparseValues, self.values)
        return result


class Accessor:
    """A typed view into a bufferView.  A bufferView contains raw binary data.  An accessor
    provides a typed view into a bufferView or a subset of a bufferView similar to how
    WebGL's `vertexAttribPointer()` defines an attribute in a buffer.
    """

    def __init__(self, buffer_view, byte_offset, component_type, count, extensions, extras, max, min, name, normalized,
                 sparse, type):
        self.buffer_view = buffer_view
        self.byte_offset = byte_offset
        self.component_type = component_type
        self.count = count
        self.extensions = extensions
        self.extras = extras
        self.max = max
        self.min = min
        self.name = name
        self.normalized = normalized
        self.sparse = sparse
        self.type = type

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        buffer_view = from_union([from_int, from_none], obj.get("bufferView"))
        byte_offset = from_union([from_int, from_none], obj.get("byteOffset"))
        component_type = from_int(obj.get("componentType"))
        count = from_int(obj.get("count"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        max = from_union([lambda x: from_list(from_float, x), from_none], obj.get("max"))
        min = from_union([lambda x: from_list(from_float, x), from_none], obj.get("min"))
        name = from_union([from_str, from_none], obj.get("name"))
        normalized = from_union([from_bool, from_none], obj.get("normalized"))
        sparse = from_union([AccessorSparse.from_dict, from_none], obj.get("sparse"))
        type = from_str(obj.get("type"))
        return Accessor(buffer_view, byte_offset, component_type, count, extensions, extras, max, min, name, normalized,
                        sparse, type)

    def to_dict(self):
        result = {}
        result["bufferView"] = from_union([from_int, from_none], self.buffer_view)
        result["byteOffset"] = from_union([from_int, from_none], self.byte_offset)
        result["componentType"] = from_int(self.component_type)
        result["count"] = from_int(self.count)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["max"] = from_union([lambda x: from_list(to_float, x), from_none], self.max)
        result["min"] = from_union([lambda x: from_list(to_float, x), from_none], self.min)
        result["name"] = from_union([from_str, from_none], self.name)
        result["normalized"] = from_union([from_bool, from_none], self.normalized)
        result["sparse"] = from_union([lambda x: to_class(AccessorSparse, x), from_none], self.sparse)
        result["type"] = from_str(self.type)
        return result


class AnimationChannelTarget:
    """The index of the node and TRS property to target.

    The index of the node and TRS property that an animation channel targets.
    """

    def __init__(self, extensions, extras, node, path):
        self.extensions = extensions
        self.extras = extras
        self.node = node
        self.path = path

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        node = from_union([from_int, from_none], obj.get("node"))
        path = from_str(obj.get("path"))
        return AnimationChannelTarget(extensions, extras, node, path)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["node"] = from_union([from_int, from_none], self.node)
        result["path"] = from_str(self.path)
        return result


class AnimationChannel:
    """Targets an animation's sampler at a node's property."""

    def __init__(self, extensions, extras, sampler, target):
        self.extensions = extensions
        self.extras = extras
        self.sampler = sampler
        self.target = target

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        sampler = from_int(obj.get("sampler"))
        target = AnimationChannelTarget.from_dict(obj.get("target"))
        return AnimationChannel(extensions, extras, sampler, target)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["sampler"] = from_int(self.sampler)
        result["target"] = to_class(AnimationChannelTarget, self.target)
        return result


class AnimationSampler:
    """Combines input and output accessors with an interpolation algorithm to define a keyframe
    graph (but not its target).
    """

    def __init__(self, extensions, extras, input, interpolation, output):
        self.extensions = extensions
        self.extras = extras
        self.input = input
        self.interpolation = interpolation
        self.output = output

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        input = from_int(obj.get("input"))
        interpolation = from_union([from_str, from_none], obj.get("interpolation"))
        output = from_int(obj.get("output"))
        return AnimationSampler(extensions, extras, input, interpolation, output)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["input"] = from_int(self.input)
        result["interpolation"] = from_union([from_str, from_none], self.interpolation)
        result["output"] = from_int(self.output)
        return result


class Animation:
    """A keyframe animation."""

    def __init__(self, channels, extensions, extras, name, samplers):
        self.channels = channels
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.samplers = samplers

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        channels = from_list(AnimationChannel.from_dict, obj.get("channels"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        samplers = from_list(AnimationSampler.from_dict, obj.get("samplers"))
        return Animation(channels, extensions, extras, name, samplers)

    def to_dict(self):
        result = {}
        result["channels"] = from_list(lambda x: to_class(AnimationChannel, x), self.channels)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["samplers"] = from_list(lambda x: to_class(AnimationSampler, x), self.samplers)
        return result


class Asset:
    """Metadata about the glTF asset."""

    def __init__(self, copyright, extensions, extras, generator, min_version, version):
        self.copyright = copyright
        self.extensions = extensions
        self.extras = extras
        self.generator = generator
        self.min_version = min_version
        self.version = version

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        copyright = from_union([from_str, from_none], obj.get("copyright"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        generator = from_union([from_str, from_none], obj.get("generator"))
        min_version = from_union([from_str, from_none], obj.get("minVersion"))
        version = from_str(obj.get("version"))
        return Asset(copyright, extensions, extras, generator, min_version, version)

    def to_dict(self):
        result = {}
        result["copyright"] = from_union([from_str, from_none], self.copyright)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["generator"] = from_union([from_str, from_none], self.generator)
        result["minVersion"] = from_union([from_str, from_none], self.min_version)
        result["version"] = from_str(self.version)
        return result


class BufferView:
    """A view into a buffer generally representing a subset of the buffer."""

    def __init__(self, buffer, byte_length, byte_offset, byte_stride, extensions, extras, name, target):
        self.buffer = buffer
        self.byte_length = byte_length
        self.byte_offset = byte_offset
        self.byte_stride = byte_stride
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.target = target

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        buffer = from_int(obj.get("buffer"))
        byte_length = from_int(obj.get("byteLength"))
        byte_offset = from_union([from_int, from_none], obj.get("byteOffset"))
        byte_stride = from_union([from_int, from_none], obj.get("byteStride"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        target = from_union([from_int, from_none], obj.get("target"))
        return BufferView(buffer, byte_length, byte_offset, byte_stride, extensions, extras, name, target)

    def to_dict(self):
        result = {}
        result["buffer"] = from_int(self.buffer)
        result["byteLength"] = from_int(self.byte_length)
        result["byteOffset"] = from_union([from_int, from_none], self.byte_offset)
        result["byteStride"] = from_union([from_int, from_none], self.byte_stride)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["target"] = from_union([from_int, from_none], self.target)
        return result


class Buffer:
    """A buffer points to binary geometry, animation, or skins."""

    def __init__(self, byte_length, extensions, extras, name, uri):
        self.byte_length = byte_length
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.uri = uri

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        byte_length = from_int(obj.get("byteLength"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        uri = from_union([from_str, from_none], obj.get("uri"))
        return Buffer(byte_length, extensions, extras, name, uri)

    def to_dict(self):
        result = {}
        result["byteLength"] = from_int(self.byte_length)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["uri"] = from_union([from_str, from_none], self.uri)
        return result


class CameraOrthographic:
    """An orthographic camera containing properties to create an orthographic projection matrix."""

    def __init__(self, extensions, extras, xmag, ymag, zfar, znear):
        self.extensions = extensions
        self.extras = extras
        self.xmag = xmag
        self.ymag = ymag
        self.zfar = zfar
        self.znear = znear

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        xmag = from_float(obj.get("xmag"))
        ymag = from_float(obj.get("ymag"))
        zfar = from_float(obj.get("zfar"))
        znear = from_float(obj.get("znear"))
        return CameraOrthographic(extensions, extras, xmag, ymag, zfar, znear)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["xmag"] = to_float(self.xmag)
        result["ymag"] = to_float(self.ymag)
        result["zfar"] = to_float(self.zfar)
        result["znear"] = to_float(self.znear)
        return result


class CameraPerspective:
    """A perspective camera containing properties to create a perspective projection matrix."""

    def __init__(self, aspect_ratio, extensions, extras, yfov, zfar, znear):
        self.aspect_ratio = aspect_ratio
        self.extensions = extensions
        self.extras = extras
        self.yfov = yfov
        self.zfar = zfar
        self.znear = znear

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        aspect_ratio = from_union([from_float, from_none], obj.get("aspectRatio"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        yfov = from_float(obj.get("yfov"))
        zfar = from_union([from_float, from_none], obj.get("zfar"))
        znear = from_float(obj.get("znear"))
        return CameraPerspective(aspect_ratio, extensions, extras, yfov, zfar, znear)

    def to_dict(self):
        result = {}
        result["aspectRatio"] = from_union([to_float, from_none], self.aspect_ratio)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["yfov"] = to_float(self.yfov)
        result["zfar"] = from_union([to_float, from_none], self.zfar)
        result["znear"] = to_float(self.znear)
        return result


class Camera:
    """A camera's projection.  A node can reference a camera to apply a transform to place the
    camera in the scene.
    """

    def __init__(self, extensions, extras, name, orthographic, perspective, type):
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.orthographic = orthographic
        self.perspective = perspective
        self.type = type

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        orthographic = from_union([CameraOrthographic.from_dict, from_none], obj.get("orthographic"))
        perspective = from_union([CameraPerspective.from_dict, from_none], obj.get("perspective"))
        type = from_str(obj.get("type"))
        return Camera(extensions, extras, name, orthographic, perspective, type)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["orthographic"] = from_union([lambda x: to_class(CameraOrthographic, x), from_none], self.orthographic)
        result["perspective"] = from_union([lambda x: to_class(CameraPerspective, x), from_none], self.perspective)
        result["type"] = from_str(self.type)
        return result


class Image:
    """Image data used to create a texture. Image can be referenced by URI or `bufferView`
    index. `mimeType` is required in the latter case.
    """

    def __init__(self, buffer_view, extensions, extras, mime_type, name, uri):
        self.buffer_view = buffer_view
        self.extensions = extensions
        self.extras = extras
        self.mime_type = mime_type
        self.name = name
        self.uri = uri

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        buffer_view = from_union([from_int, from_none], obj.get("bufferView"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        mime_type = from_union([from_str, from_none], obj.get("mimeType"))
        name = from_union([from_str, from_none], obj.get("name"))
        uri = from_union([from_str, from_none], obj.get("uri"))
        return Image(buffer_view, extensions, extras, mime_type, name, uri)

    def to_dict(self):
        result = {}
        result["bufferView"] = from_union([from_int, from_none], self.buffer_view)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["mimeType"] = from_union([from_str, from_none], self.mime_type)
        result["name"] = from_union([from_str, from_none], self.name)
        result["uri"] = from_union([from_str, from_none], self.uri)
        return result


class TextureInfo:
    """The emissive map texture.

    The base color texture.

    The metallic-roughness texture.

    Reference to a texture.
    """

    def __init__(self, extensions, extras, index, tex_coord):
        self.extensions = extensions
        self.extras = extras
        self.index = index
        self.tex_coord = tex_coord

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        index = from_int(obj.get("index"))
        tex_coord = from_union([from_int, from_none], obj.get("texCoord"))
        return TextureInfo(extensions, extras, index, tex_coord)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["index"] = from_int(self.index)
        result["texCoord"] = from_union([from_int, from_none], self.tex_coord)
        return result


class MaterialNormalTextureInfoClass:
    """The normal map texture.

    Reference to a texture.
    """

    def __init__(self, extensions, extras, index, scale, tex_coord):
        self.extensions = extensions
        self.extras = extras
        self.index = index
        self.scale = scale
        self.tex_coord = tex_coord

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        index = from_int(obj.get("index"))
        scale = from_union([from_float, from_none], obj.get("scale"))
        tex_coord = from_union([from_int, from_none], obj.get("texCoord"))
        return MaterialNormalTextureInfoClass(extensions, extras, index, scale, tex_coord)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["index"] = from_int(self.index)
        result["scale"] = from_union([to_float, from_none], self.scale)
        result["texCoord"] = from_union([from_int, from_none], self.tex_coord)
        return result


class MaterialOcclusionTextureInfoClass:
    """The occlusion map texture.

    Reference to a texture.
    """

    def __init__(self, extensions, extras, index, strength, tex_coord):
        self.extensions = extensions
        self.extras = extras
        self.index = index
        self.strength = strength
        self.tex_coord = tex_coord

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        index = from_int(obj.get("index"))
        strength = from_union([from_float, from_none], obj.get("strength"))
        tex_coord = from_union([from_int, from_none], obj.get("texCoord"))
        return MaterialOcclusionTextureInfoClass(extensions, extras, index, strength, tex_coord)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["index"] = from_int(self.index)
        result["strength"] = from_union([to_float, from_none], self.strength)
        result["texCoord"] = from_union([from_int, from_none], self.tex_coord)
        return result


class MaterialPBRMetallicRoughness:
    """A set of parameter values that are used to define the metallic-roughness material model
    from Physically-Based Rendering (PBR) methodology. When not specified, all the default
    values of `pbrMetallicRoughness` apply.

    A set of parameter values that are used to define the metallic-roughness material model
    from Physically-Based Rendering (PBR) methodology.
    """

    def __init__(self, base_color_factor, base_color_texture, extensions, extras, metallic_factor,
                 metallic_roughness_texture, roughness_factor):
        self.base_color_factor = base_color_factor
        self.base_color_texture = base_color_texture
        self.extensions = extensions
        self.extras = extras
        self.metallic_factor = metallic_factor
        self.metallic_roughness_texture = metallic_roughness_texture
        self.roughness_factor = roughness_factor

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        base_color_factor = from_union([lambda x: from_list(from_float, x), from_none], obj.get("baseColorFactor"))
        base_color_texture = from_union([TextureInfo.from_dict, from_none], obj.get("baseColorTexture"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        metallic_factor = from_union([from_float, from_none], obj.get("metallicFactor"))
        metallic_roughness_texture = from_union([TextureInfo.from_dict, from_none], obj.get("metallicRoughnessTexture"))
        roughness_factor = from_union([from_float, from_none], obj.get("roughnessFactor"))
        return MaterialPBRMetallicRoughness(base_color_factor, base_color_texture, extensions, extras, metallic_factor,
                                            metallic_roughness_texture, roughness_factor)

    def to_dict(self):
        result = {}
        result["baseColorFactor"] = from_union([lambda x: from_list(to_float, x), from_none], self.base_color_factor)
        result["baseColorTexture"] = from_union([lambda x: to_class(TextureInfo, x), from_none],
                                                self.base_color_texture)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["metallicFactor"] = from_union([to_float, from_none], self.metallic_factor)
        result["metallicRoughnessTexture"] = from_union([lambda x: to_class(TextureInfo, x), from_none],
                                                        self.metallic_roughness_texture)
        result["roughnessFactor"] = from_union([to_float, from_none], self.roughness_factor)
        return result


class Material:
    """The material appearance of a primitive."""

    def __init__(self, alpha_cutoff, alpha_mode, double_sided, emissive_factor, emissive_texture, extensions, extras,
                 name, normal_texture, occlusion_texture, pbr_metallic_roughness):
        self.alpha_cutoff = alpha_cutoff
        self.alpha_mode = alpha_mode
        self.double_sided = double_sided
        self.emissive_factor = emissive_factor
        self.emissive_texture = emissive_texture
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.normal_texture = normal_texture
        self.occlusion_texture = occlusion_texture
        self.pbr_metallic_roughness = pbr_metallic_roughness

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        alpha_cutoff = from_union([from_float, from_none], obj.get("alphaCutoff"))
        alpha_mode = from_union([from_str, from_none], obj.get("alphaMode"))
        double_sided = from_union([from_bool, from_none], obj.get("doubleSided"))
        emissive_factor = from_union([lambda x: from_list(from_float, x), from_none], obj.get("emissiveFactor"))
        emissive_texture = from_union([TextureInfo.from_dict, from_none], obj.get("emissiveTexture"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        normal_texture = from_union([MaterialNormalTextureInfoClass.from_dict, from_none], obj.get("normalTexture"))
        occlusion_texture = from_union([MaterialOcclusionTextureInfoClass.from_dict, from_none],
                                       obj.get("occlusionTexture"))
        pbr_metallic_roughness = from_union([MaterialPBRMetallicRoughness.from_dict, from_none],
                                            obj.get("pbrMetallicRoughness"))
        return Material(alpha_cutoff, alpha_mode, double_sided, emissive_factor, emissive_texture, extensions, extras,
                        name, normal_texture, occlusion_texture, pbr_metallic_roughness)

    def to_dict(self):
        result = {}
        result["alphaCutoff"] = from_union([to_float, from_none], self.alpha_cutoff)
        result["alphaMode"] = from_union([from_str, from_none], self.alpha_mode)
        result["doubleSided"] = from_union([from_bool, from_none], self.double_sided)
        result["emissiveFactor"] = from_union([lambda x: from_list(to_float, x), from_none], self.emissive_factor)
        result["emissiveTexture"] = from_union([lambda x: to_class(TextureInfo, x), from_none], self.emissive_texture)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["normalTexture"] = from_union([lambda x: to_class(MaterialNormalTextureInfoClass, x), from_none],
                                             self.normal_texture)
        result["occlusionTexture"] = from_union([lambda x: to_class(MaterialOcclusionTextureInfoClass, x), from_none],
                                                self.occlusion_texture)
        result["pbrMetallicRoughness"] = from_union([lambda x: to_class(MaterialPBRMetallicRoughness, x), from_none],
                                                    self.pbr_metallic_roughness)
        return result


class MeshPrimitive:
    """Geometry to be rendered with the given material."""

    def __init__(self, attributes, extensions, extras, indices, material, mode, targets):
        self.attributes = attributes
        self.extensions = extensions
        self.extras = extras
        self.indices = indices
        self.material = material
        self.mode = mode
        self.targets = targets

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        attributes = from_dict(from_int, obj.get("attributes"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        indices = from_union([from_int, from_none], obj.get("indices"))
        material = from_union([from_int, from_none], obj.get("material"))
        mode = from_union([from_int, from_none], obj.get("mode"))
        targets = from_union([lambda x: from_list(lambda x: from_dict(from_int, x), x), from_none], obj.get("targets"))
        return MeshPrimitive(attributes, extensions, extras, indices, material, mode, targets)

    def to_dict(self):
        result = {}
        result["attributes"] = from_dict(from_int, self.attributes)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["indices"] = from_union([from_int, from_none], self.indices)
        result["material"] = from_union([from_int, from_none], self.material)
        result["mode"] = from_union([from_int, from_none], self.mode)
        result["targets"] = from_union([lambda x: from_list(lambda x: from_dict(from_int, x), x), from_none],
                                       self.targets)
        return result


class Mesh:
    """A set of primitives to be rendered.  A node can contain one mesh.  A node's transform
    places the mesh in the scene.
    """

    def __init__(self, extensions, extras, name, primitives, weights):
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.primitives = primitives
        self.weights = weights

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        primitives = from_list(MeshPrimitive.from_dict, obj.get("primitives"))
        weights = from_union([lambda x: from_list(from_float, x), from_none], obj.get("weights"))
        return Mesh(extensions, extras, name, primitives, weights)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["primitives"] = from_list(lambda x: to_class(MeshPrimitive, x), self.primitives)
        result["weights"] = from_union([lambda x: from_list(to_float, x), from_none], self.weights)
        return result


class Node:
    """A node in the node hierarchy.  When the node contains `skin`, all `mesh.primitives` must
    contain `JOINTS_0` and `WEIGHTS_0` attributes.  A node can have either a `matrix` or any
    combination of `translation`/`rotation`/`scale` (TRS) properties. TRS properties are
    converted to matrices and postmultiplied in the `T * R * S` order to compose the
    transformation matrix; first the scale is applied to the vertices, then the rotation, and
    then the translation. If none are provided, the transform is the identity. When a node is
    targeted for animation (referenced by an animation.channel.target), only TRS properties
    may be present; `matrix` will not be present.
    """

    def __init__(self, camera, children, extensions, extras, matrix, mesh, name, rotation, scale, skin, translation,
                 weights):
        self.camera = camera
        self.children = children
        self.extensions = extensions
        self.extras = extras
        self.matrix = matrix
        self.mesh = mesh
        self.name = name
        self.rotation = rotation
        self.scale = scale
        self.skin = skin
        self.translation = translation
        self.weights = weights

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        camera = from_union([from_int, from_none], obj.get("camera"))
        children = from_union([lambda x: from_list(from_int, x), from_none], obj.get("children"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        matrix = from_union([lambda x: from_list(from_float, x), from_none], obj.get("matrix"))
        mesh = from_union([from_int, from_none], obj.get("mesh"))
        name = from_union([from_str, from_none], obj.get("name"))
        rotation = from_union([lambda x: from_list(from_float, x), from_none], obj.get("rotation"))
        scale = from_union([lambda x: from_list(from_float, x), from_none], obj.get("scale"))
        skin = from_union([from_int, from_none], obj.get("skin"))
        translation = from_union([lambda x: from_list(from_float, x), from_none], obj.get("translation"))
        weights = from_union([lambda x: from_list(from_float, x), from_none], obj.get("weights"))
        return Node(camera, children, extensions, extras, matrix, mesh, name, rotation, scale, skin, translation,
                    weights)

    def to_dict(self):
        result = {}
        result["camera"] = from_union([from_int, from_none], self.camera)
        result["children"] = from_union([lambda x: from_list(from_int, x), from_none], self.children)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["matrix"] = from_union([lambda x: from_list(to_float, x), from_none], self.matrix)
        result["mesh"] = from_union([from_int, from_none], self.mesh)
        result["name"] = from_union([from_str, from_none], self.name)
        result["rotation"] = from_union([lambda x: from_list(to_float, x), from_none], self.rotation)
        result["scale"] = from_union([lambda x: from_list(to_float, x), from_none], self.scale)
        result["skin"] = from_union([from_int, from_none], self.skin)
        result["translation"] = from_union([lambda x: from_list(to_float, x), from_none], self.translation)
        result["weights"] = from_union([lambda x: from_list(to_float, x), from_none], self.weights)
        return result


class Sampler:
    """Texture sampler properties for filtering and wrapping modes."""

    def __init__(self, extensions, extras, mag_filter, min_filter, name, wrap_s, wrap_t):
        self.extensions = extensions
        self.extras = extras
        self.mag_filter = mag_filter
        self.min_filter = min_filter
        self.name = name
        self.wrap_s = wrap_s
        self.wrap_t = wrap_t

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        mag_filter = from_union([from_int, from_none], obj.get("magFilter"))
        min_filter = from_union([from_int, from_none], obj.get("minFilter"))
        name = from_union([from_str, from_none], obj.get("name"))
        wrap_s = from_union([from_int, from_none], obj.get("wrapS"))
        wrap_t = from_union([from_int, from_none], obj.get("wrapT"))
        return Sampler(extensions, extras, mag_filter, min_filter, name, wrap_s, wrap_t)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["magFilter"] = from_union([from_int, from_none], self.mag_filter)
        result["minFilter"] = from_union([from_int, from_none], self.min_filter)
        result["name"] = from_union([from_str, from_none], self.name)
        result["wrapS"] = from_union([from_int, from_none], self.wrap_s)
        result["wrapT"] = from_union([from_int, from_none], self.wrap_t)
        return result


class Scene:
    """The root nodes of a scene."""

    def __init__(self, extensions, extras, name, nodes):
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.nodes = nodes

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        nodes = from_union([lambda x: from_list(from_int, x), from_none], obj.get("nodes"))
        return Scene(extensions, extras, name, nodes)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["nodes"] = from_union([lambda x: from_list(from_int, x), from_none], self.nodes)
        return result


class Skin:
    """Joints and matrices defining a skin."""

    def __init__(self, extensions, extras, inverse_bind_matrices, joints, name, skeleton):
        self.extensions = extensions
        self.extras = extras
        self.inverse_bind_matrices = inverse_bind_matrices
        self.joints = joints
        self.name = name
        self.skeleton = skeleton

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        inverse_bind_matrices = from_union([from_int, from_none], obj.get("inverseBindMatrices"))
        joints = from_list(from_int, obj.get("joints"))
        name = from_union([from_str, from_none], obj.get("name"))
        skeleton = from_union([from_int, from_none], obj.get("skeleton"))
        return Skin(extensions, extras, inverse_bind_matrices, joints, name, skeleton)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["inverseBindMatrices"] = from_union([from_int, from_none], self.inverse_bind_matrices)
        result["joints"] = from_list(from_int, self.joints)
        result["name"] = from_union([from_str, from_none], self.name)
        result["skeleton"] = from_union([from_int, from_none], self.skeleton)
        return result


class Texture:
    """A texture and its sampler."""

    def __init__(self, extensions, extras, name, sampler, source):
        self.extensions = extensions
        self.extras = extras
        self.name = name
        self.sampler = sampler
        self.source = source

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        name = from_union([from_str, from_none], obj.get("name"))
        sampler = from_union([from_int, from_none], obj.get("sampler"))
        source = from_union([from_int, from_none], obj.get("source"))
        return Texture(extensions, extras, name, sampler, source)

    def to_dict(self):
        result = {}
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extras"] = from_extra(self.extras)
        result["name"] = from_union([from_str, from_none], self.name)
        result["sampler"] = from_union([from_int, from_none], self.sampler)
        result["source"] = from_union([from_int, from_none], self.source)
        return result


class Gltf:
    """The root object for a glTF asset."""

    def __init__(self, accessors, animations, asset, buffers, buffer_views, cameras, extensions, extensions_required,
                 extensions_used, extras, images, materials, meshes, nodes, samplers, scene, scenes, skins, textures):
        self.accessors = accessors
        self.animations = animations
        self.asset = asset
        self.buffers = buffers
        self.buffer_views = buffer_views
        self.cameras = cameras
        self.extensions = extensions
        self.extensions_required = extensions_required
        self.extensions_used = extensions_used
        self.extras = extras
        self.images = images
        self.materials = materials
        self.meshes = meshes
        self.nodes = nodes
        self.samplers = samplers
        self.scene = scene
        self.scenes = scenes
        self.skins = skins
        self.textures = textures

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        accessors = from_union([lambda x: from_list(Accessor.from_dict, x), from_none], obj.get("accessors"))
        animations = from_union([lambda x: from_list(Animation.from_dict, x), from_none], obj.get("animations"))
        asset = Asset.from_dict(obj.get("asset"))
        buffers = from_union([lambda x: from_list(Buffer.from_dict, x), from_none], obj.get("buffers"))
        buffer_views = from_union([lambda x: from_list(BufferView.from_dict, x), from_none], obj.get("bufferViews"))
        cameras = from_union([lambda x: from_list(Camera.from_dict, x), from_none], obj.get("cameras"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extensions_required = from_union([lambda x: from_list(from_str, x), from_none], obj.get("extensionsRequired"))
        extensions_used = from_union([lambda x: from_list(from_str, x), from_none], obj.get("extensionsUsed"))
        extras = obj.get("extras")
        images = from_union([lambda x: from_list(Image.from_dict, x), from_none], obj.get("images"))
        materials = from_union([lambda x: from_list(Material.from_dict, x), from_none], obj.get("materials"))
        meshes = from_union([lambda x: from_list(Mesh.from_dict, x), from_none], obj.get("meshes"))
        nodes = from_union([lambda x: from_list(Node.from_dict, x), from_none], obj.get("nodes"))
        samplers = from_union([lambda x: from_list(Sampler.from_dict, x), from_none], obj.get("samplers"))
        scene = from_union([from_int, from_none], obj.get("scene"))
        scenes = from_union([lambda x: from_list(Scene.from_dict, x), from_none], obj.get("scenes"))
        skins = from_union([lambda x: from_list(Skin.from_dict, x), from_none], obj.get("skins"))
        textures = from_union([lambda x: from_list(Texture.from_dict, x), from_none], obj.get("textures"))
        return Gltf(accessors, animations, asset, buffers, buffer_views, cameras, extensions, extensions_required,
                    extensions_used, extras, images, materials, meshes, nodes, samplers, scene, scenes, skins, textures)

    def to_dict(self):
        result = {}
        result["accessors"] = from_union([lambda x: from_list(lambda x: to_class(Accessor, x), x), from_none],
                                         self.accessors)
        result["animations"] = from_union([lambda x: from_list(lambda x: to_class(Animation, x), x), from_none],
                                          self.animations)
        result["asset"] = to_class(Asset, self.asset)
        result["buffers"] = from_union([lambda x: from_list(lambda x: to_class(Buffer, x), x), from_none], self.buffers)
        result["bufferViews"] = from_union([lambda x: from_list(lambda x: to_class(BufferView, x), x), from_none],
                                           self.buffer_views)
        result["cameras"] = from_union([lambda x: from_list(lambda x: to_class(Camera, x), x), from_none], self.cameras)
        result["extensions"] = from_union([lambda x: from_dict(from_extension, x), from_none],
                                          self.extensions)
        result["extensionsRequired"] = from_union([lambda x: from_list(from_str, x), from_none],
                                                  self.extensions_required)
        result["extensionsUsed"] = from_union([lambda x: from_list(from_str, x), from_none], self.extensions_used)
        result["extras"] = from_extra(self.extras)
        result["images"] = from_union([lambda x: from_list(lambda x: to_class(Image, x), x), from_none], self.images)
        result["materials"] = from_union([lambda x: from_list(lambda x: to_class(Material, x), x), from_none],
                                         self.materials)
        result["meshes"] = from_union([lambda x: from_list(lambda x: to_class(Mesh, x), x), from_none], self.meshes)
        result["nodes"] = from_union([lambda x: from_list(lambda x: to_class(Node, x), x), from_none], self.nodes)
        result["samplers"] = from_union([lambda x: from_list(lambda x: to_class(Sampler, x), x), from_none],
                                        self.samplers)
        result["scene"] = from_union([from_int, from_none], self.scene)
        result["scenes"] = from_union([lambda x: from_list(lambda x: to_class(Scene, x), x), from_none], self.scenes)
        result["skins"] = from_union([lambda x: from_list(lambda x: to_class(Skin, x), x), from_none], self.skins)
        result["textures"] = from_union([lambda x: from_list(lambda x: to_class(Texture, x), x), from_none],
                                        self.textures)
        return result


def gltf_from_dict(s):
    return Gltf.from_dict(s)


def gltf_to_dict(x):
    return to_class(Gltf, x)
