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
    "AddObjectHelper",
    "object_add_grid_scale",
    "object_add_grid_scale_apply_operator",
    "object_image_guess",
    "world_to_camera_view",
    )


import bpy

from bpy.props import (
        BoolProperty,
        BoolVectorProperty,
        FloatVectorProperty,
        )


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
            if properties.is_property_set("rotation"):
                # ugh, 'view_align' callback resets
                value = properties.rotation[:]
                properties.view_align = view_align
                properties.rotation = value
                del value
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
            rotation = Matrix()

        # set the operator properties
        if operator:
            properties.rotation = rotation.to_euler()

    return location * rotation


def object_data_add(context, obdata, operator=None, use_active_layer=True, name=None):
    """
    Add an object using the view context and preference to to initialize the
    location, rotation and layer.

    :arg context: The context to use.
    :type context: :class:`bpy.types.Context`
    :arg obdata: the data used for the new object.
    :type obdata: valid object data type or None.
    :arg operator: The operator, checked for location and rotation properties.
    :type operator: :class:`bpy.types.Operator`
    :arg name: Optional name
    :type name: string
    :return: the newly created object in the scene.
    :rtype: :class:`bpy.types.ObjectBase`
    """
    scene = context.scene

    # ugh, could be made nicer
    for ob in scene.objects:
        ob.select = False

    if name is None:
        name = "Object" if obdata is None else obdata.name

    obj_new = bpy.data.objects.new(name, obdata)

    base = scene.objects.link(obj_new)
    base.select = True

    v3d = None
    if context.space_data and context.space_data.type == 'VIEW_3D':
        v3d = context.space_data

    if v3d and v3d.local_view:
        base.layers_from_view(context.space_data)

    if operator is not None and any(operator.layers):
        base.layers = operator.layers
    else:
        if use_active_layer:
            if v3d and v3d.local_view:
                base.layers[scene.active_layer] = True
            else:
                if v3d and not v3d.lock_camera_and_layers:
                    base.layers = [True if i == v3d.active_layer
                                   else False for i in range(len(v3d.layers))]
                else:
                    base.layers = [True if i == scene.active_layer
                                   else False for i in range(len(scene.layers))]
        else:
            if v3d:
                base.layers_from_view(context.space_data)

        if operator is not None:
            operator.layers = base.layers

    obj_new.matrix_world = add_object_align_init(context, operator)

    obj_act = scene.objects.active

    # XXX
    # caused because entering edit-mode does not add a empty undo slot!
    if context.user_preferences.edit.use_enter_edit_mode:
        if not (obj_act and
                obj_act.mode == 'EDIT' and
                obj_act.type == obj_new.type):

            _obdata = bpy.data.meshes.new(name)
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
        # scene.objects.active = obj_new

        # Match up UV layers, this is needed so adding an object with UV's
        # doesn't create new layers when there happens to be a naming mis-match.
        uv_new = obdata.uv_layers.active
        if uv_new is not None:
            uv_act = obj_act.data.uv_layers.active
            if uv_act is not None:
                uv_new.name = uv_act.name

        bpy.ops.object.join()  # join into the active.
        if obdata:
            bpy.data.meshes.remove(obdata)
        # base is freed, set to active object
        base = scene.object_bases.active

        bpy.ops.object.mode_set(mode='EDIT')
    else:
        scene.objects.active = obj_new
        if context.user_preferences.edit.use_enter_edit_mode:
            bpy.ops.object.mode_set(mode='EDIT')

    return base


class AddObjectHelper:
    def view_align_update_callback(self, context):
        if not self.view_align:
            self.rotation.zero()

    view_align = BoolProperty(
            name="Align to View",
            default=False,
            update=view_align_update_callback,
            )
    location = FloatVectorProperty(
            name="Location",
            subtype='TRANSLATION',
            )
    rotation = FloatVectorProperty(
            name="Rotation",
            subtype='EULER',
            )
    layers = BoolVectorProperty(
            name="Layers",
            size=20,
            subtype='LAYER',
            options={'HIDDEN', 'SKIP_SAVE'},
            )

    @classmethod
    def poll(self, context):
        return context.scene.library is None


def object_add_grid_scale(context):
    """
    Return scale which should be applied on object
    data to align it to grid scale
    """

    space_data = context.space_data

    if space_data and space_data.type == 'VIEW_3D':
        return space_data.grid_scale_unit

    return 1.0


def object_add_grid_scale_apply_operator(operator, context):
    """
    Scale an operators distance values by the grid size.
    """
    grid_scale = object_add_grid_scale(context)

    properties = operator.properties
    properties_def = properties.bl_rna.properties
    for prop_id in properties_def.keys():
        if not properties.is_property_set(prop_id):
            prop_def = properties_def[prop_id]
            if prop_def.unit == 'LENGTH' and prop_def.subtype == 'DISTANCE':
                setattr(operator, prop_id,
                        getattr(operator, prop_id) * grid_scale)


def object_image_guess(obj, bm=None):
    """
    Return a single image used by the object,
    first checking the texture-faces, then the material.
    """
    # TODO, cycles/nodes materials
    me = obj.data
    if bm is None:
        if obj.mode == 'EDIT':
            import bmesh
            bm = bmesh.from_edit_mesh(me)

    if bm is not None:
        tex_layer = bm.faces.layers.tex.active
        if tex_layer is not None:
            for f in bm.faces:
                image = f[tex_layer].image
                if image is not None:
                    return image
    else:
        tex_layer = me.uv_textures.active
        if tex_layer is not None:
            for tf in tex_layer.data:
                image = tf.image
                if image is not None:
                    return image

    for m in obj.data.materials:
        if m is not None:
            # backwards so topmost are highest priority
            for mtex in reversed(m.texture_slots):
                if mtex and mtex.use_map_color_diffuse:
                    texture = mtex.texture
                    if texture and texture.type == 'IMAGE':
                        image = texture.image
                        if image is not None:
                            return image
    return None


def world_to_camera_view(scene, obj, coord):
    """
    Returns the camera space coords for a 3d point.
    (also known as: normalized device coordinates - NDC).

    Where (0, 0) is the bottom left and (1, 1)
    is the top right of the camera frame.
    values outside 0-1 are also supported.
    A negative 'z' value means the point is behind the camera.

    Takes shift-x/y, lens angle and sensor size into account
    as well as perspective/ortho projections.

    :arg scene: Scene to use for frame size.
    :type scene: :class:`bpy.types.Scene`
    :arg obj: Camera object.
    :type obj: :class:`bpy.types.Object`
    :arg coord: World space location.
    :type coord: :class:`mathutils.Vector`
    :return: a vector where X and Y map to the view plane and
       Z is the depth on the view axis.
    :rtype: :class:`mathutils.Vector`
    """
    from mathutils import Vector

    co_local = obj.matrix_world.normalized().inverted() * coord
    z = -co_local.z

    camera = obj.data
    frame = [-v for v in camera.view_frame(scene=scene)[:3]]
    if camera.type != 'ORTHO':
        if z == 0.0:
            return Vector((0.5, 0.5, 0.0))
        else:
            frame = [(v / (v.z / z)) for v in frame]

    min_x, max_x = frame[1].x, frame[2].x
    min_y, max_y = frame[0].y, frame[1].y

    x = (co_local.x - min_x) / (max_x - min_x)
    y = (co_local.y - min_y) / (max_y - min_y)

    return Vector((x, y, z))
