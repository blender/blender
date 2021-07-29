# -*- coding: utf-8 -*-
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
import os
import importlib.util as getutil

import bpy
import sverchok

node_default_dict = {}
node_default_functions = {}


def register_defaults():
    # print('called')
    node_default_dict.update(get_dict())
    # print('____', node_default_dict)


def get_dict():

    datafiles = os.path.join(bpy.utils.user_resource('DATAFILES', path='sverchok', create=True))
    extra_path = ["node_defaults", "node_defaults.json"]
    path_to_defaults = os.path.join(datafiles, *extra_path)

    if os.path.exists(path_to_defaults):
        with open(path_to_defaults) as d:
            print('loading default dict..')
            my_dict = ''.join(d.readlines())
            return ast.literal_eval(my_dict)

    return {}


def get_function(func, node):
    """
    this function takes the variable passed as <func> (which is "filename.functionname") 
    and looks at ../datafiles/node_defaults/filename.py for a function called functionname.

    assuming it is found, it will add the callable function reference to the 
    node_default_functions dict, (node_default_function[<func>] = function_reference)

    each func lookup should only be needed once per session, repeats will be taken directly from
    the dict being built.
    """

    if func in node_default_functions:
        return node_default_functions[func]

    filename, functionname = func.split('.')

    datafiles = os.path.join(bpy.utils.user_resource('DATAFILES', path='sverchok', create=True))
    extra_path = ["node_defaults", filename + '.py']
    path_to_function = os.path.join(datafiles, *extra_path)

    if os.path.exists(path_to_function):
        # print('--- first time getting function path for ', node.bl_idname)
        spec = getutil.spec_from_file_location(func, path_to_function)
        macro_module = getutil.module_from_spec(spec)
        spec.loader.exec_module(macro_module)
        node_default_functions[func] = getattr(macro_module, functionname)
        return node_default_functions[func]


def set_defaults_if_defined(node):
    if not hasattr(node, 'bl_idname') and not len(node_default_dict):
        return

    node_settings = node_default_dict.get(node.bl_idname)
    if not node_settings:
        return

    props = node_settings.get('props')
    if props:
        for prop, value in props:
            setattr(node, prop, value)

    # execute!
    func = node_settings.get('function')
    if func:
        got_function = get_function(func, node)
        try:
            got_function(node)
        except Exception as err:
            print('failed to load', node.bl_idname, '\'s custom default script..')
            print('reason: ', repr(err))
