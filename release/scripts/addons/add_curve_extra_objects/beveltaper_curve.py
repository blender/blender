# ##### BEGIN GPL LICENSE BLOCK #####
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ##### END GPL LICENSE BLOCK #####

# DevBo Task: https://developer.blender.org/T37377

bl_info = {
    "name": "Bevel/Taper Curve",
    "author": "Cmomoney",
    "version": (1, 1),
    "blender": (2, 69, 0),
    "location": "View3D > Object > Bevel/Taper",
    "description": "Adds bevel and/or taper curve to active curve",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Curve/Bevel_-Taper_Curve",
    "category": "Curve"}


import bpy
from bpy.types import (
        Operator,
        Menu,
        )
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntProperty,
        )
from bpy_extras.object_utils import (
        AddObjectHelper,
        object_data_add,
        )


def add_taper(self, context):
    scale_ends1 = self.scale_ends1
    scale_ends2 = self.scale_ends2
    scale_mid = self.scale_mid
    verts = [
            (-2.0, 1.0 * scale_ends1, 0.0, 1.0),
            (-1.0, 0.75 * scale_mid, 0.0, 1.0),
            (0.0, 1.5 * scale_mid, 0.0, 1.0),
            (1.0, 0.75 * scale_mid, 0.0, 1.0),
            (2.0, 1.0 * scale_ends2, 0.0, 1.0)
            ]
    make_path(self, context, verts)


def add_type5(self, context):
    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [0.0 * scale_x, 0.049549 * scale_y,
            0.0, 0.031603 * scale_x, 0.047013 * scale_y,
            0.0, 0.05 * scale_x, 0.0 * scale_y, 0.0,
            0.031603 * scale_x, -0.047013 * scale_y,
            0.0, 0.0 * scale_x, -0.049549 * scale_y,
            0.0, -0.031603 * scale_x, -0.047013 * scale_y,
            0.0, -0.05 * scale_x, -0.0 * scale_y, 0.0,
            -0.031603 * scale_x, 0.047013 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.008804 * scale_x, 0.049549 * scale_y, 0.0),
            (0.021304 * scale_x, 0.02119 * scale_y, 0.0),
            (0.05 * scale_x, 0.051228 * scale_y, 0.0),
            (0.036552 * scale_x, -0.059423 * scale_y, 0.0),
            (0.008804 * scale_x, -0.049549 * scale_y, 0.0),
            (-0.021304 * scale_x, -0.02119 * scale_y, 0.0),
            (-0.05 * scale_x, -0.051228 * scale_y, 0.0),
            (-0.036552 * scale_x, 0.059423 * scale_y, 0.0)]
            ]
    rhandles = [
            [(0.008803 * scale_x, 0.049549 * scale_y, 0.0),
            (0.036552 * scale_x, 0.059423 * scale_y, 0.0),
            (0.05 * scale_x, -0.051228 * scale_y, 0.0),
            (0.021304 * scale_x, -0.02119 * scale_y, 0.0),
            (-0.008803 * scale_x, -0.049549 * scale_y, 0.0),
            (-0.036552 * scale_x, -0.059423 * scale_y, 0.0),
            (-0.05 * scale_x, 0.051228 * scale_y, 0.0),
            (-0.021304 * scale_x, 0.02119 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type4(self, context):
    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.0 * scale_x, 0.017183 * scale_y,
            0.0, 0.05 * scale_x, 0.0 * scale_y,
            0.0, 0.0 * scale_x, -0.017183 * scale_y,
            0.0, -0.05 * scale_x, -0.0 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.017607 * scale_x, 0.017183 * scale_y, 0.0),
            (0.05 * scale_x, 0.102456 * scale_y, 0.0),
            (0.017607 * scale_x, -0.017183 * scale_y, 0.0),
            (-0.05 * scale_x, -0.102456 * scale_y, 0.0)]
            ]
    rhandles = [
            [(0.017607 * scale_x, 0.017183 * scale_y, 0.0),
            (0.05 * scale_x, -0.102456 * scale_y, 0.0),
            (-0.017607 * scale_x, -0.017183 * scale_y, 0.0),
            (-0.05 * scale_x, 0.102456 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type3(self, context):
    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.017183 * scale_x, 0.0 * scale_y,
            0.0, 0.0 * scale_x, 0.05 * scale_y,
            0.0, 0.017183 * scale_x, 0.0 * scale_y,
            0.0, 0.0 * scale_x, -0.05 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.017183 * scale_x, -0.017607 * scale_y, 0.0),
            (-0.102456 * scale_x, 0.05 * scale_y, 0.0),
            (0.017183 * scale_x, 0.017607 * scale_y, 0.0),
            (0.102456 * scale_x, -0.05 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.017183 * scale_x, 0.017607 * scale_y, 0.0),
            (0.102456 * scale_x, 0.05 * scale_y, 0.0),
            (0.017183 * scale_x, -0.017607 * scale_y, 0.0),
            (-0.102456 * scale_x, -0.05 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type2(self, context):
    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.05 * scale_x, 0.0 * scale_y,
            0.0, 0.0 * scale_x, 0.05 * scale_y,
            0.0, 0.05 * scale_x, 0.0 * scale_y,
            0.0, 0.0 * scale_x, -0.05 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.05 * scale_x, -0.047606 * scale_y, 0.0),
            (-0.047606 * scale_x, 0.05 * scale_y, 0.0),
            (0.05 * scale_x, 0.047607 * scale_y, 0.0),
            (0.047606 * scale_x, -0.05 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.05 * scale_x, 0.047607 * scale_y, 0.0),
            (0.047607 * scale_x, 0.05 * scale_y, 0.0),
            (0.05 * scale_x, -0.047607 * scale_y, 0.0),
            (-0.047607 * scale_x, -0.05 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type1(self, context):
    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.05 * scale_x, 0.0 * scale_y,
            0.0, 0.0 * scale_x, 0.05 * scale_y,
            0.0, 0.05 * scale_x, 0.0 * scale_y,
            0.0, 0.0 * scale_x, -0.05 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.05 * scale_x, -0.027606 * scale_y, 0.0),
            (-0.027606 * scale_x, 0.05 * scale_y, 0.0),
            (0.05 * scale_x, 0.027606 * scale_y, 0.0),
            (0.027606 * scale_x, -0.05 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.05 * scale_x, 0.027607 * scale_y, 0.0),
            (0.027607 * scale_x, 0.05 * scale_y, 0.0),
            (0.05 * scale_x, -0.027607 * scale_y, 0.0),
            (-0.027607 * scale_x, -0.05 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def make_path(self, context, verts):
    target = bpy.context.scene.objects.active
    bpy.ops.curve.primitive_nurbs_path_add(
            view_align=False, enter_editmode=False, location=(0, 0, 0)
            )
    target.data.taper_object = bpy.context.scene.objects.active
    taper = bpy.context.scene.objects.active
    taper.name = target.name + '_Taper'
    bpy.context.scene.objects.active = target
    points = taper.data.splines[0].points

    for i in range(len(verts)):
        points[i].co = verts[i]


def make_curve(self, context, verts, lh, rh):
    target = bpy.context.scene.objects.active
    curve_data = bpy.data.curves.new(
                        name=target.name + '_Bevel', type='CURVE'
                        )
    curve_data.dimensions = '3D'

    for p in range(len(verts)):
        c = 0
        spline = curve_data.splines.new(type='BEZIER')
        spline.use_cyclic_u = True
        spline.bezier_points.add(len(verts[p]) / 3 - 1)
        spline.bezier_points.foreach_set('co', verts[p])

        for bp in spline.bezier_points:
            bp.handle_left_type = 'ALIGNED'
            bp.handle_right_type = 'ALIGNED'
            bp.handle_left.xyz = lh[p][c]
            bp.handle_right.xyz = rh[p][c]
            c += 1

    object_data_add(context, curve_data, operator=self)
    target.data.bevel_object = bpy.context.scene.objects.active
    bpy.context.scene.objects.active = target


class add_tapercurve(Operator):
    bl_idname = "curve.tapercurve"
    bl_label = "Add Curve as Taper"
    bl_description = ("Add taper curve to Active Curve\n"
                      "Needs an existing Active Curve")
    bl_options = {'REGISTER', 'UNDO'}

    scale_ends1 = FloatProperty(
            name="End Width Left",
            description="Adjust left end taper",
            default=0.0,
            min=0.0
            )
    scale_ends2 = FloatProperty(
            name="End Width Right",
            description="Adjust right end taper",
            default=0.0,
            min=0.0
            )
    scale_mid = FloatProperty(
            name="Center Width",
            description="Adjust taper at center",
            default=1.0,
            min=0.0
            )
    link1 = BoolProperty(
            name="Link Ends",
            description="Link the End Width Left / Right settings\n"
                        "End Width Left will be editable ",
            default=True
            )
    link2 = BoolProperty(
            name="Link Ends / Center",
            description="Link the End Widths with the Center Width",
            default=False
            )
    diff = FloatProperty(
            name="Difference",
            default=1,
            description="Difference between ends and center while linked"
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return context.mode == 'OBJECT' and obj and obj.type == "CURVE"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label("Settings:")
        split = layout.split(percentage=0.95, align=True)
        split.active = not self.link2
        col = split.column(align=True)
        col.prop(self, "scale_ends1")

        row = split.row(align=True)
        row.scale_y = 2.0
        col_sub = col.column(align=True)
        col_sub.active = not self.link1
        col_sub.prop(self, "scale_ends2")
        row.prop(self, "link1", toggle=True, text="", icon="LINKED")

        split = layout.split(percentage=0.95, align=True)
        col = split.column(align=True)
        col.prop(self, "scale_mid")

        row = split.row(align=True)
        row.scale_y = 2.0
        col_sub = col.column(align=True)
        col_sub.active = self.link2
        row.prop(self, "link2", toggle=True, text="", icon="LINKED")
        col_sub.prop(self, "diff")

    def execute(self, context):
        if self.link1:
            self.scale_ends2 = self.scale_ends1

        if self.link2:
            self.scale_ends2 = self.scale_ends1 = self.scale_mid - self.diff

        add_taper(self, context)

        return {'FINISHED'}


class add_bevelcurve(Operator, AddObjectHelper):
    bl_idname = "curve.bevelcurve"
    bl_label = "Add Curve as Bevel"
    bl_description = ("Add bevel curve to Active Curve\n"
                      "Needs an existing Active Curve")
    bl_options = {'REGISTER', 'UNDO'}

    types = IntProperty(
            name="Type",
            description="Type of bevel curve",
            default=1,
            min=1, max=5
            )
    scale_x = FloatProperty(
            name="Scale X",
            description="Scale on X axis",
            default=1.0
            )
    scale_y = FloatProperty(
            name="Scale Y",
            description="Scale on Y axis",
            default=1.0
            )
    link = BoolProperty(
            name="Link XY",
            description="Link the Scale on X/Y axis",
            default=True
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return context.mode == 'OBJECT' and obj and obj.type == "CURVE"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        # AddObjectHelper props
        col.prop(self, "view_align")
        col.prop(self, "location")
        col.prop(self, "rotation")

        col = layout.column(align=True)
        col.label("Settings:")
        col.prop(self, "types")

        split = layout.split(percentage=0.95, align=True)
        col = split.column(align=True)
        col.prop(self, "scale_x")
        row = split.row(align=True)
        row.scale_y = 2.0
        col.prop(self, "scale_y")
        row.prop(self, "link", toggle=True, text="", icon="LINKED")

    def execute(self, context):
        if self.link:
            self.scale_y = self.scale_x
        if self.types == 1:
            add_type1(self, context)
        if self.types == 2:
            add_type2(self, context)
        if self.types == 3:
            add_type3(self, context)
        if self.types == 4:
            add_type4(self, context)
        if self.types == 5:
            add_type5(self, context)

        return {'FINISHED'}


class Bevel_Taper_Curve_Menu(Menu):
    bl_label = "Bevel/Taper"
    bl_idname = "OBJECT_MT_bevel_taper_curve_menu"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.bevelcurve")
        layout.operator("curve.tapercurve")


def menu_funcs(self, context):
    if bpy.context.scene.objects.active.type == "CURVE":
        self.layout.menu("OBJECT_MT_bevel_taper_curve_menu")


def register():
    bpy.utils.register_module(__name__)
    bpy.types.VIEW3D_MT_object.append(menu_funcs)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.VIEW3D_MT_object.remove(menu_funcs)


if __name__ == "__main__":
    register()
