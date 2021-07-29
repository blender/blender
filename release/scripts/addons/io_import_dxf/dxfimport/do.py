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
import os
import re
from mathutils import Vector, Matrix, Euler, Color, geometry
from math import pi, radians, sqrt

import bmesh
from .. import dxfgrabber
from . import convert, is_, groupsort
from .line_merger import line_merger
from ..transverse_mercator import TransverseMercator


try:
    from pyproj import Proj, transform as proj_transform
    PYPROJ = True
except:
    PYPROJ = False

BY_LAYER = 0
BY_DXFTYPE = 1
BY_CLOSED_NO_BULGE_POLY = 2
SEPARATED = 3
LINKED_OBJECTS = 4
GROUP_INSTANCES = 5
BY_BLOCK = 6


def transform(p1, p2, c1, c2, c3):
    if PYPROJ:
        if type(p1) is Proj and type(p2) is Proj:
            if p1.srs != p2.srs:
                return proj_transform(p1, p2, c1, c2, c3)
            else:
                return (c1, c2, c3)
        elif type(p2) is TransverseMercator:
            wgs84 = Proj(init="EPSG:4326")
            if p1.srs != wgs84.srs:
                t2, t1, t3 = proj_transform(p1, wgs84, c1, c2, c3)
            else:
                t1, t2, t3 = c2, c1, c3  # mind c2, c1 inversion
            tm1, tm2 = p2.fromGeographic(t1, t2)
            return (tm1, tm2, t3)
    else:
        if p1.spherical:
            t1, t2 = p2.fromGeographic(c2, c1)  # mind c2, c1 inversion
            return (t1, t2, c3)
        else:
            return (c1, c2, c3)


def float_len(f):
    s = str(f)
    if 'e' in s:
        return int(s[3:5])
    else:
        return len(s[2:])


class Indicator:
    spherical = False
    euclidean = False

    def __init__(self, i):
        if i.upper() == "SPHERICAL":
            self.spherical = True
        elif i.upper() == "EUCLIDEAN":
            self.euclidean = True
        else:
            raise AttributeError("Indication must be 'spherical' or 'euclidean'. Given: " + str(i))


class Do:
    __slots__ = (
        "dwg", "combination", "known_blocks", "import_text", "import_light", "export_acis", "merge_lines",
        "do_bounding_boxes", "acis_files", "errors", "block_representation", "recenter", "did_group_instance",
        "objects_before", "pDXF", "pScene", "thickness_and_width", "but_group_by_att", "current_scene",
        "dxf_unit_scale"
    )

    def __init__(self, dxf_filename, c=BY_LAYER, import_text=True, import_light=True, export_acis=True,
                 merge_lines=True, do_bbox=True, block_rep=LINKED_OBJECTS, recenter=False, pDXF=None, pScene=None,
                 thicknessWidth=True, but_group_by_att=True, dxf_unit_scale=1.0):
        self.dwg = dxfgrabber.readfile(dxf_filename, {"assure_3d_coords": True})
        self.combination = c
        self.known_blocks = {}
        self.import_text = import_text
        self.import_light = import_light
        self.export_acis = export_acis
        self.merge_lines = merge_lines
        self.do_bounding_boxes = do_bbox
        self.acis_files = []
        self.errors = set([])
        self.block_representation = block_rep
        self.recenter = recenter
        self.did_group_instance = False
        self.objects_before = []
        self.pDXF = pDXF
        self.pScene = pScene
        self.thickness_and_width = thicknessWidth
        self.but_group_by_att = but_group_by_att
        self.current_scene = None
        self.dxf_unit_scale = dxf_unit_scale

    def proj(self, co, elevation=0):
        """
        :param co: coordinate
        :param elevation: float (lwpolyline code 38)
        :return: transformed coordinate if self.pScene is defined
        """
        if self.pScene is not None and self.pDXF is not None:
            u = self.dxf_unit_scale
            if len(co) == 3:
                c1, c2, c3 = co
                c3 += elevation
            else:
                c1, c2 = co
                c3 = elevation
            if u != 1.0:
                c1 *= u
                c2 *= u
                c3 *= u
            # add
            add = Vector((0, 0, 0))
            if "latitude" in self.current_scene and "longitude" in self.current_scene:
                if PYPROJ and type(self.pScene) not in (TransverseMercator, Indicator):
                    wgs84 = Proj(init="EPSG:4326")
                    cscn_lat = self.current_scene.get('latitude', 0)
                    cscn_lon = self.current_scene.get('longitude', 0)
                    cscn_alt = self.current_scene.get('altitude', 0)
                    add = Vector(transform(wgs84, self.pScene, cscn_lon, cscn_lat, cscn_alt))

            # projection
            newco = Vector(transform(self.pDXF, self.pScene, c1, c2, c3))
            newco = newco - add
            if any((c == float("inf") or c == float("-inf") for c in newco)):
                self.errors.add("Projection results in +/- infinity coordinates.")
            return newco
        else:
            u = self.dxf_unit_scale
            if u != 1:
                if len(co) == 3:
                    c1, c2, c3 = co
                    c3 += elevation
                else:
                    c1, c2 = co
                    c3 = elevation
                c1 *= u
                c2 *= u
                c3 *= u
                return Vector((c1, c2, c3))
            else:
                return Vector((co[0], co[1], co[2] + elevation if len(co) == 3 else elevation))

    def georeference(self, scene, center):
        if "latitude" not in scene and "longitude" not in scene:
            if type(self.pScene) is TransverseMercator:
                scene['latitude'] = self.pScene.lat
                scene['longitude'] = self.pScene.lon
                scene['altitude'] = 0
            elif type(self.pScene) is not None:
                wgs84 = Proj(init="EPSG:4326")
                latlon = transform(self.pScene, wgs84, center[0], center[1], center[2])
                scene['longitude'] = latlon[0]
                scene['latitude'] = latlon[1]
                scene['altitude'] = latlon[2]

    """ GEOMETRY DXF TYPES TO BLENDER CURVES FILTERS"""
    # type(self, dxf entity, blender curve data)

    def _cubic_bezier_closed(self, ptuple, curve):
        count = (len(ptuple)-1)/3
        points = [ptuple[-2]]
        ptuples = ptuple[:-2]
        points += [p for p in ptuples]

        spl = curve.splines.new('BEZIER')
        spl.use_cyclic_u = True
        b = spl.bezier_points
        b.add(count - 1)
        for i, j in enumerate(range(1, len(points), 3)):
            b[i].handle_left = self.proj(points[j - 1])
            b[i].co = self.proj(points[j])
            b[i].handle_right = self.proj(points[j + 1])

    def _cubic_bezier_open(self, points, curve):
        count = (len(points) - 1) / 3 + 1
        spl = curve.splines.new('BEZIER')
        b = spl.bezier_points
        b.add(count - 1)

        b[0].co = self.proj(points[0])
        b[0].handle_left = self.proj(points[0])
        b[0].handle_right = self.proj(points[1])

        b[-1].co = self.proj(points[-1])
        b[-1].handle_right = self.proj(points[-1])
        b[-1].handle_left = self.proj(points[-2])

        for i, j in enumerate(range(3, len(points) - 2, 3), 1):
            b[i].handle_left = self.proj(points[j - 1])
            b[i].co = self.proj(points[j])
            b[i].handle_right = self.proj(points[j + 1])

    def _cubic_bezier(self, points, curve, is_closed):
        """
        points: control points; list of (x,y,z) tuples
        curve: Blender curve data of type "CURVE" (object.data) where the bezier should be added to
        is_closed: True / False to indicate if the curve is open or closed
        """
        if is_closed:
            self._cubic_bezier_closed(points, curve)
        else:
            self._cubic_bezier_open(points, curve)

    def _poly(self, points, curve, elevation=0, is_closed=False):
        """
        points: list of (x,y,z)
        curve: Blender curve data of type "CURVE" (object.data) to which the poly should be added to
        param elevation: float (lwpolyline code 38)
        is_closed: True / False to indicate if the polygon is open or closed
        """
        p = curve.splines.new("POLY")
        p.use_smooth = False
        p.use_cyclic_u = is_closed
        p.points.add(len(points) - 1)

        for i, pt in enumerate(points):
            p.points[i].co = self.proj(pt, elevation).to_4d()

    def _gen_poly(self, en, curve, elevation=0):
        if any([b != 0 for b in en.bulge]):
            self._cubic_bezier(convert.bulgepoly_to_cubic(self, en), curve, en.is_closed)
        else:
            self._poly(en.points, curve, elevation, en.is_closed)

    def polyline(self, en, curve):
        """
        en: DXF entity of type `POLYLINE`
        curve: Blender data structure of type `CURVE`
        """
        self._gen_poly(en, curve)

    def polygon(self, en, curve):
        """
        en: DXF entity of type `POLYGON`
        curve: Blender data structure of type `CURVE`
        """
        self._gen_poly(en, curve)

    def lwpolyline(self, en, curve):
        """
        en: DXF entity of type `LWPOLYLINE`
        curve: Blender data structure of type `CURVE`
        """
        self._gen_poly(en, curve, en.elevation)

    def line(self, en, curve):
        """
        en: DXF entity of type `LINE`
        curve: Blender data structure of type `CURVE`
        """
        self._poly([en.start, en.end], curve, 0, False)

    def arc(self, en, curve=None, aunits=None, angdir=None, angbase=None):
        """
        en: dxf entity (en.start_angle, en.end_angle, en.center, en.radius)
        curve: optional; Blender curve data of type "CURVE" (object.data) to which the arc should be added to
        return control points of a cubic spline (do be used in a spline with bulges / series of arcs)
        note: en.start_angle + en.end_angle: angles measured from the angle base (angbase) in the direction of
              angdir (1 = clockwise, 0 = counterclockwise)
        """
        treshold = 0.005

        if aunits is None:
            aunits = self.dwg.header.get('$AUNITS', 0)
        if angbase is None:
            angbase = self.dwg.header.get('$ANGBASE', 0)
        if angdir is None:
            angdir = self.dwg.header.get('$ANGDIR', 0)
        kappa = 0.5522848

        # TODO: add support for 1 (dms) and 4 (survey)
        if aunits == 0:
            # Degree
            s = radians(en.start_angle+angbase)
            e = radians(en.end_angle+angbase)
        elif aunits == 2:
            # Gradians
            s = radians(0.9 * (en.start_angle + angbase))
            e = radians(0.9 * (en.end_angle + angbase))
        else:
            # Radians
            s = en.start_angle+angbase
            e = en.end_angle+angbase

        if s > e:
            e += 2 * pi
        angle = e - s

        vc = Vector(en.center)
        x_vec = Vector((1, 0, 0))
        radius = en.radius

        # turn clockwise
        if angdir == 0:
            rot = Matrix.Rotation(radians(-90), 3, "Z")
            start = x_vec * radius * Matrix.Rotation(-s, 3, "Z")
            end = x_vec * radius * Matrix.Rotation(-e, 3, "Z")

        # turn counter-clockwise
        else:
            rot = Matrix.Rotation(radians(90), 3, "Z")
            start = x_vec * radius * Matrix.Rotation(s, 3, "Z")
            end = x_vec * radius * Matrix.Rotation(e, 3, "Z")

        # start
        spline = list()
        spline.append(vc + start)
        if abs(angle) - pi / 2 > treshold:  # if angle is more than pi/2 incl. treshold
            spline.append(vc + start + start * kappa * rot)
        else:
            spline.append(vc + start + start * kappa * angle / (pi / 2) * rot)

        # fill if angle is larger than 90 degrees
        a = pi / 2
        if abs(angle) - treshold > a:
            fill = start

            while abs(angle) - a > treshold:
                fillnext = fill * rot
                spline.append(vc + fillnext + fill * kappa)
                spline.append(vc + fillnext)
                # if this was the last fill control point
                if abs(angle) - a - pi / 2 < treshold:
                    end_angle = (abs(angle) - a) * abs(angle) / angle
                    spline.append(vc + fillnext + fillnext * kappa * end_angle / (pi / 2) * rot)
                else:
                    spline.append(vc + fillnext + fillnext * kappa * rot)
                fill = fillnext
                a += pi / 2

        else:
            end_angle = angle

        # end
        spline.append(vc + end + end * -kappa * end_angle / (pi / 2) * rot)
        spline.append(vc + end)

        if len(spline) % 3 != 1:
            print("DXF-IMPORT: DO ARC: CHECK PLEASE: ", len(spline), spline)

        # curve is None means arc is called from bulge conversion
        # nothing should be projected at this stage, since the
        # lwpolyline (the only entity with bulges) will be projected
        # as a whole afterwards (small little error; took ages to debug)
        if curve is not None:
            self._cubic_bezier_open(spline, curve)
            return spline
        else:
            return spline

    def circle(self, en, curve, major=Vector((1, 0, 0))):
        """
        en: dxf entity
        curve: Blender curve data of type "CURVE" (object.data) to which the circle should be added to
        major: optional; if the circle is used as a base for an ellipse, major denotes the ellipse's major direction
        """
        c = curve.splines.new("BEZIER")
        c.use_cyclic_u = True
        b = c.bezier_points
        b.add(3)

        for i in range(4):
            b[i].handle_left_type = 'AUTO'
            b[i].handle_right_type = 'AUTO'

        vc = self.proj(en.center)
        clockwise = Matrix(((0, -1, 0), (1, 0, 0), (0, 0, 1)))

        r = major
        if len(r) == 2:
            r = r.to_3d()
        r = r * en.radius

        try:
            b[0].co = vc + r
            b[1].co = vc + r * clockwise
            b[2].co = vc + r * clockwise * clockwise
            b[3].co = vc + r * clockwise * clockwise * clockwise
        except:
            print("Circle center: ", vc, "radius: ", r)
            raise

        return c

    def scale_controlpoint(self, p, factor):
        """
        p: Blender control-point
        factor: (Float)
        Repositions left and right handle of a bezier control point.
        """
        p.handle_left = p.co + (p.handle_left - p.co) * factor
        p.handle_right = p.co + (p.handle_right - p.co) * factor

    def ellipse(self, en, curve):
        """
        center: (x,y,z) of circle center
        major: the ellipse's major direction
        ratio: ratio between major and minor axis lengths (always < 1)
        curve: Blender curve data of type "CURVE" (object.data) to which the ellipse should be added to
        """
        major = Vector(en.major_axis)
        en.__dict__["radius"] = major.length
        c = self.circle(en, curve, major.normalized())
        b = c.bezier_points

        if en.ratio < 1:
            for i in range(4):
                b[i].handle_left_type = 'ALIGNED'
                b[i].handle_right_type = 'ALIGNED'

            vc = self.proj(en.center)
            clockwise = Matrix(((0, -1, 0), (1, 0, 0), (0, 0, 1)))
            if len(major) == 2:
                major = major.to_3d()
            minor = major * en.ratio * clockwise

            lh = b[1].handle_left - b[1].co
            rh = b[1].handle_right - b[1].co
            b[1].co = vc + minor
            b[1].handle_left = b[1].co + lh
            b[1].handle_right = b[1].co + rh
            b[3].co = vc + minor * clockwise * clockwise
            b[3].handle_left = b[3].co + rh
            b[3].handle_right = b[3].co + lh

            self.scale_controlpoint(b[0], en.ratio)
            self.scale_controlpoint(b[2], en.ratio)

    def spline(self, en, curve, _3D=True):
        """
        en: DXF entity of type `SPLINE`
        curve: Blender data structure of type `CURVE`
        """
        if _3D:
            curve.dimensions = "3D"
        spline = convert.bspline_to_cubic(self, en, curve, self.errors)
        if spline is None:
            self.errors.add("Not able to import bspline with degree > 3")
        else:
            self._cubic_bezier(spline, curve, en.is_closed)

    def helix(self, en, curve):
        """
        en: DXF entity of type `HELIX`
        curve: Blender data structure of type `CURVE`
        """
        self.spline(en, curve, not en.is_planar)

    """ GEOMETRY DXF TYPES TO BLENDER MESHES FILTERS"""
    # type(self, dxf entity, blender bmesh data)

    def _gen_meshface(self, points, bm):
        """
        points: list of (x,y,z) tuples
        bm: bmesh to add the (face-) points
        Used by the3dface() and solid()
        """

        def _is_on_edge(point):
            return abs(sum((e - point).length for e in (edge1, edge2)) - (edge1 - edge2).length) < 0.01

        points = list(points)

        i = 0
        while i < len(points):
            if points.count(points[i]) > 1:
                points.pop(i)
            else:
                i += 1

        verts = []
        for p in points:
            verts.append(bm.verts.new(self.proj(p)))

        # add only an edge if len points < 3
        if len(points) == 2:
            bm.edges.new(verts)
        elif len(points) > 2:
            face = bm.faces.new(verts)

            if len(points) == 4:
                for i in range(2):
                    edge1 = verts[i].co
                    edge2 = verts[i + 1].co
                    opposite1 = verts[i + 2].co
                    opposite2 = verts[(i + 3) % 4].co
                    ii = geometry.intersect_line_line(edge1, edge2, opposite1, opposite2)
                    if ii is not None:
                        if _is_on_edge(ii[0]):
                            try:
                                bm.faces.remove(face)
                            except Exception as e:
                                pass
                            iv = bm.verts.new(ii[0])
                            bm.faces.new((verts[i], iv, verts[(i + 3) % 4]))
                            bm.faces.new((verts[i + 1], iv, verts[i + 2]))

    def the3dface(self, en, bm):
        """ f: dxf entity
            bm: Blender bmesh data to which the 3DFACE should be added to.
            <-? bm.from_mesh(object.data) ?->
        """
        if en.points[-1] == en.points[-2]:
            points = en.points[:3]
        else:
            points = en.points
        self._gen_meshface(points, bm)

    def solid(self, en, bm):
        """ f: dxf entity
            bm: Blender bmesh data to which the SOLID should be added to.
            <-? bm.from_mesh(object.data) ?->
        """
        p = en.points
        points = (p[0], p[1], p[3], p[2])
        self._gen_meshface(points, bm)

    def trace(self, en, bm):
        self.solid(en, bm)

    def point(self, en, bm):
        """
        en: DXF entity of type `POINT`
        bm: Blender bmesh instance
        """
        bm.verts.new(en.point)

    def polyface(self, en, bm):
        """
        pf: polyface
        bm: Blender bmesh data to which the POLYFACE should be added to.
        <-? bm.from_mesh(object.data) ?->
        """
        for v in en.vertices:
            bm.verts.new(v.location)

        bm.verts.ensure_lookup_table()
        for subface in en:
            idx = subface.indices()
            points = []
            for p in idx:
                if p not in points:
                    points.append(p)
            if len(points) in (3, 4):
                bm.faces.new([bm.verts[i] for i in points])

    def polymesh(self, en, bm):
        """
        en: POLYMESH entitiy
        bm: Blender bmesh instance
        """
        mc = en.mcount if not en.is_mclosed else en.mcount + 1
        nc = en.ncount if not en.is_nclosed else en.ncount + 1
        for i in range(1, mc):
            i = i % en.mcount
            i_ = (i - 1) % en.mcount
            for j in range(1, nc):
                j = j % en.ncount
                j_ = (j - 1) % en.ncount
                face = []
                face.append(bm.verts.new(en.get_location((i_, j_))))
                face.append(bm.verts.new(en.get_location((i, j_))))
                face.append(bm.verts.new(en.get_location((i, j))))
                face.append(bm.verts.new(en.get_location((i_, j))))

                bm.faces.new(face)

    def mesh(self, en, bm):
        """
        mesh: dxf entity
        m: Blender MESH data (object.data) to which the dxf-mesh should be added
        """
        # verts:
        for v in en.vertices:
            bm.verts.new(v)

        # edges:
        bm.verts.ensure_lookup_table()
        if any((c < 0 for c in en.edge_crease_list)):
            layerkey = bm.edges.layers.crease.new("SubsurfCrease")
            for i, edge in enumerate(en.edges):
                bme = bm.edges.new([bm.verts[edge[0]], bm.verts[edge[1]]])
                bme[layerkey] = -en.edge_crease_list[i]
        else:
            for i, edge in enumerate(en.edges):
                bm.edges.new([bm.verts[edge[0]], bm.verts[edge[1]]])

        # faces:
        for face in en.faces:
            bm.faces.new([bm.verts[i] for i in face])

    """ SEPARATE BLENDER OBJECTS FROM (CON)TEXT / STRUCTURE DXF TYPES """
    # type(self, dxf entity, name string)
    #     returns blender object

    def _extrusion(self, obj, entity):
        """
        extrusion describes the normal vector of the entity
        """
        if entity.dxftype not in {"LINE", "POINT"}:
            if is_.extrusion(entity):
                transformation = convert.extrusion_to_matrix(entity)
                obj.location = transformation * obj.location
                obj.rotation_euler.rotate(transformation)

    def _bbox(self, objects, scene):
        xmin = ymin = zmin = float('+inf')
        xmax = ymax = zmax = float('-inf')
        scene.update()

        for obj in objects:
            om = obj.matrix_basis
            for v in obj.bound_box:
                p = om * Vector(v)
                if p.x < xmin:
                    xmin = p.x
                if p.x > xmax:
                    xmax = p.x
                if p.y < ymin:
                    ymin = p.y
                if p.y > ymax:
                    ymax = p.y
                if p.z < zmin:
                    zmin = p.z
                if p.z > zmax:
                    zmax = p.z

        if xmin == float('+inf'):
            xmin = 0
        if ymin == float('+inf'):
            ymin = 0
        if zmin == float('+inf'):
            zmin = 0
        if xmax == float('-inf'):
            xmax = 0
        if ymax == float('-inf'):
            ymax = 0
        if zmax == float('-inf'):
            zmax = 0

        return xmin, ymin, zmin, xmax, ymax, zmax

    def _object_bbox(self, objects, scene, name, do_widgets=True):
        xmin, ymin, zmin, xmax, ymax, zmax = self._bbox(objects, scene)

        # creating bbox geometry
        bm = bmesh.new()
        verts = []
        # left side vertices
        verts.append(bm.verts.new((xmin, ymin, zmin)))
        verts.append(bm.verts.new((xmin, ymin, zmax)))
        verts.append(bm.verts.new((xmin, ymax, zmax)))
        verts.append(bm.verts.new((xmin, ymax, zmin)))
        # right side vertices
        verts.append(bm.verts.new((xmax, ymin, zmin)))
        verts.append(bm.verts.new((xmax, ymin, zmax)))
        verts.append(bm.verts.new((xmax, ymax, zmax)))
        verts.append(bm.verts.new((xmax, ymax, zmin)))

        # creating the widgets
        if do_widgets:
            for i in range(2):
                for j in range(4):
                    point = verts[j + (i * 4)]
                    prevp = verts[((j - 1) % 4) + (i * 4)]
                    nextp = verts[((j + 1) % 4) + (i * 4)]
                    neigh = verts[(j + (i * 4) + 4) % 8]

                    for con in (prevp, nextp, neigh):
                        vec = Vector(con.co - point.co) / 10
                        bm.edges.new((point, bm.verts.new(point.co + vec)))

        d = bpy.data.meshes.new(name + "BBOX")
        bm.to_mesh(d)
        o = bpy.data.objects.new(name, d)
        return o

    def _vertex_duplication(self, x, y, x_count, y_count):
        bm = bmesh.new()
        for i in range(x_count):
            for j in range(y_count):
                bm.verts.new(Vector((x * i, y * j, 0.)))

        d = bpy.data.meshes.new("vertex_duplicator")
        bm.to_mesh(d)
        return d

    def point_object(self, en, scene, name=None):
        if name is None:
            name = en.dxftype
        o = bpy.data.objects.new("Point", None)
        o.location = self.proj(en.point)
        self._extrusion(o, en)
        scene.objects.link(o)

        group = self._get_group(en.layer)
        group.objects.link(o)

    def light(self, en, scene, name):
        """
        en: dxf entity
        name: ignored; exists to make separate and merged objects methods universally callable from _call_types()
        Creates, links and returns a new light object depending on the type and color of the dxf entity.
        """
        # light_type : distant = 1; point = 2; spot = 3
        if self.import_light:
            type_map = ["NONE", "SUN", "POINT", "SPOT"]
            layer = self.dwg.layers[en.layer]
            lamp = bpy.data.lamps.new(en.name, type_map[en.light_type])
            if en.color != 256:
                aci = en.color
            else:
                aci = layer.color
            c = dxfgrabber.aci_to_true_color(aci)
            lamp.color = Color(c.rgb())
            if en.light_type == 3:
                lamp.spot_size = en.hotspot_angle
            o = bpy.data.objects.new(en.name, lamp)
            o.location = self.proj(en.position)
            dir = self.proj(en.target) - self.proj(en.position)
            o.rotation_quaternion = dir.rotation_difference(Vector((0, 0, -1)))
            scene.objects.link(o)
            return o

    def mtext(self, en, scene, name):
        """
        en: dxf entity
        name: ignored; exists to make separate and merged objects methods universally callable from _call_types()
        Returns a new multi-line text object.
        """
        if self.import_text:
            text = en.plain_text()
            name = text[:8]
            d = bpy.data.curves.new(name, "FONT")
            o = bpy.data.objects.new(name, d)
            d.body = text
            d.size = en.height
            if en.rect_width is not None:
                if en.rect_width > 50:
                    width = 50
                    ratio = 50 / en.rect_width
                    d.size = en.height * ratio * 1.4  # XXX HACK
                    scale = (1 / ratio, 1 / ratio, 1 / ratio)
                    d.space_line = en.line_spacing
                else:
                    width = en.rect_width
                    scale = (1, 1, 1)
                d.text_boxes[0].width = width
                o.scale = scale

            # HACK
            d.space_line *= 1.5

            o.rotation_euler = Vector((1, 0, 0)).rotation_difference(en.xdirection).to_euler()
            o.location = en.insert
            return o

    def text(self, en, scene, name):
        """
        en: dxf entity
        name: ignored; exists to make separate and merged objects methods universally callable from _call_types()
        Returns a new single line text object.
        """
        if self.import_text:
            name = en.text[:8]
            d = bpy.data.curves.new(name, "FONT")
            d.body = en.plain_text()
            d.size = en.height
            o = bpy.data.objects.new(name, d)
            o.rotation_euler = Euler((0, 0, radians(en.rotation)), 'XYZ')
            basepoint = self.proj(en.basepoint) if hasattr(en, "basepoint") else self.proj((0, 0, 0))
            o.location = self.proj((en.insert)) + basepoint
            if hasattr(en, "thickness"):
                et = en.thickness / 2
                d.extrude = abs(et)
                if et > 0:
                    o.location.z += et
                elif et < 0:
                    o.location.z -= et
            return o

    def block_linked_object(self, entity, scene, name=None, override_group=None, invisible=None, recursion_level=0):
        """
        entity: DXF entity of type `BLOCK`
        name: to be consistent with all functions that get mapped to a DXF type; is set when called from insert()
        override_group: is set when called from insert() (bpy_types.Group)
        invisible: boolean to control the visibility of the returned object
        Returns an object. All blocks with the same name share the same geometry (Linked Blender Objects). If the
        entities in a block have to mapped to different Blender-types (like curve, mesh, surface, text, light) a list of
        sub-objects is being created. `INSERT`s inside a block are being added to the same list. If the list has more
        than one element they are being parented to a newly created Blender-Empty which is return instead of the
        single object in the sub-objects-list that is being returned only if it is the only object in the list.
        """
        def _recursive_copy_inserts(parent, known_inserts, inserts, group, invisible):
            for ki in known_inserts:
                new_insert = ki.copy()
                _recursive_copy_inserts(new_insert, ki.children, None, group, invisible)
                if new_insert.name not in group.objects:
                    group.objects.link(new_insert)
                if invisible is not None:
                    new_insert.hide = bool(invisible)
                if inserts is not None:
                    inserts.append(new_insert)
                new_insert.parent = parent
                scene.objects.link(new_insert)

        if name is None:
            name = entity.name
        # get group
        if override_group is not None:
            group = override_group
        else:
            group = self._get_group(entity.layer)

        # get object(s)
        objects = []
        inserts = []
        if name not in self.known_blocks.keys():
            block_inserts = [en for en in entity if is_.insert(en.dxftype)]
            bc = (en for en in entity if is_.combined_entity(en))
            bs = (en for en in entity if is_.separated_entity(en) and not is_.insert(en.dxftype))

            if self.combination != SEPARATED:
                objects += self.combined_objects(bc, scene, "BL|" + name, group)
            else:
                bs = (en for en in entity if (is_.combined_entity(en) or is_.separated_entity(en)) and
                      not is_.insert(en.dxftype))
            objects += self.separated_entities(bs, scene, "BL|" + name, group)

            # create inserts - RECURSION
            insert_bounding_boxes = []
            for INSERT in block_inserts:
                insert = self.insert(INSERT, scene, None, group, invisible, recursion_level + 1)
                if len(insert.children) > 0:
                    i_copy = bpy.data.objects.new(insert.name, None)
                    i_copy.matrix_basis = insert.matrix_basis
                    scene.objects.link(i_copy)
                    group.objects.link(i_copy)
                    kids = insert.children[:]
                    for child in kids:
                        child.parent = i_copy
                    inserts.append(i_copy)
                    insert_bounding_boxes.append(insert)
                else:
                    inserts.append(insert)

            # determining the main object o
            if len(objects) > 1 or len(insert_bounding_boxes) > 0:
                if self.do_bounding_boxes:
                    o = self._object_bbox(objects + insert_bounding_boxes, scene, name, recursion_level == 0)
                    scene.objects.link(o)
                else:
                    o = bpy.data.objects.new(name, None)
                    scene.objects.link(o)
                if len(objects) > 0:
                    for obj in objects:
                        obj.parent = o

            elif len(objects) == 1:
                o = objects.pop(0)
            else:
                # strange case but possible according to the testfiles
                o = bpy.data.objects.new(name, None)
                scene.objects.link(o)

            # unlink bounding boxes of inserts
            for ib in insert_bounding_boxes:
                if ib.name in group.objects:
                    group.objects.unlink(ib)
                scene.objects.unlink(ib)

            # parent inserts to this block before any transformation on the block is being applied
            for obj in inserts:
                obj.parent = o

            # put a copy of the retreived objects into the known_blocks dict, so that the attributes being added to
            # the object from this point onwards (from INSERT attributes) are not being copied to new/other INSERTs
            self.known_blocks[name] = [[o.copy() for o in objects], inserts]

            # so that it gets assigned to the group and inherits visibility too
            objects.append(o)

            self.known_blocks[name].append(o.copy())
        else:
            known_objects, known_inserts, known_o = self.known_blocks[name]

            for known_object in known_objects:
                oc = known_object.copy()
                scene.objects.link(oc)
                objects.append(oc)

            o = known_o.copy()
            scene.objects.link(o)

            _recursive_copy_inserts(o, known_inserts, inserts, group, invisible)

            # parent objects to o
            for obj in objects:
                obj.parent = o

            objects.append(o)

        # link group
        for obj in objects:
            if obj.name not in group.objects:
                group.objects.link(obj)

        # visibility
        if invisible is not None:
            for obj in objects:
                obj.hide = bool(invisible)

        # block transformations
        o.location = self.proj(entity.basepoint)

        return o

    def block_group_instances(self, entity, scene, name=None, override_group=None, invisible=None, recursion_level=0):
        self.did_group_instance = True

        if name is None:
            name = entity.name
        # get group
        if override_group is not None:
            group = override_group
        else:
            group = self._get_group(entity.layer)

        block_group = self._get_group(entity.name+"_BLOCK")

        if "Blocks" not in bpy.data.scenes:
            block_scene = bpy.data.scenes.new("Blocks")
        else:
            block_scene = bpy.data.scenes["Blocks"]

        # create the block
        if len(block_group.objects) == 0 or name not in self.known_blocks.keys():
            bpy.context.screen.scene = block_scene
            block_inserts = [en for en in entity if is_.insert(en.dxftype)]
            bc = (en for en in entity if is_.combined_entity(en))
            bs = (en for en in entity if is_.separated_entity(en) and not is_.insert(en.dxftype))

            objects = []
            if self.combination != SEPARATED:
                objects += self.combined_objects(bc, block_scene, "BL|" + name, block_group)
            else:
                bs = (en for en in entity if (is_.combined_entity(en) or is_.separated_entity(en)) and
                      not is_.insert(en.dxftype))
            objects += self.separated_entities(bs, block_scene, "BL|" + name, block_group)

            # create inserts - RECURSION
            inserts = []
            for INSERT in block_inserts:
                i = self.insert(INSERT, block_scene, None, block_group, invisible, recursion_level + 1, True)
                inserts.append(i)

            bbox = self._object_bbox(objects + inserts, block_scene, name, True)

            for i in inserts:
                sub_group = i.dupli_group
                block_scene.objects.unlink(i)
                block_group.objects.unlink(i)
                i_empty = bpy.data.objects.new(i.name, None)
                i_empty.matrix_basis = i.matrix_basis
                i_empty.dupli_type = "GROUP"
                i_empty.dupli_group = sub_group
                block_group.objects.link(i_empty)
                block_scene.objects.link(i_empty)

            self.known_blocks[name] = [objects, inserts, bbox]
        else:
            bbox = self.known_blocks[name][2]

        bpy.context.screen.scene = scene
        o = bbox.copy()
        # o.empty_draw_size = 0.3
        o.dupli_type = "GROUP"
        o.dupli_group = block_group
        group.objects.link(o)
        if invisible is not None:
            o.hide = invisible
        o.location = self.proj(entity.basepoint)
        scene.objects.link(o)
        # block_scene.update()

        return o

    def insert(self, entity, scene, name, group=None, invisible=None, recursion_level=0, need_group_inst=None):
        """
        entity: DXF entity
        name: String; not used but required to be consistent with the methods being called from _call_type()
        group: Blender group of type (bpy_types.group) being set if called from block()
        invisible: boolean to control visibility; being set if called from block()
        """
        aunits = self.dwg.header.get('$AUNITS', 0)

        # check if group instances are needed
        kids = sum(1 for i in self.dwg.blocks[entity.name] if i.dxftype == "INSERT")
        sep = sum(1 for sep in self.dwg.blocks[entity.name] if is_.separated_entity(sep))
        objtypes = sum(1 for ot, ens in groupsort.by_blender_type(en for en in self.dwg.blocks[entity.name]
                                                                  if is_.combined_entity(en))
                       if ot in {"object_mesh", "object_curve"})
        if need_group_inst is None:
            need_group_inst = (entity.row_count or entity.col_count) > 1 and \
                              (kids > 0 or objtypes > 1 or sep > 1 or (objtypes > 0 and sep > 0))

        if group is None:
            group = self._get_group(entity.layer)

        if self.block_representation == GROUP_INSTANCES or need_group_inst:
            o = self.block_group_instances(self.dwg.blocks[entity.name], scene, entity.name, group,
                                           entity.invisible, recursion_level)
        else:
            o = self.block_linked_object(self.dwg.blocks[entity.name], scene, entity.name, group,
                                         entity.invisible, recursion_level)

        # column & row
        if (entity.row_count or entity.col_count) > 1:
            if len(o.children) == 0 and self.block_representation == LINKED_OBJECTS and not need_group_inst:
                    arr_row = o.modifiers.new("ROW", "ARRAY")
                    arr_col = o.modifiers.new("COL", "ARRAY")
                    arr_row.show_expanded = False
                    arr_row.count = entity.row_count
                    arr_row.relative_offset_displace = (0, entity.row_spacing, 0)
                    arr_col.show_expanded = False
                    arr_col.count = entity.col_count
                    arr_col.relative_offset_displace = (entity.col_spacing / 2, 0, 0)

            else:
                instance = o
                x = (Vector(o.bound_box[4]) - Vector(o.bound_box[0])).length
                y = (Vector(o.bound_box[3]) - Vector(o.bound_box[0])).length
                dm = self._vertex_duplication(x * entity.col_spacing / 2, y * entity.row_spacing,
                                              entity.col_count, entity.row_count)
                o = bpy.data.objects.new(entity.name, dm)
                instance.parent = o
                o.dupli_type = "VERTS"

        # insert transformations
        rot = radians(entity.rotation) if aunits == 0 else entity.rotation
        o.location += self.proj(entity.insert)
        o.rotation_euler = Euler((0, 0, rot))
        o.scale = entity.scale

        # mirror (extrusion value of an INSERT ENTITY)
        self._extrusion(o, entity)

        # visibility
        if invisible is None:
            o.hide = bool(entity.invisible)
        else:
            o.hide = bool(invisible)

        # attributes
        if self.import_text:
            if entity.attribsfollow:
                for a in entity.attribs:
                    # Blender custom property
                    o[a.tag] = a.text
                    attname = entity.name + "_" + a.tag
                    scene.objects.link(self.text(a, scene, attname))

        return o

    """ COMBINED BLENDER OBJECT FROM GEOMETRY DXF TYPES """
    # type(self, dxf entities, object name string)
    #     returns blender object

    def _check3D_object(self, curve):
        """
        Checks if a curve object has coordinates that are elevated from the z-plane.
        If so the curve.dimensions are set to 3D.
        """
        if any((p.co.z != 0 for spline in curve.splines for p in spline.points)) or \
           any((p.co.z != 0 for spline in curve.splines for p in spline.bezier_points)):
            curve.dimensions = '3D'

    def _merge_lines(self, lines, curve):
        """
        lines: list of LINE entities
        curve: Blender curve data
        merges a list of LINE entities to a polygon-point-list and adds it to the Blender curve
        """
        polylines = line_merger(lines)
        for polyline in polylines:
            self._poly(polyline, curve, 0, polyline[0] == polyline[-1])

    def _thickness(self, bm, thickness):
        """
        Used for mesh types
        """
        if not self.thickness_and_width:
            return
        original_faces = [face for face in bm.faces]
        bm.normal_update()
        for face in original_faces:
            normal = face.normal.copy()
            if normal.z < 0:
                normal *= -1
            ret = bmesh.ops.extrude_face_region(bm, geom=[face])
            new_geom = ret["geom"]
            verts = (g for g in new_geom if type(g) == bmesh.types.BMVert)
            for v in verts:
                v.co += normal * thickness
            del ret
        del original_faces

    def _thickness_and_width(self, obj, entity, scene):
        """
        Used for curve types
        """
        if not self.thickness_and_width:
            return
        has_varying_width = is_.varying_width(entity)
        th = entity.thickness
        w = 0
        if hasattr(entity, "width"):
            if len(entity.width) > 0 and len(entity.width[0]) > 0:
                w = entity.width[0][0]

        if w == 0 and not has_varying_width:
            if th != 0:
                obj.data.extrude = abs(th / 2)
                if th > 0:
                    obj.location.z += th / 2
                else:
                    obj.location.z -= th / 2
            obj.data.dimensions = "3D"
            obj.data.twist_mode = "Z_UP"

        else:
            # CURVE BEVEL
            ew = entity.width
            max_w = max((w for w_pair in ew for w in w_pair))

            bevd = bpy.data.curves.new("BEVEL", "CURVE")
            bevdp = bevd.splines.new("POLY")
            bevdp.points.add(1)
            bevdp.points[0].co = Vector((-max_w / 2, 0, 0, 0))
            bevdp.points[1].co = Vector((max_w / 2, 0, 0, 0))

            bevel = bpy.data.objects.new("BEVEL", bevd)
            obj.data.bevel_object = bevel
            scene.objects.link(bevel)

            # CURVE TAPER
            if has_varying_width and len(ew) == 1:
                tapd = bpy.data.curves.new("TAPER", "CURVE")
                tapdp = tapd.splines.new("POLY")
                # lenlist = convert.bulgepoly_to_lenlist(entity)
                # amount = len(ew) if entity.is_closed else len(ew) - 1

                tapdp.points[0].co = Vector((0, ew[0][0] / max_w, 0, 0))
                tapdp.points.add(1)
                tapdp.points[1].co = Vector((1, ew[0][1] / max_w, 0, 0))

                # for i in range(1, amount):
                #     start_w = ew[i][0]
                #     end_w = ew[i][1]
                #     tapdp.points.add(2)
                #     tapdp.points[-2].co = Vector((sum(lenlist[:i]), start_w / max_w, 0, 0))
                #     tapdp.points[-1].co = Vector((sum(lenlist[:i + 1]), end_w / max_w, 0, 0))

                taper = bpy.data.objects.new("TAPER", tapd)
                obj.data.taper_object = taper
                scene.objects.link(taper)

            # THICKNESS FOR CURVES HAVING A WIDTH
            if th != 0:
                solidify = obj.modifiers.new("THICKNESS", "SOLIDIFY")
                solidify.thickness = th
                solidify.use_even_offset = True
                solidify.offset = 1
                solidify.show_expanded = False

            # make the shading look good
            esp = obj.modifiers.new("EdgeSplit", "EDGE_SPLIT")
            esp.show_expanded = False

    def _subdivision(self, obj, entity):
        if entity.subdivision_levels > 0:
            subd = obj.modifiers.new("SubD", "SUBSURF")
            subd.levels = entity.subdivision_levels
            subd.show_expanded = False

    def polys_to_mesh(self, entities, scene, name):
        d = bpy.data.meshes.new(name)
        bm = bmesh.new()
        m = Matrix(((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)))
        for en in entities:
            t = m
            verts = []
            if is_.extrusion(en):
                t = convert.extrusion_to_matrix(en)
            for p in en.points:
                verts.append(bm.verts.new(self.proj((t*Vector(p)).to_3d())))
            if len(verts) > 2:
                bm.faces.new(verts)
            elif len(verts) == 2:
                bm.edges.new(verts)

        bm.to_mesh(d)
        o = bpy.data.objects.new(name, d)
        scene.objects.link(o)
        return o

    def object_mesh(self, entities, scene, name):
        """
        entities: list of DXF entities
        name: name of the returned Blender object (String)
        Accumulates all entities into a Blender bmesh and returns a Blender object containing it.
        """
        d = bpy.data.meshes.new(name)
        bm = bmesh.new()

        i = 0
        for en in entities:
            i += 1
            if en.dxftype == "3DFACE":
                self.the3dface(en, bm)
            else:
                dxftype = getattr(self, en.dxftype.lower(), None)
                if dxftype is not None:
                    dxftype(en, bm)
                else:
                    self.errors.add(en.dxftype.lower() + " - unknown dxftype")
        if i > 0:
            if hasattr(en, "thickness"):
                if en.thickness != 0:
                    self._thickness(bm, en.thickness)
            bm.to_mesh(d)
            o = bpy.data.objects.new(name, d)
            # for POLYFACE
            if hasattr(en, "extrusion"):
                self._extrusion(o, en)
            if hasattr(en, "subdivision_levels"):
                self._subdivision(o, en)
            return o
        return None

    def object_curve(self, entities, scene, name):
        """
        entities: list of DXF entities
        name: name of the returned Blender object (String)
        Accumulates all entities in the list into a Blender curve and returns a Blender object containing it.
        """
        d = bpy.data.curves.new(name, "CURVE")

        i = 0
        lines = []
        for en in entities:
            i += 1
            TYPE = en.dxftype
            if TYPE == "LINE" and self.merge_lines:
                lines.append(en)
                continue
            typefunc = getattr(self, TYPE.lower(), None)
            if typefunc is not None:
                typefunc(en, d)
            else:
                self.errors.add(en.dxftype.lower() + " - unknown dxftype")

        if len(lines) > 0:
            self._merge_lines(lines, d)

        if i > 0:
            self._check3D_object(d)
            o = bpy.data.objects.new(name, d)
            self._thickness_and_width(o, en, scene)
            self._extrusion(o, en)
            return o

        return None

    def object_surface(self, entities, scene, name):
        """
        entities: list of DXF entities
        name: name of the returned Blender object (String) (for future use and also to make it callable from
              _call_types()
        Returns None. Exports all NURB entities to ACIS files if the GUI option for it is set.
        """
        def _get_acis_filename(name, ending):
            df = self.dwg.filename
            dir = os.path.dirname(df)
            filename = bpy.path.display_name(df)
            return os.path.join(dir, "{}_{}.{}".format(filename, name, ending))

        if self.export_acis:
            for en in entities:
                if name in self.acis_files:
                    name = name + "." + str(len([n for n in self.acis_files if name in n])).zfill(3)
                # store SAB files
                if self.dwg.header.get("$ACADVER", "AC1024") > "AC1024":
                    filename = _get_acis_filename(name, "sab")
                    self.acis_files.append(name)
                    with open(filename, 'wb') as f:
                        f.write(en.acis)
                # store SAT files
                else:
                    filename = _get_acis_filename(name, "sat")
                    self.acis_files.append(name)
                    with open(filename, 'w') as f:
                        f.write('\n'.join(en.acis))
        return None

    """ ITERATE OVER DXF ENTITIES AND CREATE BLENDER OBJECTS """

    def _get_group(self, name):
        """
        name: name of group (String)
        Finds group by name or creates it if it does not exist.
        """
        groups = bpy.data.groups
        if name in groups.keys():
            group = groups[name]
        else:
            group = bpy.data.groups.new(name)
        return group

    def _call_object_types(self, TYPE, entities, group, name, scene, separated=False):
        """
        TYPE: DXF type
        entities: list of DXF entities
        group: Blender group (type: bpy_types.Group)
        name: name of the object that is being created and returned (String)
        separated: flag to make _call_types uniformly available for combined_objects() and separated_objects()
        """
        if separated:
            entity = entities[0]
        else:
            entity = entities

        # call merged geometry methods

        if TYPE is True:  # TYPE == True == is_.closed_poly_no_bulge for all entities
            o = self.polys_to_mesh(entities, scene, name)
        elif is_.mesh(TYPE):
            o = self.object_mesh(entities, scene, name)
        elif is_.curve(TYPE):
            o = self.object_curve(entities, scene, name)
        elif is_.nurbs(TYPE):
            o = self.object_surface(entities, scene, name)

        # call separate object methods (or merged geometry if TYPE depending on type)
        else:
            try:
                type_func = getattr(self, TYPE.lower(), None)
                o = type_func(entity, scene, name)
            except (AttributeError, TypeError):
                # don't call self.light(en), self.mtext(o, en), self.text(o, en) with a list of entities
                if is_.separated(TYPE) and not separated:
                    self.errors.add("DXF-Import: multiple %ss cannot be merged into a Blender object." % TYPE)
                elif TYPE == "not_mergeable":
                    self.errors.add("DXF-Import: Not mergeable dxf type '%s' should not be called in merge-mode" % TYPE)
                else:
                    self.errors.add("DXF-import: Unsupported dxftype: %s" % TYPE)
                raise

        if type(o) == bpy.types.Object:
            if o.name not in scene.objects:
                scene.objects.link(o)

            if o.name not in group.objects:
                group.objects.link(o)
        return o

    def _recenter(self, scene, name):
        bpy.context.screen.scene = scene
        scene.update()
        bpy.ops.object.select_all(action='DESELECT')

        recenter_objects = (o for o in scene.objects if "BEVEL" not in o.name and "TAPER" not in o.name
                            and o not in self.objects_before)
        xmin, ymin, zmin, xmax, ymax, zmax = self._bbox(recenter_objects, scene)
        vmin = Vector((xmin, ymin, zmin))
        vmax = Vector((xmax, ymax, zmax))
        center = vmin + (vmax - vmin) / 2
        for o in (o for o in scene.objects if "BEVEL" not in o.name and "TAPER" not in o.name
                  and o not in self.objects_before and o.parent is None):
            o.location = o.location - center
            o.select = True

        if not self.did_group_instance:
            bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

        if self.pDXF is not None:
            self.georeference(scene, center)
        else:
            scene[name + "_recenter"] = center

    def _dupliface(self, blockname, inserts, blgroup, scene):
        """
        go through all inserts and check if there is one having no rotation or extrusion (if there is none
        then there is none... any other measure to hide the original is also not water proof; just try the first
        and most obvious way and if it doesn't work, we will not try to cover uncoverable cases)
        - Place the duplicator object with a first face according to the chosen start insert.
        - Add the block as a child object to the duplicator object. If there are any recursive inserts in the
          insert, keep appending children to the children.
        - Any subsequent inserts in the list are represented just by a face in the duplicator object.
        """
        aunits = self.dwg.header.get('$AUNITS', 0)
        f = 20
        base = [
            Vector(( sqrt(((1/f)**2))/2, sqrt(((1/f)**2))/2,0)),
            Vector(( sqrt(((1/f)**2))/2,-sqrt(((1/f)**2))/2,0)),
            Vector((-sqrt(((1/f)**2))/2,-sqrt(((1/f)**2))/2,0)),
            Vector((-sqrt(((1/f)**2))/2, sqrt(((1/f)**2))/2,0)),
        ]

        bm = bmesh.new()
        location = None
        for entity in inserts:
            extrusion = convert.extrusion_to_matrix(entity)
            scale = Matrix(((entity.scale[0],0,0,0),(0,entity.scale[1],0,0),(0,0,entity.scale[2],0),(0,0,0,1)))
            rotation = radians(entity.rotation) if aunits == 0 else entity.rotation
            rotm = scale * extrusion * extrusion.Rotation(rotation, 4, "Z")
            if location is None:
                location = rotm * Vector(entity.insert)
                entity.insert = (0, 0, 0)
                transformation = rotm
            else:
                transformation = rotm.Translation((extrusion * Vector(entity.insert))-location) * rotm
            verts = []
            for v in base:
                verts.append(bm.verts.new(transformation * v))
            bm.faces.new(verts)



        m = bpy.data.meshes.new(blockname+"_geometry")
        bm.to_mesh(m)
        o = bpy.data.objects.new(blockname, m)
        o.location = location
        scene.objects.link(o)

        self._nest_block(o, blockname, blgroup, scene)
        o.dupli_type = "FACES"
        o.use_dupli_faces_scale = True
        o.dupli_faces_scale = f

    def _nest_block(self, parent, name, blgroup, scene):
        b = self.dwg.blocks[name]
        e = bpy.data.objects.new(name, None)
        scene.objects.link(e)
        #e.location = parent.location
        e.parent = parent
        for TYPE, grouped in groupsort.by_dxftype(b):
            if TYPE == "INSERT":
                for en in grouped:
                    self._nest_block(e, en.name, blgroup, scene)
            else:
                o = self._call_object_types(TYPE, grouped, blgroup, name+"_"+TYPE, scene)
                #o.location = e.location
                o.parent = e

    def combined_objects(self, entities, scene, override_name=None, override_group=None):
        """
        entities: list of dxf entities
        override_group & override_name: for use within insert() and block()
        Adds multiple dxf entities to one Blender object (per blender or dxf type).
        """
        objects = []
        for layer_name, layer_ents in groupsort.by_layer(entities):
            # group and name
            if override_group is None:
                group = self._get_group(layer_name)
            else:
                group = override_group
            if override_name is not None:
                layer_name = override_name

            # sort
            if self.combination == BY_LAYER:
                group_sorted = groupsort.by_blender_type(layer_ents)
            elif self.combination == BY_DXFTYPE or self.combination == BY_BLOCK:
                group_sorted = groupsort.by_dxftype(layer_ents)
            elif self.combination == BY_CLOSED_NO_BULGE_POLY:
                group_sorted = groupsort.by_closed_poly_no_bulge(layer_ents)
            else:
                break

            for TYPE, grouped_entities in group_sorted:
                if self.but_group_by_att and self.combination != BY_CLOSED_NO_BULGE_POLY and self.combination != BY_BLOCK:
                    for atts, by_att in groupsort.by_attributes(grouped_entities):
                        thickness, subd, width, extrusion = atts
                        if extrusion is None:  # unset extrusion defaults to (0, 0, 1)
                            extrusion = (0, 0, 1)
                        att = ""
                        if thickness != 0:
                            att += "thickness" + str(thickness) + ", "
                        if subd > 0:
                            att += "subd" + str(subd) + ", "
                        if width != [(0, 0)]:
                            att += "width" + str(width) + ", "
                        if extrusion != (0, 0, 1):
                            att += "extrusion" + str([str(round(c, 1)) + ".." + str(c)[-1:] for c in extrusion]) + ", "
                        name = layer_name + "_" + TYPE.replace("object_", "") + "_" + att

                        o = self._call_object_types(TYPE, by_att, group, name, scene, False)
                        if o is not None:
                            objects.append(o)
                else:
                    if type(TYPE) is bool and not TYPE:
                        for ttype, sub_entities in groupsort.by_blender_type(grouped_entities):
                            name = layer_name + "_" + ttype.replace("object_", "")
                            o = self._call_object_types(ttype, sub_entities, group, name, scene, False)
                            if o is not None:
                                objects.append(o)
                    else:
                        if TYPE == "INSERT" and self.combination == BY_BLOCK:
                            for NAME, grouped_inserts in groupsort.by_insert_block_name(grouped_entities):
                                sorted_inserts = []
                                separates = []
                                for i in grouped_inserts:
                                    sames = 1
                                    for c in range(2):
                                        if i.scale[c+1] - i.scale[0] < 0.00001:
                                            sames += 1
                                    if not (sames == 3 or (sames == 2 and i.scale[2] == 1)):
                                        print(i.scale)
                                        separates.append(i)
                                    else:
                                        if i.extrusion == (0, 0, 1) and i.rotation == 0.0 and i.scale == (1, 1, 1):
                                            sorted_inserts.insert(0, i)
                                        else:
                                            sorted_inserts.append(i)

                                if len(sorted_inserts) > 0:
                                    self._dupliface(NAME, sorted_inserts, group, scene)
                                for s in separates:
                                    self.insert(s, scene, NAME, group)
                        else:
                            name = layer_name + "_" + TYPE.replace("object_", "") if type(TYPE) is str else "MERGED_POLYS"
                            o = self._call_object_types(TYPE, grouped_entities, group, name, scene, False)
                            if o is not None:
                                objects.append(o)
        return objects

    def separated_entities(self, entities, scene, override_name=None, override_group=None):
        """
        entities: list of dxf entities
        override_group & override_name: for use within insert() and block()
        Adds multiple DXF entities to multiple Blender objects.
        """
        def _do_it(en):
            # group and name
            if override_group is None:
                group = self._get_group(en.layer)
            else:
                group = override_group

            if override_name is None:
                name = en.dxftype
            else:
                name = override_name

            if en.dxftype == "POINT":
                o = self.point_object(en)
            else:
                o = self._call_object_types(en.dxftype, [en], group, name, scene, separated=True)

            if o is not None:
                objects.append(o)

        objects = []
        split_entities = []

        for en in entities:
            split = convert.split_by_width(en)
            if len(split) > 1:
                split_entities.extend(split)
                continue
            _do_it(en)

        for en in split_entities:
            _do_it(en)

        return objects

    def entities(self, name, scene=None):
        """
        Iterates over all DXF entities according to the options set by user.
        """
        if scene is None:
            scene = bpy.context.scene

        self.current_scene = scene

        if self.recenter:
            self.objects_before += scene.objects[:]

        if self.combination == BY_BLOCK:
            self.combined_objects((en for en in self.dwg.modelspace()), scene)
        elif self.combination != SEPARATED:
            self.combined_objects((en for en in self.dwg.modelspace() if is_.combined_entity(en)), scene)
            self.separated_entities((en for en in self.dwg.modelspace() if is_.separated_entity(en)), scene)
        else:
            self.separated_entities((en for en in self.dwg.modelspace() if en.dxftype != "ATTDEF"), scene)

        if self.recenter:
            self._recenter(scene, name)
        elif self.pDXF is not None:
            self.georeference(scene, Vector((0, 0, 0)))

        if type(self.pScene) is TransverseMercator:
            scene['SRID'] = "tmerc"
        elif self.pScene is not None:  # assume Proj
            scene['SRID'] = re.findall("\+init=(.+)\s", self.pScene.srs)[0]

        bpy.context.screen.scene = scene

        return self.errors
        # trying to import dimensions:
        # self.separated_objects((block for block in self.dwg.blocks if block.name.startswith("*")))
