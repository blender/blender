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

import os
import re
import ast
import traceback
from ast import literal_eval

import bpy
from mathutils import Vector, Matrix, Euler, Quaternion, Color
from bpy.props import FloatProperty, StringProperty, BoolProperty, EnumProperty
from sverchok.node_tree import SverchCustomTreeNode, StringsSocket, VerticesSocket, MatrixSocket
from sverchok.data_structure import updateNode, SvGetSocketAnyType, SvSetSocketAnyType, Matrix_generate

'''
- SET:  `path.to.prop`
- GET:  `path.to.prop`
- DO:   `eval_text(a, b, [True])
        `read_text(a, [True])
        `do_function(a) with x

'''


def read_text(fp, update=True):
    """
    if args has separators then look on local disk else in .blend.
    update writes the changes to the textfile in blender
    """
    texts = bpy.data.texts

    internal_file = False
    text_name = fp
    if not (os.sep in fp) and (fp in texts):
        # print(fp)
        # file in blend, but linked outside
        # print('internal file!')
        internal_file = True
        fp = texts[text_name].filepath
        fp = bpy.path.abspath(fp)

    with open(fp) as new_text:
        text_body = ''.join(new_text.readlines())

    if internal_file and update:
        texts[text_name].from_string(text_body)

    return literal_eval(text_body)


# def eval_text(node, function_text, out_text, update=True):
def eval_text(function_text, out_text, update=True):
    """
    eval_text(function_text, out_text, update=True)
        :   function_text
            a reference to a file inside blender. This text should be initiated outside
            of blender or made external by saving and loading. The content of this file is
            what writes to the out_text.

        :   out_text
            the internal text file to read from. The content of which might be changing on
            each update.

        :   update
            this parameter isn't very useful at the moment, but keep it to True if you
            want to update the content of the internal text file. Else only the external
            file will be read.
    """
    texts = bpy.data.texts
    text = texts[function_text]

    if update:
        fp = text.filepath
        fp = bpy.path.abspath(fp)
        with open(fp) as new_text:
            text_body = ''.join(new_text.readlines())
            text.from_string(text_body)

    # at this point text is updated and can be executed.
    # could be cached in node.
    text = texts[function_text]
    exec(text.as_string())

    # if function_text execed OK, then it has written to texts[out_text]
    # This file out_text should exist.
    out_data = None
    if out_text in texts:
        written_data = texts[out_text].as_string()
        out_data = literal_eval(written_data)

    return out_data


def get_params(prop, pat):
    """function to convert the string representation to literal arguments ready for passing"""
    regex = re.compile(pat)
    return literal_eval(regex.findall(prop)[0])


def process_macro(node, macro, prop_to_eval):
    params = get_params(prop_to_eval, '\(.*?\)')
    tvar = None
    fn = None

    if macro == 'eval_text':
        if 2 <= len(params) <= 3:
            fn = eval_text
    else:
        if 1 <= len(params) <= 2:
            fn = read_text

    if not fn:
        return

    # do this once, if success skip the try on the next update
    if not node.eval_success:
        try:
            tvar = fn(*params)
        except Exception as err:
            if node.full_traceback:
                print(traceback.format_exc())
            else:
                fail_msg = "nope, {type} with ({params}) failed - try full traceback"
                print(fail_msg.format(type=macro, params=str(params)))
            node.previous_eval_str = ""
        finally:
            node.eval_success = False if (tvar is None) else True
            print('success?', node.eval_success)
            return tvar
    else:
        print('running {macro} unevalled'.format(macro=macro))
        return fn(*params)


def process_prop_string(node, prop_to_eval):
    """
    First it is evaluated in a try/except scenario, and if that went OK then the next update
    is without try/except.

    example eval strings might be:
        objs['Cube'].location
        objs['Cube'].matrix_world

    I have expressively not implemented a wide range of features, imo that's what Scriped Node
    is best at.

    """
    tvar = None

    c = bpy.context
    scene = c.scene
    data = bpy.data
    objs = data.objects
    mats = data.materials
    meshes = data.meshes
    texts = data.texts

    # yes there's a massive assumption here too.
    if not node.eval_success:
        try:
            tvar = eval(prop_to_eval)
        except Exception as err:
            if node.full_traceback:
                print(traceback.format_exc())
            else:
                print("nope, crash n burn hard - try full traceback")
            node.previous_eval_str = ""
        finally:
            print('evalled', tvar)
            node.eval_success = False if (tvar is None) else True
    else:
        tvar = eval(prop_to_eval)

    return tvar


def process_input_to_bpy(node, tvar, stype):
    """
    this is one-way, node is the reference to the current eval node. tvar is the current
    variable being introduced into bpy. First it is executed in a try/except scenario,
    and if that went OK then the next update is without try/except.
    """

    c = bpy.context
    scene = c.scene
    data = bpy.data
    objs = data.objects
    mats = data.materials
    meshes = data.meshes

    if stype == 'MatrixSocket':
        tvar = str(tvar[:])

    fxed = (node.eval_str.strip() + " = {x}").format(x=tvar)

    # yes there's a massive assumption here.
    if not node.eval_success:
        success = False
        try:
            exec(fxed)
            success = True
            node.previous_eval_str = node.eval_str
        except Exception as err:
            if node.full_traceback:
                print(traceback.format_exc())
            else:
                print("nope, crash n burn - try full traceback")
            success = False
            node.previous_eval_str = ""
        finally:
            node.eval_success = success
    else:
        exec(fxed)


def process_input_dofunction(node, x):
    """
    This function aims to facilitate the repeated execution of a python file
    located inside Blender. Similar to Scripted Node but with the restriction
    that it has one input by design. Realistically the input can be an array,
    and therefore nested with a collection of variables.

    The python file to exec shall be specified in the eval string like so:

        `do_function('file_name.py') with x`

    Here x is the value of the input socket, this will automatically be in the
    scope of the function when EK calls it. First it is executed in a
    try/except scenario, and if that went OK then the next update is without
    try/except.

    The content of file_name.py can be anything that executes, function or
    a flat file. The following convenience variables will be present.
    """

    c = bpy.context
    scene = c.scene
    data = bpy.data
    objs = data.objects
    mats = data.materials
    meshes = data.meshes
    texts = data.texts

    # extract filename
    # if filename not in .blend return and throw error
    function_file = get_params(node.eval_str, '\(.*?\)')

    if not (function_file in texts):
        print('function_file, not found -- check spelling')
        node.eval_success = False
        node.previous_eval_str = ""
        return

    text = texts[function_file]
    raw_text_str = text.as_string()

    # yes there's a massive assumption here.
    if not node.eval_success:
        success = False
        try:
            exec(raw_text_str)
            success = True
            node.previous_eval_str = node.eval_str
        except Exception as err:
            if node.full_traceback:
                print(traceback.format_exc())
            else:
                print("nope, crash n burn - try full traceback")
            success = False
            node.previous_eval_str = ""
        finally:
            node.eval_success = success
    else:
        exec(raw_text_str)


def wrap_output_data(tvar):
    if isinstance(tvar, Vector):
        data = [[tvar[:]]]
    elif isinstance(tvar, Matrix):
        data = [[r[:] for r in tvar[:]]]
    elif isinstance(tvar, (Euler, Quaternion)):
        tvar = tvar.to_matrix().to_4x4()
        data = [[r[:] for r in tvar[:]]]
    elif isinstance(tvar, list):
        data = [tvar]
    else:
        data = tvar

    return data


class EvalKnievalNode(bpy.types.Node, SverchCustomTreeNode):

    ''' Eval Knieval Node '''
    bl_idname = 'EvalKnievalNode'
    bl_label = 'Eval Knieval Node'
    bl_icon = 'OUTLINER_OB_EMPTY'

    x = FloatProperty(
        name='x', description='x variable', default=0.0, precision=5, update=updateNode)

    eval_str = StringProperty(update=updateNode)
    previous_eval_str = StringProperty()

    mode = StringProperty(default="input")
    previous_mode = StringProperty(default="input")

    eval_success = BoolProperty(default=False)

    # not hooked up yet.
    eval_knieval_mode = BoolProperty(
        default=True,
        description="when unticked, try/except is done only once")

    # hyper: because it's above mode.
    current_hyper = StringProperty(default="SET")
    hyper_options = [
        ("DO",  "Do",  "", 0),
        ("SET", "Set", "", 1),
        ("GET", "Get", "", 2)
    ]

    def mode_change(self, context):

        if not (self.selected_hyper == self.current_hyper):
            self.label = self.selected_hyper
            self.update_outputs_and_inputs()
            self.current_hyper = self.selected_hyper
            updateNode(self, context)

    selected_hyper = EnumProperty(
        items=hyper_options,
        name="Behavior",
        description="Choices of behavior",
        default="SET",
        update=mode_change)

    full_traceback = BoolProperty()

    def init(self, context):
        self.inputs.new('StringsSocket', "x").prop_name = 'x'
        self.width = 400

    def draw_buttons(self, context, layout):
        if self.selected_hyper in {'DO', 'SET'}:
            row = layout.row()
            # row.separator()
            row.label('')

        row = layout.row()
        row.prop(self, 'selected_hyper', expand=True)
        row = layout.row()
        row.prop(self, 'eval_str', text='')

    def draw_buttons_ext(self, context, layout):
        row = layout.row()
        # row.prop(self, 'eval_knieval_mode', text='eval knieval mode')
        row.prop(self, 'full_traceback', text='full traceback')

    def update_outputs_and_inputs(self):
        self.mode = {
            'SET': 'input',
            'GET': 'output',
            'DO': 'input'
            }.get(self.selected_hyper, None)

        if not (self.mode == self.previous_mode):
            self.set_sockets()
            self.previous_mode = self.mode
            self.eval_success = False

    def update(self):
        """
        Update behaves like the conductor, it detects the modes and sends flow control
        to functions that know how to deal with socket data consistent with those modes.

        It also avoids extra calculation by figuring out if input/output critera are
        met before anything is processed. It returns early if it can.

        """
        inputs = self.inputs
        outputs = self.outputs

        if self.mode == "input" and len(inputs) == 0:
            return
        elif self.mode == "output" and len(outputs) == 0:
            return

        if len(self.eval_str) <= 4:
            return

        if not (self.eval_str == self.previous_eval_str):
            self.eval_success = False

        {
            "input": self.input_mode,
            "output": self.output_mode
        }.get(self.mode, lambda: None)()

        self.set_ui_color()

    def input_mode(self):
        inputs = self.inputs
        if (len(inputs) == 0) or (not inputs[0].links):
            print('has no link!')
            return

        # then morph default socket type to whatever we plug into it.
        from_socket = inputs[0].links[0].from_socket
        incoming_socket_type = type(from_socket)
        stype = {
            VerticesSocket: 'VerticesSocket',
            MatrixSocket: 'MatrixSocket',
            StringsSocket: 'StringsSocket'
        }.get(incoming_socket_type, None)

        # print(incoming_socket_type, from_socket, stype)

        if not stype:
            print('unidentified flying input')
            return

        # if the current self.input socket is different to incoming
        if not (stype == self.inputs[0].bl_idname):
            self.morph_input_socket_type(stype)

        # only one nesting level supported, for types other than matrix.
        # else x gets complicated. x is complex already, this forces
        # simplicity
        tvar = None
        if stype == 'MatrixSocket':
            prop = SvGetSocketAnyType(self, inputs[0])
            tvar = Matrix_generate(prop)[0]
            # print('---repr-\n', repr(tvar))
        else:
            tvar = SvGetSocketAnyType(self, inputs[0])[0][0]

        # input can still be empty or []
        if not tvar:
            return

        if self.eval_str.endswith("with x"):
            process_input_dofunction(self, tvar)
        else:
            process_input_to_bpy(self, tvar, stype)

    def output_mode(self):
        outputs = self.outputs
        if (len(outputs) == 0) or (not outputs[0].links):
            print('has no link!')
            return

        prop_to_eval = self.eval_str.strip()
        macro = prop_to_eval.split("(")[0]
        tvar = None

        if macro in ['eval_text', 'read_text']:
            tvar = process_macro(self, macro, prop_to_eval)
        else:
            tvar = process_prop_string(self, prop_to_eval)

        # explicit None must be caught. not 0 or False
        if tvar is None:
            return

        if not (self.previous_eval_str == self.eval_str):
            print("tvar: ", tvar)
            self.morph_output_socket_type(tvar)

        # finally we can set this.
        data = wrap_output_data(tvar)
        SvSetSocketAnyType(self, 0, data)
        self.previous_eval_str = self.eval_str

    def set_sockets(self):
        """
        Triggered by mode changes between [input, output] this removes the socket
        from one side and adds a socket to the other side. This way you have something
        to plug into. When you connect a node to a socket, the socket can then be
        automagically morphed to match the socket-type. (morphing is however done in the
        morph functions)
        """
        a, b = {
            'input': (self.inputs, self.outputs),
            'output': (self.outputs, self.inputs)
        }[self.mode]
        b.clear()

        a.new('StringsSocket', 'x')
        if self.mode == 'input':
            a[0].prop_name = 'x'

    def set_ui_color(self):
        self.use_custom_color = True
        self.color = (1.0, 1.0, 1.0) if self.eval_success else (0.98, 0.6, 0.6)

    def morph_output_socket_type(self, tvar):
        """
        Set the output according to the data types
        the body of this if-statement is done only infrequently,
        when the eval string is not the same as the last eval.
        """
        outputs = self.outputs
        output_socket_type = 'StringsSocket'

        if isinstance(tvar, Vector):
            output_socket_type = 'VerticesSocket'
        elif isinstance(tvar, (list, tuple)):
            output_socket_type = 'VerticesSocket'
        elif isinstance(tvar, (Matrix, Euler, Quaternion)):
            output_socket_type = 'MatrixSocket'
        elif isinstance(tvar, (int, float)):
            output_socket_type = 'StringsSocket'

        links = outputs[0].links
        needs_reconnect = False

        if links and links[0]:
            needs_reconnect = True
            link = links[0]
            node_to = link.to_node
            socket_to = link.to_socket

        # needs clever reconnect? maybe not.
        if outputs[0].bl_idname != output_socket_type:
            outputs.clear()
            outputs.new(output_socket_type, 'x')

        if needs_reconnect:
            ng = self.id_data
            ng.links.new(outputs[0], socket_to)

    def morph_input_socket_type(self, new_type):
        """
        Recasts current input socket type to conform to incoming type
        Preserves the connection.
        """

        # where is the data coming from?
        inputs = self.inputs
        link = inputs[0].links[0]
        node_from = link.from_node
        socket_from = link.from_socket

        # flatten and reinstate
        inputs.clear()
        inputs.new(new_type, 'x')

        # reconnect
        ng = self.id_data
        ng.links.new(socket_from, inputs[0])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(EvalKnievalNode)


def unregister():
    bpy.utils.unregister_class(EvalKnievalNode)
