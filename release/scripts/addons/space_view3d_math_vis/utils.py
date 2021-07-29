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

# <pep8 compliant>

import bpy


def console_namespace():
    import console_python
    get_consoles = console_python.get_console
    consoles = getattr(get_consoles, "consoles", None)
    if consoles:
        for console, stdout, stderr in get_consoles.consoles.values():
            return console.locals
    return {}


def is_display_list(listvar):
    from mathutils import Vector

    for var in listvar:
        if type(var) is not Vector:
            return False
    return True


class VarStates:

    @staticmethod
    def store_states():
        # Store the display states, called upon unregister the Add-on
        # This is useful when you press F8 to reload the Addons.
        # Then this function preserves the display states of the
        # console variables.
        state_props = bpy.context.window_manager.MathVisStatePropList
        variables = get_math_data()
        for key, ktype in variables.items():
            if key and key not in state_props:
                prop = state_props.add()
                prop.name = key
                prop.ktype = ktype.__name__
                prop.state = [True, False]

    @staticmethod
    def get_index(key):
        index = bpy.context.window_manager.MathVisStatePropList.find(key)
        return index

    @staticmethod
    def delete(key):
        state_props = bpy.context.window_manager.MathVisStatePropList
        index = state_props.find(key)
        if index != -1:
            state_props.remove(index)

    @staticmethod
    def toggle_display_state(key):
        state_props = bpy.context.window_manager.MathVisStatePropList
        if key in state_props:
            state_props[key].state[0] = not state_props[key].state[0]
        else:
            print("Odd: Can not find key %s in MathVisStateProps" % (key))

    @staticmethod
    def toggle_lock_state(key):
        state_props = bpy.context.window_manager.MathVisStatePropList
        if key in state_props:
            state_props[key].state[1] = not state_props[key].state[1]
        else:
            print("Odd: Can not find key %s in MathVisStateProps" % (key))


def get_math_data():
    from mathutils import Matrix, Vector, Quaternion, Euler

    locals = console_namespace()
    if not locals:
        return {}

    variables = {}
    for key, var in locals.items():
        if key[0] == "_" or not var:
            continue
        if type(var) in {Matrix, Vector, Quaternion, Euler} or \
           type(var) in {tuple, list} and is_display_list(var):

            variables[key] = type(var)

    return variables


def cleanup_math_data():

    locals = console_namespace()
    if not locals:
        return

    variables = get_math_data()

    for key in variables.keys():
        index = VarStates.get_index(key)
        if index == -1:
            continue

        state_prop = bpy.context.window_manager.MathVisStatePropList.get(key)
        if state_prop.state[1]:
            continue

        del locals[key]
        bpy.context.window_manager.MathVisStatePropList.remove(index)


def console_math_data():
    from mathutils import Matrix, Vector, Quaternion, Euler

    data_matrix = {}
    data_quat = {}
    data_euler = {}
    data_vector = {}
    data_vector_array = {}

    for key, var in console_namespace().items():
        if key[0] == "_":
            continue

        state_prop = bpy.context.window_manager.MathVisStatePropList.get(key)
        if state_prop:
            disp, lock = state_prop.state
            if not disp:
                continue

        var_type = type(var)

        if var_type is Matrix:
            if len(var.col) != 4 or len(var.row) != 4:
                if len(var.col) == len(var.row):
                    var = var.to_4x4()
                else:  # todo, support 4x3 matrix
                    continue
            data_matrix[key] = var
        elif var_type is Vector:
            if len(var) < 3:
                var = var.to_3d()
            data_vector[key] = var
        elif var_type is Quaternion:
            data_quat[key] = var
        elif var_type is Euler:
            data_euler[key] = var
        elif var_type in {list, tuple} and is_display_list(var):
            data_vector_array[key] = var

    return data_matrix, data_quat, data_euler, data_vector, data_vector_array
