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

# <pep8-80 compliant>

__all__ = (
    "add_object_align_init",
    "object_data_add",
    )


import bpy
import mathutils


def add_object_align_init(context, operator):
    """
    Return a matrix using the operator settings and view context.

    :arg context: The context to use.
    :type context: :class:`bpy.types.Context`
    :arg operator: The operator, checked for location and rotation properties.
    :type operator: :class:`bpy.types.Operator`
    :return: the matrix from the context and settings.
    :rtype: :class:`mathutils.Matrix`
    """

    from mathutils import Matrix, Vector, Euler
    properties = operator.properties if operator is not None else None

    space_data = context.space_data
    if space_data and space_data.type != 'VIEW_3D':
        space_data = None

    # location
    if operator and properties.is_property_set("location"):
        location = Matrix.Translation(Vector(properties.location))
    else:
        if space_data:  # local view cursor is detected below
            location = Matrix.Translation(space_data.cursor_location)
        else:
            location = Matrix.Translation(context.scene.cursor_location)

        if operator:
            properties.location = location.to_translation()

    # rotation
    view_align = (context.user_preferences.edit.object_align == 'VIEW')
    view_align_force = False
    if operator:
        if properties.is_property_set("view_align"):
            view_align = view_align_force = operator.view_align
        else:
            properties.view_align = view_align

    if operator and (properties.is_property_set("rotation") and
                     not view_align_force):

        rotation = Euler(properties.rotation).to_matrix().to_4x4()
    else:
        if view_align and space_data:
            rotation = space_data.region_3d.view_matrix.to_3x3().inverted()
            rotation.resize_4x4()
        else:
            rotation = mathutils.Matrix()

        # set the operator properties
        if operator:
            properties.rotation = rotation.to_euler()

    return location * rotation


def object_data_add(context, obdata, operator=None, use_active_layer=True):
    """
    Add an object using the view context and preference to to initialize the
    location, rotation and layer.

    :arg context: The context to use.
    :type context: :class:`bpy.types.Context`
    :arg obdata: the data used for the new object.
    :type obdata: valid object data type or None.
    :arg operator: The operator, checked for location and rotation properties.
    :type operator: :class:`bpy.types.Operator`
    :return: the newly created object in the scene.
    :rtype: :class:`bpy.types.ObjectBase`
    """
    scene = context.scene

    # ugh, could be made nicer
    for ob in scene.objects:
        ob.select = False

    obj_new = bpy.data.objects.new(obdata.name, obdata)

    base = scene.objects.link(obj_new)
    base.select = True

    v3d = None
    if context.space_data and context.space_data.type == 'VIEW_3D':
        v3d = context.space_data

    if use_active_layer:
        if v3d.local_view:
            base.layers_from_view(context.space_data)
            base.layers[scene.active_layer] = True
        else:
            base.layers = [True if i == scene.active_layer else False for i in range(len(scene.layers))]
    if v3d:
        base.layers_from_view(context.space_data)

    obj_new.matrix_world = add_object_align_init(context, operator)

    obj_act = scene.objects.active

    # XXX
    # caused because entering edit-mode does not add a empty undo slot!
    if context.user_preferences.edit.use_enter_edit_mode:
        if not (obj_act and
                obj_act.mode == 'EDIT' and
                obj_act.type == obj_new.type):

            _obdata = bpy.data.meshes.new(obdata.name)
            obj_act = bpy.data.objects.new(_obdata.name, _obdata)
            obj_act.matrix_world = obj_new.matrix_world
            scene.objects.link(obj_act)
            scene.objects.active = obj_act
            bpy.ops.object.mode_set(mode='EDIT')
            # need empty undo step
            bpy.ops.ed.undo_push(message="Enter Editmode")
    # XXX

    if obj_act and obj_act.mode == 'EDIT' and obj_act.type == obj_new.type:
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.object.mode_set(mode='OBJECT')

        obj_act.select = True
        scene.update()  # apply location
        #scene.objects.active = obj_new

        bpy.ops.object.join()  # join into the active.
        bpy.data.meshes.remove(obdata)

        bpy.ops.object.mode_set(mode='EDIT')
    else:
        scene.objects.active = obj_new
        if context.user_preferences.edit.use_enter_edit_mode:
            bpy.ops.object.mode_set(mode='EDIT')

    return base
