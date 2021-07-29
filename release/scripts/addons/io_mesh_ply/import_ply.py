# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import re
import struct


class element_spec(object):
    __slots__ = ("name",
                 "count",
                 "properties",
                 )

    def __init__(self, name, count):
        self.name = name
        self.count = count
        self.properties = []

    def load(self, format, stream):
        if format == b'ascii':
            stream = stream.readline().split()
        return [x.load(format, stream) for x in self.properties]

    def index(self, name):
        for i, p in enumerate(self.properties):
            if p.name == name:
                return i
        return -1


class property_spec(object):
    __slots__ = ("name",
                 "list_type",
                 "numeric_type",
                 )

    def __init__(self, name, list_type, numeric_type):
        self.name = name
        self.list_type = list_type
        self.numeric_type = numeric_type

    def read_format(self, format, count, num_type, stream):
        if format == b'ascii':
            if num_type == 's':
                ans = []
                for i in range(count):
                    s = stream[i]
                    if len(s) < 2 or s[0] != '"' or s[-1] != '"':
                        print('Invalid string', s)
                        print('Note: ply_import.py does not handle whitespace in strings')
                        return None
                    ans.append(s[1:-1])
                stream[:count] = []
                return ans
            if num_type == 'f' or num_type == 'd':
                mapper = float
            else:
                mapper = int
            ans = [mapper(x) for x in stream[:count]]
            stream[:count] = []
            return ans
        else:
            if num_type == 's':
                ans = []
                for i in range(count):
                    fmt = format + 'i'
                    data = stream.read(struct.calcsize(fmt))
                    length = struct.unpack(fmt, data)[0]
                    fmt = '%s%is' % (format, length)
                    data = stream.read(struct.calcsize(fmt))
                    s = struct.unpack(fmt, data)[0]
                    ans.append(s[:-1])  # strip the NULL
                return ans
            else:
                fmt = '%s%i%s' % (format, count, num_type)
                data = stream.read(struct.calcsize(fmt))
                return struct.unpack(fmt, data)

    def load(self, format, stream):
        if self.list_type is not None:
            count = int(self.read_format(format, 1, self.list_type, stream)[0])
            return self.read_format(format, count, self.numeric_type, stream)
        else:
            return self.read_format(format, 1, self.numeric_type, stream)[0]


class object_spec(object):
    __slots__ = ("specs",
                )
    'A list of element_specs'
    def __init__(self):
        self.specs = []

    def load(self, format, stream):
        return dict([(i.name, [i.load(format, stream) for j in range(i.count)]) for i in self.specs])

        '''
        # Longhand for above LC
        answer = {}
        for i in self.specs:
            answer[i.name] = []
            for j in range(i.count):
                if not j % 100 and meshtools.show_progress:
                    Blender.Window.DrawProgressBar(float(j) / i.count, 'Loading ' + i.name)
                answer[i.name].append(i.load(format, stream))
        return answer
            '''


def read(filepath):
    format = b''
    texture = b''
    version = b'1.0'
    format_specs = {b'binary_little_endian': '<',
                    b'binary_big_endian': '>',
                    b'ascii': b'ascii'}
    type_specs = {b'char': 'b',
                  b'uchar': 'B',
                  b'int8': 'b',
                  b'uint8': 'B',
                  b'int16': 'h',
                  b'uint16': 'H',
                  b'short': 'h',
                  b'ushort': 'H',
                  b'int': 'i',
                  b'int32': 'i',
                  b'uint': 'I',
                  b'uint32': 'I',
                  b'float': 'f',
                  b'float32': 'f',
                  b'float64': 'd',
                  b'double': 'd',
                  b'string': 's'}
    obj_spec = object_spec()
    invalid_ply = (None, None, None)

    with open(filepath, 'rb') as plyf:
        signature = plyf.readline()

        if not signature.startswith(b'ply'):
            print('Signature line was invalid')
            return invalid_ply

        valid_header = False
        for line in plyf:
            tokens = re.split(br'[ \r\n]+', line)

            if len(tokens) == 0:
                continue
            if tokens[0] == b'end_header':
                valid_header = True
                break
            elif tokens[0] == b'comment':
                if len(tokens) < 2:
                    continue
                elif tokens[1] == b'TextureFile':
                    if len(tokens) < 4:
                        print('Invalid texture line')
                    else:
                        texture = tokens[2]
                continue
            elif tokens[0] == b'obj_info':
                continue
            elif tokens[0] == b'format':
                if len(tokens) < 3:
                    print('Invalid format line')
                    return invalid_ply
                if tokens[1] not in format_specs:
                    print('Unknown format', tokens[1])
                    return invalid_ply
                try:
                    version_test = float(tokens[2])
                except Exception as ex:
                    print('Unknown version', ex)
                    version_test = None
                if version_test != float(version):
                    print('Unknown version', tokens[2])
                    return invalid_ply
                del version_test
                format = tokens[1]
            elif tokens[0] == b'element':
                if len(tokens) < 3:
                    print(b'Invalid element line')
                    return invalid_ply
                obj_spec.specs.append(element_spec(tokens[1], int(tokens[2])))
            elif tokens[0] == b'property':
                if not len(obj_spec.specs):
                    print('Property without element')
                    return invalid_ply
                if tokens[1] == b'list':
                    obj_spec.specs[-1].properties.append(property_spec(tokens[4], type_specs[tokens[2]], type_specs[tokens[3]]))
                else:
                    obj_spec.specs[-1].properties.append(property_spec(tokens[2], None, type_specs[tokens[1]]))
        if not valid_header:
            print("Invalid header ('end_header' line not found!)")
            return invalid_ply

        obj = obj_spec.load(format_specs[format], plyf)

    return obj_spec, obj, texture


import bpy


def load_ply_mesh(filepath, ply_name):
    from bpy_extras.io_utils import unpack_face_list
    # from bpy_extras.image_utils import load_image  # UNUSED

    obj_spec, obj, texture = read(filepath)
    if obj is None:
        print('Invalid file')
        return

    uvindices = colindices = None
    colmultiply = None

    # noindices = None # Ignore normals

    for el in obj_spec.specs:
        if el.name == b'vertex':
            vindices_x, vindices_y, vindices_z = el.index(b'x'), el.index(b'y'), el.index(b'z')
            # noindices = (el.index('nx'), el.index('ny'), el.index('nz'))
            # if -1 in noindices: noindices = None
            uvindices = (el.index(b's'), el.index(b't'))
            if -1 in uvindices:
                uvindices = None
            colindices = el.index(b'red'), el.index(b'green'), el.index(b'blue')
            if -1 in colindices:
                colindices = None
            else:  # if not a float assume uchar
                colmultiply = [1.0 if el.properties[i].numeric_type in {'f', 'd'} else (1.0 / 255.0) for i in colindices]

        elif el.name == b'face':
            findex = el.index(b'vertex_indices')
        elif el.name == b'tristrips':
            trindex = el.index(b'vertex_indices')
        elif el.name == b'edge':
            eindex1, eindex2 = el.index(b'vertex1'), el.index(b'vertex2')

    mesh_faces = []
    mesh_uvs = []
    mesh_colors = []

    def add_face(vertices, indices, uvindices, colindices):
        mesh_faces.append(indices)
        if uvindices:
            mesh_uvs.append([(vertices[index][uvindices[0]], vertices[index][uvindices[1]]) for index in indices])
        if colindices:
            mesh_colors.append([(vertices[index][colindices[0]] * colmultiply[0],
                                 vertices[index][colindices[1]] * colmultiply[1],
                                 vertices[index][colindices[2]] * colmultiply[2],
                                 ) for index in indices])

    if uvindices or colindices:
        # If we have Cols or UVs then we need to check the face order.
        add_face_simple = add_face

        # EVIL EEKADOODLE - face order annoyance.
        def add_face(vertices, indices, uvindices, colindices):
            if len(indices) == 4:
                if indices[2] == 0 or indices[3] == 0:
                    indices = indices[2], indices[3], indices[0], indices[1]
            elif len(indices) == 3:
                if indices[2] == 0:
                    indices = indices[1], indices[2], indices[0]

            add_face_simple(vertices, indices, uvindices, colindices)

    verts = obj[b'vertex']

    if b'face' in obj:
        for f in obj[b'face']:
            ind = f[findex]
            len_ind = len(ind)
            if len_ind <= 4:
                add_face(verts, ind, uvindices, colindices)
            else:
                # Fan fill the face
                for j in range(len_ind - 2):
                    add_face(verts, (ind[0], ind[j + 1], ind[j + 2]), uvindices, colindices)

    if b'tristrips' in obj:
        for t in obj[b'tristrips']:
            ind = t[trindex]
            len_ind = len(ind)
            for j in range(len_ind - 2):
                add_face(verts, (ind[j], ind[j + 1], ind[j + 2]), uvindices, colindices)

    mesh = bpy.data.meshes.new(name=ply_name)

    mesh.vertices.add(len(obj[b'vertex']))

    mesh.vertices.foreach_set("co", [a for v in obj[b'vertex'] for a in (v[vindices_x], v[vindices_y], v[vindices_z])])

    if b'edge' in obj:
        mesh.edges.add(len(obj[b'edge']))
        mesh.edges.foreach_set("vertices", [a for e in obj[b'edge'] for a in (e[eindex1], e[eindex2])])

    if mesh_faces:
        mesh.tessfaces.add(len(mesh_faces))
        mesh.tessfaces.foreach_set("vertices_raw", unpack_face_list(mesh_faces))

        if uvindices or colindices:
            if uvindices:
                uvlay = mesh.tessface_uv_textures.new()
            if colindices:
                vcol_lay = mesh.tessface_vertex_colors.new()

            if uvindices:
                for i, f in enumerate(uvlay.data):
                    ply_uv = mesh_uvs[i]
                    for j, uv in enumerate(f.uv):
                        uv[0], uv[1] = ply_uv[j]

            if colindices:
                for i, f in enumerate(vcol_lay.data):
                    # XXX, colors dont come in right, needs further investigation.
                    ply_col = mesh_colors[i]
                    if len(ply_col) == 4:
                        f_col = f.color1, f.color2, f.color3, f.color4
                    else:
                        f_col = f.color1, f.color2, f.color3

                    for j, col in enumerate(f_col):
                        col.r, col.g, col.b = ply_col[j]

    mesh.validate()
    mesh.update()

    if texture and uvindices:

        import os
        import sys
        from bpy_extras.image_utils import load_image

        encoding = sys.getfilesystemencoding()
        encoded_texture = texture.decode(encoding=encoding)
        name = bpy.path.display_name_from_filepath(texture)
        image = load_image(encoded_texture, os.path.dirname(filepath), recursive=True, place_holder=True)

        if image:
            texture = bpy.data.textures.new(name=name, type='IMAGE')
            texture.image = image

            material = bpy.data.materials.new(name=name)
            material.use_shadeless = True

            mtex = material.texture_slots.add()
            mtex.texture = texture
            mtex.texture_coords = 'UV'
            mtex.use_map_color_diffuse = True

            mesh.materials.append(material)
            for face in mesh.uv_textures[0].data:
                face.image = image

    return mesh


def load_ply(filepath):
    import time

    t = time.time()
    ply_name = bpy.path.display_name_from_filepath(filepath)

    mesh = load_ply_mesh(filepath, ply_name)
    if not mesh:
        return {'CANCELLED'}

    scn = bpy.context.scene

    obj = bpy.data.objects.new(ply_name, mesh)
    scn.objects.link(obj)
    scn.objects.active = obj
    obj.select = True

    print('\nSuccessfully imported %r in %.3f sec' % (filepath, time.time() - t))
    return {'FINISHED'}


def load(operator, context, filepath=""):
    return load_ply(filepath)
