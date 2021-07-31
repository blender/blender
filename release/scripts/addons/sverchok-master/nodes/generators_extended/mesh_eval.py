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

import ast
from math import *

import bpy
from bpy.props import BoolProperty, StringProperty, EnumProperty, FloatVectorProperty, IntProperty
from mathutils import Vector
import json
import io

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import fullList, updateNode, dataCorrect, match_long_repeat

"""
JSON format:
    {
        "vertices": [[1, 2, 3], [2, 3, 4], [4,5,6]],
        "edges": [[0,1]],
        "faces": [[0,1,2]]
    }
    In vertices list, any of numbers can be replaced by string (variable name).
"""

# To be extended?
safe_names = dict(sin=sin, cos=cos, pi=pi, sqrt=sqrt)

    
def get_variables(string):
    root = ast.parse(string, mode='eval')
    result = {node.id for node in ast.walk(root) if isinstance(node, ast.Name)}
    return result.difference(safe_names.keys())

# It could be safer...
def safe_eval(string, variables):
    env = dict()
    env.update(safe_names)
    env.update(variables)
    env["__builtins__"] = {}
    root = ast.parse(string, mode='eval')
    return eval(compile(root, "<expression>", 'eval'), env)

def evaluate(json, variables):
    result = {}
    result['edges'] = json['edges']
    result['faces'] = json['faces']
    result['vertices'] = []

    groups = {}
    
    for idx, vertex in enumerate(json['vertices']):
        v = []
        if isinstance(vertex, (list, tuple)) and len(vertex) == 3:
            coords = vertex
        elif isinstance(vertex, (list, tuple)) and len(vertex) == 4 and isinstance(vertex[-1], (str, list, tuple)):
            coords = vertex[:-1]
            g = vertex[-1]
            if isinstance(g, str):
                groupnames = [g]
            else:
                groupnames = g
            for groupname in groupnames:
                if groupname in groups:
                    groups[groupname].append(idx)
                else:
                    groups[groupname] = [idx]

        for c in coords:
            if isinstance(c, str):
                try:
                    val = safe_eval(c, variables)
                    #self.debug("EVAL: {} with {} => {}".format(c, variables, val))
                except NameError as e:
                    self.exception(e)
                    val = 0.0
            else:
                val = c
            v.append(val)
        result['vertices'].append(v)
    return result, groups

def selected_masks_adding(node):
    """ adding new list masks nodes if none """
    if node.outputs[0].is_linked: return
    loc = node.location

    tree = bpy.context.space_data.edit_tree
    links = tree.links

    mo = tree.nodes.new('MaskListNode')
    mv = tree.nodes.new('SvMoveNodeMK2')
    rf = tree.nodes.new('SvGenFloatRange')
    vi = tree.nodes.new('GenVectorsNode')
    mi = tree.nodes.new('SvMaskJoinNode')
    vd = tree.nodes.new('ViewerNode2')
    mo.location = loc+Vector((300,0))
    mv.location = loc+Vector((550,0))
    vi.location = loc+Vector((350,-225))
    rf.location = loc+Vector((0,-225))
    mi.location = loc+Vector((800,0))
    vd.location = loc+Vector((1000,0))

    links.new(node.outputs[0], mo.inputs[0])   #verts
    links.new(node.outputs[3], mo.inputs[1])   #mask
    links.new(mo.outputs[0], mi.inputs[0])   #mask
    links.new(mo.outputs[3], mv.inputs[0])   #True out
    links.new(vi.outputs[0], mv.inputs[1])   #vector
    links.new(rf.outputs[0], vi.inputs[2])   #range
    links.new(mv.outputs[0], mi.inputs[1])   #True in
    links.new(mo.outputs[4], mi.inputs[2])   #False
    links.new(mi.outputs[0], vd.inputs[0])   #Verts
    links.new(node.outputs[2], vd.inputs[1])   #Faces
    mi.Level = 2
    mo.level = 2
    rf.mode='FRANGE_COUNT'
    rf.stop_=4
    rf.count_=4

class SvJsonFromMesh(bpy.types.Operator):
    "JSON from selected mesh"
    bl_idname = "node.sverchok_json_from_mesh"
    bl_label = "JSON from mesh"
    bl_options = {'REGISTER'}

    nodename = StringProperty(name='nodename')
    treename = StringProperty(name='treename')

    def execute(self, context):
        node = bpy.data.node_groups[self.treename].nodes[self.nodename]
        if not bpy.context.selected_objects[0].type == 'MESH':
            node.info("JSON from mesh: selected object is not mesh")
            self.report({'INFO'}, 'It is not a mesh selected')
            return

        object = bpy.context.selected_objects[0]
        mesh = object.data
        result = {}
        verts = []
        isselected = False
        for v in mesh.vertices:
            names = set()
            for grp in v.groups:
                name = object.vertex_groups[grp.group].name
                names.add(name)
            if v.select:
                names.add('Selected')
                isselected = True
            if names:
                vertex = self.round(v) + [list(sorted(names))]
            else:
                vertex = self.round(v)
            verts.append(vertex)

        if isselected:
            if not 'Selected' in node.inputs.keys() and not node.outputs[0].is_linked:
                node.outputs.new('StringsSocket', 'Selected')
                if node.sample_tree:
                    selected_masks_adding(node)
        result['vertices'] = verts
        result['edges'] = mesh.edge_keys
        result['faces'] = [list(p.vertices) for p in mesh.polygons]

        self.write_values(self.nodename, json.dumps(result, indent=2))
        bpy.data.node_groups[self.treename].nodes[self.nodename].filename = self.nodename
        return{'FINISHED'}

    def round(self, vector):
        precision = bpy.data.node_groups[self.treename].nodes[self.nodename].precision
        vector = [round(x, precision) for x in vector.co[:]]
        return vector

    def write_values(self,text,values):
        texts = bpy.data.texts.items()
        exists = False
        for t in texts:
            if bpy.data.texts[t[0]].name == text:
                exists = True
                break

        if not exists:
            bpy.data.texts.new(text)
        bpy.data.texts[text].clear()
        bpy.data.texts[text].write(values)

class SvMeshEvalNode(bpy.types.Node, SverchCustomTreeNode):
    bl_idname = 'SvMeshEvalNode'
    bl_label = 'Mesh Expression'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def on_update(self, context):
        self.adjust_sockets()
        updateNode(self, context)

    filename = StringProperty(default="", update=on_update)
    precision = IntProperty(name = "Precision",
                    description = "Number of decimal places used for coordinates when generating JSON from selection",
                    min=0, max=10, default=8,
                    update=updateNode)

    sample_tree = BoolProperty(name = "Example tree",
                    description = "Create example nodes when generating JSON from selection",
                    default = False,
                    update=updateNode)

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.prop_search(self, 'filename', bpy.data, 'texts', text='', icon='TEXT')
        row = layout.row()

        do_text = row.operator('node.sverchok_json_from_mesh', text='from selection')
        do_text.nodename = self.name
        do_text.treename = self.id_data.name

    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.prop(self, "precision")
        layout.prop(self, "sample_tree")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "a")

        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Faces")

    def load_json(self):
        internal_file = bpy.data.texts[self.filename]
        f = io.StringIO(internal_file.as_string())
        json_data = json.load(f)
        self.validate_json(json_data)
        return json_data
    
    def validate_json(self, json):
        if not "vertices" in json:
            raise Exception("JSON does not have `vertices' key")
        if not "edges" in json:
            raise Exception("JSON does not have `edges' key")
        if not "faces" in json:
            raise Exception("JSON does not have `faces' key")

    def get_variables(self):
        variables = set()
        json = self.load_json()
        if not json:
            return variables

        for vertex in json["vertices"]:
            if isinstance(vertex, (list, tuple)) and len(vertex) == 3:
                coords = vertex
            elif isinstance(vertex, (list, tuple)) and len(vertex) == 4 and isinstance(vertex[-1], (str, list, tuple)):
                coords = vertex[:-1]

            for c in coords:
                if isinstance(c, str):
                    vs = get_variables(c)
                    variables.update(vs)

        return list(sorted(list(variables)))
    
    def get_group_names(self):
        groups = set()
        json = self.load_json()
        if not json:
            return groups
        
        for vertex in json["vertices"]:
            if isinstance(vertex, (list, tuple)) and len(vertex) == 4 and isinstance(vertex[-1], (str, list, tuple)):
                g = vertex[-1]
                if isinstance(g, str):
                    names = [g]
                else:
                    names = g
                for name in names:
                    if name in ['Vertices', 'Edges', 'Faces']:
                        raise Exception("Invalid name for vertex group. It should not be Vertices, Edges or Faces.")
                    groups.add(name)

        return list(sorted(list(groups)))

    def get_defaults(self):
        result = {}
        json = self.load_json()
        if not json or 'defaults' not in json:
            return result
        if not isinstance(json['defaults'], dict):
            return result
        return json['defaults']

    def adjust_sockets(self):
        variables = self.get_variables()
        #self.debug("adjust_sockets:" + str(variables))
        #self.debug("inputs:" + str(self.inputs.keys()))
        for key in self.inputs.keys():
            if key not in variables:
                self.debug("Input {} not in variables {}, remove it".format(key, str(variables)))
                self.inputs.remove(self.inputs[key])
        for v in variables:
            if v not in self.inputs:
                self.debug("Variable {} not in inputs {}, add it".format(v, str(self.inputs.keys())))
                self.inputs.new('StringsSocket', v)

        groups = self.get_group_names()
        for key in self.outputs.keys():
            if key in ['Vertices', 'Edges', 'Faces']:
                continue
            if key not in groups:
                self.debug("Output {} not in groups {}, remove it".format(key, str(groups)))
                self.outputs.remove(self.outputs[key])
        for name in sorted(groups):
            if name not in self.outputs:
                self.debug("Group {} not in outputs {}, add it".format(name, str(self.outputs.keys())))
                self.outputs.new('StringsSocket', name)


    def update(self):
        '''
        update analyzes the state of the node and returns if the criteria to start processing
        are not met.
        '''

        # keeping the file internal for now.
        if not (self.filename in bpy.data.texts):
            return

        self.adjust_sockets()

    def get_input(self):
        variables = self.get_variables()
        defaults = self.get_defaults()
        result = {}
        default_results = {}

        for var in defaults.keys():
            d = defaults[var]
            if isinstance(d, (int, float)):
                default_results[var] = d

        for var in variables:
            if var in self.inputs and self.inputs[var].is_linked:
                result[var] = self.inputs[var].sv_get()[0]
                default_results[var] = result[var][0]
            else:
                value = defaults.get(var, 1.0)
                if isinstance(value, str):
                    #self.debug("Eval: {} = {}, {}".format(var, value, default_results))
                    value = safe_eval(value, default_results)
                    default_results[var] = value
                result[var] = [value]
            #self.debug("get_input: {} => {}".format(var, result[var]))
        return result

    def groups_to_masks(self, groups, size):
        result = {}
        for name in groups:
            result[name] = [idx in groups[name] for idx in range(size)]
        return result

    def process(self):

        if not self.outputs[0].is_linked:
            return

        var_names = self.get_variables()
        inputs = self.get_input()

        result_vertices = []
        result_edges = []
        result_faces = []
        result_masks_dict = {}

        template = self.load_json()

        if var_names:
            input_values = [inputs[name] for name in var_names]
            parameters = match_long_repeat(input_values)
        else:
            parameters = [[[]]]
        for values in zip(*parameters):
            variables = dict(zip(var_names, values))

            json, groups = evaluate(template, variables)
            verts = json['vertices']
            result_vertices.append(verts)
            result_edges.append(json['edges'])
            result_faces.append(json['faces'])

            masks = self.groups_to_masks(groups, len(verts))
            for name in masks.keys():
                if name in result_masks_dict:
                    result_masks_dict[name].add(masks[name])
                else:
                    result_masks_dict[name] = [masks[name]]

        self.outputs['Vertices'].sv_set(result_vertices)
        self.outputs['Edges'].sv_set(result_edges)
        self.outputs['Faces'].sv_set(result_faces)

        for name in result_masks_dict.keys():
            self.outputs[name].sv_set(result_masks_dict[name])

    def storage_set_data(self, storage):
        geom = storage['geom']
        filename = storage['params']['filename']

        bpy.data.texts.new(filename)
        bpy.data.texts[filename].clear()
        bpy.data.texts[filename].write(geom)

    def storage_get_data(self, storage):
        if self.filename and self.filename in bpy.data.texts:
            text = bpy.data.texts[self.filename].as_string()
            storage['geom'] = text
        else:
            self.warning("Unknown filename: {}".format(self.filename))


def register():
    bpy.utils.register_class(SvJsonFromMesh)
    bpy.utils.register_class(SvMeshEvalNode)


def unregister():
    bpy.utils.unregister_class(SvJsonFromMesh)
    bpy.utils.unregister_class(SvMeshEvalNode)

