# GPL # Original Author Liero #

import bpy
from bpy.types import Operator
from bpy.props import (
        StringProperty,
        BoolProperty,
        EnumProperty,
        )


def centro(sel):
    x = sum([obj.location[0] for obj in sel]) / len(sel)
    y = sum([obj.location[1] for obj in sel]) / len(sel)
    z = sum([obj.location[2] for obj in sel]) / len(sel)
    return (x, y, z)


class P2E(Operator):
    bl_idname = "object.parent_to_empty"
    bl_label = "Parent to Empty"
    bl_description = "Parent selected objects to a new Empty"
    bl_options = {"REGISTER", "UNDO"}

    nombre = StringProperty(
                    name="",
                    default='OBJECTS',
                    description='Give the empty / group a name'
                    )
    grupo = BoolProperty(
                    name="Create Group",
                    default=False,
                    description="Also add objects to a group"
                    )
    locat = EnumProperty(
                    name='',
                    items=[('CURSOR', 'Cursor', 'Cursor'), ('ACTIVE', 'Active', 'Active'),
                           ('CENTER', 'Center', 'Selection Center')],
                    description='Empty location',
                    default='CENTER'
                   )
    renom = BoolProperty(
                    name="Add Prefix",
                    default=False,
                    description="Add prefix to objects name"
                    )

    @classmethod
    def poll(cls, context):
        objs = context.selected_objects
        return (len(objs) > 0)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "nombre")
        column = layout.column(align=True)
        column.prop(self, "locat")
        column.prop(self, "grupo")
        column.prop(self, "renom")

    def execute(self, context):
        objs = context.selected_objects
        act = context.object
        sce = context.scene

        try:
            bpy.ops.object.mode_set()
        except:
            pass

        if self.locat == 'CURSOR':
            loc = sce.cursor_location
        elif self.locat == 'ACTIVE':
            loc = act.location
        else:
            loc = centro(objs)

        bpy.ops.object.add(type='EMPTY', location=loc)
        context.object.name = self.nombre
        context.object.show_name = True
        context.object.show_x_ray = True

        if self.grupo:
            bpy.ops.group.create(name=self.nombre)
            bpy.ops.group.objects_add_active()

        for o in objs:
            o.select = True
            if not o.parent:
                bpy.ops.object.parent_set(type='OBJECT')
            if self.grupo:
                bpy.ops.group.objects_add_active()
            o.select = False
        for o in objs:
            if self.renom:
                o.name = self.nombre + '_' + o.name
        return {'FINISHED'}


class PreFix(Operator):
    bl_idname = "object.toggle_prefix"
    bl_label = "Toggle Sufix"
    bl_description = "Toggle parent name as sufix for c"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        act = context.object
        return (act and act.type == 'EMPTY')

    def execute(self, context):
        act = context.object
        objs = act.children
        prefix = act.name + '_'
        remove = False
        for o in objs:
            if o.name.startswith(prefix):
                remove = True
                break

        if remove is True:
            for o in objs:
                if o.name.startswith(prefix):
                    o.name = o.name.partition(prefix)[2]
        else:
            for o in objs:
                o.name = prefix + o.name

        return {'FINISHED'}
