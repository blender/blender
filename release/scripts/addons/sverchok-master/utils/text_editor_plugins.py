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

import re

import bpy

from sverchok.utils.logging import debug, info, error

sv_error_message = '''\
______________Sverchok Script Generator Node rules_______________

For this operation to work the current line must contain the text:
:   'def sv_main(**variables**):'

Where '**variables**' is something like:
:   'verts=[], petal_size=2.3, num_petals=1'

There are three types of input streams that this node can interpret:
- 'v' (vertices, 3-tuple coordinates)
- 's' (data: float, integer),
- 'm' (matrices: nested lists 4*4)

        For more information see the wiki
        see also the bundled templates for clarification
'''


def has_selection(self, text):
    return not (text.select_end_line == text.current_line and
                text.current_character == text.select_end_character)


def converted(test_str):

    r = re.compile('(?P<name>\w+)=(?P<defval>.*?|\[\])[,\)]')
    k = [m.groupdict() for m in r.finditer(test_str)]
    # debug(k)

    # convert dict
    socket_mapping = {
        '[]': 'v'
        # assume more will follow
    }

    indent = "    "
    socket_members = []
    for variable in k:
        stype = variable['defval']
        sname = variable['name']
        shorttype = socket_mapping.get(stype, 's')
        list_item = str([shorttype, sname, {0}])
        l = list_item.format(sname)
        socket_members.append(indent * 2 + l)
    socket_items = ",\n".join(socket_members)
    declaration = "\n" + indent + 'in_sockets = [\n'
    declaration += socket_items
    declaration += "]"
    return declaration


class SN_Parser(object):

    '''
    example sv lang:
    ================

    <sv_main>
      inputs:
        verts: v [[]] some_socket_name
        edges: s [[]]
        radius: s 0.3
      outputs:
        verts: v verts_out
        edges: s edges_out
        mask: s masked_items_out

    '''

    def __init__(self, fstring, indentation=4):
        self.in_sockets = []
        self.out_sockets = []
        self.state = -1
        self.output_lines = ""
        self.file_as_string = fstring
        self.indent = indentation * ' '
        self.convert()
        self.construct()

    def convert(self):
        split_lines = self.file_as_string.split('\n')
        for line in [i for i in split_lines if i]:
            line_type = self.process(line)

    def get_filename_from_svmain(self, line):
        pat = ':(.+)>'
        regex = re.compile(pat)
        return regex.findall(line)[0]

    def process(self, line):
        line = line.strip()

        if '<sv_main:' in line:
            self.state = 0
            self.python_file_name = self.get_filename_from_svmain(line)
            self.output_lines += 'def sv_main('
            return

        if line.startswith('inputs'):
            self.state = 1
            return

        if line.startswith('outputs'):
            self.state = 2
            return

        if self.state == 1:
            if line.count(':'):
                leftside, rightside = line.split(':')
                '''
                leftside is name of variable,
                rightside is type and default value

                edges: s [[]] ui_name_on_socket <- optional 3rd param
                edges: s [[]]                   <- default socket name
                '''

                var = lambda: None
                var.name = leftside.strip()
                args = rightside.split(' ')
                args = [v for v in args if v]
                num_args = len(args)
                if not (num_args in {2, 3}):
                    return

                args = [v.strip() for v in args]
                if num_args == 2:
                    var.type, var.default = args
                else:
                    var.type, var.default, var.socket_name = args
                self.in_sockets.append(var)
            return

        if self.state == 2:
            if line.count(':'):
                leftside, rightside = line.split(':')
                '''
                leftside is name of socket_output,
                rightside is type and variable sent to socket

                mask: s masked_items_out
                '''
                var = lambda: None
                var.name = leftside.strip()
                args = rightside.split(' ')
                args = [v for v in args if v]
                if not len(args) == 2:
                    return

                args = [v.strip() for v in args]
                var.type, var.socket_variable = args
                self.out_sockets.append(var)
            return

    def construct(self):
        _lines = str(self.output_lines)

        ''' add defaults to sv_main '''
        for var in self.in_sockets:
            _lines += (var.name + '=' + var.default + ', ')
        _lines = _lines[:-2] + '):\n'

        self.output_lines = _lines

        ''' add out_socket_vars '''
        out_variables = ''
        for var in self.out_sockets:
            out_variables += (self.indent + var.socket_variable + ' = []\n')
        out_variables += '\n'
        self.output_lines += out_variables

        ''' add in_sockets '''
        self.output_lines += self.make_sockets('in', self.in_sockets)

        ''' add out_sockets '''
        self.output_lines += '\n'
        self.output_lines += self.make_sockets('out', self.out_sockets)

        ''' add return statement '''
        if self.in_sockets and self.out_sockets:
            self.output_lines += '\n'
            self.output_lines += self.indent + 'return in_sockets, out_sockets'
            self.output_lines += '\n'

    def make_sockets(self, direction, sockets):
        sockets_str = ''
        socket_type = direction + '_sockets'
        sockets_str += (self.indent + socket_type + ' = [\n')
        for var in sockets:
            ind = self.indent * 2
            sock_name = var.socket_name if hasattr(var, 'socket_name') else var.name
            variables = {
                'type': var.type,
                'sock_name': sock_name,
                'var_ref': var.name if direction == 'in' else var.socket_variable
            }
            k = """['{type}', '{sock_name}', {var_ref}],\n"""
            k = k.format(**variables)
            sockets_str += ind + k

        sockets_str = sockets_str[:-2]
        sockets_str += '\n' + (self.indent + "]\n")
        return sockets_str

    @property
    def result(self):
        return self.output_lines


class SvVarnamesToSockets(bpy.types.Operator):

    bl_label = "sv_varname_rewriter"
    bl_idname = "text.varname_rewriter"

    def execute(self, context):
        bpy.ops.text.select_line()
        bpy.ops.text.copy()
        copied_text = bpy.data.window_managers[0].clipboard
        if "def sv_main(" not in copied_text:
            self.report({'INFO'}, "ERROR - LOOK CONSOLE")
            error(sv_error_message)
            return {'CANCELLED'}
        answer = converted(copied_text)

        if answer:
            info(answer)
            bpy.data.window_managers[0].clipboard = answer
            bpy.ops.text.move(type='LINE_BEGIN')
            bpy.ops.text.move(type='NEXT_LINE')
            bpy.ops.text.paste()
        return {'FINISHED'}


class SvLangConverter(bpy.types.Operator):

    bl_label = "Convert SvLang to Script"
    bl_idname = "text.svlang_converter"

    def execute(self, context):
        edit_text = bpy.context.edit_text
        txt = edit_text.as_string()
        sv_obj = SN_Parser(txt)
        result = sv_obj.result

        filename = sv_obj.python_file_name
        if not filename:
            self.report({'INFO'}, "<svmain:filename> must include filename!")
            return {'CANCELLED'}

        new_text = bpy.data.texts.new(filename)
        new_text.from_string(result)

        for area in bpy.context.screen.areas:
            if area.type == 'TEXT_EDITOR':
                if edit_text.name == area.spaces[0].text.name:
                    area.spaces[0].text = new_text
                    break

        del sv_obj
        return {'FINISHED'}


class SvNodeRefreshFromTextEditor(bpy.types.Operator):

    bl_label = "Refesh Current Script"
    bl_idname = "text.noderefresh_from_texteditor"

    def execute(self, context):
        ngs = bpy.data.node_groups
        if not ngs:
            self.report({'INFO'}, "No NodeGroups")
            return {'FINISHED'}

        edit_text = bpy.context.edit_text
        text_file_name = edit_text.name
        is_sv_tree = lambda ng: ng.bl_idname in {'SverchCustomTreeType', 'SvRxTree'}
        ngs = list(filter(is_sv_tree, ngs))

        if not ngs:
            self.report({'INFO'}, "No Sverchok / svrx NodeGroups")
            return {'FINISHED'}

        node_types = set([
            'SvScriptNode', 'SvScriptNodeMK2', 'SvScriptNodeLite',
            'SvProfileNode', 'SvTextInNode', 'SvGenerativeArtNode',
            'SvRxNodeScript', 'SvProfileNodeMK2'])

        for ng in ngs:
            nodes = [n for n in ng.nodes if n.bl_idname in node_types]
            if not nodes:
                continue
            for n in nodes:
                if hasattr(n, "script_name") and n.script_name == text_file_name:
                    try:
                        n.load()
                    except SyntaxError as err:
                        msg = "SyntaxError : {0}".format(err)
                        self.report({"WARNING"}, msg)
                        return {'CANCELLED'}
                    except:
                        self.report({"WARNING"}, 'unspecified error in load()')
                        return {'CANCELLED'}
                elif hasattr(n, "text_file_name") and n.text_file_name == text_file_name:
                    pass  # no nothing for profile node, just update ng, could use break...
                elif hasattr(n, "current_text") and n.current_text == text_file_name:
                    n.reload()
                elif n.bl_idname == 'SvRxNodeScript' and n.text_file == text_file_name:
                    # handle SVRX node reload
                    n.load_text()
                else:
                    pass

            # update node group with affected nodes
            ng.update()


        return {'FINISHED'}


class BasicTextMenu(bpy.types.Menu):
    bl_idname = "TEXT_MT_svplug_menu"
    bl_label = "Plugin Menu"

    @property
    def text_selected(self):
        text = bpy.context.edit_text
        return not (text.current_character == text.select_end_character)

    def draw(self, context):
        layout = self.layout

        if not self.text_selected:
            layout.operator("text.varname_rewriter", text='generate in_sockets')
        layout.operator("text.svlang_converter", text='convert svlang')


# store keymaps here to access after registration
addon_keymaps = []


def add_keymap():

    # handle the keymap
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon

    if not kc:
        debug('no keyconfig path found. that\'s ok')
        return

    km = kc.keymaps.new(name='Text', space_type='TEXT_EDITOR')
    keymaps = km.keymap_items

    if 'noderefresh_from_texteditor' in dir(bpy.ops.text):
        ''' SHORTCUT 1 Node Refresh: Ctrl + Return '''
        ident_str = 'text.noderefresh_from_texteditor'
        if not (ident_str in keymaps):
            new_shortcut = keymaps.new(ident_str, 'RET', 'PRESS', ctrl=1, head=0)
            addon_keymaps.append((km, new_shortcut))

        ''' SHORTCUT 2 Show svplugMenu Ctrl + I (no text selected) '''
        new_shortcut = keymaps.new('wm.call_menu', 'I', 'PRESS', ctrl=1, head=0)
        new_shortcut.properties.name = 'TEXT_MT_svplug_menu'
        addon_keymaps.append((km, new_shortcut))

        debug('added keyboard items to Text Editor.')


def remove_keymap():

    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()


def register():
    bpy.utils.register_class(BasicTextMenu)
    bpy.utils.register_class(SvVarnamesToSockets)
    bpy.utils.register_class(SvLangConverter)
    bpy.utils.register_class(SvNodeRefreshFromTextEditor)
    add_keymap()


def unregister():
    remove_keymap()
    bpy.utils.unregister_class(SvVarnamesToSockets)
    bpy.utils.unregister_class(SvLangConverter)
    bpy.utils.unregister_class(BasicTextMenu)
    bpy.utils.unregister_class(SvNodeRefreshFromTextEditor)
