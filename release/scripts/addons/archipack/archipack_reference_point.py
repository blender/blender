# -*- coding:utf-8 -*-

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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110- 1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
#
# ----------------------------------------------------------
import bpy
from bpy.types import Operator, PropertyGroup, Object, Panel
from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    FloatProperty,
    EnumProperty
    )
from mathutils import Vector
from .bmesh_utils import BmeshEdit as bmed


def update(self, context):
    self.update(context)


class archipack_reference_point(PropertyGroup):
    location_2d = FloatVectorProperty(
        subtype='XYZ',
        name="position 2d",
        default=Vector((0, 0, 0))
        )
    location_3d = FloatVectorProperty(
        subtype='XYZ',
        name="position 3d",
        default=Vector((0, 0, 0))
        )
    symbol_scale = FloatProperty(
        name="Screen scale",
        default=1,
        min=0.01,
        update=update)
    symbol_type = EnumProperty(
        name="Symbol type",
        default='WALL',
        items=(
            ('WALL', 'Wall', '', 0),
            ('ROOF', 'Roof', '', 1)),
        update=update)

    @classmethod
    def filter(cls, o):
        """
            Filter object with this class in data
            return
            True when object contains this datablock
            False otherwhise
            usage:
            class_name.filter(object) from outside world
            self.__class__.filter(object) from instance
        """
        try:
            return cls.__name__ in o
        except:
            pass
        return False

    @classmethod
    def datablock(cls, o):
        """
            Retrieve datablock from base object
            return
                datablock when found
                None when not found
            usage:
                class_name.datablock(object) from outside world
                self.__class__.datablock(object) from instance
        """
        try:
            return getattr(o, cls.__name__)[0]
        except:
            pass
        return None

    def update(self, context):

        o = context.active_object

        if self.datablock(o) != self:
            return

        s = self.symbol_scale

        if self.symbol_type == 'WALL':

            verts = [(s * x, s * y, s * z) for x, y, z in [
                (-0.25, 0.25, 0.0), (0.25, 0.25, 0.0), (-0.25, -0.25, 0.0), (0.25, -0.25, 0.0),
                (0.0, 0.0, 0.487), (-0.107, 0.107, 0.216), (0.108, 0.107, 0.216), (-0.107, -0.107, 0.216),
                (0.108, -0.107, 0.216), (-0.05, 0.05, 0.5), (0.05, 0.05, 0.5), (0.05, -0.05, 0.5),
                (-0.05, -0.05, 0.5), (-0.193, 0.193, 0.0), (0.193, 0.193, 0.0), (0.193, -0.193, 0.0),
                (-0.193, -0.193, 0.0), (0.0, 0.0, 0.8), (0.0, 0.8, -0.0), (0.0, 0.0, -0.0),
                (0.0, 0.0, 0.0), (0.05, 0.05, 0.674), (-0.05, 0.674, -0.05), (0.0, 0.8, -0.0),
                (-0.05, -0.05, 0.674), (-0.05, 0.674, 0.05), (0.05, 0.674, -0.05), (-0.129, 0.129, 0.162),
                (0.129, 0.129, 0.162), (-0.129, -0.129, 0.162), (0.129, -0.129, 0.162), (0.0, 0.0, 0.8),
                (-0.05, 0.05, 0.674), (0.05, -0.05, 0.674), (0.05, 0.674, 0.05), (0.8, -0.0, -0.0),
                (0.0, -0.0, -0.0), (0.674, 0.05, -0.05), (0.8, -0.0, -0.0), (0.674, 0.05, 0.05),
                (0.674, -0.05, -0.05), (0.674, -0.05, 0.05)]]

            edges = [(1, 0), (0, 9), (9, 10), (10, 1), (3, 1), (10, 11),
                (11, 3), (2, 3), (11, 12), (12, 2), (0, 2), (12, 9),
                (6, 5), (8, 6), (7, 8), (5, 7), (17, 24), (17, 20),
                (18, 25), (18, 19), (13, 14), (14, 15), (15, 16), (16, 13),
                (4, 6), (15, 30), (17, 21), (26, 22), (23, 22), (23, 34),
                (18, 26), (28, 27), (30, 28), (29, 30), (27, 29), (14, 28),
                (13, 27), (16, 29), (4, 7), (4, 8), (4, 5), (31, 33),
                (31, 32), (21, 32), (24, 32), (24, 33), (21, 33), (25, 22),
                (25, 34), (26, 34), (35, 39), (35, 36), (40, 37), (38, 37),
                (38, 41), (35, 40), (39, 37), (39, 41), (40, 41)]

        elif self.symbol_type == 'ROOF':

            verts = [(s * x, s * y, s * z) for x, y, z in [
                (-0.25, 0.25, 0.0), (0.25, 0.25, 0.0), (-0.25, -0.25, 0.0), (0.25, -0.25, 0.0),
                (0.0, 0.0, 0.487), (-0.107, 0.107, 0.216), (0.108, 0.107, 0.216), (-0.107, -0.107, 0.216),
                (0.108, -0.107, 0.216), (-0.05, 0.05, 0.5), (0.05, 0.05, 0.5), (0.05, -0.05, 0.5),
                (-0.05, -0.05, 0.5), (-0.193, 0.193, 0.0), (0.193, 0.193, 0.0), (0.193, -0.193, 0.0),
                (-0.193, -0.193, 0.0), (0.0, 0.0, 0.8), (0.0, 0.8, -0.0), (0.0, 0.0, 0.0),
                (0.05, 0.05, 0.673), (-0.05, 0.674, -0.05), (-0.05, -0.05, 0.673), (-0.05, 0.674, 0.05),
                (0.05, 0.674, -0.05), (-0.129, 0.129, 0.162), (0.129, 0.129, 0.162), (-0.129, -0.129, 0.162),
                (0.129, -0.129, 0.162), (-0.05, 0.05, 0.673), (0.05, -0.05, 0.673), (0.05, 0.674, 0.05),
                (0.8, -0.0, -0.0), (0.674, 0.05, -0.05), (0.674, 0.05, 0.05), (0.674, -0.05, -0.05),
                (0.674, -0.05, 0.05), (0.108, 0.0, 0.216), (0.09, 0.0, 0.261), (0.001, 0.107, 0.216),
                (0.001, -0.107, 0.216), (-0.107, 0.0, 0.216), (0.0, -0.089, 0.261), (0.0, 0.089, 0.261),
                (-0.089, 0.0, 0.261), (0.0, 0.042, 0.694), (-0.042, 0.0, 0.694), (0.0, -0.042, 0.694),
                (0.042, 0.0, 0.694)]]

            edges = [
                (1, 0), (0, 9), (10, 1), (3, 1), (11, 3), (2, 3), (12, 2), (0, 2),
                (17, 22), (17, 19), (18, 23), (13, 14), (14, 15), (15, 16), (16, 13),
                (15, 28), (17, 20), (24, 21), (18, 24), (14, 26), (13, 25), (16, 27),
                (45, 29), (46, 29), (47, 30), (48, 30), (23, 21), (23, 31), (24, 31),
                (32, 34), (35, 33), (32, 35), (34, 33), (34, 36), (35, 36), (28, 37),
                (6, 38), (26, 37), (26, 39), (25, 39), (5, 43), (5, 44), (25, 41),
                (27, 41), (7, 44), (8, 42), (28, 40), (27, 40), (20, 45), (22, 46),
                (22, 47), (20, 48), (18, 19), (18, 21), (18, 31), (17, 30), (17, 29),
                (32, 19), (32, 33), (32, 36), (4, 6), (4, 7), (4, 8), (4, 5), (8, 38),
                (6, 43), (7, 42), (9, 10), (10, 11), (11, 12), (12, 9)]

        bm = bmed._start(context, o)
        bm.clear()
        for v in verts:
            bm.verts.new(v)
        bm.verts.ensure_lookup_table()
        for ed in edges:
            bm.edges.new((bm.verts[ed[0]], bm.verts[ed[1]]))
        bmed._end(bm, o)


class ARCHIPACK_PT_reference_point(Panel):
    bl_idname = "ARCHIPACK_PT_reference_point"
    bl_label = "Reference point"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'ArchiPack'

    @classmethod
    def poll(cls, context):
        return archipack_reference_point.filter(context.active_object)

    def draw(self, context):
        o = context.active_object
        props = archipack_reference_point.datablock(o)
        if props is None:
            return
        layout = self.layout
        if (o.location - props.location_2d).length < 0.01:
            layout.operator('archipack.move_to_3d')
            layout.operator('archipack.move_2d_reference_to_cursor')
        else:
            layout.operator('archipack.move_to_2d')
        layout.prop(props, 'symbol_scale')
        layout.separator()
        layout.operator('archipack.apply_holes')


class ARCHIPACK_OT_reference_point(Operator):
    """Add reference point"""
    bl_idname = "archipack.reference_point"
    bl_label = "Reference point"
    bl_description = "Add reference point"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}
    location_3d = FloatVectorProperty(
        subtype='XYZ',
        name="position 3d",
        default=Vector((0, 0, 0))
        )
    symbol_type = EnumProperty(
        name="Symbol type",
        default='WALL',
        items=(
            ('WALL', 'Wall', '', 0),
            ('ROOF', 'Roof', '', 1))
        )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.label("Use Properties panel (N) to define parms", icon='INFO')

    def create(self, context):
        x, y, z = context.scene.cursor_location
        # bpy.ops.object.empty_add(type='ARROWS', radius=0.5, location=Vector((x, y, 0)))
        m = bpy.data.meshes.new(name="Reference")
        o = bpy.data.objects.new("Reference", m)
        o.location = Vector((x, y, 0))
        context.scene.objects.link(o)
        d = o.archipack_reference_point.add()
        d.location_2d = Vector((x, y, 0))
        d.location_3d = self.location_3d
        d.symbol_type = self.symbol_type
        o.select = True
        context.scene.objects.active = o
        d.update(context)
        return o

    def execute(self, context):
        if context.mode == "OBJECT":
            o = self.create(context)
            o.select = True
            context.scene.objects.active = o
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_move_to_3d(Operator):
    bl_idname = "archipack.move_to_3d"
    bl_label = "Move to 3d"
    bl_description = "Move point to 3d position"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return archipack_reference_point.filter(context.active_object)

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            props = archipack_reference_point.datablock(o)
            if props is None:
                return {'CANCELLED'}
            o.location = props.location_3d
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_apply_holes(Operator):
    bl_idname = "archipack.apply_holes"
    bl_label = "Apply holes"
    bl_description = "Apply and remove holes from scene"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return archipack_reference_point.filter(context.active_object)

    def apply_boolean(self, context, o):
        # mods = [m for m in o.modifiers if m.type == 'BOOLEAN']
        ctx = bpy.context.copy()
        ctx['object'] = o
        for mod in o.modifiers[:]:
            ctx['modifier'] = mod
            try:
                bpy.ops.object.modifier_apply(ctx, apply_as='DATA',
                                              modifier=ctx['modifier'].name)
            except:
                pass

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            to_remove = []

            for c in o.children:
                if 'archipack_hybridhole' in c:
                    self.apply_boolean(context, c)
                    to_remove.append(c)

            for c in o.children:
                if c.data is not None and "archipack_wall2" in c.data:
                    self.apply_boolean(context, c)

            for c in o.children:
                if c.data is not None and (
                        "archipack_window" in c.data or
                        "archipack_door" in c.data):
                    for h in c.children:
                        if "archipack_hole" in h:
                            to_remove.append(h)

            bpy.ops.object.select_all(action="DESELECT")
            for r in to_remove:
                r.hide_select = False
                r.select = True
                context.scene.objects.active = r
            bpy.ops.object.delete(use_global=False)

            o.select = True
            context.scene.objects.active = o

            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_move_to_2d(Operator):
    bl_idname = "archipack.move_to_2d"
    bl_label = "Move to 2d"
    bl_description = "Move point to 2d position"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return archipack_reference_point.filter(context.active_object)

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            props = archipack_reference_point.datablock(o)
            if props is None:
                return {'CANCELLED'}
            props.location_3d = o.location
            o.location = props.location_2d
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_store_2d_reference(Operator):
    bl_idname = "archipack.store_2d_reference"
    bl_label = "Set 2d"
    bl_description = "Set 2d reference position"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return archipack_reference_point.filter(context.active_object)

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            props = archipack_reference_point.datablock(o)
            if props is None:
                return {'CANCELLED'}
            x, y, z = o.location
            props.location_2d = Vector((x, y, 0))
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_move_2d_reference_to_cursor(Operator):
    bl_idname = "archipack.move_2d_reference_to_cursor"
    bl_label = "Change 2d"
    bl_description = "Change 2d reference position to cursor location without moving childs"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return archipack_reference_point.filter(context.active_object)

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            props = archipack_reference_point.datablock(o)
            if props is None:
                return {'CANCELLED'}
            bpy.ops.object.select_all(action="DESELECT")
            bpy.ops.archipack.reference_point(location_3d=props.location_3d)
            for child in o.children:
                child.select = True
            bpy.ops.archipack.parent_to_reference()
            context.scene.objects.unlink(o)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_parent_to_reference(Operator):
    bl_idname = "archipack.parent_to_reference"
    bl_label = "Parent"
    bl_description = "Make selected object childs of parent reference point"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return archipack_reference_point.filter(context.active_object)

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            props = archipack_reference_point.datablock(o)
            if props is None:
                return {'CANCELLED'}
            sel = [obj for obj in context.selected_objects if obj != o and obj.parent != o]
            itM = o.matrix_world.inverted()
            # print("parent_to_reference parenting:%s objects" % (len(sel)))
            for child in sel:
                rs = child.matrix_world.to_3x3().to_4x4()
                loc = itM * child.matrix_world.translation
                child.parent = None
                child.matrix_parent_inverse.identity()
                child.location = Vector((0, 0, 0))
                child.parent = o
                child.matrix_world = rs
                child.location = loc
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


def register():
    bpy.utils.register_class(archipack_reference_point)
    Object.archipack_reference_point = CollectionProperty(type=archipack_reference_point)
    bpy.utils.register_class(ARCHIPACK_PT_reference_point)
    bpy.utils.register_class(ARCHIPACK_OT_reference_point)
    bpy.utils.register_class(ARCHIPACK_OT_move_to_3d)
    bpy.utils.register_class(ARCHIPACK_OT_move_to_2d)
    bpy.utils.register_class(ARCHIPACK_OT_store_2d_reference)
    bpy.utils.register_class(ARCHIPACK_OT_move_2d_reference_to_cursor)
    bpy.utils.register_class(ARCHIPACK_OT_parent_to_reference)
    bpy.utils.register_class(ARCHIPACK_OT_apply_holes)


def unregister():
    bpy.utils.unregister_class(archipack_reference_point)
    del Object.archipack_reference_point
    bpy.utils.unregister_class(ARCHIPACK_PT_reference_point)
    bpy.utils.unregister_class(ARCHIPACK_OT_reference_point)
    bpy.utils.unregister_class(ARCHIPACK_OT_move_to_3d)
    bpy.utils.unregister_class(ARCHIPACK_OT_move_to_2d)
    bpy.utils.unregister_class(ARCHIPACK_OT_store_2d_reference)
    bpy.utils.unregister_class(ARCHIPACK_OT_move_2d_reference_to_cursor)
    bpy.utils.unregister_class(ARCHIPACK_OT_parent_to_reference)
    bpy.utils.unregister_class(ARCHIPACK_OT_apply_holes)
