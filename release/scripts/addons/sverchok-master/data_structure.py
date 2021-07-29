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

from math import radians
import itertools
import time
import ast
import copy
import bpy
from mathutils import Vector, Matrix
import numpy as np

DEBUG_MODE = False
HEAT_MAP = False
RELOAD_EVENT = False

# this is set correctly later.
SVERCHOK_NAME = "sverchok"

cache_viewer_baker = {}

sentinel = object()



#####################################################
################### cache magic #####################
#####################################################

#handle for object in node and neuro node
temp_handle = {}

def handle_delete(handle):
    if handle in temp_handle:
        del temp_handle[handle]

def handle_read(handle):
    if not (handle in temp_handle):
        return (False, [])
    return (True, temp_handle[handle]['prop'])

def handle_write(handle, prop):
    handle_delete(handle)

    temp_handle[handle] = {"prop" : prop}

def handle_check(handle, prop):
    if handle in handle_check and \
            prop == handle_check[handle]['prop']:
        return True
    return False


#####################################################
################ list matching magic ################
#####################################################


def repeat_last(lst):
    """
    creates an infinite iterator the first each element in lst
    and then keep repeating the last element,
    use with terminating input
    """
    i = -1
    while lst:
        i += 1
        if len(lst) > i:
            yield lst[i]
        else:
            yield lst[-1]


def match_long_repeat(lsts):
    """return matched list, using the last value to fill lists as needed
    longest list matching [[1,2,3,4,5], [10,11]] -> [[1,2,3,4,5], [10,11,11,11,11]]
    """
    max_l = 0
    tmp = []
    for l in lsts:
        max_l = max(max_l, len(l))
    for l in lsts:
        if len(l) == max_l:
            tmp.append(l)
        else:
            tmp.append(repeat_last(l))
    return list(map(list, zip(*zip(*tmp))))


def match_long_cycle(lsts):
    """return matched list, cycling the shorter lists
    longest list matching, cycle [[1,2,3,4,5] ,[10,11]] -> [[1,2,3,4,5] ,[10,11,10,11,10]]
    """
    max_l = 0
    tmp = []
    for l in lsts:
        max_l = max(max_l, len(l))
    for l in lsts:
        if len(l) == max_l:
            tmp.append(l)
        else:
            tmp.append(itertools.cycle(l))
    return list(map(list, zip(*zip(*tmp))))


# when you intent to use lenght of first list to control WHILE loop duration
# and you do not want to change the length of the first list, but you want the second list
# lenght to by not less than the length of the first
def second_as_first_cycle(F, S):
    if len(F) > len(S):
        return list(map(list, zip(*zip(*[F, itertools.cycle(S)]))))[1]
    else:
        return S

def match_cross(lsts):
    """ return cross matched lists
    [[1,2], [5,6,7]] -> [[1,1,1,2,2,2], [5,6,7,5,6,7]]
    """
    return list(map(list, zip(*itertools.product(*lsts))))


def match_cross2(lsts):
    """ return cross matched lists
    [[1,2], [5,6,7]] ->[[1, 2, 1, 2, 1, 2], [5, 5, 6, 6, 7, 7]]
    """
    return list(reversed(list(map(list, zip(*itertools.product(*reversed(lsts)))))))


# Shortest list decides output length [[1,2,3,4,5], [10,11]] -> [[1,2], [10, 11]]
def match_short(lsts):
    """return lists of equal length using the Shortest list to decides length
    Shortest list decides output length [[1,2,3,4,5], [10,11]] -> [[1,2], [10, 11]]
    """
    return list(map(list, zip(*zip(*lsts))))


def fullList(l, count):
    """extends list l so len is at least count if needed with the
    last element of l"""
    d = count - len(l)
    if d > 0:
        l.extend([l[-1] for a in range(d)])
    return

def fullList_deep_copy(l, count):
    """the same that full list function but 
    it have correct work with objects such as lists."""
    d = count - len(l) 
    if d > 0: 
        l.extend([copy.deepcopy(l[-1]) for _ in range(d)]) 
    return

def sv_zip(*iterables):
    """zip('ABCD', 'xy') --> Ax By
    like standard zip but list instead of tuple
    """
    iterators = [iter(it) for it in iterables]
    sentinel = object() # use internal sentinel
    while iterators:
        result = []
        for it in iterators:
            elem = next(it, sentinel)
            if elem is sentinel:
                return
            result.append(elem)
        yield result


#####################################################
################# list levels magic #################
#####################################################

# working with nesting levels
# define data floor
# NOTE, these function cannot possibly work in all scenarios, use with care

def dataCorrect(data, nominal_dept=2):
    """data from nasting to standart: TO container( objects( lists( floats, ), ), )
    """
    dept = levelsOflist(data)
    output = []
    if not dept: # for empty lists
        return []
    if dept < 2:
        return [dept, data]
    else:
        output = dataStandart(data, dept, nominal_dept)
        return output


def dataSpoil(data, dept):
    """from standart data to initial levels: to nested lists
     container( objects( lists( nested_lists( floats, ), ), ), ) это невозможно!
    """
    __doc__ = 'preparing and making spoil'

    def Spoil(dat, dep):
        __doc__ = 'making spoil'
        out = []
        if dep:
            for d in dat:
                out.append([Spoil(d, dep-1)])
        else:
            out = dat
        return out
    lol = levelsOflist(data)
    if dept > lol:
        out = Spoil(data, dept-lol)
    else:
        out = data
    return out


def dataStandart(data, dept, nominal_dept):
    """data from nasting to standart: TO container( objects( lists( floats, ), ), )"""
    deptl = dept - 1
    output = []
    for object in data:
        if deptl >= nominal_dept:
            output.extend(dataStandart(object, deptl, nominal_dept))
        else:
            output.append(data)
            return output
    return output


def levelsOflist(lst):
    """calc list nesting only in countainment level integer"""
    level = 1
    for n in lst:
        if n and isinstance(n, (list, tuple)):
            level += levelsOflist(n)
        return level
    return 0

def get_data_nesting_level(data, data_types=(float, int, np.float64)):
    """
    data: number, or list of numbers, or list of lists, etc.
    data_types: list or tuple of types.

    Detect nesting level of actual data.
    "Actual" data is detected by belonging to one of data_types.
    This method searches only for first instance of "actual data",
    so it does not support cases when different elements of source
    list have different nesting.
    Returns integer.
    Raises an exception if at some point it encounters element
    which is not a tuple, list, or one of data_types.
    
    get_data_nesting_level(1) == 0
    get_data_nesting_level([]) == 1
    get_data_nesting_level([1]) == 1
    get_data_nesting_level([[(1,2,3)]]) == 3
    """

    def helper(data, recursion_depth):
        """ Needed only for better error reporting. """
        if type(data) in data_types:
            return 0
        elif type(data) in (list, tuple):
            if len(data) == 0:
                return 1
            else:
                return helper(data[0], recursion_depth+1) + 1
        elif data is None:
            raise TypeError("get_data_nesting_level: encountered None at nesting level {}".format(recursion_depth))
        else:
            raise TypeError("get_data_nesting_level: unexpected type `{}' of element `{}' at nesting level {}".format(type(data), data, recursion_depth))

    return helper(data, 0)

def ensure_nesting_level(data, target_level, data_types=(float, int, np.float64)):
    """
    data: number, or list of numbers, or list of lists, etc.
    target_level: data nesting level required for further processing.
    data_types: list or tuple of types.

    Wraps data in so many [] as required to achieve target nesting level.
    Raises an exception, if data already has too high nesting level.

    ensure_nesting_level(17, 0) == 17
    ensure_nesting_level(17, 1) == [17]
    ensure_nesting_level([17], 1) == [17]
    ensure_nesting_level([17], 2) == [[17]]
    ensure_nesting_level([(1,2,3)], 3) == [[(1,2,3)]]
    ensure_nesting_level([[[17]]], 1) => exception
    """

    current_level = get_data_nesting_level(data, data_types)
    if current_level > target_level:
        raise TypeError("ensure_nesting_level: input data already has nesting level of {}. Required level was {}.".format(current_level, target_level))
    result = data
    for i in range(target_level - current_level):
        result = [result]
    return result

def transpose_list(lst):
    """
    Transpose a list of lists.

    transpose_list([[1,2], [3,4]]) == [[1,3], [2, 4]]
    """
    return list(map(list, zip(*lst)))

def describe_data_shape(data):
    """
    Describe shape of data in human-readable form.
    Returns string.
    Can be used for debugging or for displaying information to user.
    Note: this method inspects only first element of each list/tuple,
    expecting they are all homogenous (that is usually true in Sverchok).

    describe_data_shape(None) == 'Level 0: NoneType'
    describe_data_shape(1) == 'Level 0: int'
    describe_data_shape([]) == 'Level 1: list [0]'
    describe_data_shape([1]) == 'Level 1: list [1] of int'
    describe_data_shape([[(1,2,3)]]) == 'Level 3: list [1] of list [1] of tuple [3] of int'
    """
    def helper(data):
        if not isinstance(data, (list, tuple)):
            return 0, type(data).__name__
        else:
            result = type(data).__name__
            result += " [{}]".format(len(data))
            if len(data) > 0:
                child = data[0]
                child_nesting, child_result = helper(child)
                result += " of " + child_result
            else:
                child_nesting = 0
            return (child_nesting + 1), result

    nesting, result = helper(data)
    return "Level {}: {}".format(nesting, result)

#####################################################
################### matrix magic ####################
#####################################################

# tools that makes easier to convert data
# from string to matrixes, vertices,
# lists, other and vise versa


def Matrix_listing(prop):
    """Convert Matrix() into Sverchok data"""
    mat_out = []
    for i, matrix in enumerate(prop):
        unit = []
        for k, m in enumerate(matrix):
            # [Matrix0, Matrix1, ... ]
            unit.append(m[:])
        mat_out.append((unit))
    return mat_out


def Matrix_generate(prop):
    """Generate Matrix() data from Sverchok data"""
    mat_out = []
    for i, matrix in enumerate(prop):
        unit = Matrix()
        for k, m in enumerate(matrix):
            # [Matrix0, Matrix1, ... ]
            unit[k] = Vector(m)
        mat_out.append(unit)
    return mat_out


def Matrix_location(prop, list=False):
    """return a list of locations represeting the translation of the matrices"""
    Vectors = []
    for p in prop:
        if list:
            Vectors.append(p.translation[:])
        else:
            Vectors.append(p.translation)
    return [Vectors]


def Matrix_scale(prop, list=False):
    """return a Vector()/list represeting the scale factor of the matrices"""
    Vectors = []
    for p in prop:
        if list:
            Vectors.append(p.to_scale()[:])
        else:
            Vectors.append(p.to_scale())
    return [Vectors]


def Matrix_rotation(prop, list=False):
    """return (Vector, rotation) utility function for Matrix Destructor.
    if list is true the Vector() is decomposed into tuple format.
    """
    Vectors = []
    for p in prop:
        q = p.to_quaternion()
        if list:
            vec, angle = q.to_axis_angle()
            Vectors.append((vec[:], angle))
        else:
            Vectors.append(q.to_axis_angle())
    return [Vectors]


def Vector_generate(prop):
    """return a list of Vector() objects from a standard Sverchok data"""
    return [[Vector(v) for v in obj] for obj in prop]


def Vector_degenerate(prop):
    """return a simple list of values instead of Vector() objects"""
    return [[v[0:3] for v in obj] for obj in prop]


def Edg_pol_generate(prop):
    edg_pol_out = []
    if len(prop[0][0]) == 2:
        type = 'edg'
    elif len(prop[0]) > 2:
        type = 'pol'
    for ob in prop:
        list = []
        for p in ob:
            list.append(p)
        edg_pol_out.append(list)
    # [ [(n1,n2,n3), (n1,n7,n9), p, p, p, p...], [...],... ] n = vertexindex
    return type, edg_pol_out


def matrixdef(orig, loc, scale, rot, angle, vec_angle=[[]]):
    modif = []
    for i, de in enumerate(orig):
        ma = de.copy()

        if loc[0]:
            k = min(len(loc[0])-1, i)
            mat_tran = de.Translation(loc[0][k])
            ma *= mat_tran

        if vec_angle[0] and rot[0]:
            k = min(len(rot[0])-1, i)
            a = min(len(vec_angle[0])-1, i)

            vec_a = vec_angle[0][a].normalized()
            vec_b = rot[0][k].normalized()

            mat_rot = vec_b.rotation_difference(vec_a).to_matrix().to_4x4()
            ma = ma * mat_rot

        elif rot[0]:
            k = min(len(rot[0])-1, i)
            a = min(len(angle[0])-1, i)
            mat_rot = de.Rotation(radians(angle[0][a]), 4, rot[0][k].normalized())
            ma = ma * mat_rot

        if scale[0]:
            k = min(len(scale[0])-1, i)
            scale2 = scale[0][k]
            id_m = Matrix.Identity(4)
            for j in range(3):
                id_m[j][j] = scale2[j]
            ma *= id_m

        modif.append(ma)
    return modif


####
#### random stuff
####

def enum_item(s):
    """return a list usable in enum property from a list with one value"""
    s = [(i,i,"") for i in s]
    return s


#####################################################
############### debug settings magic ################
#####################################################


def sverchok_debug(mode):
    """
    set debug mode to mode
    """
    global DEBUG_MODE
    DEBUG_MODE = mode
    return DEBUG_MODE


def setup_init():
    """
    setup variables needed for sverchok to function
    """
    global DEBUG_MODE
    global HEAT_MAP
    global SVERCHOK_NAME
    import sverchok
    SVERCHOK_NAME = sverchok.__name__
    addon = bpy.context.user_preferences.addons.get(SVERCHOK_NAME)
    if addon:
        DEBUG_MODE = addon.preferences.show_debug
        HEAT_MAP = addon.preferences.heat_map
    else:
        print("Setup of preferences failed")


#####################################################
###############  heat map system     ################
#####################################################


def heat_map_state(state):
    """
    colors the nodes based on execution time
    """
    global HEAT_MAP
    HEAT_MAP = state
    sv_ng = [ng for ng in bpy.data.node_groups if ng.bl_idname == 'SverchCustomTreeType']
    if state:
        for ng in sv_ng:
            color_data = {node.name: (node.color[:], node.use_custom_color) for node in ng.nodes}
            if not ng.sv_user_colors:
                ng.sv_user_colors = str(color_data)
    else:
        for ng in sv_ng:
            if not ng.sv_user_colors:
                print("{0} has no colors".format(ng.name))
                continue
            color_data = ast.literal_eval(ng.sv_user_colors)
            for name, node in ng.nodes.items():
                if name in color_data:
                    color, use = color_data[name]
                    setattr(node, 'color', color)
                    setattr(node, 'use_custom_color', use)
            ng.sv_user_colors = ""

#####################################################
############### update system magic! ################
#####################################################


def updateNode(self, context):
    """
    When a node has changed state and need to call a partial update.
    For example a user exposed bpy.prop
    """
    self.process_node(context)

##############################################################
##############################################################
############## changable type of socket magic ################
########### if you have separate socket solution #############
#################### welcome to provide #####################
##############################################################
##############################################################

def changable_sockets(node, inputsocketname, outputsocketname):
    '''
    arguments: node, name of socket to follow, list of socket to change
    '''
    in_socket = node.inputs[inputsocketname]
    ng = node.id_data
    if in_socket.links:
        in_other = get_other_socket(in_socket)
        if not in_other:
            return
        outputs = node.outputs
        s_type = in_other.bl_idname
        if s_type == 'SvDummySocket':
            return #
        if outputs[outputsocketname[0]].bl_idname != s_type:
            node.id_data.freeze(hard=True)
            to_links = {}
            for n in outputsocketname:
                out_socket = outputs[n]
                to_links[n] = [l.to_socket for l in out_socket.links]
                outputs.remove(outputs[n])
            for n in outputsocketname:
                new_out_socket = outputs.new(s_type, n)
                for to_socket in to_links[n]:
                    ng.links.new(to_socket, new_out_socket)
            node.id_data.unfreeze(hard=True)


def replace_socket(socket, new_type, new_name=None, new_pos=None):
    '''
    Replace a socket with a socket of new_type and keep links
    '''

    socket_name = new_name or socket.name
    socket_pos = new_pos or socket.index
    ng = socket.id_data

    ng.freeze()

    if socket.is_output:
        outputs = socket.node.outputs
        to_sockets = [l.to_socket for l in socket.links]

        outputs.remove(socket)
        new_socket = outputs.new(new_type, socket_name)
        outputs.move(len(outputs)-1, socket_pos)

        for to_socket in to_sockets:
            ng.links.new(new_socket, to_socket)

    else:
        inputs = socket.node.inputs
        from_socket = socket.links[0].from_socket if socket.is_linked else None

        inputs.remove(socket)
        new_socket = inputs.new(new_type, socket_name)
        inputs.move(len(inputs)-1, socket_pos)

        if from_socket:
            ng.links.new(from_socket, new_socket)

    ng.unfreeze()

    return new_socket


def get_other_socket(socket):
    """
    Get next real upstream socket.
    This should be expanded to support wifi nodes also.
    Will return None if there isn't a another socket connect
    so no need to check socket.links
    """
    if not socket.is_linked:
        return None
    if not socket.is_output:
        other = socket.links[0].from_socket
    else:
        other = socket.links[0].to_socket

    if other.node.bl_idname == 'NodeReroute':
        if not socket.is_output:
            return get_other_socket(other.node.inputs[0])
        else:
            return get_other_socket(other.node.outputs[0])
    else:  #other.node.bl_idname == 'WifiInputNode':
        return other


###########################################
# Multysocket magic / множественный сокет #
###########################################

#     utility function for handling n-inputs, for usage see Test1.py
#     for examples see ListJoin2, LineConnect, ListZip
#     min parameter sets minimum number of sockets
#     setup two variables in Node class
#     create Fixed inputs socket, the multi socket will not change anything
#     below min
#     base_name = StringProperty(default='Data ')
#     multi_socket_type = StringProperty(default='StringsSocket')

# the named argument min will be replaced soonish.

def multi_socket(node, min=1, start=0, breck=False, out_count=None):
    '''
     min - integer, minimal number of sockets, at list 1 needed
     start - integer, starting socket.
     breck - boolean, adding bracket to name of socket x[0] x[1] x[2] etc
     output - integer, deal with output, if>0 counts number of outputs multy sockets
     base name added in separated node in self.base_name = 'some_name', i.e. 'x', 'data'
     node.multi_socket_type - type of socket, as .bl_idname

    '''
    #probably incorrect state due or init or change of inputs
    # do nothing
    ng = node.id_data

    if min < 1:
        min = 1
    if out_count is None:
        if not node.inputs:
            return
        if node.inputs[-1].links:
            length = start + len(node.inputs)
            if breck:
                name = node.base_name + '[' + str(length) + ']'
            else:
                name = node.base_name + str(length)
            node.inputs.new(node.multi_socket_type, name)
        else:
            while len(node.inputs) > min and not node.inputs[-2].links:
                node.inputs.remove(node.inputs[-1])
    elif isinstance(out_count, int):
        lenod = len(node.outputs)
        ng.freeze(True)
        print(out_count)
        if out_count > 30:
            out_count = 30
        if lenod < out_count:
            while len(node.outputs) < out_count:
                length = start + len(node.outputs)
                if breck:
                    name = node.base_name + '[' + str(length)+ ']'
                else:
                    name = node.base_name + str(length)
                node.outputs.new(node.multi_socket_type, name)
        else:
            while len(node.outputs) > out_count:
                node.outputs.remove(node.outputs[-1])
        ng.unfreeze(True)


#####################################
# socket data cache                 #
#####################################


def SvGetSocketAnyType(self, socket, default=None, deepcopy=True):
    """Old interface, don't use"""
    return socket.sv_get(default, deepcopy)


def SvSetSocketAnyType(self, socket_name, out):
    """Old interface, don't use"""

    self.outputs[socket_name].sv_set(out)


def socket_id(socket):
    """return an usable and semi stable hash"""
    return socket.socket_id

def node_id(node):
    """return a stable hash for the lifetime of the node
    needs StringProperty called n_id in the node
    """
    return node.node_id



###############
# decorators!
###############
# not used but kept...

def checking_links(process):
    '''Decorator for process method of node.
    This decorator does stanard checks for mandatory input and output links.
    '''

    def real_process(node):
        # check_mandatory_links() node method should return True
        # if all mandatory inputs and outputs are linked.
        # If it returns False then node will just skip processing.
        if hasattr(node, "check_mandatory_links"):
            if not node.check_mandatory_links():
                return
        else:
            # If check_mandatory_links() method is not defined, then node can
            # define list of mandatory inputs and/or outputs.
            # Node will skip processing if any of mandatory inputs is not linked.
            # It will also skip processing if none of mandatory outputs is linked.
            if hasattr(node, "input_descriptors"):
                mandatory_inputs = [descriptor.name for descriptor in node.input_descriptors if descriptor.is_mandatory]
                if not all([node.inputs[name].is_linked for name in mandatory_inputs]):
                    print("Node {}: skip processing: not all of mandatory inputs {} are linked.".format(node.name, mandatory_inputs))
                    return
            if hasattr(node, "output_descriptors"):
                mandatory_outputs = [descriptor.name for descriptor in node.output_descriptors if descriptor.is_mandatory]
                if not any([node.outputs[name].is_linked for name in mandatory_outputs]):
                    print("Node {}: skip processing: none of mandatory outputs {} are linked.".format(node.name, mandatory_outputs))
                    return

        return process(node)

    real_process.__name__ = process.__name__
    real_process.__doc__ = process.__doc__
    return real_process

def iterate_process(method, matcher, *inputs, node=None):
    '''Shortcut function for usual iteration over set of input lists.

    This is shortcut for boilerplate code like

        res1 = []
        res2 = []
        params = match_long_repeat([input1,input2])
        for i1, i2 in zip(*params):
            r1,r2 = self.method(i1,i2)
            res1.append(r1)
            res2.append(r2)
        return res1, res2
    '''

    data = matcher(inputs)
    if node is None:
        results = [list(method(*d)) for d in zip(*data)]
    else:
        results = [list(method(node, *d)) for d in zip(*data)]
    return list(zip(*results))

class Input(object):
    '''Node input socket metainformation descriptor.'''

    def __init__(self, socktype, name, identifier=None, is_mandatory=True, default=sentinel, deepcopy=True):
        self.socktype = socktype
        self.name = name
        self.identifier = identifier if identifier is not None else name
        self.default = default
        self.deepcopy = deepcopy
        self.is_mandatory = is_mandatory

    def __str__(self):
        return self.name

    def create(self, node):
        return node.inputs.new(self.socktype, self.name, self.identifier)

    def get(self, node):
        return node.inputs[self.name].sv_get(default=self.default, deepcopy=self.deepcopy)

class Output(object):
    '''Node output socket metainformation descriptor.'''

    def __init__(self, socktype, name, is_mandatory=True):
        self.socktype = socktype
        self.name = name
        self.is_mandatory = is_mandatory

    def __str__(self):
        return self.name

    def create(self, node):
        node.outputs.new(self.socktype, self.name)

    def set(self, node, value):
        if node.outputs[self.name].is_linked:
            node.outputs[self.name].sv_set(value)

def match_inputs(matcher, inputs, outputs):
    '''Decorator for inputs/outputs boilerplate.

    Usage:

    @match_inputs(match_long_repeat,
                  inputs=[Input(...), Input(...)],
                  outputs=[Output(...), Output(...)])
    def process(self, i1, i2):
        ...
        return res1, res2

    This is shortcut for code like

    def process(self):
        i1s = self.inputs['i1'].sv_get(..)
        i2s = self.inputs['i2'].sv_get(..)
        res1 = []
        res2 = []

        params = match_long_repeat([i1s, i2s])
        for i1,i2 in zip(*params):
            ...
            res1.append(r1)
            res2.append(r2)

        if self.outputs['r1'].is_linked:
            self.outputs['r1'].sv_set(res1)
        if self.outputs['r2'].is_linked:
            self.outputs['r2'].sv_set(res2)
    '''

    def decorator(process):
        def real_process(node):
            inputs_data = [input_descriptor.get(node) for input_descriptor in inputs]
            results = iterate_process(process, matcher, *inputs_data, node=node)
            for result, output_descriptor in zip(results, outputs):
                output_descriptor.set(node, result)

        real_process.__name__ = process.__name__
        real_process.__doc__ = process.__doc__

        return real_process

    return decorator

def std_links_processing(matcher):
    '''Shortcut decorator for "standard" inputs/outputs sockets processing routine.

    This is shortcut for combination of @checking_links and @match_inputs.
    Inputs and outputs descriptors are taken from node.input_descriptors and
    node.output_descriptors correspondingly.
    '''

    def decorator(process):
        def real_process(node):
            nonlocal process
            process = match_inputs(matcher, node.input_descriptors, node.output_descriptors)(process)
            process = checking_links(process)
            return process(node)

        real_process.__name__ = process.__name__
        real_process.__doc__ = process.__doc__

        return real_process

    return decorator
