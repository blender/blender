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
import importlib.util as getutil
import bpy
import nodeitems_utils

import sverchok
from sverchok.menu import make_node_cats
from sverchok.utils import get_node_class_reference
from sverchok.utils.docstring import SvDocstring
from sverchok.ui.sv_icons import custom_icon
from sverchok.utils.sv_default_macros import macros, DefaultMacros
from nodeitems_utils import _node_categories

# pylint: disable=c0326

sv_tree_types = {'SverchCustomTreeType', 'SverchGroupTreeType'}
node_cats = make_node_cats()
addon_name = sverchok.__name__

loop = {}
loop_reverse = {}
local_macros = {}
ddir = lambda content: [n for n in dir(content) if not n.startswith('__')]


def format_item(k, v):
    return k + " | " + v['display_name']

def format_macro_item(k, v):
    return '< ' + k.replace('_', ' ') + " | " + slice_docstring(v)

def slice_docstring(desc):
    return SvDocstring(desc).get_shorthand()

def ensure_short_description(description):
    '''  the font is not fixed width, it makes litle sense to calculate chars '''
    hardcoded_maxlen = 20
    if description:
        if len(description) > hardcoded_maxlen:
            description = description[:hardcoded_maxlen]
        description = ' | ' + description
    return description

def ensure_valid_show_string(item):
    # nodetype = getattr(bpy.types, item[0])
    nodetype = get_node_class_reference(item[0])
    loop_reverse[nodetype.bl_label] = item[0]
    description = nodetype.bl_rna.get_shorthand()
    return nodetype.bl_label + ensure_short_description(description)

def function_iterator(module_file):
    for name in ddir(module_file):
        obj = getattr(module_file, name)
        if callable(obj) and SvDocstring(obj.__doc__).has_shorthand():
            yield name, obj.__doc__

def get_main_macro_module(fullpath):
    if os.path.exists(fullpath):
        print('--- first time getting sv_macro_module --- ')
        spec = getutil.spec_from_file_location("macro_module.name", fullpath)
        macro_module = getutil.module_from_spec(spec)
        spec.loader.exec_module(macro_module)
        local_macros['sv_macro_module'] = macro_module
        return macro_module

def fx_extend(idx, datastorage):
    datafiles = os.path.join(bpy.utils.user_resource('DATAFILES', path='sverchok', create=True))
    fullpath = os.path.join(datafiles, 'user_macros', 'macros.py')

    # load from previous obtained module, else get from fullpath.
    macro_module = local_macros.get('sv_macro_module')
    if not macro_module:
        macro_module = get_main_macro_module(fullpath)
    if not macro_module:
        return

    for func_name, func_descriptor in function_iterator(macro_module):
        datastorage.append((func_name, format_macro_item(func_name, func_descriptor), '', idx))
        idx +=1


def gather_items():
    fx = []
    idx = 0
    for _, node_list in node_cats.items():
        for item in node_list:
            if item[0] in {'separator', 'NodeReroute'}:
                continue
            
            fx.append((str(idx), ensure_valid_show_string(item), '', idx))
            idx += 1

    for k, v in macros.items():
        fx.append((k, format_item(k, v), '', idx))
        idx += 1
    
    fx_extend(idx, fx)

    return fx


def item_cb(self, context):
    return loop.get('results') or [("A","A", '', 0),]


class SvExtraSearch(bpy.types.Operator):
    """ Extra Search library """
    bl_idname = "node.sv_extra_search"
    bl_label = "Extra Search"
    bl_property = "my_enum"

    my_enum = bpy.props.EnumProperty(items=item_cb)

    def bl_idname_from_bl_label(self, context):
        macro_result = loop['results'][int(self.my_enum)]
        bl_label = macro_result[1].split(' | ')[0].strip()
        return loop_reverse[bl_label]

    def execute(self, context):
        # print(context.space_data.cursor_location)  (in nodeview space)
        # self.report({'INFO'}, "Selected: %s" % self.my_enum)
        if self.my_enum.isnumeric():
            macro_bl_idname = self.bl_idname_from_bl_label(self)
            DefaultMacros.ensure_nodetree(self, context)
            bpy.ops.node.sv_macro_interpretter(macro_bl_idname=macro_bl_idname)
        else:
            macro_reference = macros.get(self.my_enum)

            if macro_reference:
                handler, term = macro_reference.get('ident')
                getattr(DefaultMacros, handler)(self, context, term)

            elif hasattr(local_macros['sv_macro_module'], self.my_enum):
                func = getattr(local_macros['sv_macro_module'], self.my_enum)
                func(self, context)

        return {'FINISHED'}

    def invoke(self, context, event):
        context.space_data.cursor_location_from_region(event.mouse_region_x, event.mouse_region_y)
        loop['results'] = gather_items()
        wm = context.window_manager
        wm.invoke_search_popup(self)
        return {'FINISHED'}


classes = [SvExtraSearch,]


def register():
    for class_name in classes:
        bpy.utils.register_class(class_name)


def unregister():
    for class_name in classes:
        bpy.utils.unregister_class(class_name)
