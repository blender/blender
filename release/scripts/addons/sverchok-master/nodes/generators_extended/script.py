# BEGIN GPL LICENSE BLOCK #####
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
# END GPL LICENSE BLOCK #####

# Scripted Node MK1 (this file) does not support Object Sockets yet.


import ast
import os
import traceback

import bpy
from bpy.props import (
    StringProperty,
    EnumProperty,
    BoolProperty,
    FloatVectorProperty,
    IntVectorProperty
)

from sverchok.utils.sv_update_utils import sv_get_local_path
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import dataCorrect, updateNode

FAIL_COLOR = (0.8, 0.1, 0.1)
READY_COLOR = (0, 0.8, 0.95)

defaults = list(range(32))
sv_path = os.path.dirname(sv_get_local_path()[0])

sock_dict = {
    'v': 'VerticesSocket',
    's': 'StringsSocket',
    'm': 'MatrixSocket'
}


def new_output_socket(node, name, stype):
    socket_type = sock_dict.get(stype)
    if socket_type:
        node.outputs.new(socket_type, name)


def new_input_socket(node, stype, name, dval):
    socket_type = sock_dict.get(stype)
    if socket_type:
        socket = node.inputs.new(socket_type, name)
        socket.default = dval

        if isinstance(dval, (float, int)):
            offset = len(node.inputs)
            if isinstance(dval, float):
                socket.prop_type = "float_list"
                node.float_list[offset] = dval
            else:  # dval is int
                socket.prop_type = "int_list"
                node.int_list[offset] = dval
            socket.prop_index = offset


class SvDefaultScriptTemplate(bpy.types.Operator):
    ''' Imports example script or template file in bpy.data.texts'''

    bl_idname = 'node.sverchok_script_template'
    bl_label = 'Template'
    bl_options = {'REGISTER'}

    script_name = StringProperty(name='name', default='')

    def execute(self, context):
        n = context.node
        templates_path = os.path.join(sv_path, "node_scripts", "templates")

        fullpath = [templates_path, self.script_name]
        if not n.user_name == 'templates':
            fullpath.insert(1, n.user_name)

        path_to_template = os.path.join(*fullpath)
        bpy.ops.text.open(
            filepath=path_to_template,
            internal=True)

        n.script_name = self.script_name

        return {'FINISHED'}


class SvScriptUICallbackOp(bpy.types.Operator):
    ''' Used by Scripted Operators '''

    bl_idname = "node.script_ui_callback"
    bl_label = "Sverchok script ui"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def execute(self, context):
        fn_name = self.fn_name
        n = context.node
        node_function = n.node_dict[hash(n)]['node_function']

        f = getattr(node_function, fn_name, None)
        if not f:
            fmsg = "{0} has no function named '{1}'"
            msg = fmsg.format(n.name, fn_name)
            self.report({"WARNING"}, msg)
            return {'CANCELLED'}
        f()
        return {'FINISHED'}


class SvScriptNodeCallbackOp(bpy.types.Operator):
    ''' Used by ScriptNode Operators '''

    bl_idname = "node.sverchok_callback"
    bl_label = "Sverchok scriptnode callback"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def execute(self, context):
        n = context.node
        fn_name = self.fn_name
        f = getattr(n, fn_name, None)

        if not f:
            msg = "{0} has no function named '{1}'".format(n.name, fn_name)
            self.report({"WARNING"}, msg)
            return {'CANCELLED'}

        if fn_name == "load":
            f()
        elif fn_name == "nuke_me":
            f(context)

        return {'FINISHED'}


class SvScriptNode(bpy.types.Node, SverchCustomTreeNode):

    ''' Script node '''
    bl_idname = 'SvScriptNode'
    bl_label = 'Scripted Node'
    bl_icon = 'SCRIPTPLUGINS'

    def avail_templates(self, context):
        fullpath = [sv_path, "node_scripts", "templates"]
        if not self.user_name == 'templates':
            fullpath.append(self.user_name)

        templates_path = os.path.join(*fullpath)
        items = [(t, t, "") for t in next(os.walk(templates_path))[2]]
        return items

    def avail_users(self, context):
        users = 'templates', 'zeffii', 'nikitron', 'ly', 'ko', 'elfnor'
        return [(j, j, '') for j in users]

    files_popup = EnumProperty(
        items=avail_templates,
        name='template_files',
        description='choose file to load as template')

    user_name = EnumProperty(
        name='users',
        items=avail_users)

    int_list = IntVectorProperty(
        name='int_list', description="Integer list",
        default=defaults, size=32, update=updateNode)

    float_list = FloatVectorProperty(
        name='float_list', description="Float list",
        default=defaults, size=32, update=updateNode)

    script_name = StringProperty()
    script_str = StringProperty()
    button_names = StringProperty()
    has_buttons = BoolProperty(default=0)

    node_dict = {}

    def init(self, context):
        self.node_dict[hash(self)] = {}
        self.use_custom_color = False

    def load(self):
        # print('in load')
        if self.script_name:
            # print('self script_name', self.script_name)
            if self.script_name in bpy.data.texts:
                # print('yep, it is in texts')
                self.script_str = bpy.data.texts[self.script_name].as_string()
                self.label = self.script_name
                self.load_function()
        # print('end of load')

    def load_function(self):
        self.reset_node_dict()
        self.load_py()

    def indicate_ready_state(self):
        self.use_custom_color = True
        self.color = READY_COLOR

    def reset_node_dict(self):
        self.node_dict[hash(self)] = {}
        self.button_names = ""
        self.has_buttons = False
        self.use_custom_color = False

    def nuke_me(self, context):
        in_out = [self.inputs, self.outputs]
        for socket_set in in_out:
            socket_set.clear()

        self.reset_node_dict()
        self.script_name = ""
        self.script_str = ""

    def set_node_function(self, node_function):
        self.node_dict[hash(self)]['node_function'] = node_function

    def get_node_function(self):
        return self.node_dict[hash(self)].get('node_function')

    def draw_buttons_ext(self, context, layout):
        col = layout.column()
        col.prop(self, 'user_name')

    def draw_buttons(self, context, layout):
        sv_callback = 'node.sverchok_callback'
        sv_template = 'node.sverchok_script_template'
        sn_callback = 'node.script_ui_callback'

        col = layout.column(align=True)
        row = col.row()

        if not self.script_str:
            row.prop(self, 'files_popup', '')
            import_operator = row.operator(sv_template, text='', icon='IMPORT')
            import_operator.script_name = self.files_popup

            row = col.row()
            row.prop_search(self, 'script_name', bpy.data, 'texts', text='', icon='TEXT')
            row.operator(sv_callback, text='', icon='PLUGIN').fn_name = 'load'

        else:
            row.operator(sv_callback, text='Reload').fn_name = 'load'
            row.operator(sv_callback, text='Clear').fn_name = 'nuke_me'

            if self.has_buttons:
                row = layout.row()
                for fname in self.button_names.split('|'):
                    row.operator(sn_callback, text=fname).fn_name = fname

    def load_py(self):
        node_functor = None
        try:
            exec(self.script_str, globals(), locals())
            f = vars()
            node_functor = f.get('sv_main')

        except UnboundLocalError:
            print('no sv_main found')

        finally:
            if node_functor:
                params = node_functor.__defaults__
                details = [node_functor, params, f]
                self.process_introspected(details)
            else:
                print("load_py failed, introspection didn\'t find sv_main")
                self.reset_node_dict()

    def process_introspected(self, details):
        node_function, params, f = details
        del f['sv_main']
        globals().update(f)

        self.set_node_function(node_function)

        # no exception handling, let's get the exact error!
        # errors here are errors in the user script. without reporting
        # you have no idea what to fix.
        function_output = node_function(*params)
        num_return_params = len(function_output)

        if num_return_params == 2:
            in_sockets, out_sockets = function_output
        if num_return_params == 3:
            self.has_buttons = True
            in_sockets, out_sockets, ui_ops = function_output

        if self.has_buttons:
            self.process_operator_buttons(ui_ops)

        if in_sockets and out_sockets:
            self.create_or_update_sockets(in_sockets, out_sockets)

        self.indicate_ready_state()

    def process_operator_buttons(self, ui_ops):
        named_buttons = []
        for button_name, button_function in ui_ops:
            f = self.get_node_function()
            setattr(f, button_name, button_function)
            named_buttons.append(button_name)
        self.button_names = "|".join(named_buttons)

    def create_or_update_sockets(self, in_sockets, out_sockets):
        if not self.inputs:
            for socket_type, name, dval in in_sockets:
                new_input_socket(self, socket_type, name, dval)
        else:
            self.update_existing_sockets(params=in_sockets, direction='in')

        if not self.outputs:
            for socket_type, name, data in out_sockets:
                new_output_socket(self, name, socket_type)
        else:
            self.update_existing_sockets(params=out_sockets, direction='out')

    def update_existing_sockets(self, params, direction):
        '''
        this mammoth will run twice per manual reload, once for self.inputs
        and once for self.outputs.

        - if sockets didn't change, it ends early
        - if sockets changed, but no links are found, it removes all sockets
          and recreates them, then returns flow control. (slider values are lost)
        - if outputs change, and some links are found they are stored,
          sockets are removed, recreated, and returning sockets which had previous
          connections are reconnected
        '''

        IO = self.inputs if (direction == 'in') else self.outputs

        def print_debug(a, b):
            d = direction
            first_line = 'current {dir}puts  : {val}'.format(dir=d, val=a)
            second_line = '\nnew required {dir} : {val}'.format(dir=d, val=b)
            print(first_line + second_line)

        def get_names_from(cur_sockets, new_sockets, direction):
            a = [i.name for i in cur_sockets]
            b = [name for x, name, y in new_sockets]
            print_debug(a, b)
            return a, b

        '''
        the following variable clarification is needed:
        a : list of sockets currently on the UI
        b : this is the list of sockets names wanted by the script
        (a == b) == (user refresh didn't involve changes to sockets)
        '''
        a, b = get_names_from(IO, params, direction)
        if a == b:
            return

        has_links = lambda: any([socket.is_linked for socket in IO])

        '''
        [ ] collect current slider values too, i guess, but gets messy
        '''

        if has_links:
            io_dict = None
            '''
            # collect links
            [x] nlist = get current connections
            [x] delete from nlist any socket info not in b
            '''
            io_dict = self.get_connections(direction, IO)
            _a = set(io_dict.keys())
            _b = set(b)
            keep = _a & _b
            removeable = keep ^ _a
            for k in removeable:
                io_dict.pop(k)

        '''
        # flatten
        [x] IO.clear()
        [x] repopulate
        '''
        self.flatten_sockets(IO, direction, params)

        if has_links and io_dict:
            '''
            # reattach
            [X] repopulate old links
            '''
            ng = self.id_data
            for key, val in io_dict.items():
                if direction == 'in':
                    _from = val.node.outputs[val.sock.name]
                    _to = IO[key]
                    ng.links.new(_from, _to)

                if direction == 'out':
                    _from = IO[key]
                    _to = val.node.inputs[val.sock.name]
                    ng.links.new(_from, _to)

    def flatten_sockets(self, IO, direction, params):
        IO.clear()
        if direction == 'in':
            for socket_type, name, dval in params:
                new_input_socket(self, socket_type, name, dval)

        elif direction == 'out':
            for socket_type, name, data in params:
                new_output_socket(self, name, socket_type)

    def get_connections(self, direction, IO):
        io_dict = {}
        for s in IO:
            if not s.is_linked:
                continue

            r = lambda: None
            r.nodelink = s.links[0]
            if direction == 'in':
                r.node = r.nodelink.from_node
                r.sock = r.nodelink.from_socket
            else:
                r.node = r.nodelink.to_node
                r.sock = r.nodelink.to_socket
            io_dict[s.name] = r
        return io_dict

    def process(self):
        inputs = self.inputs

        if not inputs:
            return

        if not hash(self) in self.node_dict:
            if self.script_str:
                self.load_function()

                # maybe reset files_popup and username here

            else:
                self.load()

        if not self.get_node_function():
            return

        node_function = self.get_node_function()
        defaults = node_function.__defaults__

        fparams = []
        input_names = [i.name for i in inputs]
        for idx, name in enumerate(input_names):
            self.get_input_or_default(name, defaults[idx], fparams)

        if (len(fparams) == len(input_names)):
            self.set_outputs(node_function, fparams)

    def get_input_or_default(self, name, this_val, fparams):
        ''' fill up the fparams list with found or default values '''
        socket = self.inputs[name]

        # this deals with incoming links only.
        if socket.links:
            if isinstance(this_val, list):
                try:
                    this_val = socket.sv_get()
                    this_val = dataCorrect(this_val)
                except:
                    pass
            elif isinstance(this_val, (int, float)):
                try:
                    k = str(socket.sv_get())
                    kfree = k[2:-2]
                    this_val = ast.literal_eval(kfree)
                    # this_val = socket.sv_get()[0][0]
                except:
                    pass

        # this catches movement on UI sliders.
        elif isinstance(this_val, (int, float)):
            # extra pussyfooting for the load sequence.
            t = socket.sv_get()
            if t and t[0] and t[0][0]:
                this_val = t[0][0]

        # either found input, or uses default.
        fparams.append(this_val)

    def set_outputs(self, node_function, fparams):
        outputs = self.outputs

        out = node_function(*fparams)
        if len(out) == 2:
            _, out_sockets = out

            for _, name, data in out_sockets:
                if name in outputs:
                    outputs[name].sv_set(data)

    def update_socket(self, context):
        self.update()

classes = [
    SvScriptNode,
    SvDefaultScriptTemplate,
    SvScriptNodeCallbackOp,
    SvScriptUICallbackOp
]


def register():
    for class_name in classes:
        bpy.utils.register_class(class_name)


def unregister():
    for class_name in classes:
        bpy.utils.unregister_class(class_name)
