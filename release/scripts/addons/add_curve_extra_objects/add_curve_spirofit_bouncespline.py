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


bl_info = {
    "name": "SpiroFit, BounceSpline and Catenary",
    "author": "Antonio Osprite, Liero, Atom, Jimmy Hazevoet",
    "version": (0, 2, 1),
    "blender": (2, 78, 0),
    "location": "Toolshelf > Create Tab",
    "description": "SpiroFit, BounceSpline and Catenary adds "
                   "splines to selected mesh or objects",
    "warning": "",
    "wiki_url": "",
    "category": "Object",
    }

import bpy
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        IntProperty,
        StringProperty,
        )
from mathutils import (
        Matrix,
        Vector,
        )
from math import (
        sin, cos,
        pi, sqrt,
        pow, radians
        )
import random as r


# ------------------------------------------------------------
# "Build a spiral that fit the active object"
# Spirofit, original blender 2.45 script by: Antonio Osprite
# http://www.kino3d.com/forum/viewtopic.php?t=5374
# ------------------------------------------------------------
def distance(v1, v2):
    d = (Vector(v1) - Vector(v2)).length
    return d


def spiral_point(step, radius, z_coord, spires, waves, wave_iscale, rndm):
    x = radius * cos(spires * step) + (r.random() - 0.5) * rndm
    y = radius * sin(spires * step) + (r.random() - 0.5) * rndm
    z = z_coord + (cos(waves * step * pi) * wave_iscale) + (r.random() - 0.5) * rndm
    return [x, y, z]


def spirofit_spline(obj,
            spire_resolution=4,
            spires=4,
            offset=0.0,
            waves=0,
            wave_iscale=0.0,
            rndm_spire=0.0,
            direction=False,
            map_method='RAYCAST'
            ):

    points = []
    bb = obj.bound_box
    bb_xmin = min([v[0] for v in bb])
    bb_ymin = min([v[1] for v in bb])
    bb_zmin = min([v[2] for v in bb])
    bb_xmax = max([v[0] for v in bb])
    bb_ymax = max([v[1] for v in bb])
    bb_zmax = max([v[2] for v in bb])

    radius = distance([bb_xmax, bb_ymax, bb_zmin], [bb_xmin, bb_ymin, bb_zmin]) / 2.0
    height = bb_zmax - bb_zmin
    cx = (bb_xmax + bb_xmin) / 2.0
    cy = (bb_ymax + bb_ymin) / 2.0
    steps = spires * spire_resolution

    for i in range(steps + 1):
        t = bb_zmin + (2 * pi / steps) * i
        z = bb_zmin + (float(height) / steps) * i
        if direction:
            t = -t
        cp = spiral_point(t, radius, z, spires, waves, wave_iscale, rndm_spire)

        if map_method == 'RAYCAST':
            success, hit, nor, index = obj.ray_cast(Vector(cp), (Vector([cx, cy, z]) - Vector(cp)))
            if success:
                points.append((hit + offset * nor))

        elif map_method == 'CLOSESTPOINT':
            success, hit, nor, index = obj.closest_point_on_mesh(cp)
            if success:
                points.append((hit + offset * nor))

    return points


class SpiroFitSpline(Operator):
    bl_idname = "object.add_spirofit_spline"
    bl_label = "SpiroFit"
    bl_description = "Wrap selected mesh in a spiral"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    map_method = EnumProperty(
            name="Mapping",
            default='RAYCAST',
            description="Mapping method",
            items=[('RAYCAST', 'Ray cast', 'Ray casting'),
                   ('CLOSESTPOINT', 'Closest point', 'Closest point on mesh')]
            )
    direction = BoolProperty(
            name="Direction",
            description="Spire direction",
            default=False
            )
    spire_resolution = IntProperty(
            name="Spire Resolution",
            default=8,
            min=3,
            max=1024,
            soft_max=128,
            description="Number of steps for one turn"
            )
    spires = IntProperty(
            name="Spires",
            default=4,
            min=1,
            max=1024,
            soft_max=128,
            description="Number of turns"
            )
    offset = FloatProperty(
            name="Offset",
            default=0.0,
            precision=3,
            description="Use normal direction to offset spline"
            )
    waves = IntProperty(
            name="Wave",
            default=0,
            min=0,
            description="Wave amount"
            )
    wave_iscale = FloatProperty(
            name="Wave Intensity",
            default=0.0,
            min=0.0,
            precision=3,
            description="Wave intensity scale"
            )
    rndm_spire = FloatProperty(
            name="Randomise",
            default=0.0,
            min=0.0,
            precision=3,
            description="Randomise spire"
            )
    spline_name = StringProperty(
            name="Name",
            default="SpiroFit"
            )
    spline_type = EnumProperty(
            name="Spline",
            default='BEZIER',
            description="Spline type",
            items=[('POLY', 'Poly', 'Poly spline'),
                   ('BEZIER', 'Bezier', 'Bezier spline')]
            )
    resolution_u = IntProperty(
            name="Resolution U",
            default=12,
            min=0,
            max=64,
            description="Curve resolution u"
            )
    bevel = FloatProperty(
            name="Bevel Radius",
            default=0.0,
            min=0.0,
            precision=3,
            description="Bevel depth"
            )
    bevel_res = IntProperty(
            name="Bevel Resolution",
            default=0,
            min=0,
            max=32,
            description="Bevel resolution"
            )
    extrude = FloatProperty(
            name="Extrude",
            default=0.0,
            min=0.0,
            precision=3,
            description="Extrude amount"
            )
    twist_mode = EnumProperty(
            name="Twisting",
            default='MINIMUM',
            description="Twist method, type of tilt calculation",
            items=[('Z_UP', "Z-Up", 'Z Up'),
                   ('MINIMUM', "Minimum", 'Minimum'),
                   ('TANGENT', "Tangent", 'Tangent')]
            )
    twist_smooth = FloatProperty(
            name="Smooth",
            default=0.0,
            min=0.0,
            precision=3,
            description="Twist smoothing amount for tangents"
            )
    tilt = FloatProperty(
            name="Tilt",
            default=0.0,
            precision=3,
            description="Spline handle tilt"
            )
    random_radius = FloatProperty(
            name="Randomise",
            default=0.0,
            min=0.0,
            precision=3,
            description="Randomise radius of spline controlpoints"
            )
    x_ray = BoolProperty(
            name="X-Ray",
            default=False,
            description="X-Ray - make the object draw in front of others"
            )
    random_seed = IntProperty(
            name="Random Seed",
            default=1,
            min=0,
            description="Random seed number"
            )
    origin_to_start = BoolProperty(
            name="Origin at Start",
            description="Set origin at first point of spline",
            default=False
            )
    refresh = BoolProperty(
            name="Refresh",
            description="Refresh spline",
            default=False
            )
    auto_refresh = BoolProperty(
            name="Auto",
            description="Auto refresh spline",
            default=True
            )

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        row = col.row(align=True)

        if self.auto_refresh is False:
            self.refresh = False
        elif self.auto_refresh is True:
            self.refresh = True

        row.prop(self, "auto_refresh", toggle=True, icon="AUTO", icon_only=True)
        row.prop(self, "refresh", toggle=True, icon="FILE_REFRESH", icon_only=True)
        row.operator("object.add_spirofit_spline", text="Add")
        row.prop(self, "x_ray", toggle=True, icon_only=True, icon="RESTRICT_VIEW_OFF")
        row.prop(self, "origin_to_start", toggle=True, icon="CURVE_DATA", icon_only=True)

        col = layout.column(align=True)
        col.prop(self, "spline_name")
        col.separator()
        col.prop(self, "map_method")
        col.separator()
        col.prop(self, "spire_resolution")
        row = col.row(align=True).split(0.9, align=True)
        row.prop(self, "spires")
        row.prop(self, "direction", toggle=True, text="", icon="ARROW_LEFTRIGHT")
        col.prop(self, "offset")
        col.prop(self, "waves")
        col.prop(self, "wave_iscale")
        col.prop(self, "rndm_spire")
        col.prop(self, "random_seed")
        draw_spline_settings(self)

    @classmethod
    def poll(self, context):
        ob = context.active_object
        return ((ob is not None) and
                (context.mode == 'OBJECT'))

    def invoke(self, context, event):
        self.refresh = True
        return self.execute(context)

    def execute(self, context):
        if not self.refresh:
            return {'PASS_THROUGH'}

        obj = context.active_object
        if obj.type != 'MESH':
            self.report({'WARNING'},
                        "Active Object is not a Mesh. Operation Cancelled")
            return {'CANCELLED'}

        undo = context.user_preferences.edit.use_global_undo
        context.user_preferences.edit.use_global_undo = False

        bpy.ops.object.select_all(action='DESELECT')

        r.seed(self.random_seed)

        points = spirofit_spline(
                obj,
                self.spire_resolution,
                self.spires,
                self.offset,
                self.waves,
                self.wave_iscale,
                self.rndm_spire,
                self.direction,
                self.map_method
                )

        add_curve_object(
                points,
                obj.matrix_world,
                self.spline_name,
                self.spline_type,
                self.resolution_u,
                self.bevel,
                self.bevel_res,
                self.extrude,
                self.random_radius,
                self.twist_mode,
                self.twist_smooth,
                self.tilt,
                self.x_ray
                )

        if self.origin_to_start is True:
            move_origin_to_start()

        if self.auto_refresh is False:
            self.refresh = False

        context.user_preferences.edit.use_global_undo = undo
        return {'FINISHED'}


# ------------------------------------------------------------
# Bounce spline / Fiber mesh
# Original script by Liero and Atom
# https://blenderartists.org/forum/showthread.php?331750-Fiber-Mesh-Emulation
# ------------------------------------------------------------
def noise(var=1):
    rand = Vector((r.gauss(0, 1), r.gauss(0, 1), r.gauss(0, 1)))
    vec = rand.normalized() * var
    return vec


def bounce_spline(obj,
            number=1000,
            ang_noise=0.25,
            offset=0.0,
            extra=50,
            active_face=False
            ):

    dist, points = 1000, []
    poly = obj.data.polygons

    if active_face:
        try:
            n = poly.active
        except:
            print("No active face selected")
            pass
    else:
        n = r.randint(0, len(poly) - 1)

    end = poly[n].normal.copy() * -1
    start = poly[n].center
    points.append(start + offset * end)

    for i in range(number):
        for ray in range(extra + 1):
            end += noise(ang_noise)
            try:
                hit, nor, index = obj.ray_cast(start, end * dist)[-3:]
            except:
                index = -1
            if index != -1:
                start = hit - nor / 10000
                end = end.reflect(nor).normalized()
                points.append(hit + offset * nor)
                break
        if index == -1:
            return points
    return points


class BounceSpline(Operator):
    bl_idname = "object.add_bounce_spline"
    bl_label = "Bounce Spline"
    bl_description = "Fill selected mesh with a spline"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    bounce_number = IntProperty(
            name="Bounces",
            default=1000,
            min=1,
            max=100000,
            soft_max=10000,
            description="Number of bounces"
            )
    ang_noise = FloatProperty(
            name="Angular Noise",
            default=0.25,
            min=0.0,
            precision=3,
            description="Add some noise to ray direction"
            )
    offset = FloatProperty(
            name="Offset",
            default=0.0,
            precision=3,
            description="Use normal direction to offset spline"
            )
    extra = IntProperty(
            name="Extra",
            default=50,
            min=0,
            max=1000,
            description="Number of extra tries if it fails to hit mesh"
            )
    active_face = BoolProperty(
            name="Active Face",
            default=False,
            description="Starts from active face or a random one"
            )
    spline_name = StringProperty(
            name="Name",
            default="BounceSpline"
            )
    spline_type = EnumProperty(
            name="Spline",
            default='BEZIER',
            description="Spline type",
            items=[('POLY', "Poly", "Poly spline"),
                   ('BEZIER', "Bezier", "Bezier spline")]
            )
    resolution_u = IntProperty(
            name="Resolution U",
            default=12,
            min=0,
            max=64,
            description="Curve resolution u"
            )
    bevel = FloatProperty(
            name="Bevel Radius",
            default=0.0,
            min=0.0,
            precision=3,
            description="Bevel depth"
            )
    bevel_res = IntProperty(
            name="Bevel Resolution",
            default=0,
            min=0,
            max=32,
            description="Bevel resolution"
            )
    extrude = FloatProperty(
            name="Extrude",
            default=0.0,
            min=0.0,
            precision=3,
            description="Extrude amount"
            )
    twist_mode = EnumProperty(
            name="Twisting",
            default='MINIMUM',
            description="Twist method, type of tilt calculation",
            items=[('Z_UP', "Z-Up", 'Z Up'),
                   ('MINIMUM', "Minimum", 'Minimum'),
                   ('TANGENT', "Tangent", 'Tangent')]
            )
    twist_smooth = FloatProperty(
            name="Smooth",
            default=0.0,
            min=0.0,
            precision=3,
            description="Twist smoothing amount for tangents"
            )
    tilt = FloatProperty(
            name="Tilt",
            default=0.0,
            precision=3,
            description="Spline handle tilt"
            )
    random_radius = FloatProperty(
            name="Randomise",
            default=0.0,
            min=0.0,
            precision=3,
            description="Randomise radius of spline controlpoints"
            )
    x_ray = BoolProperty(
            name="X-Ray",
            default=False,
            description="X-Ray - make the object draw in front of others"
            )
    random_seed = IntProperty(
            name="Random Seed",
            default=1,
            min=0,
            description="Random seed number"
            )
    origin_to_start = BoolProperty(
            name="Origin at Start",
            description="Set origin at first point of spline",
            default=False
            )
    refresh = BoolProperty(
            name="Refresh",
            description="Refresh spline",
            default=False
            )
    auto_refresh = BoolProperty(
            name="Auto",
            description="Auto refresh spline",
            default=True
            )

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        row = col.row(align=True)
        if self.auto_refresh is False:
            self.refresh = False
        elif self.auto_refresh is True:
            self.refresh = True

        row.prop(self, "auto_refresh", toggle=True, icon="AUTO", icon_only=True)
        row.prop(self, "refresh", toggle=True, icon="FILE_REFRESH", icon_only=True)
        row.operator("object.add_bounce_spline", text="Add")
        row.prop(self, "x_ray", toggle=True, icon_only=True, icon="RESTRICT_VIEW_OFF")
        row.prop(self, "origin_to_start", toggle=True, icon="CURVE_DATA", icon_only=True)

        col = layout.column(align=True)
        col.prop(self, "spline_name")
        col.separator()
        col.prop(self, "bounce_number")
        row = col.row(align=True).split(0.9, align=True)
        row.prop(self, "ang_noise")
        row.prop(self, "active_face", toggle=True, text="", icon="SNAP_FACE")
        col.prop(self, "offset")
        col.prop(self, "extra")
        col.prop(self, "random_seed")
        draw_spline_settings(self)

    @classmethod
    def poll(self, context):
        ob = context.active_object
        return ((ob is not None) and
                (context.mode == 'OBJECT'))

    def invoke(self, context, event):
        self.refresh = True
        return self.execute(context)

    def execute(self, context):
        if not self.refresh:
            return {'PASS_THROUGH'}

        obj = context.active_object
        if obj.type != 'MESH':
            return {'CANCELLED'}

        undo = context.user_preferences.edit.use_global_undo
        context.user_preferences.edit.use_global_undo = False

        bpy.ops.object.select_all(action='DESELECT')

        r.seed(self.random_seed)

        points = bounce_spline(
                obj,
                self.bounce_number,
                self.ang_noise,
                self.offset,
                self.extra,
                self.active_face
                )

        add_curve_object(
                points,
                obj.matrix_world,
                self.spline_name,
                self.spline_type,
                self.resolution_u,
                self.bevel,
                self.bevel_res,
                self.extrude,
                self.random_radius,
                self.twist_mode,
                self.twist_smooth,
                self.tilt,
                self.x_ray
                )

        if self.origin_to_start is True:
            move_origin_to_start()

        if self.auto_refresh is False:
            self.refresh = False

        context.user_preferences.edit.use_global_undo = undo
        return {'FINISHED'}


# ------------------------------------------------------------
# Hang Catenary curve between two selected objects
# ------------------------------------------------------------
def catenary_curve(
            start=[-2, 0, 2],
            end=[2, 0, 2],
            steps=24,
            a=2.0
            ):

    points = []
    lx = end[0] - start[0]
    ly = end[1] - start[1]
    lr = sqrt(pow(lx, 2) + pow(ly, 2))
    lv = lr / 2 - (end[2] - start[2]) * a / lr
    zv = start[2] - pow(lv, 2) / (2 * a)
    slx = lx / steps
    sly = ly / steps
    slr = lr / steps
    i = 0
    while i <= steps:
        x = start[0] + i * slx
        y = start[1] + i * sly
        z = zv + pow((i * slr) - lv, 2) / (2 * a)
        points.append([x, y, z])
        i += 1
    return points


class CatenaryCurve(Operator):
    bl_idname = "object.add_catenary_curve"
    bl_label = "Catenary"
    bl_description = "Hang a curve between two selected objects"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    steps = IntProperty(
            name="Steps",
            description="Resolution of the curve",
            default=24,
            min=2,
            max=1024,
            )
    var_a = FloatProperty(
            name="a",
            description="Catenary variable a",
            precision=3,
            default=2.0,
            min=0.01,
            max=100.0
            )
    spline_name = StringProperty(
            name="Name",
            default="Catenary"
            )
    spline_type = EnumProperty(
            name="Spline",
            default='BEZIER',
            description="Spline type",
            items=[('POLY', "Poly", "Poly spline"),
                   ('BEZIER', "Bezier", "Bezier spline")]
            )
    resolution_u = IntProperty(
            name="Resolution U",
            default=12,
            min=0,
            max=64,
            description="Curve resolution u"
            )
    bevel = FloatProperty(
            name="Bevel Radius",
            default=0.0,
            min=0.0,
            precision=3,
            description="Bevel depth"
            )
    bevel_res = IntProperty(
            name="Bevel Resolution",
            default=0,
            min=0,
            max=32,
            description="Bevel resolution"
            )
    extrude = FloatProperty(
            name="Extrude",
            default=0.0,
            min=0.0,
            precision=3,
            description="Extrude amount"
            )
    twist_mode = EnumProperty(
            name="Twisting",
            default='MINIMUM',
            description="Twist method, type of tilt calculation",
            items=[('Z_UP', "Z-Up", 'Z Up'),
                   ('MINIMUM', "Minimum", "Minimum"),
                   ('TANGENT', "Tangent", "Tangent")]
            )
    twist_smooth = FloatProperty(
            name="Smooth",
            default=0.0,
            min=0.0,
            precision=3,
            description="Twist smoothing amount for tangents"
            )
    tilt = FloatProperty(
            name="Tilt",
            default=0.0,
            precision=3,
            description="Spline handle tilt"
            )
    random_radius = FloatProperty(
            name="Randomise",
            default=0.0,
            min=0.0,
            precision=3,
            description="Randomise radius of spline controlpoints"
            )
    x_ray = BoolProperty(
            name="X-Ray",
            default=False,
            description="X-Ray - make the object draw in front of others"
            )
    random_seed = IntProperty(
            name="Random Seed",
            default=1,
            min=0,
            description="Random seed number"
            )
    origin_to_start = BoolProperty(
            name="Origin at Start",
            description="Set origin at first point of spline",
            default=False
            )
    refresh = BoolProperty(
            name="Refresh",
            description="Refresh spline",
            default=False
            )
    auto_refresh = BoolProperty(
            name="Auto",
            description="Auto refresh spline",
            default=True
            )

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        row = col.row(align=True)

        if self.auto_refresh is False:
            self.refresh = False
        elif self.auto_refresh is True:
            self.refresh = True

        row.prop(self, "auto_refresh", toggle=True, icon="AUTO", icon_only=True)
        row.prop(self, "refresh", toggle=True, icon="FILE_REFRESH", icon_only=True)
        row.operator("object.add_catenary_curve", text="Add")
        row.prop(self, "x_ray", toggle=True, icon_only=True, icon="RESTRICT_VIEW_OFF")
        row.prop(self, "origin_to_start", toggle=True, icon="CURVE_DATA", icon_only=True)

        col = layout.column(align=True)
        col.prop(self, "spline_name")
        col.separator()
        col.prop(self, "steps")
        col.prop(self, "var_a")

        draw_spline_settings(self)
        col = layout.column(align=True)
        col.prop(self, "random_seed")

    @classmethod
    def poll(self, context):
        ob = context.active_object
        return ob is not None

    def invoke(self, context, event):
        self.refresh = True
        return self.execute(context)

    def execute(self, context):
        if not self.refresh:
            return {'PASS_THROUGH'}

        try:
            ob1 = bpy.context.active_object
            ob1.select = False
            ob2 = bpy.context.selected_objects[0]
            start = ob1.location
            end = ob2.location
            if (start[0] == end[0]) and (start[1] == end[1]):
                self.report({"WARNING"},
                            "Objects have the same X, Y location. Operation Cancelled")

                return {'CANCELLED'}
        except:
            self.report({"WARNING"},
                        "Catenary could not be completed. Operation Cancelled")
            return {'CANCELLED'}

        bpy.ops.object.select_all(action='DESELECT')

        undo = context.user_preferences.edit.use_global_undo
        context.user_preferences.edit.use_global_undo = False

        r.seed(self.random_seed)

        points = catenary_curve(
                start,
                end,
                self.steps,
                self.var_a
                )
        add_curve_object(
                points,
                Matrix(),
                self.spline_name,
                self.spline_type,
                self.resolution_u,
                self.bevel,
                self.bevel_res,
                self.extrude,
                self.random_radius,
                self.twist_mode,
                self.twist_smooth,
                self.tilt,
                self.x_ray
                )

        if self.origin_to_start is True:
            move_origin_to_start()
        else:
            bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')

        if self.auto_refresh is False:
            self.refresh = False

        context.user_preferences.edit.use_global_undo = undo
        return {'FINISHED'}


# ------------------------------------------------------------
# Generate curve object from given points
# ------------------------------------------------------------
def add_curve_object(
            verts,
            matrix,
            spline_name="Spline",
            spline_type='BEZIER',
            resolution_u=12,
            bevel=0.0,
            bevel_resolution=0,
            extrude=0.0,
            spline_radius=0.0,
            twist_mode='MINIMUM',
            twist_smooth=0.0,
            tilt=0.0,
            x_ray=False
            ):

    curve = bpy.data.curves.new(spline_name, 'CURVE')
    curve.dimensions = '3D'
    spline = curve.splines.new(spline_type)
    cur = bpy.data.objects.new(spline_name, curve)

    spline.radius_interpolation = 'BSPLINE'
    spline.tilt_interpolation = 'BSPLINE'

    if spline_type == 'BEZIER':
        spline.bezier_points.add(int(len(verts) - 1))
        for i in range(len(verts)):
            spline.bezier_points[i].co = verts[i]
            spline.bezier_points[i].handle_right_type = 'AUTO'
            spline.bezier_points[i].handle_left_type = 'AUTO'
            spline.bezier_points[i].radius += spline_radius * r.random()
            spline.bezier_points[i].tilt = radians(tilt)
    else:
        spline.points.add(int(len(verts) - 1))
        for i in range(len(verts)):
            spline.points[i].co = verts[i][0], verts[i][1], verts[i][2], 1

    bpy.context.scene.objects.link(cur)
    cur.data.use_uv_as_generated = True
    cur.data.resolution_u = resolution_u
    cur.data.fill_mode = 'FULL'
    cur.data.bevel_depth = bevel
    cur.data.bevel_resolution = bevel_resolution
    cur.data.extrude = extrude
    cur.data.twist_mode = twist_mode
    cur.data.twist_smooth = twist_smooth
    cur.matrix_world = matrix
    bpy.context.scene.objects.active = cur
    cur.select = True
    if x_ray is True:
        cur.show_x_ray = x_ray
    return


def move_origin_to_start():
    active = bpy.context.active_object
    spline = active.data.splines[0]
    if spline.type == 'BEZIER':
        start = active.matrix_world * spline.bezier_points[0].co
    else:
        start = active.matrix_world * spline.points[0].co
        start = start[:-1]
    cursor = bpy.context.scene.cursor_location.copy()
    bpy.context.scene.cursor_location = start
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.context.scene.cursor_location = cursor


def draw_spline_settings(self):
    layout = self.layout
    col = layout.column(align=True)

    col.prop(self, "spline_type")
    col.separator()
    col.prop(self, "resolution_u")
    col.prop(self, "bevel")
    col.prop(self, "bevel_res")
    col.prop(self, "extrude")

    if self.spline_type == 'BEZIER':
        col.prop(self, "random_radius")
    col.separator()
    col.prop(self, "twist_mode")
    col.separator()

    if self.twist_mode == 'TANGENT':
        col.prop(self, "twist_smooth")

    if self.spline_type == 'BEZIER':
        col.prop(self, "tilt")


# ------------------------------------------------------------
# Tools Panel > Create
# ------------------------------------------------------------
class SplinePanel(Panel):
    bl_space_type = "VIEW_3D"
    bl_context = "objectmode"
    bl_region_type = "TOOLS"
    bl_label = "Spline"
    bl_category = "Create"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        col = self.layout.column(align=True)
        col.operator(SpiroFitSpline.bl_idname, icon="FORCE_MAGNETIC")
        col.operator(BounceSpline.bl_idname, icon="FORCE_HARMONIC")
        col.operator(CatenaryCurve.bl_idname, icon="FORCE_CURVE")


# ------------------------------------------------------------
# Register
# ------------------------------------------------------------
def register():
    bpy.utils.register_class(SplinePanel)
    bpy.utils.register_class(SpiroFitSpline)
    bpy.utils.register_class(BounceSpline)
    bpy.utils.register_class(CatenaryCurve)


def unregister():
    bpy.utils.unregister_class(SplinePanel)
    bpy.utils.unregister_class(SpiroFitSpline)
    bpy.utils.unregister_class(BounceSpline)
    bpy.utils.unregister_class(CatenaryCurve)


if __name__ == "__main__":
    register()
