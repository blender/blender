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

# author Linus Yng, partly based on script node 

import os
import traceback

import bpy
from bpy.props import (
    StringProperty,
    EnumProperty,
    BoolProperty,
    FloatVectorProperty,
    IntVectorProperty,
    BoolVectorProperty
)

FAIL_COLOR = (0.8, 0.1, 0.1)
READY_COLOR = (0, 0.8, 0.95)

from sverchok.utils.sv_update_utils import sv_get_local_path
from sverchok.utils import script_importhelper
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode , node_id

OLD_OP = "node.sverchok_generic_callback_old"
sv_path = os.path.dirname(sv_get_local_path()[0])


class SvDefaultScript2Template(bpy.types.Operator):
    ''' Imports example script or template file in bpy.data.texts'''

    bl_idname = 'node.sverchok_script2_template'
    bl_label = 'Template'
    bl_options = {'REGISTER'}

    script_name = StringProperty(name='name', default='')

    def execute(self, context):
        # if a script is already in text.data list then 001 .002
        # are automatically append by ops.text.open
        templates_path = os.path.join(sv_path, "node_scripts", "SN2-templates")
        path_to_template = os.path.join(templates_path, self.script_name)
        bpy.ops.text.open(filepath=path_to_template, internal=True)
        return {'FINISHED'}


class SvLoadScript(bpy.types.Operator):
    """ Load script data """
    bl_idname = "node.sverchok_load_script2"
    bl_label = "Sverchok text input"
    bl_options = {'REGISTER', 'UNDO'}

    # from object in
    fn_name = StringProperty(name='Function name')

    def execute(self, context):
        n = context.node
        fn_name = self.fn_name

        f = getattr(n, fn_name, None)
        if not f:
            msg = "{0} has no function named '{1}'".format(n.name, fn_name)
            self.report({"WARNING"}, msg)
            return {'CANCELLED'}
        try:
            f()
        except SyntaxError as err:
            msg = "SyntaxError'{0}'".format(err)
            self.report({"WARNING"}, msg)
            n.mark_error(err)
            traceback.print_exc()
            return {'CANCELLED'}
        except TypeError as err:
            n.mark_error(err)
            traceback.print_exc()
            self.report({"WARNING"}, "No valid script in textfile {}".format(n.script_name))
            return {'CANCELLED'}
        except Exception as err:
            n.mark_error(err)
            traceback.print_exc()
            self.report({"WARNING"}, "No valid script in textfile {}".format(n.script_name))
            return {'CANCELLED'}
        return {'FINISHED'}


socket_types = {
    'v': 'VerticesSocket',
    's': 'StringsSocket',
    'm': 'MatrixSocket'
}

# for number lists
defaults = [0 for i in range(32)]

class SvScriptNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    
    bl_idname = 'SvScriptNodeMK2'
    bl_label = 'Script 2'
    bl_icon = 'SCRIPTPLUGINS'

    def avail_templates(self, context):
        templates_path = os.path.join(sv_path, "node_scripts", "SN2-templates")
        items = [(t, t, "") for t in next(os.walk(templates_path))[2]]
        items.sort(key=lambda x:x[0].upper())
        return items
    

    files_popup = EnumProperty(
        items=avail_templates,
        name='template_files',
        description='choose file to load as template')

    #  dict used for storing SvScript objects, access with self.script
    #  property
    script_objects = {}
    
    n_id = StringProperty()
    
    # Also used to keep track if a script is loaded.
    script_str = StringProperty(description = "The acutal script as text")
    script_name = StringProperty(name = "Text file", 
                                 description = "Blender Text object containing script")
    
    # properties that the script can expose either in draw or as socket
    # management needs to be reviewed.
    
    int_list = IntVectorProperty(
        name='int_list', description="Integer list",
        default=defaults, size=32, update=updateNode)

    float_list = FloatVectorProperty(
        name='float_list', description="Float list",
        default=defaults, size=32, update=updateNode)
    
    bool_list = BoolVectorProperty(
        name='bool_list', description="Boolean list",
        default=defaults, size=32, update=updateNode)
    
    def enum_callback(self, context):
        script = self.script
        if hasattr(script, "enum_func"):
            return script.enum_func(context)
        else:
            return None
        
    generic_enum = EnumProperty(    
        items=enum_callback,
        name='Scripted Enum',
        description='See script for description')
    
    #  better logic should be taken from old script node
    #  should support reordering and removal
    
    def create_sockets(self):
        script = self.script
        if script:
            for args in filter(lambda a: a[1] not in self.inputs, script.inputs):
                socket_types
                stype = socket_types[args[0]]
                name = args[1]
                if len(args) == 2:  
                    self.inputs.new(stype, name)
                # has default
                elif len(args) == 3:
                    socket = self.inputs.new(stype, name)
                    default_value = args[2]
                    #  offset feel stupid but the conseqences are max 32
                    #  socket of both types in 32 of both.
                    #  but it should be possible to support
                    #  exposing selected value in draw_buttons
                    offset = len(self.inputs)
                    if isinstance(default_value, int):
                        self.int_list[offset] = default_value
                        socket.prop_type = "int_list"
                    elif isinstance(default_value, float):
                        self.float_list[offset] = default_value
                        socket.prop_type = "float_list"
                    socket.prop_index = offset
                    
            for args in script.outputs:            
                if len(args) > 1 and args[1] not in self.outputs:
                    stype = socket_types[args[0]]
                    self.outputs.new(stype, args[1])
    
    def clear(self):
        self.script_str = ""
        del self.script
        self.inputs.clear()
        self.outputs.clear()
        self.use_custom_color = False
    
    def load(self):
        if not self.script_name in bpy.data.texts:
            self.script_name = ""
            self.clear()
            return
        self.script_str = bpy.data.texts[self.script_name].as_string()
        print("loading...")
        # load in a different namespace using import helper
        self.script = script_importhelper.load_script(self.script_str, self.script_name)
        if self.script:
            self.use_custom_color = True
            self.color = READY_COLOR
            self.create_sockets()
    
    def update(self):
        if not self.script_str:
            return
            
        script = self.script
        
        if not script:
            self.load()
            script = self.script
            if not script:
                return
                    
        if hasattr(script, 'update'):
            script.update()
 
    
    def process(self):
        script = self.script
        
        if not script:
            self.load()
            script = self.script
            if not script:
                return
        
        if hasattr(script, "process"):
            script.process()
    
                        
    def copy(self, node):
        self.n_id = ""
        node_id(self)
                
    def sv_init(self, context):
        node_id(self)
        self.color = FAIL_COLOR
    
    # property functions for accessing self.script
    def _set_script(self, value):
        n_id = node_id(self)
        value.node = self
        self.script_objects[n_id] = value
        
    def _del_script(self):       
        n_id = node_id(self)
        if n_id in self.script_objects:
            del self.script_objects[n_id]
    
    def _get_script(self):
        n_id = node_id(self)
        script = self.script_objects.get(n_id)
        if script:
            script.node = self # paranoid update often safety setting
        return script
        
    script = property(_get_script, _set_script, _del_script, "The script object for this node")
    
    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        row = col.row()
        if not self.script_str:
            row.prop(self, 'files_popup', '')
            import_operator = row.operator('node.sverchok_script2_template', text='', icon='IMPORT')
            import_operator.script_name = self.files_popup
            row = col.row()
            row.prop_search(self, 'script_name', bpy.data, 'texts', text='', icon='TEXT')
            row.operator('node.sverchok_load_script2', text='', icon='PLUGIN').fn_name = 'load'
        else:
            script = self.script
            row = col.row()
            row.operator("node.sverchok_load_script2", text='Reload').fn_name = 'load'
            row.operator(OLD_OP, text='Clear').fn_name = 'clear'
            if hasattr(script, "draw_buttons"):
                script.draw_buttons(context, layout)
    
    def draw_label(self):
        '''
        Uses script.name as label if possible, otherwise the class name.
        If not script is loaded the .bl_label is used
        '''
        script = self.script
        if script:
            if hasattr(script, 'name'):
                return script.name
            else:
                return script.__class__.__name__
        else:
            return self.bl_label
            

def register():
    bpy.utils.register_class(SvScriptNodeMK2)    
    bpy.utils.register_class(SvLoadScript)
    bpy.utils.register_class(SvDefaultScript2Template)

def unregister():
    bpy.utils.unregister_class(SvScriptNodeMK2)
    bpy.utils.unregister_class(SvLoadScript)
    bpy.utils.unregister_class(SvDefaultScript2Template)
