#!BPY

"""
Name: 'PLY...'
Blender: 237
Group: 'Import'
Tip: 'Import a Stanford PLY file'
"""

__author__ = "Bruce Merry"
__version__ = "0.91"
__bpydoc__ = """\
This script imports Stanford PLY files into Blender. It supports per-vertex
normals, and per-face colours and texture coordinates.

Usage:

Run this script from "File->Import" and select the desired PLY file.
"""

# Copyright (C) 2004, 2005: Bruce Merry, bmerry@cs.uct.ac.za
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


# Portions of this code are taken from mod_meshtools.py in Blender
# 2.32.

import Blender, meshtools
import re, struct, StringIO

class element_spec:
        name = ""
        count = 0
        def __init__(self, name, count):
                self.name = name
                self.count = count
                self.properties = []

        def load(self, format, stream):
                if format == "ascii":
                        stream = re.split('\s+', stream.readline())
                return map(lambda x: x.load(format, stream), self.properties)

        def index(self, name):
                for p in range(len(self.properties)):
                        if self.properties[p].name == name: return p
                return -1

class property_spec:
        name = ""
        list_type = ""
        numeric_type = ""
        def __init__(self, name, list_type, numeric_type):
                self.name = name
                self.list_type = list_type
                self.numeric_type = numeric_type

        def read_format(self, format, count, num_type, stream):
                if format == "ascii":
                        if (num_type == 's'):
                                ans = []
                                for i in range(count):
                                        s = stream[i]
                                        if len(s) < 2 or s[0] != "\"" or s[-1] != "\"":
                                                print "Invalid string", s
                                                print "Note: ply_import.py does not handle whitespace in strings"
                                                return None
                                        ans.append(s[1:-1])
                                stream[:count] = []
                                return ans
                        if (num_type == 'f' or num_type == 'd'):
                                mapper = float
                        else:
                                mapper = int
                        ans = map(lambda x: mapper(x), stream[:count])
                        stream[:count] = []
                        return ans
                else:
                        if (num_type == 's'):
                                ans = []
                                for i in range(count):
                                        fmt = format + "i"
                                        data = stream.read(struct.calcsize(fmt))
                                        length = struct.unpack(fmt, data)[0]
                                        fmt = format + str(length) + "s"
                                        data = stream.read(struct.calcsize(fmt))
                                        s = struct.unpack(fmt, data)[0]
                                        ans.append(s[0:-1]) # strip the NULL
                                return ans
                        else:
                                fmt = format + str(count) + num_type
                                data = stream.read(struct.calcsize(fmt));
                                return struct.unpack(fmt, data)

        def load(self, format, stream):
                if (self.list_type != None):
                        count = int(self.read_format(format, 1, self.list_type, stream)[0])
                        return self.read_format(format, count, self.numeric_type, stream)
                else:
                        return self.read_format(format, 1, self.numeric_type, stream)[0]

class object_spec:
        "A list of element_specs"
        specs = []

        def load(self, format, stream):
                answer = {}
                for i in self.specs:
                        answer[i.name] = []
                        for j in range(i.count):
                                if not j % 100 and meshtools.show_progress:
                                        Blender.Window.DrawProgressBar(float(j) / i.count, "Loading " + i.name)
                                answer[i.name].append(i.load(format, stream))
                return answer

def read(filename):
        format = ""
        version = "1.0"
        format_specs = {"binary_little_endian": "<",
                        "binary_big_endian": ">",
                        "ascii": "ascii"}
        type_specs = {"char": "b",
                      "uchar": "B",
                      "int8": "b",
                      "uint8": "B",
                      "int16": "h",
                      "uint16": "H",
                      "int": "i",
                      "int32": "i",
                      "uint": "I",
                      "uint32": "I",
                      "float": "f",
                      "float32": "f",
                      "float64": "d",
                      "string": "s"}
        obj_spec = object_spec()

        try:
                file = open(filename, "rb")
                signature = file.readline()
                if (signature != "ply\n"):
                        print "Signature line was invalid"
                        return None
                while 1:
                        line = file.readline()
                        tokens = re.split(r'[ \n]+', line)
                        if (len(tokens) == 0):
                                continue
                        if (tokens[0] == 'end_header'):
                                break
                        elif (tokens[0] == 'comment' or tokens[0] == 'obj_info'):
                                continue
                        elif (tokens[0] == 'format'):
                                if (len(tokens) < 3):
                                        print "Invalid format line"
                                        return None
                                if (tokens[1] not in format_specs.keys()):
                                        print "Unknown format " + tokens[1]
                                        return None
                                if (tokens[2] != version):
                                        print "Unknown version " + tokens[2]
                                        return None
                                format = tokens[1]
                        elif (tokens[0] == "element"):
                                if (len(tokens) < 3):
                                        print "Invalid element line"
                                        return None
                                obj_spec.specs.append(element_spec(tokens[1], int(tokens[2])))
                        elif (tokens[0] == "property"):
                                if (not len(obj_spec.specs)):
                                        print "Property without element"
                                        return None
                                if (tokens[1] == "list"):
                                        obj_spec.specs[-1].properties.append(property_spec(tokens[4], type_specs[tokens[2]], type_specs[tokens[3]]))
                                else:
                                        obj_spec.specs[-1].properties.append(property_spec(tokens[2], None, type_specs[tokens[1]]))
                obj = obj_spec.load(format_specs[format], file)

        except IOError, (errno, strerror):
                file.close()
                return None

        file.close()
        return (obj_spec, obj);

def add_face(mesh, vertices, varr, indices, uvindices, colindices):
        face = Blender.NMesh.Face()
        for index in indices:
                vertex = vertices[index];
                face.v.append(varr[index])
                if uvindices:
                        face.uv.append((vertex[uvindices[0]], 1.0 - vertex[uvindices[1]]))
                if colindices:
                        if not uvindices: face.uv.append((0, 0)) # Force faceUV
                        face.col.append(Blender.NMesh.Col(vertex[colindices[0]], vertex[colindices[1]], vertex[colindices[2]], 255))
        mesh.faces.append(face)

def filesel_callback(filename):
        (obj_spec, obj) = read(filename)
        if obj == None:
                print "Invalid file"
                return
        vmap = {}
        varr = []
        uvindices = None
        noindices = None
        colindices = None
        for el in obj_spec.specs:
                if el.name == "vertex":
                        vindices = (el.index("x"), el.index("y"), el.index("z"))
                        if el.index("nx") >= 0 and el.index("ny") >= 0 and el.index("nz") >= 0:
                                noindices = (el.index("nx"), el.index("ny"), el.index("nz"))
                        if el.index("s") >= 0 and el.index("t") >= 0:
                                uvindices = (el.index("s"), el.index("t"))
                        if el.index("red") >= 0 and el.index("green") and el.index("blue") >= 0:
                                colindices = (el.index("red"), el.index("green"), el.index("blue"))
                elif el.name == "face":
                        findex = el.index("vertex_indices")

        mesh = Blender.NMesh.GetRaw()
        for v in obj["vertex"]:
                x = v[vindices[0]]
                y = v[vindices[1]]
                z = v[vindices[2]]
                if noindices > 0:
                        nx = v[noindices[0]]
                        ny = v[noindices[1]]
                        nz = v[noindices[2]]
                        vkey = (x, y, z, nx, ny, nz)
                else:
                        vkey = (x, y, z)
                if not vmap.has_key(vkey):
                        mesh.verts.append(Blender.NMesh.Vert(x, y, z))
                        if noindices > 0:
                                mesh.verts[-1].no[0] = nx
                                mesh.verts[-1].no[1] = ny
                                mesh.verts[-1].no[2] = nz
                        vmap[vkey] = mesh.verts[-1]
                varr.append(vmap[vkey])
        for f in obj["face"]:
                ind = f[findex]
                nind = len(ind)
                if nind <= 4:
                        add_face(mesh, obj["vertex"], varr, ind, uvindices, colindices)
                else:
                        for j in range(nind - 2):
                                add_face(mesh, obj["vertex"], varr, (ind[0], ind[j + 1], ind[j + 2]), uvindices, colindices)


        obj = None # Reclaim memory

        if noindices:
                normals = 1
        else:
                normals = 0
        objname = Blender.sys.splitext(Blender.sys.basename(filename))[0]
        if not meshtools.overwrite_mesh_name:
                objname = meshtools.versioned_name(objname)
        Blender.NMesh.PutRaw(mesh, objname, not normals)
        Blender.Object.GetSelected()[0].name = objname
        Blender.Redraw()
        Blender.Window.DrawProgressBar(1.0, '')
        message = "Successfully imported " + Blender.sys.basename(filename)
        meshtools.print_boxed(message)

Blender.Window.FileSelector(filesel_callback, "Import PLY")

