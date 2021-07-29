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

'''
None of this file is in a working condition. skip this file.

Eventual purpose of this file is to store the convenience functions which
can be used for regular nodes or as part of recipes for script nodes. These
functions will initially be sub optimal quick implementations, then optimized
only for speed, never for aesthetics or line count or cleverness.

'''

import math
import numpy as np
from functools import wraps
import time

import bpy
import bmesh
import mathutils
from mathutils import Matrix

from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.utils.sv_bmesh_utils import pydata_from_bmesh
from sverchok.data_structure import match_long_repeat

identity_matrix = Matrix()

# constants
PI = math.pi
HALF_PI = PI / 2
QUARTER_PI = PI / 4
TAU = PI * 2
TWO_PI = TAU
N = identity_matrix

# ----------------- vectorize wrapper ---------------


def vectorize(func):
    '''
    Will create a yeilding vectorized generator of the
    function it is applied to.
    Note: parameters must be passed as kw arguments
    '''
    @wraps(func)
    def inner(**kwargs):
        names, values = kwargs.keys(), kwargs.values()
        values = match_long_repeat(values)
        multiplex = {k:v for k, v in zip(names, values)}
        for i in range(len(values[0])):
            single_kwargs = {k:v[i] for k, v in multiplex.items()}
            yield func(**single_kwargs)

    return inner


# ----------------- sn1 specific helper for autowrapping to iterables ----
# this will be moved to elsewhere.

def sn1_autowrap(*params):
    for p in params:
        if isinstance(p, (float, int)):
            p = [p]
        yield p

def sn1_autodict(names, var_dict):
    return {k:v for k, v in var_dict.items() if k in set(names.split(' '))}


# ----------------- light weight functions ---------------


def circle(radius=1.0, phase=0, nverts=20, matrix=None, mode='pydata'):
    '''
    parameters:
        radius: float
        phase:  where to start the unit circle
        nverts: number of verts of the circle
        matrix: transformation matrix [not implemented yet]
        mode:   'np' or 'pydata'

        :  'pydata'
        usage:
            Verts, Edges, Faces = circle(nverts=20, radius=1.6, mode='pydata')
        info:
            Each return type will be a nested list.
            Verts: will generate [[x0,y0,z0],[x1,y1,z1], ....[xN,yN,zN]]
            Edges: will generate [[a,b],[b,c], ....[n,a]]
            Faces: a single wrapped polygon around the bounds of the shape

        :  'np'
        usage:
            Verts, Edges, Faces = circle(nverts=20, radius=1.6, mode='np')

    outputs Verts, Edges, Faces

        info:
            Each return type will be a numpy array
            Verts: generates [n*4] - Array([[x0,y0,z0,w0],[x1,y1,z1,w1], ....[xN,yN,zN,wN]])
            Edges: will be a [n*2] - Array([[a,b],[b,c], ....[n,a]])
            Faces: a single wrapped polygon around the bounds of the shape

            to convert to pydata please consult the numpy manual.

    '''

    if mode in {'pydata', 'bm'}:

        verts = []
        theta = TAU / nverts
        for i in range(nverts):
            rad = i * theta
            verts.append((math.sin(rad + phase) * radius, math.cos(rad + phase) * radius, 0))

        edges = [[i, i+1] for i in range(nverts-1)] + [[nverts-1, 0]]
        faces = [i for i in range(nverts)]

        if mode == 'pydata':
            return verts, edges, [faces]
        else:
            return bmesh_from_pydata(verts, edges, [faces])

    if mode == 'np':

        t = np.linspace(0, np.pi * 2 * (nverts - 1 / nverts), nverts)
        circ = np.array([np.cos(t + phase) * radius, np.sin(t + phase) * radius, np.zeros(nverts), np.zeros(nverts)])
        verts = np.transpose(circ)
        edges = np.array([[i, i+1] for i in range(nverts-1)] + [[nverts-1, 0]])
        faces = np.array([[i for i in range(nverts)]])
        return verts, edges, faces



def arc(radius=1.0, phase=0, angle=PI, nverts=20, matrix=None, mode='pydata'):
    '''
    arc is similar to circle, with the exception that it does not close.

    parameters:
        radius: float
        phase:  where to start the arc
        nverts: number of verts of the arc
        matrix: transformation matrix [not implemented yet]
        mode:   'np' or 'pydata'

    outputs Verts, Edges, Faces

        info:
            Each return type will be a nested list.
            Verts: will generate [[x0,y0,z0],[x1,y1,z1], ....[xN,yN,zN]]
            Edges: will generate [[a,b],[b,c], ...] (not cyclic)
            Faces: a single wrapped polygon around the bounds of the shape

    '''

    if mode in {'pydata', 'bm'}:

        verts = []
        theta = angle / (nverts-1)
        for i in range(nverts):
            rad = i * theta
            verts.append((math.sin(rad + phase) * radius, math.cos(rad + phase) * radius, 0))

        edges = [[i, i+1] for i in range(nverts-1)]
        faces = [i for i in range(nverts)]

        if mode == 'pydata':
            return verts, edges, [faces]
        else:
            return bmesh_from_pydata(verts, edges, [faces])

    if mode == 'np':

        t = np.linspace(0, angle, nverts)
        circ = np.array([np.cos(t + phase) * radius, np.sin(t + phase) * radius, np.zeros(nverts), np.zeros(nverts)])
        verts = np.transpose(circ)
        edges = np.array([[i, i+1] for i in range(nverts-1)])
        faces = np.array([[i for i in range(nverts)]])
        return verts, edges, faces


def quad(side=1.0, radius=0.0, nverts=5, matrix=None, mode='pydata'):
    '''
    parameters:
        side:   gives the length of side of the rect
        radius: gives the radius of the rounded corners.
                - If the passed radius is equal to side/2 then you'll get a circle
                - if the passed radius exceeds side/2, then you will get rect
        nverts: if nverts is equal or greater than 2 then you will get rounded courners
                if the above radius is smaller or equal to side/2.
        matrix: ---
        mode:   ---

    outputs Verts, Edges, Faces

        info:
            Each return type will be a nested list.
            Verts: will generate [[x0,y0,z0],[x1,y1,z1], ....[xN,yN,zN]]
            Edges: will generate [[a,b],[b,c], ....[n,a]]
            Faces: a single wrapped polygon around the bounds of the shape


    '''

    if mode in {'pydata', 'bm'}:
        dim = side / 2

        edges, faces = [], []

        if radius > 0.0 and radius < dim and nverts >= 2:
            verts = []
            theta = HALF_PI / (nverts-1)
            ext = dim - radius
            coords = [[ext, ext], [ext, -ext], [-ext, -ext], [-ext, ext]]
            for (x, y), corner in zip(coords, range(4)):
                for i in range(nverts):
                    rad = theta * i
                    verts.append(((math.sin(rad + (corner*HALF_PI)) * radius) + x, (math.cos(rad + (corner*HALF_PI)) * radius) + y, 0))

        elif radius > 0.0 and radius == dim and nverts >= 2:
            verts, edges, faces = circle(radius=dim, nverts=((nverts*4)-4))

        else:
            verts = [[-dim, dim, 0], [dim, dim, 0], [dim, -dim, 0], [-dim, -dim, 0]]
        # elif radius == 0.0 or (radius > 0.0 and radius > dim):

        num_verts = len(verts)
        if not edges:
            edges = [[i, i+1] for i in range(num_verts-1)] + [[num_verts-1, 0]]
            faces = [i for i in range(num_verts)]

        if mode == 'pydata':
            return verts, edges, [faces]
        else:
            return bmesh_from_pydata(verts, edges, [faces])

    if mode == 'np':
        pass


def arc_slice(outer_radius=1.0, inner_radius=0.8, phase=0, angle=PI, nverts=20, matrix=None, mode='pydata'):
    '''
    this generator makes a flat donut section. Like arc, but with a inner and outer radius to determine
    the thickness of the slice.

    '''
    if mode in {'pydata', 'bm'}:

        # if outer_radius == inner_radius:
        #    return arc ? :)  or [], [], []

        if outer_radius < inner_radius:
            outer_radius, inner_radius = inner_radius, outer_radius

        verts = []
        theta = angle / (nverts-1)

        for i in range(nverts):
            rad = i * theta
            verts.append((math.sin(rad + phase) * outer_radius, math.cos(rad + phase) * outer_radius, 0))

        for i in reversed(range(nverts)):
            rad = i * theta
            verts.append((math.sin(rad + phase) * inner_radius, math.cos(rad + phase) * inner_radius, 0))

        num_verts = len(verts)
        edges = [[i, i+1] for i in range(num_verts-1)] + [[num_verts-1, 0]]
        faces = [i for i in range(num_verts)]

        if mode == 'pydata':
            return verts, edges, [faces]
        else:
            return bmesh_from_pydata(verts, edges, [faces])

    if mode == 'np':
        pass


def rect(dim_x=1.0, dim_y=1.62, radius=0.0, nverts=5, matrix=None, mode='pydata'):

    xdim = dim_x / 2
    ydim = dim_y / 2

    if mode in {'pydata', 'bm'}:
        verts = []

        if radius == 0.0 or nverts < 2:
            verts = [[-xdim, ydim, 0], [xdim, ydim, 0], [xdim, -ydim, 0], [-xdim, -ydim, 0]]

        elif radius > 0.0 and radius < min(abs(dim_x), abs(dim_y)) and nverts >= 2:
            theta = HALF_PI / (nverts-1)
            xdim = xdim - radius
            ydim = ydim - radius
            coords = [[xdim, ydim], [xdim, -ydim], [-xdim, -ydim], [-xdim, ydim]]
            for (x, y), corner in zip(coords, range(4)):
                for i in range(nverts):
                    rad = theta * i
                    verts.append(((math.sin(rad + (corner*HALF_PI)) * radius) + x, (math.cos(rad + (corner*HALF_PI)) * radius) + y, 0))

        num_verts = len(verts)
        edges = [[i, i+1] for i in range(num_verts-1)] + [[num_verts-1, 0]]
        faces = [i for i in range(num_verts)]

        if mode == 'pydata':
            return verts, edges, [faces]
        else:
            return bmesh_from_pydata(verts, edges, [faces])

    if mode == 'np':
        pass



def grid(dim_x=1.0, dim_y=1.62, nx=2, ny=2, anchor=0, matrix=None, mode='pydata'):
    '''

    dim_x   -   total dimension on x side
    dim_y   -   total dimension on y side
    nx      -   num verts on x side
    ny      -   num verts on y side
    anchor  -   1 --- 2 --- 3
                -           -
                8     0     4
                -           -
                7 --- 6 --- 5
                default is centered (0)

    '''

    xside = dim_x / 2
    yside = dim_y / 2
    nx = max(2, nx)
    ny = max(2, ny)

    anchors = {
        1: (0,      dim_x, 0,      dim_y),
        2: (-xside, xside, 0,      dim_y),
        3: (-dim_x, 0,     0,      dim_y),
        4: (-dim_x, 0,     -yside, yside),
        5: (-dim_x, 0,     0,     -dim_y),
        6: (-xside, xside, 0,     -dim_y),
        7: (0,      dim_x, 0,     -dim_y),
        8: (0,      dim_x, -yside, yside),
        0: (-xside, xside, -yside, yside)
    }.get(anchor, (-xside, xside, -yside, yside))


    if mode in {'pydata', 'bm'}:
        verts = []
        faces = []
        add_face = faces.append
        total_range = ((ny-1) * (nx))

        a, b = anchors[:2]
        c, d = anchors[2:]
        x = np.linspace(a, b, nx)
        y = np.linspace(c, d, ny)
        f = np.vstack(np.meshgrid(x, y, 0)).reshape(3, -1).T
        verts = f.tolist()

        for i in range(total_range):
            if not ((i + 1) % nx == 0):  # +1 is the shift
                add_face([i, i+nx, i+nx+1, i+1])  # clockwise

        if mode == 'pydata':
            return verts, [], faces
        else:
            return bmesh_from_pydata(vert, [], faces)


    if mode == 'np':
        pass


def line(p1=[(0,0,0)], p2=[(1,0,0)], nverts=2, mode='pydata'):
    '''
    line(p1=[(0,0,0)], p2=[(1,0,0)], nverts=2, mode='pydata')
    not finished..

    '''
    nv = nverts

    if mode in {'pydata', 'bm'}:
        verts = []
        edges = []

        num_verts = 0
        for v1, v2 in zip(p1, p2):
            if nv == 2:
                verts.extend([v1, v2])
            elif nv > 2:
                x_seg = (v2[0] - v1[0]) / (nv-1)
                y_seg = (v2[1] - v1[1]) / (nv-1)
                z_seg = (v2[2] - v1[2]) / (nv-1)
                verts.append(v1)
                verts.extend([[v1[0] + (x_seg * i), v1[1] + (y_seg * i), v1[2] + (z_seg * i)] for i in range(1, nv-1)])
                verts.append(v2)

            edges.extend([[i + num_verts, i + 1 + num_verts] for i in range(nv-1)])
            num_verts = len(verts)

        if mode == 'pydata':
            return verts, edges
        else:
            return bmesh_from_pydata(verts, edges, [])

    if mode == 'np':
        pass


# ----------- vectorized forms


arcs = vectorize(arc)
arc_slices = vectorize(arc_slice)
circles = vectorize(circle)
quads = vectorize(quad)
rects = vectorize(rect)
lines = vectorize(line)
grids = vectorize(grid)

################################################
# Newer implementation of spline interpolation
# by zeffii, ly29 and portnov
# based on implementation from looptools 4.5.2 done by Bart Crouch
# factored out from interpolation_mk3 node
################################################

class Spline(object):
    """
    Base abstract class for LinearSpline and CubicSpline.
    """
    @classmethod
    def create_knots(cls, pts, metric="DISTANCE"):
        if metric == "DISTANCE":
            tmp = np.linalg.norm(pts[:-1] - pts[1:], axis=1)
            tknots = np.insert(tmp, 0, 0).cumsum()
            tknots = tknots / tknots[-1]
        elif metric == "MANHATTAN":
            tmp = np.sum(np.absolute(pts[:-1] - pts[1:]), 1)
            tknots = np.insert(tmp, 0, 0).cumsum()
            tknots = tknots / tknots[-1]
        elif metric == "POINTS":
            tknots = np.linspace(0, 1, len(pts))
        elif metric == "CHEBYSHEV":
            tknots = np.max(np.absolute(pts[1:] - pts[:-1]), 1)
            tknots = np.insert(tknots, 0, 0).cumsum()
            tknots = tknots / tknots[-1]

        return tknots

    def __init__(self):
        # Caches
        # t -> vertex
        self._single_eval_cache = {}

    def length(self, t_in):
        """
        t_in: np.array with values in [0,1]
        """
        t_in = t_in.copy()
        t_in.sort()
        points_on_spline = self.eval(t_in)
        t = points_on_spline[:-1] - points_on_spline[1:]
        norms = np.linalg.norm(t, axis=1)
        return norms.sum()
    
    def eval_at_point(self, t):
        """
        Evaluate spline at single point.
        t: float in [0,1].
        Returns vector in Sverchok format (tuple of floats).
        """
        result = self._single_eval_cache.get(t, None)
        if result is not None:
            return result
        else:
            result = self.eval(np.array([t]))
            result = tuple(result[0])
            self._single_eval_cache[t] = result
            return result

class CubicSpline(Spline):
    def __init__(self, vertices, tknots = None, metric = None, is_cyclic = False):
        """
        vertices: vertices in Sverchok's format (list of tuples)
        tknots: np.array of shape (n-1,). If not provided - calculated automatically based on metric
        metric: string, one of "DISTANCE", "MANHATTAN", "POINTS", "CHEBYSHEV". Mandatory if tknots
                is not provided
        is_cyclic: whether the spline is cyclic

        creates a cubic spline thorugh the locations given in vertices
        """

        super().__init__()

        if is_cyclic:

            locs = np.array(vertices[-4:] + vertices + vertices[:4])
            if tknots is None:
                if metric is None:
                    raise Exception("CubicSpline: either tknots or metric must be specified")
                tknots = Spline.create_knots(locs, metric)
                scale = 1 / (tknots[-4] - tknots[4])
                base = tknots[4]
                tknots -= base
                tknots *= scale
        else:
            locs = np.array(vertices)
            if tknots is None:
                if metric is None:
                    raise Exception("CubicSpline: either tknots or metric must be specified")
                tknots = Spline.create_knots(locs, metric)

        self.tknots = tknots
        self.is_cyclic = is_cyclic

        n = len(locs)
        if n < 2:
            raise Exception("Cubic spline can't be build from less than 3 vertices")

        # a = locs
        h = tknots[1:] - tknots[:-1]
        h[h == 0] = 1e-8
        q = np.zeros((n - 1, 3))
        q[1:] = 3 / h[1:, np.newaxis] * (locs[2:] - locs[1:-1]) - 3 / \
            h[:-1, np.newaxis] * (locs[1:-1] - locs[:-2])

        l = np.zeros((n, 3))
        l[0, :] = 1.0
        u = np.zeros((n - 1, 3))
        z = np.zeros((n, 3))

        for i in range(1, n - 1):
            l[i] = 2 * (tknots[i + 1] - tknots[i - 1]) - h[i - 1] * u[i - 1]
            l[i, l[i] == 0] = 1e-8
            u[i] = h[i] / l[i]
            z[i] = (q[i] - h[i - 1] * z[i - 1]) / l[i]
        l[-1, :] = 1.0
        z[-1] = 0.0

        b = np.zeros((n - 1, 3))
        c = np.zeros((n, 3))

        for i in range(n - 2, -1, -1):
            c[i] = z[i] - u[i] * c[i + 1]
        b = (locs[1:] - locs[:-1]) / h[:, np.newaxis] - h[:, np.newaxis] * (c[1:] + 2 * c[:-1]) / 3
        d = (c[1:] - c[:-1]) / (3 * h[:, np.newaxis])

        splines = np.zeros((n - 1, 5, 3))
        splines[:, 0] = locs[:-1]
        splines[:, 1] = b
        splines[:, 2] = c[:-1]
        splines[:, 3] = d
        splines[:, 4] = tknots[:-1, np.newaxis]
        
        self.splines = splines

    def eval(self, t_in, tknots = None):
        """
        Evaluate the spline at the points in t_in, which must be an array
        with values in [0,1]
        returns and np array with the corresponding points
        """

        if tknots is None:
            tknots = self.tknots

        index = tknots.searchsorted(t_in, side='left') - 1
        index = index.clip(0, len(self.splines) - 1)
        to_calc = self.splines[index]
        ax, bx, cx, dx, tx = np.swapaxes(to_calc, 0, 1)
        t_r = t_in[:, np.newaxis] - tx
        out = ax + t_r * (bx + t_r * (cx + t_r * dx))
        return out

    def tangent(self, t_in, h=0.001, tknots=None):
        """
        Calc numerical tangents for spline at t_in
        """

        if tknots is None:
            tknots = self.tknots

        t_ph = t_in + h
        t_mh = t_in - h
        t_less_than_0 = t_mh < 0.0
        t_great_than_1 = t_ph > 1.0
        t_mh[t_less_than_0] += h
        t_ph[t_great_than_1] -= h
        tanget_ph = self.eval(t_ph)
        tanget_mh = self.eval(t_mh)
        tanget = tanget_ph - tanget_mh
        tanget[t_less_than_0 | t_great_than_1] *= 2
        return tanget

class LinearSpline(Spline):
    def __init__(self, vertices, tknots = None, metric = None, is_cyclic = False):
        """
        vertices: vertices in Sverchok's format (list of tuples)
        tknots: np.array of shape (n-1,). If not provided - calculated automatically based on metric
        metric: string, one of "DISTANCE", "MANHATTAN", "POINTS", "CHEBYSHEV". Mandatory if tknots
                is not provided
        is_cyclic: whether the spline is cyclic

        creates a cubic spline thorugh the locations given in vertices
        """

        super().__init__()

        if is_cyclic:
            pts = np.array(vertices + [vertices[0]])
        else:
            pts = np.array(vertices)

        if tknots is None:
            if metric is None:
                raise Exception("LinearSpline: either tknots or metric must be specified")
            tknots = Spline.create_knots(pts, metric)

        self.pts = pts
        self.tknots = tknots
        self.is_cyclic = is_cyclic

    def eval(self, t_in, tknots = None):
        """
        Eval the liner spline f(t) = x,y,z through the points
        in pts given the knots in tknots at the point in t_in
        """

        if tknots is None:
            tknots = self.tknots
            
        ptsT = self.pts.T
        out = np.zeros((3, len(t_in)))
        for i in range(3):
            out[i] = np.interp(t_in, tknots, ptsT[i])
        return out.T

    def tangent(self, t_in, tknots = None, h = None):
        if tknots is None:
            tknots = self.tknots

        lookup_segments = GenerateLookup(self.is_cyclic, self.pts.tolist())
        return np.array([lookup_segments.find_bucket(f) for f in t_in])

class Spline2D(object):
    """
    2D Spline (surface).
    Composed by putting 1D splines along V direction, and then interpolating
    across them (in U direction) by using another series of 1D splines.
    U and V splines can both be either linear or cubic.
    The spline can optionally be cyclic in U and/or V directions
    (so it can form a cylindrical or thoroidal surface).
    This is implemented partly in pure python, partly in numpy, so the performance
    is not very good. The performance is not very bad either, because of caching.
    """
    def __init__(self, vertices,
            u_spline_constructor = CubicSpline, v_spline_constructor = None,
            metric = "DISTANCE",
            is_cyclic_u = False, is_cyclic_v = False):
        """
        vertices: Vertices in Sverchok format, i.e. list of list of 3-tuples.
        u_spline_constructor: constructor of Spline objects.
        v_spline_constructor: constructor of Spline objects. Defaults to u_spline_constructor.
        is_cyclic_u: whether the spline is cyclic in the U direction
        is_cyclic_v: whether the spline is cyclic in the V direction
        metric: string, one of "DISTANCE", "MANHATTAN", "POINTS", "CHEBYSHEV".
        """
        self.vertices = np.array(vertices)
        if v_spline_constructor is None:
            v_spline_constructor = u_spline_constructor
        self.u_spline_constructor = u_spline_constructor
        self.v_spline_constructor = v_spline_constructor
        self.metric = metric
        self.is_cyclic_u = is_cyclic_u
        self.is_cyclic_v = is_cyclic_v

        self._v_splines = [v_spline_constructor(verts, is_cyclic=is_cyclic_v, metric=metric) for verts in vertices]

        # Caches
        # v -> Spline
        self._u_splines = {}
        # (u,v) -> vertex
        self._eval_cache = {}
        # (u,v) -> normal
        self._normal_cache = {}

    def get_u_spline(self, v, vertices):
        """Get a spline along U direction for specified value of V coordinate"""
        spline = self._u_splines.get(v, None)
        if spline is not None:
            return spline
        else:
            spline = self.u_spline_constructor(vertices, is_cyclic=self.is_cyclic_u, metric=self.metric)
            self._u_splines[v] = spline
            return spline

    def eval(self, u, v):
        """
        u, v: floats in [0, 1].
        Returns 3-tuple of floats.

        Evaluate the spline at single point.
        """

        result = self._eval_cache.get((u,v), None)
        if result is not None:
            return result
        else:
            spline_vertices = [spline.eval_at_point(v) for spline in self._v_splines]
            u_spline = self.get_u_spline(v, spline_vertices)
            result = u_spline.eval_at_point(u)
            self._eval_cache[(u,v)] = result
            return result

    def normal(self, u, v, h=0.001):
        """
        u, v: floats in [0,1].
        h: step for numeric differentials calculation.
        Returns 3-tuple of floats.

        Get the normal vector for spline at specific point.
        """

        result = self._normal_cache.get((u,v), None)
        if result is not None:
            return result
        else:
            point = np.array(self.eval(u, v))
            point_u = np.array(self.eval(u+h, v))
            point_v = np.array(self.eval(u, v+h))
            du = (point_u - point)/h
            dv = (point_v - point)/h
            n = np.cross(du, dv)
            norm = np.linalg.norm(n)
            if norm != 0:
                n = n / norm
            #print("DU: {}, DV: {}, N: {}".format(du, dv, n))
            result = tuple(n)
            self._normal_cache[(u,v)] = result
            return result

class GenerateLookup():

    def __init__(self, cyclic, vlist):
        self.lookup = {}
        self.summed_lengths = []
        self.indiv_lengths = []
        self.normals = []
        self.buckets = []
        if cyclic:
            vlist = vlist + [vlist[0]]

        self.get_seq_len(vlist)
        self.acquire_lookup_table()
        self.get_buckets()
        # for idx, (k, v) in enumerate(sorted(self.lookup.items())):
        #     print(k, v)

    def find_bucket(self, factor):
        for bucket_min, bucket_max in zip(self.buckets[:-1], self.buckets[1:]):
            if bucket_min <= factor < bucket_max:
                tval = self.lookup.get(bucket_min)  # , self.lookup.get(self.buckets[-1]))

                return tval

        # return last bucket just in case
        return self.lookup.get(self.buckets[-1])

    def get_buckets(self):
        self.buckets = [(clen / self.total_length) for clen in self.summed_lengths]
    
    def acquire_lookup_table(self):
        for current_length, segment_normal in zip(self.summed_lengths, self.normals):
            self.lookup[current_length / self.total_length] = segment_normal
        
    def get_seq_len(self, vlist):
        add_len = self.indiv_lengths.append
        add_normal = self.normals.append
        add_to_sumlist = self.summed_lengths.append
        current_length = 0.0
        for idx in range(len(vlist)-1):
            v = vlist[idx][0]-vlist[idx+1][0], vlist[idx][1]-vlist[idx+1][1], vlist[idx][2]-vlist[idx+1][2]
            length = math.sqrt((v[0]*v[0]) + (v[1]*v[1]) + (v[2]*v[2]))
            add_normal(v)
            add_len(length)
            add_to_sumlist(current_length)
            current_length += length

        self.total_length = sum(self.indiv_lengths)
            
def householder(u):
    '''
    Calculate Householder reflection matrix.

    u: mathutils.Vector or tuple of 3 floats.
    returns mathutils.Matrix.
    '''
    x,y,z = u[0], u[1], u[2]
    m = Matrix([[x*x, x*y, x*z, 0], [x*y, y*y, y*z, 0], [x*z, y*z, z*z, 0], [0,0,0,0]])
    h = Matrix() - 2*m
    return h

def autorotate_householder(e1, xx):
    '''
    A matrix of transformation which will transform xx vector into e1,
    calculated via Householder matrix.
    See http://en.wikipedia.org/wiki/QR_decomposition

    e1, xx: mathutils.Vector.
    returns mathutils.Matrix.
    '''

    sign = -1
    alpha = xx.length * sign
    u = xx - alpha*e1
    v = u.normalized()
    q = householder(v)
    return q

def autorotate_track(e1, xx, up):
    '''
    A matrix of transformation which will transform xx vector into e1,
    calculated via Blender's to_track_quat method.

    e1: string, one of "X", "Y", "Z"
    xx: mathutils.Vector.
    up: string, one of "X", "Y", "Z".
    returns mathutils.Matrix.
    '''
    rotation = xx.to_track_quat(e1, up)
    return rotation.to_matrix().to_4x4()

def autorotate_diff(e1, xx):
    '''
    A matrix of transformation which will transform xx vector into e1,
    calculated via Blender's rotation_difference method.

    e1, xx: mathutils.Vector.
    returns mathutils.Matrix.
    '''
    return xx.rotation_difference(e1).to_matrix().to_4x4()

def diameter(vertices, axis):
    """
    Calculate diameter of set of vertices along specified axis.
    
    vertices: list of mathutils.Vector or of 3-tuples of floats.
    axis: 0, 1 or 2.
    returns float.
    """
    xs = [vertex[axis] for vertex in vertices]
    M = max(xs)
    m = min(xs)
    return (M-m)


def multiply_vectors(M, vlist):
    # (4*4 matrix)  X   (3*1 vector)

    for i, v in enumerate(vlist):
        # write _in place_
        vlist[i] = (
            M[0][0]*v[0] + M[0][1]*v[1] + M[0][2]*v[2] + M[0][3]* 1.0,
            M[1][0]*v[0] + M[1][1]*v[1] + M[1][2]*v[2] + M[1][3]* 1.0, 
            M[2][0]*v[0] + M[2][1]*v[1] + M[2][2]*v[2] + M[2][3]* 1.0
        )

    return vlist
