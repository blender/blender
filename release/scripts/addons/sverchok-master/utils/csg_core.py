import math
from sverchok.utils.csg_geom import *


class CSG(object):
    """
    ## License
    Copyright (c) 2011 Evan Wallace (http://madebyevan.com/), under the MIT license.
    Python port Copyright (c) 2012 Tim Knip (http://www.floorplanner.com), under the MIT license.
    """
    def __init__(self):
        self.polygons = []

    @classmethod
    def fromPolygons(cls, polygons):
        csg = CSG()
        csg.polygons = polygons
        return csg

    def clone(self):
        csg = CSG()
        csg.polygons = map(lambda p: p.clone(), self.polygons)
        return csg

    def toPolygons(self):
        return self.polygons

    def union(self, csg):
        a = CSGNode(self.clone().polygons)
        b = CSGNode(csg.clone().polygons)
        a.clipTo(b)
        b.clipTo(a)
        b.invert()
        b.clipTo(a)
        b.invert()
        a.build(b.allPolygons())
        return CSG.fromPolygons(a.allPolygons())

    def subtract(self, csg):
        a = CSGNode(self.clone().polygons)
        b = CSGNode(csg.clone().polygons)
        a.invert()
        a.clipTo(b)
        b.clipTo(a)
        b.invert()
        b.clipTo(a)
        b.invert()
        a.build(b.allPolygons())
        a.invert()
        return CSG.fromPolygons(a.allPolygons())

    def intersect(self, csg):
        a = CSGNode(self.clone().polygons)
        b = CSGNode(csg.clone().polygons)
        a.invert()
        b.clipTo(a)
        b.invert()
        a.clipTo(b)
        b.clipTo(a)
        a.build(b.allPolygons())
        a.invert()
        return CSG.fromPolygons(a.allPolygons())

    def inverse(self):
        """
        Return a new CSG solid with solid and empty space switched. This solid is
        not modified.
        """
        csg = self.clone()
        map(lambda p: p.flip(), csg.polygons)
        return csg

    @classmethod
    def Obj_from_pydata(cls, verts, faces):
        """

        """
        polygons = []
        for face in faces:
            polyg = []
            for idx in face:
                co = verts[idx]
                polyg.append(CSGVertex(co))
            polygons.append(CSGPolygon(polyg))

        return CSG.fromPolygons(polygons)
