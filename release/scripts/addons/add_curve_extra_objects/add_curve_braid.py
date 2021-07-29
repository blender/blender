# gpl: author Jared Forsyth <github.com/jaredly>

"""
bl_info = {
    "name": "New Braid",
    "author": "Jared Forsyth <github.com/jaredly>",
    "version": (1, 0, 2),
    "blender": (2, 6, 0),
    "location": "View3D > Add > Mesh > New Braid",
    "description": "Adds a new Braid",
    "warning": "",
    "wiki_url": "",
    "category": "Add Mesh"}
"""

import bpy
from bpy.props import (
        FloatProperty,
        IntProperty,
        BoolProperty,
        )
from bpy.types import Operator
from math import (
        sin, cos,
        pi,
        )


def angle_point(center, angle, distance):
    cx, cy = center
    x = cos(angle) * distance
    y = sin(angle) * distance
    return x + cx, y + cy


def flat_hump(strands, mx=1, my=1, mz=1, resolution=2):
    num = 4 * resolution
    dy = 2 * pi / num
    dz = 2 * pi * (strands - 1) / num
    for i in range(num):
        x = i * mx
        y = cos(i * dy) * my
        z = sin(i * dz) * mz

        yield x, y, z


def circle_hump(pos, strands, humps, radius=1, mr=1, mz=.2, resolution=2):
    num = 5 * resolution
    dt = 2 * pi / humps * strands / num
    dr = 2 * pi * (strands - 1) / num
    dz = 2 * pi / num
    t0 = 2 * pi / humps * pos

    for i in range(num):
        x, y = angle_point((0, 0), i * dt + t0, radius + sin(i * dr) * mr)
        z = cos(i * dz) * mz

        yield x, y, z


def make_strands(strands, humps, radius=1, mr=1, mz=.2, resolution=2):
    positions = [0 for x in range(humps)]
    last = None
    lines = []
    at = 0

    while 0 in positions:
        if positions[at]:
            at = positions.index(0)
            last = None
        hump = list(circle_hump(at, strands, humps, radius, mr, mz, resolution))
        if last is None:
            last = hump
            lines.append(last)
        else:
            last.extend(hump)
        positions[at] = 1
        at += strands
        at %= humps

    return lines


def poly_line(curve, points, join=True, type='NURBS'):
    polyline = curve.splines.new(type)
    polyline.points.add(len(points) - 1)
    for num in range(len(points)):
        polyline.points[num].co = (points[num]) + (1,)

    polyline.order_u = len(polyline.points) - 1
    if join:
        polyline.use_cyclic_u = True


def poly_lines(objname, curvename, lines, bevel=None, joins=False, ctype='NURBS'):
    curve = bpy.data.curves.new(name=curvename, type='CURVE')
    curve.dimensions = '3D'

    obj = bpy.data.objects.new(objname, curve)
    obj.location = (0, 0, 0)  # object origin

    for i, line in enumerate(lines):
        poly_line(curve, line, joins if type(joins) == bool else joins[i], type=ctype)

    if bevel:
        curve.bevel_object = bpy.data.objects[bevel]
    return obj


def nurbs_circle(name, w, h):
    pts = [(-w / 2, 0, 0), (0, -h / 2, 0), (w / 2, 0, 0), (0, h / 2, 0)]
    return poly_lines(name, name + '_curve', [pts], joins=True)


def star_pts(r=1, ir=None, points=5, center=(0, 0)):
    """
    Create points for a star. They are 2d - z is always zero

    r: the outer radius
    ir: the inner radius
    """
    if not ir:
        ir = r / 5
    pts = []
    dt = pi * 2 / points
    for i in range(points):
        t = i * dt
        ti = (i + .5) * dt
        pts.append(angle_point(center, t, r) + (0,))
        pts.append(angle_point(center, ti, ir) + (0,))
    return pts


def defaultCircle(w=.6):
    circle = nurbs_circle('braid_circle', w, w)
    circle.hide = True
    return circle


def defaultStar():
    star = poly_lines('star', 'staz', [tuple(star_pts(points=5, r=.5, ir=.05))], type='NURBS')
    star.hide = True
    return star


def awesome_braid(strands=3, sides=5, bevel='braid_circle', pointy=False, **kwds):
    lines = make_strands(strands, sides, **kwds)
    types = {True: 'POLY', False: 'NURBS'}[pointy]
    return poly_lines('Braid', 'Braid_c', lines, bevel=bevel, joins=True, ctype=types)


class Braid(Operator):
    bl_idname = "mesh.add_braid"
    bl_label = "New Braid"
    bl_description = ("Construct a new Braid\n"
                      "Creates two objects - the hidden one is used as the Bevel control")
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    strands = IntProperty(
            name="Strands",
            description="Number of Strands",
            min=2, max=100,
            default=3
            )
    sides = IntProperty(
            name="Sides",
            description="Number of Knot sides",
            min=2, max=100,
            default=5
            )
    radius = FloatProperty(
            name="Radius",
            description="Increase / decrease the diameter in X,Y axis",
            default=1
            )
    thickness = FloatProperty(
            name="Thickness",
            description="The ratio between inner and outside diameters",
            default=.3
            )
    strandsize = FloatProperty(
            name="Bevel Depth",
            description="Individual strand diameter (similar to Curve's Bevel depth)",
            default=.3,
            min=.01, max=10
            )
    width = FloatProperty(
            name="Width",
            description="Stretch the Braids along the Z axis",
            default=.2
            )
    resolution = IntProperty(
            name="Bevel Resolution",
            description="Resolution of the Created curve\n"
                        "Increasing this value, will produce heavy geometry",
            min=1,
            max=100, soft_max=24,
            default=2
            )
    pointy = BoolProperty(
            name="Pointy",
            description="Switch between round and sharp corners",
            default=False
            )

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        col = box.column(align=True)
        col.label("Settings:")
        col.prop(self, "strands")
        col.prop(self, "sides")

        col = box.column(align=True)
        col.prop(self, "radius")
        col.prop(self, "thickness")
        col.prop(self, "width")

        col = box.column()
        col.prop(self, "pointy")

        box = layout.box()
        col = box.column(align=True)
        col.label("Geometry Options:")
        col.prop(self, "strandsize")
        col.prop(self, "resolution")

    def execute(self, context):
        circle = defaultCircle(self.strandsize)
        context.scene.objects.link(circle)
        braid = awesome_braid(
                        self.strands, self.sides,
                        bevel=circle.name,
                        pointy=self.pointy,
                        radius=self.radius,
                        mr=self.thickness,
                        mz=self.width,
                        resolution=self.resolution
                        )
        base = context.scene.objects.link(braid)

        for ob in context.scene.objects:
            ob.select = False
        base.select = True
        context.scene.objects.active = braid

        return {'FINISHED'}


def register():
    bpy.utils.register_class(Braid)


def unregister():
    bpy.utils.unregister_class(Braid)


if __name__ == "__main__":
    register()
