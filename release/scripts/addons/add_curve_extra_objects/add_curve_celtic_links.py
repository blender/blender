# Blender plugin for generating celtic knot curves from 3d meshes
#
# The MIT License (MIT)
#
# Copyright (c) 2013 Adam Newgas
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

bl_info = {
    "name": "Celtic Knot",
    "description": "",
    "author": "Adam Newgas",
    "version": (0, 1, 2),
    "blender": (2, 74, 0),
    "location": "View3D > Add > Curve",
    "warning": "",
    "wiki_url": "https://github.com/BorisTheBrave/celtic-knot/wiki",
    "category": "Add Curve"}

import bpy
import bmesh
from bpy.types import Operator
from bpy.props import (
        EnumProperty,
        FloatProperty,
        )
from collections import defaultdict
from math import (
        pi, sin,
        cos,
        )


class CelticKnotOperator(Operator):
    bl_idname = "curve.celtic_links"
    bl_label = "Celtic Links"
    bl_description = "Select a low poly Mesh Object to cover with Knitted Links"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    weave_up = FloatProperty(
            name="Weave Up",
            description="Distance to shift curve upwards over knots",
            subtype="DISTANCE",
            unit="LENGTH"
            )
    weave_down = FloatProperty(
            name="Weave Down",
            description="Distance to shift curve downward under knots",
            subtype="DISTANCE",
            unit="LENGTH"
            )
    handle_types = [
            ('ALIGNED', "Aligned", "Points at a fixed crossing angle"),
            ('AUTO', "Auto", "Automatic control points")
            ]
    handle_type = EnumProperty(
            items=handle_types,
            name="Handle Type",
            description="Controls what type the bezier control points use",
            default='AUTO'
            )

    handle_type_map = {"AUTO": "AUTOMATIC", "ALIGNED": "ALIGNED"}

    crossing_angle = FloatProperty(
            name="Crossing Angle",
            description="Aligned only: the angle between curves in a knot",
            default=pi / 4,
            min=0, max=pi / 2,
            subtype="ANGLE",
            unit="ROTATION"
            )
    crossing_strength = FloatProperty(
            name="Crossing Strength",
            description="Aligned only: strenth of bezier control points",
            soft_min=0,
            subtype="DISTANCE",
            unit="LENGTH"
            )
    geo_bDepth = FloatProperty(
            name="Bevel Depth",
            default=0.04,
            min=0, soft_min=0,
            description="Bevel Depth",
            )

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ((ob is not None) and (ob.mode == "OBJECT") and
                (ob.type == "MESH") and (context.mode == "OBJECT"))

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "handle_type")

        col = layout.column(align=True)
        col.prop(self, "weave_up")
        col.prop(self, "weave_down")

        col = layout.column(align=True)
        col.active = False if self.handle_type == 'AUTO' else True
        col.prop(self, "crossing_angle")
        col.prop(self, "crossing_strength")

        layout.prop(self, "geo_bDepth")

    def execute(self, context):
        # Cache some values
        s = sin(self.crossing_angle) * self.crossing_strength
        c = cos(self.crossing_angle) * self.crossing_strength
        handle_type = self.handle_type
        weave_up = self.weave_up
        weave_down = self.weave_down

        # Create the new object
        orig_obj = obj = context.active_object
        curve = bpy.data.curves.new("Celtic", "CURVE")
        curve.dimensions = "3D"
        curve.twist_mode = "MINIMUM"
        curve.fill_mode = "FULL"
        curve.bevel_depth = 0.015
        curve.extrude = 0.003
        curve.bevel_resolution = 4
        obj = obj.data
        midpoints = []

        # Compute all the midpoints of each edge
        for e in obj.edges.values():
            v1 = obj.vertices[e.vertices[0]]
            v2 = obj.vertices[e.vertices[1]]
            m = (v1.co + v2.co) / 2.0
            midpoints.append(m)

        bm = bmesh.new()
        bm.from_mesh(obj)
        # Stores which loops the curve has already passed through
        loops_entered = defaultdict(lambda: False)
        loops_exited = defaultdict(lambda: False)
        # Loops on the boundary of a surface

        def ignorable_loop(loop):
            return len(loop.link_loops) == 0

        # Starting at loop, build a curve one vertex at a time
        # until we start where we came from
        # Forward means that for any two edges the loop crosses
        # sharing a face, it is passing through in clockwise order
        # else anticlockwise

        def make_loop(loop, forward):
            current_spline = curve.splines.new("BEZIER")
            current_spline.use_cyclic_u = True
            first = True
            # Data for the spline
            # It's faster to store in an array and load into blender
            # at once
            cos = []
            handle_lefts = []
            handle_rights = []
            while True:
                if forward:
                    if loops_exited[loop]:
                        break
                    loops_exited[loop] = True
                    # Follow the face around, ignoring boundary edges
                    while True:
                        loop = loop.link_loop_next
                        if not ignorable_loop(loop):
                            break
                    assert loops_entered[loop] is False
                    loops_entered[loop] = True
                    v = loop.vert.index
                    prev_loop = loop
                    # Find next radial loop
                    assert loop.link_loops[0] != loop
                    loop = loop.link_loops[0]
                    forward = loop.vert.index == v
                else:
                    if loops_entered[loop]:
                        break
                    loops_entered[loop] = True
                    # Follow the face around, ignoring boundary edges
                    while True:
                        v = loop.vert.index
                        loop = loop.link_loop_prev
                        if not ignorable_loop(loop):
                            break
                    assert loops_exited[loop] is False
                    loops_exited[loop] = True
                    prev_loop = loop
                    # Find next radial loop
                    assert loop.link_loops[-1] != loop
                    loop = loop.link_loops[-1]
                    forward = loop.vert.index == v

                if not first:
                    current_spline.bezier_points.add()
                first = False
                midpoint = midpoints[loop.edge.index]
                normal = loop.calc_normal() + prev_loop.calc_normal()
                normal.normalize()
                offset = weave_up if forward else weave_down
                midpoint = midpoint + offset * normal
                cos.extend(midpoint)

                if handle_type != "AUTO":
                    tangent = loop.link_loop_next.vert.co - loop.vert.co
                    tangent.normalize()
                    binormal = normal.cross(tangent).normalized()
                    if not forward:
                        tangent *= -1
                    s_binormal = s * binormal
                    c_tangent = c * tangent
                    handle_left = midpoint - s_binormal - c_tangent
                    handle_right = midpoint + s_binormal + c_tangent
                    handle_lefts.extend(handle_left)
                    handle_rights.extend(handle_right)

            points = current_spline.bezier_points
            points.foreach_set("co", cos)
            if handle_type != "AUTO":
                points.foreach_set("handle_left", handle_lefts)
                points.foreach_set("handle_right", handle_rights)

        # Attempt to start a loop at each untouched loop in the entire mesh
        for face in bm.faces:
            for loop in face.loops:
                if ignorable_loop(loop):
                    continue
                if not loops_exited[loop]:
                    make_loop(loop, True)
                if not loops_entered[loop]:
                    make_loop(loop, False)

        # Create an object from the curve
        from bpy_extras import object_utils
        object_utils.object_data_add(context, curve, operator=None)
        # Set the handle type (this is faster than setting it pointwise)
        bpy.ops.object.editmode_toggle()
        bpy.ops.curve.select_all(action="SELECT")
        bpy.ops.curve.handle_type_set(type=self.handle_type_map[handle_type])
        # Some blender versions lack the default
        bpy.ops.curve.radius_set(radius=1.0)
        bpy.ops.object.editmode_toggle()
        # Restore active selection
        curve_obj = context.active_object

        # apply the bevel setting since it was unused
        try:
            curve_obj.data.bevel_depth = self.geo_bDepth
        except:
            pass
        context.scene.objects.active = orig_obj

        return {'FINISHED'}


def register():
    bpy.utils.register_class(CelticKnotOperator)


def unregister():
    bpy.utils.unregister_class(CelticKnotOperator)


if __name__ == "__main__":
    register()
