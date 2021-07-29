# gpl: author Cmomoney

# DevBo Task https://developer.blender.org/T37299

bl_info = {
    "name": "Curly Curves",
    "author": "Cmomoney",
    "version": (1, 1, 8),
    "blender": (2, 69, 0),
    "location": "View3D > Add > Curve > Curly Curve",
    "description": "Adds a new Curly Curve",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Curve/Curly_Curves",
    "category": "Add Curve"}

import bpy
from bpy.types import Operator
from bpy.props import (
        FloatProperty,
        IntProperty,
        )
from bpy_extras.object_utils import (
        AddObjectHelper,
        object_data_add,
        )


def add_type6(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [0.047131 * scale_x, 0.065832 * scale_y,
            0.0, 0.010396 * scale_x, -0.186771 * scale_y,
            0.0, 0.076107 * scale_x, 0.19414 * scale_y,
            0.0, 0.0 * scale_x, -1.0 * scale_y, 0.0],
            [0.451396 * scale_x, -0.48376 * scale_y,
            0.0, 0.433623 * scale_x, -0.587557 * scale_y,
            0.0, 0.525837 * scale_x, -0.423363 * scale_y,
            0.0, 0.15115 * scale_x, -0.704345 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.067558 * scale_x, 0.078418 * scale_y, 0.0),
            (0.168759 * scale_x, -0.154334 * scale_y, 0.0),
            (-0.236823 * scale_x, 0.262436 * scale_y, 0.0),
            (0.233116 * scale_x, -0.596115 * scale_y, 0.0)],
            [(0.498001 * scale_x, -0.493434 * scale_y, 0.0),
            (0.375618 * scale_x, -0.55465 * scale_y, 0.0),
            (0.634373 * scale_x, -0.49873 * scale_y, 0.0),
            (0.225277 * scale_x, -0.526814 * scale_y, 0.0)]
            ]
    rhandles = [
            [(0.161825 * scale_x, 0.053245 * scale_y, 0.0),
            (-0.262003 * scale_x, -0.242566 * scale_y, 0.0),
            (0.519691 * scale_x, 0.097329 * scale_y, 0.0),
            (-0.233116 * scale_x, -1.403885 * scale_y, 0.0)],
            [(0.404788 * scale_x, -0.474085 * scale_y, 0.0),
            (0.533397 * scale_x, -0.644158 * scale_y, 0.0),
            (0.371983 * scale_x, -0.316529 * scale_y, 0.0),
            (0.077022 * scale_x, -0.881876 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type5(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [0.047131 * scale_x, 0.065832 * scale_y, 0.0,
            0.010396 * scale_x, -0.186771 * scale_y, 0.0,
            0.076107 * scale_x, 0.19414 * scale_y, 0.0,
            0.0 * scale_x, -1.0 * scale_y, 0.0],
            [0.086336 * scale_x, -0.377611 * scale_y, 0.0,
            0.022417 * scale_x, -0.461301 * scale_y, 0.0,
            0.079885 * scale_x, -0.281968 * scale_y, 0.0,
            0.129212 * scale_x, -0.747702 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.067558 * scale_x, 0.078419 * scale_y, 0.0),
            (0.168759 * scale_x, -0.154335 * scale_y, 0.0),
            (-0.236823 * scale_x, 0.262436 * scale_y, 0.0),
            (0.233116 * scale_x, -0.596115 * scale_y, 0.0)],
            [(0.047518 * scale_x, -0.350065 * scale_y, 0.0),
            (0.086012 * scale_x, -0.481379 * scale_y, 0.0),
            (-0.049213 * scale_x, -0.253793 * scale_y, 0.0),
            (0.208763 * scale_x, -0.572534 * scale_y, 0.0)]
            ]
    rhandles = [
            [(0.161825 * scale_x, 0.053245 * scale_y, 0.0),
            (-0.262003 * scale_x, -0.242566 * scale_y, 0.0),
            (0.519691 * scale_x, 0.097329 * scale_y, 0.0),
            (-0.233116 * scale_x, -1.403885 * scale_y, 0.0)],
            [(0.125156 * scale_x, -0.405159 * scale_y, 0.0),
            (-0.086972 * scale_x, -0.426766 * scale_y, 0.0),
            (0.262886 * scale_x, -0.321908 * scale_y, 0.0),
            (0.049661 * scale_x, -0.92287 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type8(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.850431 * scale_x, -0.009091 * scale_y,
            0.0, -0.818807 * scale_x, -0.130518 * scale_y,
            0.0, -0.944931 * scale_x, 0.055065 * scale_y,
            0.0, -0.393355 * scale_x, -0.035521 * scale_y,
            0.0, 0.0 * scale_x, 0.348298 * scale_y,
            0.0, 0.393355 * scale_x, -0.035521 * scale_y,
            0.0, 0.978373 * scale_x, 0.185638 * scale_y,
            0.0, 0.771617 * scale_x, 0.272819 * scale_y,
            0.0, 0.864179 * scale_x, 0.188103 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.90478 * scale_x, -0.025302 * scale_y, 0.0),
            (-0.753279 * scale_x, -0.085571 * scale_y, 0.0),
            (-1.06406 * scale_x, -0.047879 * scale_y, 0.0),
            (-0.622217 * scale_x, -0.022501 * scale_y, 0.0),
            (0.181 * scale_x, 0.34879 * scale_y, 0.0),
            (-0.101464 * scale_x, -0.063669 * scale_y, 0.0),
            (0.933064 * scale_x, 0.03001 * scale_y, 0.0),
            (0.82418 * scale_x, 0.39899 * scale_y, 0.0),
            (0.827377 * scale_x, 0.144945 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.796079 * scale_x, 0.007121 * scale_y, 0.0),
            (-0.931521 * scale_x, -0.207832 * scale_y, 0.0),
            (-0.822288 * scale_x, 0.161045 * scale_y, 0.0),
            (0.101464 * scale_x, -0.063671 * scale_y, 0.0),
            (-0.181193 * scale_x, 0.347805 * scale_y, 0.0),
            (0.622217 * scale_x, -0.022502 * scale_y, 0.0),
            (1.022383 * scale_x, 0.336808 * scale_y, 0.0),
            (0.741059 * scale_x, 0.199468 * scale_y, 0.0),
            (0.900979 * scale_x, 0.231258 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type3(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.78652 * scale_x, -0.070157 * scale_y,
            0.0, -0.697972 * scale_x, -0.247246 * scale_y,
            0.0, -0.953385 * scale_x, -0.002048 * scale_y,
            0.0, 0.0 * scale_x, 0.0 * scale_y,
            0.0, 0.917448 * scale_x, 0.065788 * scale_y,
            0.0, 0.448535 * scale_x, 0.515947 * scale_y,
            0.0, 0.6111 * scale_x, 0.190831 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.86511 * scale_x, -0.112965 * scale_y, 0.0),
            (-0.61153 * scale_x, -0.156423 * scale_y, 0.0),
            (-1.103589 * scale_x, -0.199934 * scale_y, 0.0),
            (-0.446315 * scale_x, 0.135163 * scale_y, 0.0),
            (0.669383 * scale_x, -0.254463 * scale_y, 0.0),
            (0.721512 * scale_x, 0.802759 * scale_y, 0.0),
            (0.466815 * scale_x, 0.112232 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.707927 * scale_x, -0.027348 * scale_y, 0.0),
            (-0.846662 * scale_x, -0.40347 * scale_y, 0.0),
            (-0.79875 * scale_x, 0.201677 * scale_y, 0.0),
            (0.446315 * scale_x, -0.135163 * scale_y, 0.0),
            (1.196752 * scale_x, 0.42637 * scale_y, 0.0),
            (0.289834 * scale_x, 0.349204 * scale_y, 0.0),
            (0.755381 * scale_x, 0.269428 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type2(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.719632 * scale_x, -0.08781 * scale_y,
            0.0, -0.605138 * scale_x, -0.31612 * scale_y,
            0.0, -0.935392 * scale_x, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.935392 * scale_x, 0.0, 0.0, 0.605138 * scale_x,
            -0.316119 * scale_y, 0.0, 0.719632 * scale_x, -0.08781 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.82125 * scale_x, -0.142999 * scale_y, 0.0),
            (-0.493366 * scale_x, -0.199027 * scale_y, 0.0),
            (-1.129601 * scale_x, -0.25513 * scale_y, 0.0),
            (-0.467584 * scale_x, 0.00044 * scale_y, 0.0),
            (0.735439 * scale_x, 0.262646 * scale_y, 0.0),
            (0.797395 * scale_x, -0.517531 * scale_y, 0.0),
            (0.618012 * scale_x, -0.032614 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.618009 * scale_x, -0.032618 * scale_y, 0.0),
            (-0.797396 * scale_x, -0.517532 * scale_y, 0.0),
            (-0.735445 * scale_x, 0.262669 * scale_y, 0.0),
            (0.468041 * scale_x, -0.00044 * scale_y, 0.0),
            (1.129616 * scale_x, -0.255119 * scale_y, 0.0),
            (0.493365 * scale_x, -0.199025 * scale_y, 0.0),
            (0.821249 * scale_x, -0.143004 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type10(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.999637 * scale_x, 0.000348 * scale_y,
            0.0, 0.259532 * scale_x, -0.017841 * scale_y,
            0.0, 0.482303 * scale_x, 0.780429 * scale_y,
            0.0, 0.573183 * scale_x, 0.506898 * scale_y, 0.0],
            [0.259532 * scale_x, -0.017841 * scale_y,
            0.0, 0.554919 * scale_x, -0.140918 * scale_y,
            0.0, 0.752264 * scale_x, -0.819275 * scale_y,
            0.0, 0.824152 * scale_x, -0.514881 * scale_y, 0.0]
            ]
    lhandles = [
            [(-1.258333 * scale_x, -0.258348 * scale_y, 0.0),
            (-0.240006 * scale_x, -0.15259 * scale_y, 0.0),
            (0.79037 * scale_x, 0.857575 * scale_y, 0.0),
            (0.376782 * scale_x, 0.430157 * scale_y, 0.0)],
            [(0.224917 * scale_x, -0.010936 * scale_y, 0.0),
            (0.514858 * scale_x, -0.122809 * scale_y, 0.0),
            (1.057957 * scale_x, -0.886925 * scale_y, 0.0),
            (0.61945 * scale_x, -0.464285 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.74094 * scale_x, 0.259045 * scale_y, 0.0),
            (0.768844 * scale_x, 0.119545 * scale_y, 0.0),
            (0.279083 * scale_x, 0.729538 * scale_y, 0.0),
            (0.643716 * scale_x, 0.534458 * scale_y, 0.0)],
            [(0.294147 * scale_x, -0.024746 * scale_y, 0.0),
            (1.03646 * scale_x, -0.358598 * scale_y, 0.0),
            (0.547718 * scale_x, -0.774008 * scale_y, 0.0),
            (0.897665 * scale_x, -0.533051 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type9(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [0.260968 * scale_x, -0.668118 * scale_y,
            0.0, 0.108848 * scale_x, -0.381587 * scale_y,
            0.0, 0.537002 * scale_x, -0.77303 * scale_y,
            0.0, -0.600421 * scale_x, -0.583106 * scale_y,
            0.0, -0.600412 * scale_x, 0.583103 * scale_y,
            0.0, 0.537002 * scale_x, 0.773025 * scale_y,
            0.0, 0.108854 * scale_x, 0.381603 * scale_y,
            0.0, 0.260966 * scale_x, 0.668129 * scale_y, 0.0]
            ]
    lhandles = [
            [(0.387973 * scale_x, -0.594856 * scale_y, 0.0),
            (-0.027835 * scale_x, -0.532386 * scale_y, 0.0),
            (0.775133 * scale_x, -0.442883 * scale_y, 0.0),
            (-0.291333 * scale_x, -1.064385 * scale_y, 0.0),
            (-0.833382 * scale_x, 0.220321 * scale_y, 0.0),
            (0.291856 * scale_x, 1.112891 * scale_y, 0.0),
            (0.346161 * scale_x, 0.119777 * scale_y, 0.0),
            (0.133943 * scale_x, 0.741389 * scale_y, 0.0)]
            ]
    rhandles = [
            [(0.133951 * scale_x, -0.741386 * scale_y, 0.0),
            (0.346154 * scale_x, -0.119772 * scale_y, 0.0),
            (0.291863 * scale_x, -1.112896 * scale_y, 0.0),
            (-0.833407 * scale_x, -0.220324 * scale_y, 0.0),
            (-0.29134 * scale_x, 1.064389 * scale_y, 0.0),
            (0.775125 * scale_x, 0.442895 * scale_y, 0.0),
            (-0.029107 * scale_x, 0.533819 * scale_y, 0.0),
            (0.387981 * scale_x, 0.594873 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type7(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.850431 * scale_x, -0.009091 * scale_y,
            0.0, -0.818807 * scale_x, -0.130518 * scale_y,
            0.0, -0.944931 * scale_x, 0.055065 * scale_y, 0.0,
            -0.393355 * scale_x, -0.035521 * scale_y,
            0.0, 0.0 * scale_x, 0.348298 * scale_y,
            0.0, 0.393355 * scale_x, -0.035521 * scale_y,
            0.0, 0.944931 * scale_x, 0.055065 * scale_y,
            0.0, 0.818807 * scale_x, -0.130518 * scale_y,
            0.0, 0.850431 * scale_x, -0.009091 * scale_y, 0.0]
            ]
    lhandles = [
            [(-0.90478 * scale_x, -0.025302 * scale_y, 0.0),
            (-0.753279 * scale_x, -0.085571 * scale_y, 0.0),
            (-1.06406 * scale_x, -0.047879 * scale_y, 0.0),
            (-0.622217 * scale_x, -0.022502 * scale_y, 0.0),
            (0.181 * scale_x, 0.348791 * scale_y, 0.0),
            (-0.101464 * scale_x, -0.063671 * scale_y, 0.0),
            (0.822288 * scale_x, 0.161045 * scale_y, 0.0),
            (0.931521 * scale_x, -0.207832 * scale_y, 0.0),
            (0.796079 * scale_x, 0.007121 * scale_y, 0.0)]
            ]
    rhandles = [
            [(-0.796079 * scale_x, 0.007121 * scale_y, 0.0),
            (-0.931521 * scale_x, -0.207832 * scale_y, 0.0),
            (-0.822288 * scale_x, 0.161045 * scale_y, 0.0),
            (0.101464 * scale_x, -0.063671 * scale_y, 0.0),
            (-0.181193 * scale_x, 0.347805 * scale_y, 0.0),
            (0.622217 * scale_x, -0.022502 * scale_y, 0.0),
            (1.06406 * scale_x, -0.047879 * scale_y, 0.0),
            (0.753279 * scale_x, -0.085571 * scale_y, 0.0),
            (0.90478 * scale_x, -0.025302 * scale_y, 0.0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type4(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [0.072838 * scale_x, -0.071461 * scale_y,
            0.0, -0.175451 * scale_x, -0.130711 * scale_y,
            0.0, 0.207269 * scale_x, 0.118064 * scale_y,
            0.0, 0 * scale_x, -1.0 * scale_y, 0.0]
            ]
    lhandles = [
            [(0.042135 * scale_x, 0.039756 * scale_y, 0),
            (-0.086769 * scale_x, -0.265864 * scale_y, 0),
            (0.002865 * scale_x, 0.364657 * scale_y, 0),
            (0.233116 * scale_x, -0.596115 * scale_y, 0)]
            ]
    rhandles = [
            [(0.103542 * scale_x, -0.182683 * scale_y, 0),
            (-0.327993 * scale_x, 0.101765 * scale_y, 0),
            (0.417702 * scale_x, -0.135803 * scale_y, 0),
            (-0.233116 * scale_x, -1.403885 * scale_y, 0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def add_type1(self, context):

    scale_x = self.scale_x
    scale_y = self.scale_y
    verts = [
            [-0.71753 * scale_x, -0.08781 * scale_y,
            0, -0.60337 * scale_x, -0.31612 * scale_y, 0,
            -0.93266 * scale_x, 0, 0, 0, 0, 0, 0.93266 * scale_x,
            0, 0, 0.60337 * scale_x, 0.31612 * scale_y,
            0, 0.71753 * scale_x, 0.08781 * scale_y, 0]
            ]
    lhandles = [
            [(-0.81885 * scale_x, -0.143002 * scale_y, 0),
            (-0.491926 * scale_x, -0.199026 * scale_y, 0),
            (-1.126316 * scale_x, -0.255119 * scale_y, 0),
            (-0.446315 * scale_x, 0.135164 * scale_y, 0),
            (0.733297 * scale_x, -0.26265 * scale_y, 0),
            (0.795065 * scale_x, 0.517532 * scale_y, 0),
            (0.616204 * scale_x, 0.03262 * scale_y, 0)]
            ]
    rhandles = [
            [(-0.616204 * scale_x, -0.032618 * scale_y, 0),
            (-0.795067 * scale_x, -0.517532 * scale_y, 0),
            (-0.733297 * scale_x, 0.262651 * scale_y, 0),
            (0.446315 * scale_x, -0.135163 * scale_y, 0),
            (1.126316 * scale_x, 0.255119 * scale_y, 0),
            (0.491924 * scale_x, 0.199026 * scale_y, 0),
            (0.81885 * scale_x, 0.143004 * scale_y, 0)]
            ]
    make_curve(self, context, verts, lhandles, rhandles)


def make_curve(self, context, verts, lh, rh):

    types = self.types
    curve_data = bpy.data.curves.new(name='CurlyCurve', type='CURVE')
    curve_data.dimensions = '3D'

    for p in range(len(verts)):
        c = 0
        spline = curve_data.splines.new(type='BEZIER')
        spline.bezier_points.add(len(verts[p]) / 3 - 1)
        spline.bezier_points.foreach_set('co', verts[p])

        for bp in spline.bezier_points:
            bp.handle_left_type = 'ALIGNED'
            bp.handle_right_type = 'ALIGNED'
            bp.handle_left.xyz = lh[p][c]
            bp.handle_right.xyz = rh[p][c]
            c += 1
        # something weird with this one
        if types == 1 or types == 2 or types == 3:
            spline.bezier_points[3].handle_left.xyz = lh[p][3]
    object_data_add(context, curve_data, operator=self)


class add_curlycurve(Operator, AddObjectHelper):
    bl_idname = "curve.curlycurve"
    bl_label = "Add Curly Curve"
    bl_description = "Create a Curly Curve"
    bl_options = {'REGISTER', 'UNDO'}

    types = IntProperty(
            name="Type",
            description="Type of curly curve",
            default=1,
            min=1, max=10
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

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        # AddObjectHelper props
        col.prop(self, "view_align")
        col.prop(self, "location")
        col.prop(self, "rotation")

        col = layout.column()
        col.label("Curve:")
        col.prop(self, "types")

        col = layout.column(align=True)
        col.label("Resize:")
        col.prop(self, "scale_x")
        col.prop(self, "scale_y")

    def execute(self, context):
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
        if self.types == 6:
            add_type6(self, context)
        if self.types == 7:
            add_type7(self, context)
        if self.types == 8:
            add_type8(self, context)
        if self.types == 9:
            add_type9(self, context)
        if self.types == 10:
            add_type10(self, context)

        return {'FINISHED'}


# Registration

def add_curlycurve_button(self, context):
    self.layout.operator(
            add_curlycurve.bl_idname,
            text="Add Curly Curve",
            icon='PLUGIN'
            )


def register():
    bpy.utils.register_class(add_curlycurve)
    bpy.types.INFO_MT_curve_add.append(add_curlycurve_button)


def unregister():
    bpy.utils.unregister_class(add_curlycurve)
    bpy.types.INFO_MT_curve_add.remove(add_curlycurve_button)


if __name__ == "__main__":
    register()
