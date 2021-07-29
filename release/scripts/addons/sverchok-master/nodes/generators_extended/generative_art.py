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

''' by Eleanor Howick | 2015 https://github.com/elfnor
    LSystem code from Philip Rideout  https://github.com/prideout/lsystem '''


import math
import string
import random
from xml.etree.cElementTree import fromstring

import bpy
from bpy.props import IntProperty, StringProperty
import mathutils as mu

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, Vector_generate, Matrix_listing


"""
---------------------------------------------------
    LSystem
---------------------------------------------------
"""


class LSystem:

    """
    Takes an XML tree.
    """
    def __init__(self, xml_tree, maxObjects):
        self._tree = xml_tree
        self._maxDepth = int(self._tree.get("max_depth"))
        self._progressCount = 0
        self._maxObjects = maxObjects

    """
    Returns a list of "shapes".
    Each shape is a 2-tuple: (shape name, transform matrix).
    """
    def evaluate(self, seed=0):
        random.seed(seed)
        rule = _pickRule(self._tree, "entry")
        entry = (rule, 0, mu.Matrix.Identity(4))
        shapes = self._evaluate(entry)
        return shapes

    def _evaluate(self, entry):
        stack = [entry]
        shapes = []
        nobjects = 0
        while len(stack) > 0:
            if nobjects > self._maxObjects:
                print('max objects reached')
                break

            rule, depth, matrix = stack.pop()

            local_max_depth = self._maxDepth
            if "max_depth" in rule.attrib:
                local_max_depth = int(rule.get("max_depth"))

            if len(stack) > self._maxDepth:
                shapes.append(None)
                continue

            if depth > local_max_depth:
                if "successor" in rule.attrib:
                    successor = rule.get("successor")
                    rule = _pickRule(self._tree, successor)
                    stack.append((rule, 0, matrix))
                shapes.append(None)
                continue

            base_matrix = matrix.copy()
            for statement in rule:
                tstr = statement.get("transforms", "")
                if not(tstr):
                    tstr = ''
                    for t in ['tx', 'ty', 'tz', 'rx', 'ry', 'rz',
                              'sa', 'sx', 'sy', 'sz']:
                        tvalue = statement.get(t)
                        if tvalue:
                            n = eval(tvalue)
                            tstr += "{} {:f} ".format(t, n)
                xform = _parseXform(tstr)
                count = int(statement.get("count", 1))
                count_xform = mu.Matrix.Identity(4)
                for n in range(count):
                    count_xform *= xform
                    matrix = base_matrix * count_xform

                    if statement.tag == "call":
                        rule = _pickRule(self._tree, statement.get("rule"))
                        cloned_matrix = matrix.copy()
                        entry = (rule, depth + 1, cloned_matrix)
                        stack.append(entry)

                    elif statement.tag == "instance":
                        name = statement.get("shape")
                        if name == "None":
                            shapes.append(None)
                        else:
                            shape = (name, matrix)
                            shapes.append(shape)
                            nobjects += 1

                    else:
                        raise ValueError("bad xml", statement.tag)

                if count > 1:
                    shapes.append(None)

        return shapes
        # end of _evaluate

    def make_tube(self, mats, verts):
        """
        takes a list of vertices and a list of matrices
        the vertices are to be joined in a ring, copied and transformed
        by the 1st matrix and this ring joined to the previous ring.

        The ring dosen't have to be planar.
        outputs lists of vertices, edges and faces
        """

        edges_out = []
        verts_out = []
        faces_out = []
        vID = 0

        if len(mats) > 1:
            nring = len(verts[0])
            # end face
            faces_out.append(list(range(nring)))
            for i, m in enumerate(mats):
                for j, v in enumerate(verts[0]):
                    vout = mu.Matrix(m) * mu.Vector(v)
                    verts_out.append(vout.to_tuple())
                    vID = j + i*nring
                    # rings
                    if j != 0:
                        edges_out.append([vID, vID - 1])
                    else:
                        edges_out.append([vID, vID + nring-1])
                    # lines
                    if i != 0:
                        edges_out.append([vID, vID - nring])
                        # faces
                        if j != 0:
                            faces_out.append([vID,
                                              vID - nring,
                                              vID - nring - 1,
                                              vID-1])
                        else:
                            faces_out.append([vID,
                                              vID - nring,
                                              vID-1,
                                              vID + nring-1])
            # end face
            # reversing list fixes face normal direction keeps mesh manifold
            f = list(range(vID, vID-nring, -1))
            faces_out.append(f)
        return verts_out, edges_out, faces_out


def _pickRule(tree, name):

    rules = tree.findall("rule")
    elements = []
    for r in rules:
        if r.get("name") == name:
            elements.append(r)

    if len(elements) == 0:
        raise ValueError("bad xml",  "no rules found with name '%s'" % name)

    sum, tuples = 0, []
    for e in elements:
        weight = int(e.get("weight", 1))
        sum = sum + weight
        tuples.append((e, weight))
    n = random.randint(0, sum - 1)
    for (item, weight) in tuples:
        if n < weight:
            break
        n = n - weight
    return item

_xformCache = {}


def _parseXform(xform_string):
    if xform_string in _xformCache:
        return _xformCache[xform_string]

    matrix = mu.Matrix.Identity(4)
    tokens = xform_string.split()
    t = 0
    while t < len(tokens) - 1:
            command, t = tokens[t], t + 1

            # Translation
            if command == 'tx':
                x, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Translation(mu.Vector((x, 0, 0)))
            elif command == 'ty':
                y, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Translation(mu.Vector((0, y, 0)))
            elif command == 'tz':
                z, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Translation(mu.Vector((0, 0, z)))
            elif command == 't':
                x, t = eval(tokens[t]), t + 1
                y, t = eval(tokens[t]), t + 1
                z, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Translation(mu.Vector((x, y, z)))

            # Rotation
            elif command == 'rx':
                theta, t = _radians(eval(tokens[t])), t + 1
                matrix *= mu.Matrix.Rotation(theta, 4, 'X')

            elif command == 'ry':
                theta, t = _radians(eval(tokens[t])), t + 1
                matrix *= mu.Matrix.Rotation(theta, 4, 'Y')
            elif command == 'rz':
                theta, t = _radians(eval(tokens[t])), t + 1
                matrix *= mu.Matrix.Rotation(theta, 4, 'Z')

            # Scale
            elif command == 'sx':
                x, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Scale(x, 4, mu.Vector((1.0, 0.0, 0.0)))
            elif command == 'sy':
                y, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Scale(y, 4, mu.Vector((0.0, 1.0, 0.0)))
            elif command == 'sz':
                z, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Scale(z, 4, mu.Vector((0.0, 0.0, 1.0)))
            elif command == 'sa':
                v, t = eval(tokens[t]), t + 1
                matrix *= mu.Matrix.Scale(v, 4)
            elif command == 's':
                x, t = eval(tokens[t]), t + 1
                y, t = eval(tokens[t]), t + 1
                z, t = eval(tokens[t]), t + 1
                mx = mu.Matrix.Scale(x, 4, mu.Vector((1.0, 0.0, 0.0)))
                my = mu.Matrix.Scale(y, 4, mu.Vector((0.0, 1.0, 0.0)))
                mz = mu.Matrix.Scale(z, 4, mu.Vector((0.0, 0.0, 1.0)))
                mxyz = mx*my*mz
                matrix *= mxyz

            else:
                err_str = "unrecognized transform: '%s' at position %d in '%s'" % (command, t, xform_string)
                raise ValueError("bad xml", err_str)

    _xformCache[xform_string] = matrix
    return matrix


def _radians(d):
    return float(d * 3.141 / 180.0)

"""
---------------------------------------------------
    SvGenerativeArtNode
---------------------------------------------------
"""


class SvGenerativeArtNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Generative Art or LSystem node'''
    bl_idname = 'SvGenerativeArtNode'
    bl_label = 'Generative Art'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def updateNode_filename(self, context):
        self.process_node(context)
        self.read_xml()
        self.make_sockets()

    filename = StringProperty(default="", update=updateNode_filename)

    rseed = IntProperty(
        name='rseed', description='random seed',
        default=21, min=0, options={'ANIMATABLE'},
        update=updateNode)

    maxmats = IntProperty(
        name='maxmats', description='maximum nunber of matrices',
        default=1000, min=1, options={'ANIMATABLE'},
        update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop_search(self, 'filename', bpy.data, 'texts', text='', icon='TEXT')
        layout.prop(self, "rseed", text="r seed")
        layout.prop(self, "maxmats", text="max mats")

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")

        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Faces")
        self.outputs.new('MatrixSocket', "Matrices")

    def update(self):
        self.read_xml()
        self.make_sockets()

    def read_xml(self):
        """
        read xml from  bpy.data.texts
        """
        if self.filename and (self.filename in bpy.data.texts):
            internal_file = bpy.data.texts[self.filename]
            self.xml_text = internal_file.as_string()
            self.xml_tree = fromstring(self.xml_text)

    def xml_text_format(self):
        """
        substitute constants from xml
        and variables from socket inputs
        """
        # get constants from xml
        format_dict = {}

        if not hasattr(self, 'xml_text'):
            return

        for elem in self.xml_tree.findall("constants"):
            format_dict.update(elem.attrib)

        # add input socket values to constants dict
        for socket in self.inputs[1:]:
            format_dict[socket.name] = socket.sv_get()[0][0] if socket.is_linked else 0

        while '{' in self.xml_text:
            # using while loop
            # allows constants to be defined using other constants
            self.xml_text = self.xml_text.format(**format_dict)
        self.xml_tree = fromstring(self.xml_text)

    def make_sockets(self):
        """
        insert input sockets named after variables in xml,
        output sockets named after shape attribute values in xml
        """
        d_constants = {}
        if not hasattr(self, 'xml_tree'):
            return

        for elem in self.xml_tree.findall("constants"):
            d_constants.update(elem.attrib)

        field_ids = [v[1] for v in string.Formatter().parse(self.xml_text)]
        field_ids = set(field_ids) - set([None])
        constant_ids = set(d_constants.keys())
        socket_ids = field_ids - constant_ids

        # add new input sockets to node
        for s_name in sorted(socket_ids):
            if s_name not in self.inputs:
                self.inputs.new('StringsSocket', s_name)

        # remove any sockets with no field_ids in xml
        old_sockets = [socket
                       for socket in self.inputs[1:]
                       if socket.name not in socket_ids]
        for socket in old_sockets:
            self.inputs.remove(socket)

        # output sockets to match shape attribute values
        shape_names = set([x.attrib.get('shape')
                           for x in self.xml_tree.iter('instance')])
        # new output sockets
        for s_name in sorted(shape_names):
            if s_name not in self.outputs:
                self.outputs.new('MatrixSocket', s_name)

        # remove old output sockets
        old_sockets = [out_socket
                       for out_socket in self.outputs[3:]
                       if out_socket.name not in shape_names]
        for socket in old_sockets:
            self.outputs.remove(socket)

    def process(self):
      
        self.read_xml()
        self.make_sockets()
        self.xml_text_format()

        if not hasattr(self, 'xml_tree'):
            return
        
        if not any(output.is_linked for output in self.outputs):
            return

        lsys = LSystem(self.xml_tree, self.maxmats)
        shapes = lsys.evaluate(seed=self.rseed)
        mat_sublist = []

        edges_out = []
        verts_out = []
        faces_out = []

        # make last entry in shapes None
        # to allow make tube to finish last tube
        if shapes[-1]:
            shapes.append(None)
        # dictionary for matrix lists
        shape_names = set([x.attrib.get('shape')
                           for x in self.xml_tree.iter('instance')])
        mat_dict = {s: [] for s in shape_names}
        if self.inputs['Vertices'].is_linked:
            verts = Vector_generate(self.inputs['Vertices'].sv_get())
        for i, shape in enumerate(shapes):
            if shape:
                mat_sublist.append(shape[1])
                mat_dict[shape[0]].append(shape[1])
            else:
                if len(mat_sublist) > 0:
                    if self.inputs['Vertices'].is_linked:
                        v, e, f = lsys.make_tube(mat_sublist, verts)
                        if v:
                            verts_out.append(v)
                            edges_out.append(e)
                            faces_out.append(f)

                mat_sublist = []
        
        self.outputs['Vertices'].sv_set(verts_out)
        self.outputs['Edges'].sv_set(edges_out)
        self.outputs['Faces'].sv_set(faces_out)
        for shape in shape_names:
            self.outputs[shape].sv_set(Matrix_listing(mat_dict[shape]))


def register():
    bpy.utils.register_class(SvGenerativeArtNode)


def unregister():
    bpy.utils.unregister_class(SvGenerativeArtNode)
