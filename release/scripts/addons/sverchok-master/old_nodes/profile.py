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

''' by Dealga McArdle | 2014 '''

import re
import parser
from ast import literal_eval
from string import ascii_lowercase

import bpy
from bpy.props import BoolProperty, StringProperty, EnumProperty, FloatVectorProperty, IntProperty
from mathutils import Vector
from mathutils.geometry import interpolate_bezier

from sverchok.utils.sv_curve_utils import Arc
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import fullList, updateNode, dataCorrect


idx_map = {i: j for i, j in enumerate(ascii_lowercase)}


'''
input like:

    M|m <2v coordinate>
    L|l <2v coordinate 1> <2v coordinate 2> <2v coordinate n> [z]
    C|c <2v control1> <2v control2> <2v knot2> <int num_segments> <int even_spread> [z]
    A|a <2v rx,ry> <float rot> <int flag1> <int flag2> <2v x,y> <int num_verts> [z]
    X
    #
    -----
    <>  : mandatory field
    []  : optional field
    2v  : two point vector `a,b`
            - no space between ,
            - no backticks
            - a and b can be number literals or lowercase 1-character symbols for variables
    <int .. >
        : means the value will be cast as an int even if you input float
        : flags generally are 0 or 1.
    z   : is optional for closing a line
    X   : as a final command to close the edges (cyclic) [-1, 0]
        in addition, if the first and last vertex share coordinate space
        the last vertex is dropped and the cycle is made anyway.
    #   : single line comment prefix

'''


class PathParser(object):

    # not a full implementation, yet
    supported_types = {
        'M': 'move_to_absolute',
        'm': 'move_to_relative',
        'L': 'line_to_absolute',
        'l': 'line_to_relative',
        'C': 'bezier_curve_to_absolute',
        'c': 'bezier_curve_to_relative',
        'A': 'arc_to_absolute',
        'a': 'arc_to_relative',
        'X': 'close_now',
        '#': 'comment',
        'x': 'close_this_path'
    }

    def __init__(self, properties, segments, idx):
        self.posxy = (0, 0)
        self.previos_posxy = (0, 0)
        self.filename = properties.filename
        self.extended_parsing = properties.extended_parsing
        self.state_idx = 0
        self.path_start_index = 0
        self.previous_command = "START"
        self.section_type = None
        self.close_section = ""
        self.stripped_line = ""

        ''' segments is a dict of letters to variables mapping. '''
        self.segments = segments

        self.profile_idx = idx
        self._get_lines()

    def relative(self, a, b):
        return [a[0]+b[0], a[1]+b[1]]

    def _get_lines(self):
        ''' arrives here only if the file exists '''
        internal_file = bpy.data.texts[self.filename]
        self.lines = internal_file.as_string().split('\n')

    def determine_section_type(self, line):
        first_char = line.strip()[0]
        self.section_type = self.supported_types.get(first_char)

    def sanitize_edgekeys(self, final_verts, final_edges):
        ''' remove references to non existing vertices '''
        if len(final_verts) in final_edges[-1]:
            final_edges.pop()

    def get_geometry(self):
        '''
        This section is partial preprocessor per line found:
        lines like
            L a,b c,d e,f z
        become (after stripping/trimming)
            a,b c,d e,f
            - section_type is stored for the current line
            - close_section flag is stored depending on if z is found.
        '''

        final_verts, final_edges = [], []
        lines = [line for line in self.lines if line]

        for line in lines:
            self.determine_section_type(line)

            if self.section_type in {'move_to_absolute', 'move_to_relative'}:
                self.path_start_index = len(final_verts)

            if self.section_type in (None, 'comment'):
                continue

            if self.section_type == 'close_now':
                self.close_path(final_verts, final_edges)
                break

            if self.section_type == 'close_this_path':
                terminator = [len(final_verts)-1, self.path_start_index]
                final_edges.append(terminator)
                continue

            self.quickread_and_strip(line)

            results = self.parse_path_line()
            if results:
                verts, edges = results
                final_verts.extend(verts)
                final_edges.extend(edges)
                self.posxy = verts[-1]

            self.previous_command = self.section_type

        self.sanitize_edgekeys(final_verts, final_edges)
        return final_verts, [final_edges]

    def quickread_and_strip(self, line):
        '''
        closed segment detection. deal with closing with z or z as variable

        if the user really needs z as last value and z is indeed a variable
        and not intended to close a section, then you must add ;
        '''
        close_section = False
        last_char = line.strip()[-1].lower()
        if last_char in {'z', ';'}:
            stripped_line = line.strip()[1:-1].strip()
            close_section = (last_char == 'z')
        else:
            stripped_line = line.strip()[1:].strip()

        self.stripped_line = stripped_line
        self.close_section = close_section

    def close_path(self, final_verts, final_edges):
        '''
        does the current last index refer to a non existing index?
        this one can be removed then (immediately)
        '''
        if len(final_verts) in final_edges[-1]:
            final_edges.pop()

            ''' but is the last vertex cooincident with the first vertex
            thus allowing a closed loop. Let's check '''
            last_edge_idx = final_edges[-1][1]
            a = Vector(final_verts[0])
            b = Vector(final_verts[last_edge_idx])

            if (a-b).length < 0.0005:
                final_edges[-1][1] = 0
                final_verts.pop()
            else:
                print('here be dragons. last vertex is not close enough')

        else:
            '''
            at this point there is probably distance between end
            point and start..so this bridges the gap
            '''
            edges = [self.state_idx-1, 0]
            final_edges.extend([edges])

    def perform_MoveTo(self):
        xy = self.get_2vec(self.stripped_line)
        if self.section_type == 'move_to_absolute':
            self.posxy = (xy[0], xy[1])
        else:
            self.posxy = self.relative(self.posxy, xy)

    def perform_LineTo(self):
        ''' assumes you have posxy (current needle position) where you want it,
        and draws a line from it to the first set of 2d coordinates, and
        onwards till complete '''

        intermediate_idx, line_data = self.push_forward()
        tempstr = self.stripped_line.split(' ')

        if self.section_type == 'line_to_absolute':
            for t in tempstr:
                sub_comp = self.get_2vec(t)
                line_data.append(sub_comp)
                self.state_idx += 1
        else:
            for t in tempstr:
                sub_comp = self.get_2vec(t)
                final = self.relative(self.posxy, sub_comp)
                self.posxy = tuple(final)
                line_data.append(final)
                self.state_idx += 1

        temp_edges = self.make_edges(intermediate_idx, line_data, -1)
        return line_data, temp_edges

    def perform_CurveTo(self):
        '''
        expects 5 params:
            C x1,y1 x2,y2 x3,y3 num bool [z]
        example:
            C control1 control2 knot2 10 0 [z]
            C control1 control2 knot2 20 1 [z]
        '''

        tempstr = self.stripped_line.split(' ')
        if not len(tempstr) == 5:
            print('error on line CurveTo: ', self.stripped_line)
            return

        ''' fully defined '''
        vec = lambda v: Vector((v[0], v[1], 0))

        knot1 = [self.posxy[0], self.posxy[1]]
        if self.section_type == 'bezier_curve_to_absolute':
            handle1 = self.get_2vec(tempstr[0])
            handle2 = self.get_2vec(tempstr[1])
            knot2 = self.get_2vec(tempstr[2])
        else:
            points = []
            for j in range(3):
                point_pre = self.get_2vec(tempstr[j])
                point = self.relative(self.posxy, point_pre)
                points.append(point)
                self.posxy = tuple(point)
            handle1, handle2, knot2 = points

        r = self.get_typed(tempstr[3], int)
        s = self.get_typed(tempstr[4], int)  # not used yet
        bezier = vec(knot1), vec(handle1), vec(handle2), vec(knot2), r
        points = interpolate_bezier(*bezier)

        # parse down to 2d
        points = [[v[0], v[1]] for v in points]
        return self.find_right_index_and_make_edges(points)

    def perform_ArcTo(self):
        '''
        expects 6 parameters:
            A rx,ry rot flag1 flag2 x,y num_verts [z]
        example:
            A <2v xr,yr> <rot> <int-bool> <int-bool> <2v xend,yend> <int num_verts> [z]
        '''
        tempstr = self.stripped_line.split(' ')
        if not len(tempstr) == 6:
            print(tempstr)
            print('error on ArcTo line: ', self.stripped_line)
            return

        points = []
        start = complex(*self.posxy)
        radius = complex(*self.get_2vec(tempstr[0]))
        xaxis_rot = self.get_typed(tempstr[1], float)
        flag1 = self.get_typed(tempstr[2], int)
        flag2 = self.get_typed(tempstr[3], int)

        # numverts, requires -1 else it means segments (21 verts is 20 segments).
        num_verts = self.get_typed(tempstr[5], int) - 1

        if self.section_type == 'arc_to_absolute':
            end = complex(*self.get_2vec(tempstr[4]))
        else:
            xy_end_pre = self.get_2vec(tempstr[4])
            xy_end_final = self.relative(self.posxy, xy_end_pre)
            end = complex(*xy_end_final)

        arc = Arc(start, radius, xaxis_rot, flag1, flag2, end)

        theta = 1/num_verts
        for i in range(num_verts+1):
            point = arc.point(theta * i)
            points.append(point)

        return self.find_right_index_and_make_edges(points)

    def find_right_index_and_make_edges(self, points):
        '''
        we drop the first point.
        but maybe this should see if the previous commands was not a 'START'
        because that would mean that the first point/vertex does need to be made
        '''
        c = continuation = 1
        d = 1

        if self.previous_command in {'START', 'move_to_absolute', 'move_to_relative'}:
            c = 0
            d = -1

        points = points[c:]

        self.state_idx -= c
        intermediate_idx = self.state_idx
        self.state_idx += (len(points) + c)
        temp_edges = self.make_edges(intermediate_idx, points, d)

        return points, temp_edges

    def parse_path_line(self):
        '''
        This function gathers state for the current profile. It is run on every line of the
        given file.
        - will check lines for lowercase chars to remap, or will use the float/int values
        - it expects to know the current line type
        - it expects to have a valid value for the close_section variable
        '''

        t = self.section_type
        if t in {'move_to_absolute', 'move_to_relative'}:
            return self.perform_MoveTo()
        elif t in {'line_to_absolute', 'line_to_relative'}:
            return self.perform_LineTo()
        elif t in {'bezier_curve_to_absolute', 'bezier_curve_to_relative'}:
            return self.perform_CurveTo()
        elif t in {'arc_to_absolute', 'arc_to_relative'}:
            return self.perform_ArcTo()

    def get_2vec(self, t):
        components = t.split(',')
        sub_comp = []
        for component in components:
            pushval = self.get_typed(component, float)
            sub_comp.append(pushval)

        return sub_comp

    def get_typed(self, component, typed):
        ''' typed can be any castable type, int / float...etc ) '''
        segments = self.segments
        if component in segments:
            pushval = segments[component]['data'][self.profile_idx]

        elif self.is_component_wrapped(component):
            pushval = self.parse_basic_statement(component)

        elif self.is_component_simple_negation(component):
            pushval = self.parse_negation(component)

        else:
            pushval = component

        return typed(pushval)

    def is_component_wrapped(self, component):
        '''then we have a wrapped component, like (a+b)'''
        return (len(component) > 2) and (component[0]+component[-1] == '()')

    def is_component_simple_negation(self, comp):
        return (len(comp) == 2) and (comp[0] == '-') and (comp[1] in self.segments)

    def parse_negation(self, component):
        return -(self.segments[component[1]]['data'][self.profile_idx])

    def parse_basic_statement(self, component):
        '''
        turn: 'd-e-b+-a+1.223/2*4'
        into: ['d','-','e','-','b','+','','-','a','+','1.223','/','2','*','4']
        '''
        # extract parens, but allow internal parens if needed.. internal parens
        # are not supported in literal_eval.
        side = component[1:-1]
        pat = '([\(\)\-+*\/])'
        chopped = re.split(pat, side)

        # - replace known variable chars with intended variable
        # - remove empty elements
        for i, ch in enumerate(chopped):
            if ch in self.segments:
                chopped[i] = str(self.segments[ch]['data'][self.profile_idx])
        chopped = [ch for ch in chopped if ch]

        # - depending on the parsing mode, return found end value.
        string_repr = ''.join(chopped).strip()
        if self.extended_parsing:
            code = parser.expr(string_repr).compile()
            return eval(code)
        else:
            return literal_eval(string_repr)

    def push_forward(self):
        if self.previous_command in {'move_to_absolute', 'move_to_relative'}:
            line_data = [[self.posxy[0], self.posxy[1]]]
            intermediate_idx = self.state_idx
            self.state_idx += 1
        else:
            line_data = []
            intermediate_idx = self.state_idx

        return intermediate_idx, line_data

    def make_edges(self, intermediate_idx, line_data, offset):
        start = intermediate_idx
        end = intermediate_idx + len(line_data) + offset
        temp_edges = [[i, i+1] for i in range(start, end)]

        # move current needle to last position
        if self.close_section:
            closing_edge = [self.state_idx-1, intermediate_idx]
            temp_edges.append(closing_edge)
            self.posxy = tuple(line_data[0])
        else:
            self.posxy = tuple(line_data[-1])

        return temp_edges


class SvPrifilizerMK0(bpy.types.Operator):
    """SvPrifilizer MK0"""
    bl_idname = "node.sverchok_profilizer_mk0"
    bl_label = "SvPrifilizer Mk0"
    bl_options = {'REGISTER', 'UNDO'}

    nodename = StringProperty(name='nodename')
    treename = StringProperty(name='treename')

    def stringadd(self, x):
        precision = bpy.data.node_groups[self.treename].nodes[self.nodename].precision
        a = str(round(x[0], precision)) + ',' + str(round(x[1], precision)) + ' '
        return a
    
    def curve_points_count(self):
        count = bpy.data.node_groups[self.treename].nodes[self.nodename].curve_points_count
        return str(count)

    def execute(self, context):
        if not bpy.context.selected_objects:
            print('Pofiler: Select curve!')
            self.report({'INFO'}, 'Select CURVE first')
            return {'CANCELLED'}
        if not bpy.context.selected_objects[0].type == 'CURVE':
            print('Pofiler: NOT a curve selected')
            self.report({'INFO'}, 'It is not a curve selected for profiler')
            return {'CANCELLED'}
        objs = bpy.context.selected_objects
        names = str([o.name for o in objs])[1:-2]
        # collect paths
        op = []
        for obj in objs:
            for spl in obj.data.splines:
                op.append(spl.bezier_points)

        # define path to text
        values = '# Here is autogenerated values, \n# Please, rename text to avoid data loose.\n'
        values += '# Objects are: \n# %a' % (names)+'.\n'
        values += '# Object origin should be at 0,0,0. \n'
        values += '# Property panel has precision\n# and curve subdivision values.\n\n'
        # also future output for viewer indices
        out_points = []
        out_names = ''
        ss = 0
        for ob_points in op:
            values += '# Spline %a\n' % (ss)
            ss += 1
            # handles preperation
            curves_left  = [i.handle_left_type for i in ob_points]
            curves_right = ['v']+[i.handle_right_type for i in ob_points][:-1]
            # first collect C,L values to compile them later per point
            types = ['FREE','ALIGNED','AUTO']
            curves = ['C ' if x in types or c in types else 'L ' for x,c in zip(curves_left,curves_right)]
            # line for if curve was before line or not
            line = False
            for i,c in zip(range(len(ob_points)),curves):
                co = ob_points[i].co
                if not i:
                    # initial value
                    values += 'M '
                    co = ob_points[0].co[:]
                    values += self.stringadd(co)
                    values += '\n'
                    out_points.append(co)
                    out_names += 'M '
                    # pass if first 'M' that was used already upper
                    continue
                else:
                    if c == 'C ':
                        line = False
                        values += c
                        hr = ob_points[i-1].handle_right[:]
                        hl = ob_points[i].handle_left[:]
                        # hr[0]hr[1]hl[0]hl[1]co[0]co[1] 20 0
                        values += self.stringadd(hr)
                        values += self.stringadd(hl)
                        values += self.stringadd(co)
                        values += self.curve_points_count()
                        values += ' 0 '
                        values += '\n'
                        out_points.append(co[:])
                        out_names += 'C '
                    elif c == 'L ' and not line:
                        line = True
                        values += c
                        values += self.stringadd(co)
                        values += '\n'
                        out_points.append(co[:])
                        out_names += 'L '
                    elif c == 'L ' and line:
                        values += self.stringadd(co)
                        out_points.append(co[:])
                        out_names += 'L '
            if ob_points[0].handle_left_type in types:
                line = False
                values += 'C '
                hr = ob_points[-1].handle_right[:]
                hl = ob_points[0].handle_left[:]
                # hr[0]hr[1]hl[0]hl[1]co[0]co[1] 20 0
                values += self.stringadd(hr)
                values += self.stringadd(hl)
                values += self.stringadd(ob_points[0].co)
                values += self.curve_points_count()
                values += ' 0 '
                values += '\n'
                # preserving overlapping
                #out_points.append(ob_points[0].co[:])
                #out_names += 'C '
            if not line:
                # hacky way till be fixed x for curves not only for lines
                values += '# hacky way till be fixed x\n# for curves not only for lines'
                values += '\nL ' + self.stringadd(ob_points[0].co)
                values += '\nx \n\n'
            else:
                values += '\nx \n\n'
        self.write_values(self.nodename, values)
        print(values)
        bpy.data.node_groups[self.treename].nodes[self.nodename].filename = self.nodename
        print(out_points, out_names)
        return{'FINISHED'}

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

                


class SvProfileNode(bpy.types.Node, SverchCustomTreeNode):
    '''
    svg-like 2d profiles ///

    SvProfileNode generates one or more profiles / elevation segments using;
    assignments, variables, and a string descriptor similar to SVG.

    This node expects simple input, or vectorized input.
    - sockets with no input are automatically 0, not None
    - The longest input array will be used to extend the shorter ones, using last value repeat.
    '''

    bl_idname = 'SvProfileNode'
    bl_label = 'Profile Parametric'
    bl_icon = 'OUTLINER_OB_EMPTY'

    replacement_nodes = [('SvProfileNodeMK2', None, None)]

    def mode_change(self, context):
        if not (self.selected_axis == self.current_axis):
            self.label = self.selected_axis
            self.current_axis = self.selected_axis
            updateNode(self, context)

    axis_options = [
        ("X", "X", "", 0),
        ("Y", "Y", "", 1),
        ("Z", "Z", "", 2)
    ]
    current_axis = StringProperty(default='Z')

    selected_axis = EnumProperty(
        items=axis_options, update=mode_change, name="Type of axis",
        description="offers basic axis output vectors X|Y|Z", default="Z")

    profile_file = StringProperty(default="", update=updateNode)
    filename = StringProperty(default="", update=updateNode)
    posxy = FloatVectorProperty(default=(0.0, 0.0), size=2)
    extended_parsing = BoolProperty(default=False)

    precision = IntProperty(
        name="Precision", min=0, max=10, default=8, update=updateNode,
        description="decimal precision of coordinates when generating profile from selection")

    curve_points_count = IntProperty(
        name="Curve points count", min=1, max=100, default=20, update=updateNode,
        description="Default number of points on curve segment")


    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        row = col.row()
        do_text = row.operator('node.sverchok_profilizer_mk0', text='from selection')
        do_text.nodename = self.name
        do_text.treename = self.id_data.name
        row = col.row()
        row.prop(self, 'selected_axis', expand=True)
        row = col.row(align=True)
        # row.prop(self, "profile_file", text="")
        row.prop_search(self, 'filename', bpy.data, 'texts', text='', icon='TEXT')


    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "extended_parsing", text="extended parsing")
        layout.label("Profile Generator settings")
        layout.prop(self, "precision")
        layout.prop(self, "curve_points_count")


    def sv_init(self, context):
        self.inputs.new('StringsSocket', "a", "a")
        self.inputs.new('StringsSocket', "b", "b")

        self.outputs.new('VerticesSocket', "Verts", "Verts")
        self.outputs.new('StringsSocket', "Edges", "Edges")


    def adjust_inputs(self):
        '''
        takes care of adding new inputs until reaching 26,
        '''
        inputs = self.inputs
        if inputs[-1].is_linked:
            new_index = len(inputs)
            new_letter = idx_map.get(new_index, None)
            if new_letter:
                inputs.new('StringsSocket', new_letter, new_letter)
            else:
                print('this implementation goes up to 26 chars only, use SN or EK')
                print('- or contact Dealga')
        elif not inputs[-2].is_linked:
            inputs.remove(inputs[-1])


    def update(self):
        '''
        update analyzes the state of the node and returns if the criteria to start processing
        are not met.
        '''

        # keeping the file internal for now.
        if not (self.filename in bpy.data.texts):
            return

        if not ('Edges' in self.outputs):
            return

        elif len([1 for inputs in self.inputs if inputs.is_linked]) == 0:
            ''' must have at least one input... '''
            return

        self.adjust_inputs()


    def homogenize_input(self, segments, longest):
        '''
        edit segments in place, extend all to match length of longest
        '''
        for letter, letter_dict in segments.items():
            if letter_dict['length'] < longest:
                fullList(letter_dict['data'], longest)


    def meta_get(self, s_name, fallback, level):
        '''
        private function for the get_input function, accepts level 0..2
        - if socket has no links, then return fallback value
        - s_name can be an index instead of socket name
        '''
        inputs = self.inputs
        if inputs[s_name].is_linked:
            socket_in = inputs[s_name].sv_get()
            if level == 1:
                data = dataCorrect(socket_in)[0]
            elif level == 2:
                data = dataCorrect(socket_in)[0][0]
            else:
                data = dataCorrect(socket_in)
            return data
        else:
            return fallback


    def get_input(self):
        '''
        collect all input socket data, and track the longest sequence.
        '''
        segments = {}
        longest = 0
        for i, input_ in enumerate(self.inputs):
            letter = idx_map[i]

            ''' get socket data, or use a fallback '''
            data = self.meta_get(i, [0], 2)

            num_datapoints = len(data)
            segments[letter] = {'length': num_datapoints, 'data': data}

            if num_datapoints > longest:
                longest = num_datapoints

        return segments, longest


    def process(self):

        if not self.outputs[0].is_linked:
            return

        segments, longest = self.get_input()

        if longest < 1:
            print('logic error, longest < 1')
            return

        self.homogenize_input(segments, longest)
        full_result_verts = []
        full_result_edges = []

        for idx in range(longest):
            path_object = PathParser(self, segments, idx)
            vertices, edges = path_object.get_geometry()

            axis_fill = {
                'X': lambda coords: (0, coords[0], coords[1]),
                'Y': lambda coords: (coords[0], 0, coords[1]),
                'Z': lambda coords: (coords[0], coords[1], 0)
                }.get(self.current_axis)

            vertices = list(map(axis_fill, vertices))
            full_result_verts.append(vertices)
            full_result_edges.append(edges)

        if full_result_verts:
            outputs = self.outputs
            outputs['Verts'].sv_set(full_result_verts)

            if outputs['Edges'].is_linked:
                outputs['Edges'].sv_set(full_result_edges)

    def storage_get_data(self, node_dict):
        node_dict['path_file'] = bpy.data.texts[self.filename].as_string()


def register():
    bpy.utils.register_class(SvProfileNode)
    bpy.utils.register_class(SvPrifilizerMK0)


def unregister():
    bpy.utils.unregister_class(SvPrifilizerMK0)
    bpy.utils.unregister_class(SvProfileNode)

if __name__ == '__main__':
    register()
