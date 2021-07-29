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

"""Geometry classes and operations.
Also, vector file representation (Art).
"""

__author__ = "howard.trickey@gmail.com"

import math

# distances less than about DISTTOL will be considered
# essentially zero
DISTTOL = 1e-3
INVDISTTOL = 1e3


class Points(object):
    """Container of points without duplication, each mapped to an int.

    Points are either have dimension at least 2, maybe more.

    Implementation:
    In order to efficiently find duplicates, we quantize the points
    to triples of ints and map from quantized triples to vertex
    index.

    Attributes:
      pos: list of tuple of float - coordinates indexed by
          vertex number
      invmap: dict of (int, int, int) to int - quantized coordinates
          to vertex number map
    """

    def __init__(self, initlist=[]):
        self.pos = []
        self.invmap = dict()
        for p in initlist:
            self.AddPoint(p)

    @staticmethod
    def Quantize(p):
        """Quantize the float tuple into an int tuple.

        Args:
          p: tuple of float
        Returns:
          tuple of int - scaled by INVDISTTOL and rounded p
        """

        return tuple([int(round(v * INVDISTTOL)) for v in p])

    def AddPoint(self, p, allowdups = False):
        """Add point p to the Points set and return vertex number.

        If there is an existing point which quantizes the same,,
        don't add a new one but instead return existing index.
        Except if allowdups is True, don't do that deduping.

        Args:
          p: tuple of float - coordinates (2-tuple or 3-tuple)
        Returns:
          int - the vertex number of added (or existing) point
        """

        qp = Points.Quantize(p)
        if qp in self.invmap and not allowdups:
            return self.invmap[qp]
        else:
            self.invmap[qp] = len(self.pos)
            self.pos.append(p)
            return len(self.pos) - 1

    def AddPoints(self, points, allowdups = False):
        """Add another set of points to this set.

        We need to return a mapping from indices
        in the argument points space into indices
        in this point space.

        Args:
          points: Points - to union into this set
        Returns:
          list of int: maps added indices to new ones
        """

        vmap = [0] * len(points.pos)
        for i in range(len(points.pos)):
            vmap[i] = self.AddPoint(points.pos[i], allowdups)
        return vmap

    def AddZCoord(self, z):
        """Change this in place to have a z coordinate, with value z.

        Assumes the coordinates are currently 2d.

        Args:
          z: the value of the z coordinate to add
        Side Effect:
          self now has a z-coordinate added
        """

        assert(len(self.pos) == 0 or len(self.pos[0]) == 2)
        newinvmap = dict()
        for i, (x, y) in enumerate(self.pos):
            newp = (x, y, z)
            self.pos[i] = newp
            newinvmap[self.Quantize(newp)] = i
        self.invmap = newinvmap

    def AddToZCoord(self, i, delta):
        """Change the z-coordinate of point with index i to add delta.

        Assumes the coordinates are currently 3d.

        Args:
          i: int - index of a point
          delta: float - value to add to z-coord
        """

        (x, y, z) = self.pos[i]
        self.pos[i] = (x, y, z + delta)


class PolyArea(object):
    """Contains a Polygonal Area (polygon with possible holes).

    A polygon is a list of vertex ids, each an index given by
    a Points object. The list represents a CCW-oriented
    outer boundary (implicitly closed).
    If there are holes, they are lists of CW-oriented vertices
    that should be contained in the outer boundary.
    (So the left face of both the poly and the holes is
    the filled part.)

    Attributes:
      points: Points
      poly: list of vertex ids
      holes: list of lists of vertex ids (each a hole in poly)
      data: any - application data (can hold color, e.g.)
    """

    def __init__(self, points=None, poly=None, holes=None, data=None):
        self.points = points if points else Points()
        self.poly = poly if poly else []
        self.holes = holes if holes else []
        self.data = data

    def AddHole(self, holepa):
        """Add a PolyArea's poly as a hole of self.

        Need to reverse the contour and
        adjust the the point indexes and self.points.

        Args:
          holepa: PolyArea
        """

        vmap = self.points.AddPoints(holepa.points)
        holepoly = [vmap[i] for i in holepa.poly]
        holepoly.reverse()
        self.holes.append(holepoly)

    def ContainsPoly(self, poly, points):
        """Tests if poly is contained within self.poly.

        Args:
          poly: list of int - indices into points
          points: Points - maps to coords
        Returns:
          bool - True if poly is fully contained within self.poly
        """

        for v in poly:
            if PointInside(points.pos[v], self.poly, self.points) == -1:
                return False
        return True

    def Normal(self):
        """Returns the normal of the polyarea's main poly."""

        pos = self.points.pos
        poly = self.poly
        if len(pos) == 0 or len(pos[0]) == 2 or len(poly) == 0:
            print("whoops, not enough info to calculate normal")
            return (0.0, 0.0, 1.0)
        return Newell(poly, self.points)


class PolyAreas(object):
    """Contains a list of PolyAreas and a shared Points.

    Attributes:
      polyareas: list of PolyArea
      points: Points
    """

    def __init__(self):
        self.polyareas = []
        self.points = Points()

    def scale_and_center(self, scaled_side_target):
        """Adjust the coordinates of the polyareas so that
        it is centered at the origin and has its longest
        dimension scaled to be scaled_side_target."""

        if len(self.points.pos) == 0:
            return
        (minv, maxv) = self.bounds()
        maxside = max([maxv[i] - minv[i] for i in range(2)])
        if maxside > 0.0:
            scale = scaled_side_target / maxside
        else:
            scale = 1.0
        translate = [-0.5 * (maxv[i] + minv[i]) for i in range(2)]
        dim = len(self.points.pos[0])
        if dim == 3:
            translate.append([0.0])
        for v in range(len(self.points.pos)):
            self.points.pos[v] = tuple([scale * (self.points.pos[v][i] + \
                translate[i]) for i in range(dim)])

    def bounds(self):
        """Find bounding box of polyareas in xy.

        Returns:
          ([minx,miny],[maxx,maxy]) - all floats
        """

        huge = 1e100
        minv = [huge, huge]
        maxv = [-huge, -huge]
        for pa in self.polyareas:
            for face in [pa.poly] + pa.holes:
                for v in face:
                    vcoords = self.points.pos[v]
                    for i in range(2):
                        if vcoords[i] < minv[i]:
                            minv[i] = vcoords[i]
                        if vcoords[i] > maxv[i]:
                            maxv[i] = vcoords[i]
        if minv[0] == huge:
            minv = [0.0, 0.0]
        if maxv[0] == huge:
            maxv = [0.0, 0.0]
        return (minv, maxv)


class Model(object):
    """Contains a generic 3d model.

    A generic 3d model has vertices with 3d coordinates.
    Each vertex gets a 'vertex id', which is an index that
    can be used to refer to the vertex and can be used
    to retrieve the 3d coordinates of the point.

    The actual visible part of the geometry are the faces,
    which are n-gons (n>2), specified by a vector of the
    n corner vertices.
    Faces may also have data associated with them,
    and the data will be copied into newly created faces
    from the most likely neighbor faces..

    Attributes:
      points: geom.Points - the 3d vertices
      faces: list of list of indices (each a CCW traversal of a face)
      face_data: list of any - if present, is parallel to
          faces list and holds arbitrary data
    """

    def __init__(self):
        self.points = Points()
        self.faces = []
        self.face_data = []


class Art(object):
    """Contains a vector art diagram.

    Attributes:
      paths: list of Path objects
    """

    def __init__(self):
        self.paths = []


class Paint(object):
    """A color or pattern to fill or stroke with.

    For now, just do colors, but could later do
    patterns or images too.

    Attributes:
      color: (r,g,b) triple of floats, 0.0=no color, 1.0=max color
    """

    def __init__(self, r=0.0, g=0.0, b=0.0):
        self.color = (r, g, b)

    @staticmethod
    def CMYK(c, m, y, k):
        """Return Paint specified in CMYK model.

        Uses formula from 6.2.4 of PDF Reference.

        Args:
          c, m, y, k: float - in range [0, 1]
        Returns:
          Paint - with components in rgb form now
        """

        return Paint(1.0 - min(1.0, c + k),
            1.0 - min(1.0, m + k), 1.0 - min(1.0, y + k))

black_paint = Paint()
white_paint = Paint(1.0, 1.0, 1.0)

ColorDict = {
    'aqua': Paint(0.0, 1.0, 1.0),
    'black': Paint(0.0, 0.0, 0.0),
    'blue': Paint(0.0, 0.0, 1.0),
    'fuchsia': Paint(1.0, 0.0, 1.0),
    'gray': Paint(0.5, 0.5, 0.5),
    'green': Paint(0.0, 0.5, 0.0),
    'lime': Paint(0.0, 1.0, 0.0),
    'maroon': Paint(0.5, 0.0, 0.0),
    'navy': Paint(0.0, 0.0, 0.5),
    'olive': Paint(0.5, 0.5, 0.0),
    'purple': Paint(0.5, 0.0, 0.5),
    'red': Paint(1.0, 0.0, 0.0),
    'silver': Paint(0.75, 0.75, 0.75),
    'teal': Paint(0.0, 0.5, 0.5),
    'white': Paint(1.0, 1.0, 1.0),
    'yellow': Paint(1.0, 1.0, 0.0)
}


class Path(object):
    """Represents a path in the PDF sense, with painting instructions.

    Attributes:
      subpaths: list of Subpath objects
      filled: True if path is to be filled
      fillevenodd: True if use even-odd rule to fill (else non-zero winding)
      stroked: True if path is to be stroked
      fillpaint: Paint to fill with
      strokepaint: Paint to stroke with
    """

    def __init__(self):
        self.subpaths = []
        self.filled = False
        self.fillevenodd = False
        self.stroked = False
        self.fillpaint = black_paint
        self.strokepaint = black_paint

    def AddSubpath(self, subpath):
        """"Add a subpath."""

        self.subpaths.append(subpath)

    def Empty(self):
        """Returns True if this Path as no subpaths."""

        return not self.subpaths


class Subpath(object):
    """Represents a subpath in PDF sense, either open or closed.

    We'll represent lines, bezier pieces, circular arc pieces
    as tuples with letters giving segment type in first position
    and coordinates (2-tuples of floats) in the other positions.

    Segment types:
     ('L', a, b)       - line from a to b
     ('B', a, b, c, d) - cubic bezier from a to b, with control points c,d
     ('Q', a, b, c)    - quadratic bezier from a to b, with 1 control point c
     ('A', a, b, rad, xrot, large-arc, ccw) - elliptical arc from a to b,
       with rad=(rx, ry) as radii, xrot is x-axis rotation in degrees,
       large-arc is True if arc should be >= 180 degrees,
       ccw is True if start->end follows counter-clockwise direction
       (see SVG spec); note that after rad,
       the rest are floats or bools, not coordinate pairs
    Note that s[1] and s[2] are the start and end points for any segment s.

    Attributes:
      segments: list of segment tuples (see above)
      closed: True if closed
    """

    def __init__(self):
        self.segments = []
        self.closed = False

    def Empty(self):
        """Returns True if this subpath as no segments."""

        return not self.segments

    def AddSegment(self, seg):
        """Add a segment."""

        self.segments.append(seg)

    @staticmethod
    def SegStart(s):
        """Return start point for segment.

        Args:
          s: a segment tuple
        Returns:
          (float, float): the coordinates of the segment's start point
        """

        return s[1]

    @staticmethod
    def SegEnd(s):
        """Return end point for segment.

        Args:
          s: a segment tuple
        Returns:
          (float, float): the coordinates of the segment's end point
        """

        return s[2]


class TransformMatrix(object):
    """Transformation matrix for 2d coordinates.

    The transform matrix is:
      [ a b 0 ]
      [ c d 0 ]
      [ e f 1 ]
    and coordinate tranformation is defined by:
      [x' y' 1] = [x y 1] x TransformMatrix

    Attributes:
      a, b, c, d, e, f: floats
    """

    def __init__(self, a=1.0, b=0.0, c=0.0, d=1.0, e=0.0, f=0.0):
        self.a = a
        self.b = b
        self.c = c
        self.d = d
        self.e = e
        self.f = f

    def __str__(self):
        return str([self.a, self.b, self.c, self.d, self.e, self.f])

    def Copy(self):
        """Return a copy of this matrix."""

        return TransformMatrix(self.a, self.b, self.c, self.d, self.e, self.f)

    def ComposeTransform(self, a, b, c, d, e, f):
        """Apply the transform given the the arguments on top of this one.

        This is accomplished by returning t x sel
        where t is the transform matrix that would be formed from the args.

        Arguments:
          a, b, c, d, e, f: float - defines a composing TransformMatrix
        """

        newa = a * self.a + b * self.c
        newb = a * self.b + b * self.d
        newc = c * self.a + d * self.c
        newd = c * self.b + d * self.d
        newe = e * self.a + f * self.c + self.e
        newf = e * self.b + f * self.d + self.f
        self.a = newa
        self.b = newb
        self.c = newc
        self.d = newd
        self.e = newe
        self.f = newf

    def Apply(self, pt):
        """Return the result of applying this tranform to pt = (x,y).

        Arguments:
          (x, y) : (float, float)
        Returns:
          (x', y'): 2-tuple of floats, the result of [x y 1] x self
        """

        (x, y) = pt
        return (self.a * x + self.c * y + self.e, \
            self.b * x + self.d * y + self.f)


def ApproxEqualPoints(p, q):
    """Return True if p and q are approximately the same points.

    Args:
      p: n-tuple of float
      q: n-tuple of float
    Returns:
      bool - True if the 1-norm <= DISTTOL
    """

    for i in range(len(p)):
        if abs(p[i] - q[i]) > DISTTOL:
            return False
        return True


def PointInside(v, a, points):
    """Return 1, 0, or -1 as v is inside, on, or outside polygon.

    Cf. Eric Haines ptinpoly in Graphics Gems IV.

    Args:
      v : (float, float) or (float, float, float) - coordinates of a point
      a : list of vertex indices defining polygon (assumed CCW)
      points: Points - to get coordinates for polygon
    Returns:
      1, 0, -1: as v is inside, on, or outside polygon a
    """

    (xv, yv) = (v[0], v[1])
    vlast = points.pos[a[-1]]
    (x0, y0) = (vlast[0], vlast[1])
    if x0 == xv and y0 == yv:
        return 0
    yflag0 = y0 > yv
    inside = False
    n = len(a)
    for i in range(0, n):
        vi = points.pos[a[i]]
        (x1, y1) = (vi[0], vi[1])
        if x1 == xv and y1 == yv:
            return 0
        yflag1 = y1 > yv
        if yflag0 != yflag1:
            xflag0 = x0 > xv
            xflag1 = x1 > xv
            if xflag0 == xflag1:
                if xflag0:
                    inside = not inside
            else:
                z = x1 - (y1 - yv) * (x0 - x1) / (y0 - y1)
                if z >= xv:
                    inside = not inside
        x0 = x1
        y0 = y1
        yflag0 = yflag1
    if inside:
        return 1
    else:
        return -1


def SignedArea(polygon, points):
    """Return the area of the polgon, positive if CCW, negative if CW.

    Args:
      polygon: list of vertex indices
      points: Points
    Returns:
      float - area of polygon, positive if it was CCW, else negative
    """

    a = 0.0
    n = len(polygon)
    for i in range(0, n):
        u = points.pos[polygon[i]]
        v = points.pos[polygon[(i + 1) % n]]
        a += u[0] * v[1] - u[1] * v[0]
    return 0.5 * a


def VecAdd(a, b):
    """Return vector a-b.

    Args:
      a: n-tuple of floats
      b: n-tuple of floats
    Returns:
      n-tuple of floats - pairwise addition a+b
    """

    n = len(a)
    assert(n == len(b))
    return tuple([a[i] + b[i] for i in range(n)])


def VecSub(a, b):
    """Return vector a-b.

    Args:
      a: n-tuple of floats
      b: n-tuple of floats
    Returns:
      n-tuple of floats - pairwise subtraction a-b
    """

    n = len(a)
    assert(n == len(b))
    return tuple([a[i] - b[i] for i in range(n)])


def VecDot(a, b):
    """Return the dot product of two vectors.

    Args:
      a: n-tuple of floats
      b: n-tuple of floats
    Returns:
      n-tuple of floats - dot product of a and b
    """

    n = len(a)
    assert(n == len(b))
    sum = 0.0
    for i in range(n):
        sum += a[i] * b[i]
    return sum


def VecLen(a):
    """Return the Euclidean length of the argument vector.

    Args:
      a: n-tuple of floats
    Returns:
      float: the 2-norm of a
    """

    s = 0.0
    for v in a:
        s += v * v
    return math.sqrt(s)


def Newell(poly, points):
    """Use Newell method to find polygon normal.

    Assume poly has length at least 3 and points are 3d.

    Args:
      poly: list of int - indices into points.pos
      points: Points - assumed 3d
    Returns:
      (float, float, float) - the average normal
    """

    sumx = 0.0
    sumy = 0.0
    sumz = 0.0
    n = len(poly)
    pos = points.pos
    for i, ai in enumerate(poly):
        bi = poly[(i + 1) % n]
        a = pos[ai]
        b = pos[bi]
        sumx += (a[1] - b[1]) * (a[2] + b[2])
        sumy += (a[2] - b[2]) * (a[0] + b[0])
        sumz += (a[0] - b[0]) * (a[1] + b[1])
    return Norm3(sumx, sumy, sumz)


def Norm3(x, y, z):
    """Return vector (x,y,z) normalized by dividing by squared length.
    Return (0.0, 0.0, 1.0) if the result is undefined."""
    sqrlen = x * x + y * y + z * z
    if sqrlen < 1e-100:
        return (0.0, 0.0, 1.0)
    else:
        try:
            d = math.sqrt(sqrlen)
            return (x / d, y / d, z / d)
        except:
            return (0.0, 0.0, 1.0)


# We're using right-hand coord system, where
# forefinger=x, middle=y, thumb=z on right hand.
# Then, e.g., (1,0,0) x (0,1,0) = (0,0,1)
def Cross3(a, b):
    """Return the cross product of two vectors, a x b."""

    (ax, ay, az) = a
    (bx, by, bz) = b
    return (ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx)


def MulPoint3(p, m):
    """Return matrix multiplication of p times m
    where m is a 4x3 matrix and p is a 3d point, extended with 1."""

    (x, y, z) = p
    return (x * m[0] + y * m[3] + z * m[6] + m[9],
        x * m[1] + y * m[4] + z * m[7] + m[10],
        x * m[2] + y * m[5] + z * m[8] + m[11])
