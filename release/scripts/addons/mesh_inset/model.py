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

"""Manipulations of Models.
"""

__author__ = "howard.trickey@gmail.com"

from . import geom
from . import triquad
from . import offset
import math


def PolyAreasToModel(polyareas, bevel_amount, bevel_pitch, quadrangulate):
    """Convert a PolyAreas into a Model object.

    Assumes polyareas are in xy plane.

    Args:
      polyareas: geom.PolyAreas
      bevel_amount: float - if > 0, amount of bevel
      bevel_pitch: float - if > 0, angle in radians of bevel
      quadrangulate: bool - should n-gons be quadrangulated?
    Returns:
      geom.Model
    """

    m = geom.Model()
    if not polyareas:
        return m
    polyareas.points.AddZCoord(0.0)
    m.points = polyareas.points
    for pa in polyareas.polyareas:
        PolyAreaToModel(m, pa, bevel_amount, bevel_pitch, quadrangulate)
    return m


def PolyAreaToModel(m, pa, bevel_amount, bevel_pitch, quadrangulate):
    if bevel_amount > 0.0:
        BevelPolyAreaInModel(m, pa, bevel_amount, bevel_pitch, quadrangulate,
            False)
    elif quadrangulate:
        if len(pa.poly) == 0:
            return
        qpa = triquad.QuadrangulateFaceWithHoles(pa.poly, pa.holes, pa.points)
        m.faces.extend(qpa)
        m.face_data.extend([pa.data] * len(qpa))
    else:
        m.faces.append(pa.poly)
        # TODO: just the first part of QuadrangulateFaceWithHoles, to join
        # holes to outer poly
        m.face_data.append(pa.data)


def ExtrudePolyAreasInModel(mdl, polyareas, depth, cap_back):
    """Extrude the boundaries given by polyareas by -depth in z.

    Assumes polyareas are in xy plane.

    Arguments:
      mdl: geom.Model - where to do extrusion
      polyareas: geom.Polyareas
      depth: float
      cap_back: bool - if True, cap off the back
    Side Effects:
      For all edges in polys in polyareas, make quads in Model
      extending those edges by depth in the negative z direction.
      The application data will be the data of the face that the edge
      is part of.
    """

    for pa in polyareas.polyareas:
        back_poly = _ExtrudePoly(mdl, pa.poly, depth, pa.data, True)
        back_holes = []
        for p in pa.holes:
            back_holes.append(_ExtrudePoly(mdl, p, depth, pa.data, False))
        if cap_back:
            qpa = triquad.QuadrangulateFaceWithHoles(back_poly, back_holes,
              polyareas.points)
            # need to reverse each poly to get normals pointing down
            for i, p in enumerate(qpa):
                t = list(p)
                t.reverse()
                qpa[i] = tuple(t)
            mdl.faces.extend(qpa)
            mdl.face_data.extend([pa.data] * len(qpa))


def _ExtrudePoly(mdl, poly, depth, data, isccw):
    """Extrude the poly by -depth in z

    Arguments:
      mdl: geom.Model - where to do extrusion
      poly: list of vertex indices
      depth: float
      data: application data
      isccw: True if counter-clockwise
    Side Effects
      For all edges in poly, make quads in Model
      extending those edges by depth in the negative z direction.
      The application data will be the data of the face that the edge
      is part of.
    Returns:
      list of int - vertices for extruded poly
    """

    if len(poly) < 2:
        return
    extruded_poly = []
    points = mdl.points
    if isccw:
        incr = 1
    else:
        incr = -1
    for i, v in enumerate(poly):
        vnext = poly[(i + incr) % len(poly)]
        (x0, y0, z0) = points.pos[v]
        (x1, y1, z1) = points.pos[vnext]
        vextrude = points.AddPoint((x0, y0, z0 - depth))
        vnextextrude = points.AddPoint((x1, y1, z1 - depth))
        if isccw:
            sideface = [v, vextrude, vnextextrude, vnext]
        else:
            sideface = [v, vnext, vnextextrude, vextrude]
        mdl.faces.append(sideface)
        mdl.face_data.append(data)
        extruded_poly.append(vextrude)
    return extruded_poly


def BevelPolyAreaInModel(mdl, polyarea,
    bevel_amount, bevel_pitch, quadrangulate, as_percent):
    """Bevel the interior of polyarea in model.

    This does smart beveling: advancing edges are merged
    rather than doing an 'overlap'.  Advancing edges that
    hit an opposite edge result in a split into two beveled areas.

    If the polyarea is not in the xy plane, do the work in a
    transformed model, and then transfer the changes back.

    Arguments:
      mdl: geom.Model - where to do bevel
      polyarea geom.PolyArea - area to bevel into
      bevel_amount: float - if > 0, amount of bevel
      bevel_pitch: float - if > 0, angle in radians of bevel
      quadrangulate: bool - should n-gons be quadrangulated?
      as_percent: bool - if True, interpret amount as percent of max
    Side Effects:
      Faces and points are added to model to model the
      bevel and the interior of the polyareas.
    """

    pa_norm = polyarea.Normal()
    if pa_norm == (0.0, 0.0, 1.0):
        m = mdl
        pa_rot = polyarea
    else:
        (pa_rot, inv_rot, inv_map) = _RotatedPolyAreaToXY(polyarea, pa_norm)
        # don't have to add the original faces into model, just their points.
        m = geom.Model()
        m.points = pa_rot.points
    vspeed = math.tan(bevel_pitch)
    off = offset.Offset(pa_rot, 0.0, vspeed)
    if as_percent:
        bevel_amount = bevel_amount * off.MaxAmount() / 100.0
    off.Build(bevel_amount)
    inner_pas = AddOffsetFacesToModel(m, off, polyarea.data)
    for pa in inner_pas.polyareas:
        if quadrangulate:
            if len(pa.poly) == 0:
                continue
            qpa = triquad.QuadrangulateFaceWithHoles(pa.poly, pa.holes,
                pa.points)
            m.faces.extend(qpa)
            m.face_data.extend([pa.data] * len(qpa))
        else:
            m.faces.append(pa.poly)
            m.face_data.append(pa.data)
    if m != mdl:
        _AddTransformedPolysToModel(mdl, m.faces, m.points, m.face_data,
             inv_rot, inv_map)


def AddOffsetFacesToModel(mdl, off, data=None):
    """Add the faces due to an offset into model.

    Returns the remaining interiors of the offset as a PolyAreas.

    Args:
      mdl: geom.Model - model to add offset faces into
      off: offset.Offset
      data: any - application data to be copied to the faces
    Returns:
      geom.PolyAreas
    """

    mdl.points = off.polyarea.points
    assert(len(mdl.points.pos) == 0 or len(mdl.points.pos[0]) == 3)
    o = off
    ostack = []
    while o:
        if o.endtime != 0.0:
            for face in o.facespokes:
                n = len(face)
                for i, spoke in enumerate(face):
                    nextspoke = face[(i + 1) % n]
                    v0 = spoke.origin
                    v1 = nextspoke.origin
                    v2 = nextspoke.dest
                    v3 = spoke.dest
                    if v2 == v3:
                        mface = [v0, v1, v2]
                    else:
                        mface = [v0, v1, v2, v3]
                    mdl.faces.append(mface)
                    mdl.face_data.append(data)
        ostack.extend(o.inneroffsets)
        if ostack:
            o = ostack.pop()
        else:
            o = None
    return off.InnerPolyAreas()


def BevelSelectionInModel(mdl, bevel_amount, bevel_pitch, quadrangulate,
        as_region, as_percent):
    """Bevel all the faces in the model, perhaps as one region.

    If as_region is False, each face is beveled individually,
    otherwise regions of contiguous faces are merged into
    PolyAreas and beveled as a whole.

    TODO: something if extracted PolyAreas are not approximately
    planar.

    Args:
      mdl: geom.Model
      bevel_amount: float - amount to inset
      bevel_pitch: float - angle of bevel side
      quadrangulate: bool - should insides be quadrangulated?
      as_region: bool - should faces be merged into regions?
      as_percent: bool - should amount be interpreted as a percent
          of the maximum amount (if True) or an absolute amount?
    Side effect:
      Beveling faces will be added to the model
    """

    pas = []
    if as_region:
        pas = RegionToPolyAreas(mdl.faces, mdl.points, mdl.face_data)
    else:
        for f, face in enumerate(mdl.faces):
            pas.append(geom.PolyArea(mdl.points, face, [],
                mdl.face_data[f]))
    for pa in pas:
        BevelPolyAreaInModel(mdl, pa,
            bevel_amount, bevel_pitch, quadrangulate, as_percent)


def RegionToPolyAreas(faces, points, data):
    """Find polygonal outlines induced by union of faces.

    Finds the polygons formed by boundary edges (those not
    sharing an edge with another face in region_faces), and
    turns those into PolyAreas.
    In the general case, there will be holes inside.
    We want to associate data with the region PolyAreas.
    Just choose a representative element of data[] when
    more than one face is combined into a PolyArea.

    Args:
      faces: list of list of int - each sublist is a face (indices into points)
      points: geom.Points - gives coordinates for vertices
      data: list of any - parallel to faces, app data to put in PolyAreas
    Returns:
      list of geom.PolyArea
    """

    ans = []
    (edges, vtoe) = _GetEdgeData(faces)
    (face_adj, is_interior_edge) = _GetFaceGraph(faces, edges, vtoe, points)
    (components, ftoc) = _FindFaceGraphComponents(faces, face_adj)
    for c in range(len(components)):
        boundary_edges = set()
        betodata = dict()
        vstobe = dict()
        for e, ((vs, ve), f) in enumerate(edges):
            if ftoc[f] != c or is_interior_edge[e]:
                continue
            boundary_edges.add(e)
            # vstobe[v] is set of edges leaving v
            # (could be more than one if boundary touches itself at a vertex)
            if vs in vstobe:
                vstobe[vs].append(e)
            else:
                vstobe[vs] = [e]
            betodata[(vs, ve)] = data[f]
        polys = []
        poly_data = []
        while boundary_edges:
            e = boundary_edges.pop()
            ((vstart, ve), face_i) = edges[e]
            poly = [vstart, ve]
            datum = betodata[(vstart, ve)]
            while ve != vstart:
                if ve not in vstobe:
                    print("whoops, couldn't close boundary")
                    break
                nextes = vstobe[ve]
                if len(nextes) == 1:
                    nexte = nextes[0]
                else:
                    # find a next edge with face index face_i
                    # TODO: this is not guaranteed to work,
                    # as continuation edge may have been for a different
                    # face that is now combined with face_i by erasing
                    # interior edges. Find a better algorithm here.
                    nexte = -1
                    for ne_cand in nextes:
                        if edges[ne_cand][1] == face_i:
                            nexte = ne_cand
                            break
                    if nexte == -1:
                        # case mentioned in TODO may have happened;
                        # just choose any nexte - may mess things up
                        nexte = nextes[0]
                ((_, ve), face_i) = edges[nexte]
                if nexte not in boundary_edges:
                    print("whoops, nexte not a boundary edge", nexte)
                    break
                boundary_edges.remove(nexte)
                if ve != vstart:
                    poly.append(ve)
            polys.append(poly)
            poly_data.append(datum)
        if len(polys) == 0:
            # can happen if an entire closed polytope is given
            # at least until we do an edge check
            return []
        elif len(polys) == 1:
            ans.append(geom.PolyArea(points, polys[0], [], poly_data[0]))
        else:
            outerf = _FindOuterPoly(polys, points, faces)
            pa = geom.PolyArea(points, polys[outerf], [], poly_data[outerf])
            pa.holes = [polys[i] for i in range(len(polys)) if i != outerf]
            ans.append(pa)
    return ans


def _GetEdgeData(faces):
    """Find edges from faces, and some lookup dictionaries.

    Args:
      faces: list of list of int - each a closed CCW polygon of vertex indices
    Returns:
      (list of ((int, int), int), dict{ int->list of int}) -
        list elements are ((startv, endv), face index)
        dict maps vertices to edge indices
    """

    edges = []
    vtoe = dict()
    for findex, f in enumerate(faces):
        nf = len(f)
        for i, v in enumerate(f):
            endv = f[(i + 1) % nf]
            edges.append(((v, endv), findex))
            eindex = len(edges) - 1
            if v in vtoe:
                vtoe[v].append(eindex)
            else:
                vtoe[v] = [eindex]
    return (edges, vtoe)


def _GetFaceGraph(faces, edges, vtoe, points):
    """Find the face adjacency graph.

    Faces are adjacent if they share an edge,
    and the shared edge goes in the reverse direction,
    and if the angle between them isn't too large.

    Args:
      faces: list of list of int
      edges: list of ((int, int), int) - see _GetEdgeData
      vtoe: dict{ int->list of int } - see _GetEdgeData
      points: geom.Points
    Returns:
      (list of  list of int, list of bool) -
        first list: each sublist is adjacent face indices for each face
        second list: maps edge index to True if it separates adjacent faces
    """

    face_adj = [[] for i in range(len(faces))]
    is_interior_edge = [False] * len(edges)
    for e, ((vs, ve), f) in enumerate(edges):
        for othere in vtoe[ve]:
            ((_, we), g) = edges[othere]
            if we == vs:
                # face g is adjacent to face f
                # TODO: angle check
                if g not in face_adj[f]:
                    face_adj[f].append(g)
                    is_interior_edge[e] = True
                # Don't bother with mirror relations, will catch later
    return (face_adj, is_interior_edge)


def _FindFaceGraphComponents(faces, face_adj):
    """Partition faces into connected components.

    Args:
      faces: list of list of int
      face_adj: list of list of int - see _GetFaceGraph
    Returns:
      (list of list of int, list of int) -
        first list partitions face indices into separate lists,
            each a component
        second list maps face indices into their component index
    """

    if not faces:
        return ([], [])
    components = []
    ftoc = [-1] * len(faces)
    for i in range(len(faces)):
        if ftoc[i] == -1:
            compi = len(components)
            comp = []
            _FFGCSearch(i, faces, face_adj, ftoc, compi, comp)
            components.append(comp)
    return (components, ftoc)


def _FFGCSearch(findex, faces, face_adj, ftoc, compi, comp):
    """Depth first search helper function for _FindFaceGraphComponents

    Searches recursively through all faces connected to findex, adding
    each face found to comp and setting ftoc for that face to compi.
    """

    comp.append(findex)
    ftoc[findex] = compi
    for otherf in face_adj[findex]:
        if ftoc[otherf] == -1:
            _FFGCSearch(otherf, faces, face_adj, ftoc, compi, comp)


def _FindOuterPoly(polys, points, faces):
    """Assuming polys has one CCW-oriented face when looking
    down average normal of faces, return that one.

    Only one of the faces should have a normal whose dot product
    with the average normal of faces is positive.

    Args:
      polys: list of list of int - list of polys given by vertex indices
      points: geom.Points
      faces: list of list of int - original selected region, used to find
          average normal
    Returns:
      int - the index in polys of the outermost one
    """

    if len(polys) < 2:
        return 0
    fnorm = (0.0, 0.0, 0.0)
    for face in faces:
        if len(face) > 2:
            fnorm = geom.VecAdd(fnorm, geom.Newell(face, points))
    if fnorm == (0.0, 0.0, 0.0):
        return 0
    # fnorm is really a multiple of the normal, but fine for test below
    for i, poly in enumerate(polys):
        if len(poly) > 2:
            pnorm = geom.Newell(poly, points)
            if geom.VecDot(fnorm, pnorm) > 0:
                return i
    print("whoops, couldn't find an outermost poly")
    return 0


def _RotatedPolyAreaToXY(polyarea, norm):
    """Return a  PolyArea rotated to xy plane.

    Only the points in polyarea will be transferred.

    Args:
      polyarea: geom.PolyArea
      norm: the normal for polyarea
    Returns:
      (geom.PolyArea, (float, ..., float), dict{ int -> int }) - new PolyArea,
          4x3 inverse transform, dict mapping new verts to old ones
    """

    # find rotation matrix that takes norm to (0,0,1)
    (nx, ny, nz) = norm
    if abs(nx) < abs(ny) and abs(nx) < abs(nz):
        v = (vx, vy, vz) = geom.Norm3(0.0, nz, - ny)
    elif abs(ny) < abs(nz):
        v = (vx, vy, vz) = geom.Norm3(nz, 0.0, - nx)
    else:
        v = (vx, vy, vz) = geom.Norm3(ny, - nx, 0.0)
    (ux, uy, uz) = geom.Cross3(v, norm)
    rotmat = [ux, vx, nx, uy, vy, ny, uz, vz, nz, 0.0, 0.0, 0.0]
    # rotation matrices are orthogonal, so inverse is transpose
    invrotmat = [ux, uy, uz, vx, vy, vz, nx, ny, nz, 0.0, 0.0, 0.0]
    pointmap = dict()
    invpointmap = dict()
    newpoints = geom.Points()
    for poly in [polyarea.poly] + polyarea.holes:
        for v in poly:
            vcoords = polyarea.points.pos[v]
            newvcoords = geom.MulPoint3(vcoords, rotmat)
            newv = newpoints.AddPoint(newvcoords)
            pointmap[v] = newv
            invpointmap[newv] = v
    pa = geom.PolyArea(newpoints)
    pa.poly = [pointmap[v] for v in polyarea.poly]
    pa.holes = [[pointmap[v] for v in hole] for hole in polyarea.holes]
    pa.data = polyarea.data
    return (pa, invrotmat, invpointmap)


def _AddTransformedPolysToModel(mdl, polys, points, poly_data,
        transform, pointmap):
    """Add (transformed) the points and faces to a model.

    Add polys to mdl.  The polys have coordinates given by indices
    into points.pos; those need to be transformed by multiplying by
    the transform matrix.
    The vertices may already exist in mdl.  Rather than relying on
    AddPoint to detect the duplicate (transform rounding error makes
    that dicey), the pointmap dictionar is used to map vertex indices
    in polys into those in mdl - if they exist already.

    Args:
      mdl: geom.Model - where to put new vertices, faces
      polys: list of list of int - each sublist a poly
      points: geom.Points - coords for vertices in polys
      poly_data: list of any - parallel to polys
      transform: (float, ..., float) - 12-tuple, a 4x3 transform matrix
      pointmap: dict { int -> int } - maps new vertex indices to old ones
    Side Effects:
      The model gets new faces and vertices, based on those in polys.
      We are allowed to modify pointmap, as it will be discarded after call.
    """

    for i, coords in enumerate(points.pos):
        if i not in pointmap:
            p = geom.MulPoint3(coords, transform)
            pointmap[i] = mdl.points.AddPoint(p)
    for i, poly in enumerate(polys):
        mpoly = [pointmap[v] for v in poly]
        mdl.faces.append(mpoly)
        mdl.face_data.append(poly_data[i])
