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


from . import geom
import math
import random
from math import sqrt, hypot

# Points are 3-tuples or 2-tuples of reals: (x,y,z) or (x,y)
# Faces are lists of integers (vertex indices into coord lists)
# After triangulation/quadrangulation, the tris and quads will
# be tuples instead of lists.
# Vmaps are lists taking vertex index -> Point

TOL = 1e-7     # a tolerance for fuzzy equality
GTHRESH = 75   # threshold above which use greedy to _Quandrangulate
ANGFAC = 1.0   # weighting for angles in quad goodness measure
DEGFAC = 10.0  # weighting for degree in quad goodness measure

# Angle kind constants
Ang0 = 1
Angconvex = 2
Angreflex = 3
Angtangential = 4
Ang360 = 5


def TriangulateFace(face, points):
    """Triangulate the given face.

    Uses an easy triangulation first, followed by a constrained delauney
    triangulation to get better shaped triangles.

    Args:
      face: list of int - indices in points, assumed CCW-oriented
      points: geom.Points - holds coordinates for vertices
    Returns:
      list of (int, int, int) - 3-tuples are CCW-oriented vertices of
          triangles making up the triangulation
    """

    if len(face) <= 3:
        return [tuple(face)]
    tris = EarChopTriFace(face, points)
    bord = _BorderEdges([face])
    triscdt = _CDT(tris, bord, points)
    return triscdt


def TriangulateFaceWithHoles(face, holes, points):
    """Like TriangulateFace, but with holes inside the face.

    Works by making one complex polygon that has segments to
    and from the holes ("islands"), and then using the same method
    as TriangulateFace.

    Args:
      face: list of int - indices in points, assumed CCW-oriented
      holes: list of list of int - each sublist is like face
          but CW-oriented and assumed to be inside face
      points: geom.Points - holds coordinates for vertices
    Returns:
      list of (int, int, int) - 3-tuples are CCW-oriented vertices of
          triangles making up the triangulation
    """

    if len(holes) == 0:
        return TriangulateFace(face, points)
    allfaces = [face] + holes
    sholes = [_SortFace(h, points) for h in holes]
    joinedface = _JoinIslands(face, sholes, points)
    tris = EarChopTriFace(joinedface, points)
    bord = _BorderEdges(allfaces)
    triscdt = _CDT(tris, bord, points)
    return triscdt


def QuadrangulateFace(face, points):
    """Quadrangulate the face (subdivide into convex quads and tris).

    Like TriangulateFace, but after triangulating, join as many pairs
    of triangles as possible into convex quadrilaterals.

    Args:
      face: list of int - indices in points, assumed CCW-oriented
      points: geom.Points - holds coordinates for vertices
    Returns:
      list of 3-tuples or 4-tuples of ints - CCW-oriented vertices of
          quadrilaterals and triangles making up the quadrangulation.
    """

    if len(face) <= 3:
        return [tuple(face)]
    tris = EarChopTriFace(face, points)
    bord = _BorderEdges([face])
    triscdt = _CDT(tris, bord, points)
    qs = _Quandrangulate(triscdt, bord, points)
    return qs


def QuadrangulateFaceWithHoles(face, holes, points):
    """Like QuadrangulateFace, but with holes inside the faces.

    Args:
      face: list of int - indices in points, assumed CCW-oriented
      holes: list of list of int - each sublist is like face
          but CW-oriented and assumed to be inside face
      points: geom.Points - holds coordinates for vertices
    Returns:
      list of 3-tuples or 4-tuples of ints - CCW-oriented vertices of
          quadrilaterals and triangles making up the quadrangulation.
    """

    if len(holes) == 0:
        return QuadrangulateFace(face, points)
    allfaces = [face] + holes
    sholes = [_SortFace(h, points) for h in holes]
    joinedface = _JoinIslands(face, sholes, points)
    tris = EarChopTriFace(joinedface, points)
    bord = _BorderEdges(allfaces)
    triscdt = _CDT(tris, bord, points)
    qs = _Quandrangulate(triscdt, bord, points)
    return qs


def _SortFace(face, points):
    """Rotate face so leftmost vertex is first, where face is
    list of indices in points."""

    n = len(face)
    if n <= 1:
        return face
    lefti = 0
    leftv = face[0]
    for i in range(1, n):
        # following comparison is lexicographic on n-tuple
        # so sorts on x first, using lower y as tie breaker.
        if points.pos[face[i]] < points.pos[leftv]:
            lefti = i
            leftv = face[i]
    return face[lefti:] + face[0:lefti]


def EarChopTriFace(face, points):
    """Triangulate given face, with coords given by indexing into points.
    Return list of faces, each of which will be a triangle.
    Use the ear-chopping method."""

    # start with lowest coord in 2d space to try
    # to get a pleasing uniform triangulation if starting with
    # a regular structure (like a grid)
    start = _GetLeastIndex(face, points)
    ans = []
    incr = 1
    n = len(face)
    while n > 3:
        i = _FindEar(face, n, start, incr, points)
        vm1 = face[(i - 1) % n]
        v0 = face[i]
        v1 = face[(i + 1) % n]
        face = _ChopEar(face, i)
        n = len(face)
        incr = - incr
        if incr == 1:
            start = i % n
        else:
            start = (i - 1) % n
        ans.append((vm1, v0, v1))
    ans.append(tuple(face))
    return ans


def _GetLeastIndex(face, points):
    """Return index of coordinate that is leftmost, lowest in face."""

    bestindex = 0
    bestpos = points.pos[face[0]]
    for i in range(1, len(face)):
        pos = points.pos[face[i]]
        if pos[0] < bestpos[0] or \
                (pos[0] == bestpos[0] and pos[1] < bestpos[1]):
            bestindex = i
            bestpos = pos
    return bestindex


def _FindEar(face, n, start, incr, points):
    """An ear of a polygon consists of three consecutive vertices
    v(-1), v0, v1 such that v(-1) can connect to v1 without intersecting
    the polygon.
    Finds an ear, starting at index 'start' and moving
    in direction incr. (We attempt to alternate directions, to find
    'nice' triangulations for simple convex polygons.)
    Returns index into faces of v0 (will always find one, because
    uses a desperation mode if fails to find one with above rule)."""

    angk = _ClassifyAngles(face, n, points)
    for mode in range(0, 5):
        i = start
        while True:
            if _IsEar(face, i, n, angk, points, mode):
                return i
            i = (i + incr) % n
            if i == start:
                break  # try next higher desperation mode


def _IsEar(face, i, n, angk, points, mode):
    """Return true, false depending on ear status of vertices
    with indices i-1, i, i+1.
    mode is amount of desperation: 0 is Normal mode,
    mode 1 allows degenerate triangles (with repeated vertices)
    mode 2 allows local self crossing (folded) ears
    mode 3 allows any convex vertex (should always be one)
    mode 4 allows anything (just to be sure loop terminates!)"""

    k = angk[i]
    vm2 = face[(i - 2) % n]
    vm1 = face[(i - 1) % n]
    v0 = face[i]
    v1 = face[(i + 1) % n]
    v2 = face[(i + 2) % n]
    if vm1 == v0 or v0 == v1:
        return (mode > 0)
    b = (k == Angconvex or k == Angtangential or k == Ang0)
    c = _InCone(vm1, v0, v1, v2, angk[(i + 1) % n], points) and \
        _InCone(v1, vm2, vm1, v0, angk[(i - 1) % n], points)
    if b and c:
        return _EarCheck(face, n, angk, vm1, v0, v1, points)
    if mode < 2:
        return False
    if mode == 3:
        return SegsIntersect(vm2, vm1, v0, v1, points)
    if mode == 4:
        return b
    return True


def _EarCheck(face, n, angk, vm1, v0, v1, points):
    """Return True if the successive vertices vm1, v0, v1
    forms an ear.  We already know that it is not a reflex
    Angle, and that the local cone containment is ok.
    What remains to check is that the edge vm1-v1 doesn't
    intersect any other edge of the face (besides vm1-v0
    and v0-v1).  Equivalently, there can't be a reflex Angle
    inside the triangle vm1-v0-v1.  (Well, there are
    messy cases when other points of the face coincide with
    v0 or touch various lines involved in the ear.)"""
    for j in range(0, n):
        fv = face[j]
        k = angk[j]
        b = (k == Angreflex or k == Ang360) \
            and not(fv == vm1 or fv == v0 or fv == v1)
        if b:
            # Is fv inside closure of triangle (vm1,v0,v1)?
            c = not(Ccw(v0, vm1, fv, points) \
                          or Ccw(vm1, v1, fv, points) \
                          or Ccw(v1, v0, fv, points))
            fvm1 = face[(j - 1) % n]
            fv1 = face[(j + 1) % n]
            # To try to deal with some degenerate cases,
            # also check to see if either segment attached to fv
            # intersects either segment of potential ear.
            d = SegsIntersect(fvm1, fv, vm1, v0, points) or \
                      SegsIntersect(fvm1, fv, v0, v1, points) or \
                      SegsIntersect(fv, fv1, vm1, v0, points) or \
                      SegsIntersect(fv, fv1, v0, v1, points)
            if c or d:
                return False
    return True


def _ChopEar(face, i):
    """Return a copy of face (of length n), omitting element i."""

    return face[0:i] + face[i + 1:]


def _InCone(vtest, a, b, c, bkind, points):
    """Return true if point with index vtest is in Cone of points with
    indices a, b, c, where Angle ABC has AngleKind Bkind.
    The Cone is the set of points inside the left face defined by
    segments ab and bc, disregarding all other segments of polygon for
    purposes of inside test."""

    if bkind == Angreflex or bkind == Ang360:
        if _InCone(vtest, c, b, a, Angconvex, points):
            return False
        return not((not(Ccw(b, a, vtest, points)) \
                             and not(Ccw(b, vtest, a, points)) \
                             and Ccw(b, a, vtest, points))
                            or
                            (not(Ccw(b, c, vtest, points)) \
                             and not(Ccw(b, vtest, c, points)) \
                             and Ccw(b, a, vtest, points)))
    else:
        return Ccw(a, b, vtest, points) and Ccw(b, c, vtest, points)


def _JoinIslands(face, holes, points):
    """face is a CCW face containing the CW faces in the holes list,
    where each hole is sorted so the leftmost-lowest vertex is first.
    faces and holes are given as lists of indices into points.
    The holes should be sorted by softface.
    Add edges to make a new face that includes the holes (a Ccw traversal
    of the new face will have the inside always on the left),
    and return the new face."""

    while len(holes) > 0:
        (hole, holeindex) = _LeftMostFace(holes, points)
        holes = holes[0:holeindex] + holes[holeindex + 1:]
        face = _JoinIsland(face, hole, points)
    return face


def _JoinIsland(face, hole, points):
    """Return a modified version of face that splices in the
    vertices of hole (which should be sorted)."""

    if len(hole) == 0:
        return face
    hv0 = hole[0]
    d = _FindDiag(face, hv0, points)
    newface = face[0:d + 1] + hole + [hv0] + face[d:]
    return newface


def _LeftMostFace(holes, points):
    """Return (hole,index of hole in holes) where hole has
    the leftmost first vertex.  To be able to handle empty
    holes gracefully, call an empty hole 'leftmost'.
    Assumes holes are sorted by softface."""

    assert(len(holes) > 0)
    lefti = 0
    lefthole = holes[0]
    if len(lefthole) == 0:
        return (lefthole, lefti)
    leftv = lefthole[0]
    for i in range(1, len(holes)):
        ihole = holes[i]
        if len(ihole) == 0:
            return (ihole, i)
        iv = ihole[0]
        if points.pos[iv] < points.pos[leftv]:
            (lefti, lefthole, leftv) = (i, ihole, iv)
    return (lefthole, lefti)


def _FindDiag(face, hv, points):
    """Find a vertex in face that can see vertex hv, if possible,
    and return the index into face of that vertex.
    Should be able to find a diagonal that connects a vertex of face
    left of v to hv without crossing face, but try two
    more desperation passes after that to get SOME diagonal, even if
    it might cross some edge somewhere.
    First desperation pass (mode == 1): allow points right of hv.
    Second desperation pass (mode == 2): allow crossing boundary poly"""

    besti = - 1
    bestdist = 1e30
    for mode in range(0, 3):
        for i in range(0, len(face)):
            v = face[i]
            if mode == 0 and points.pos[v] > points.pos[hv]:
                continue  # in mode 0, only want points left of hv
            dist = _DistSq(v, hv, points)
            if dist < bestdist:
                if _IsDiag(i, v, hv, face, points) or mode == 2:
                    (besti, bestdist) = (i, dist)
        if besti >= 0:
            break  # found one, so don't need other modes
    assert(besti >= 0)
    return besti


def _IsDiag(i, v, hv, face, points):
    """Return True if vertex v (at index i in face) can see vertex hv.
    v and hv are indices into points.
    (v, hv) is a diagonal if hv is in the cone of the Angle at index i on face
    and no segment in face intersects (h, hv).
    """

    n = len(face)
    vm1 = face[(i - 1) % n]
    v1 = face[(i + 1) % n]
    k = _AngleKind(vm1, v, v1, points)
    if not _InCone(hv, vm1, v, v1, k, points):
        return False
    for j in range(0, n):
        vj = face[j]
        vj1 = face[(j + 1) % n]
        if SegsIntersect(v, hv, vj, vj1, points):
            return False
    return True


def _DistSq(a, b, points):
    """Return distance squared between coords with indices a and b in points.
    """

    diff = Sub2(points.pos[a], points.pos[b])
    return Dot2(diff, diff)


def _BorderEdges(facelist):
    """Return a set of (u,v) where u and v are successive vertex indices
    in some face in the list in facelist."""

    ans = set()
    for i in range(0, len(facelist)):
        f = facelist[i]
        for j in range(1, len(f)):
            ans.add((f[j - 1], f[j]))
        ans.add((f[-1], f[0]))
    return ans


def _CDT(tris, bord, points):
    """Tris is a list of triangles ((a,b,c), CCW-oriented indices into points)
    Bord is a set of border edges (u,v), oriented so that tris
    is a triangulation of the left face of the border(s).
    Make the triangulation "Constrained Delaunay" by flipping "reversed"
    quadrangulaterals until can flip no more.
    Return list of triangles in new triangulation."""

    td = _TriDict(tris)
    re = _ReveresedEdges(tris, td, bord, points)
    ts = set(tris)
    # reverse the reversed edges until done.
    # reversing and edge adds new edges, which may or
    # may not be reversed or border edges, to re for
    # consideration, but the process will stop eventually.
    while len(re) > 0:
        (a, b) = e = re.pop()
        if e in bord or not _IsReversed(e, td, points):
            continue
        # rotate e in quad adbc to get other diagonal
        erev = (b, a)
        tl = td.get(e)
        tr = td.get(erev)
        if not tl or not tr:
            continue  # shouldn't happen
        c = _OtherVert(tl, a, b)
        d = _OtherVert(tr, a, b)
        if c is None or d is None:
            continue  # shouldn't happen
        newt1 = (c, d, b)
        newt2 = (c, a, d)
        del td[e]
        del td[erev]
        td[(c, d)] = newt1
        td[(d, b)] = newt1
        td[(b, c)] = newt1
        td[(d, c)] = newt2
        td[(c, a)] = newt2
        td[(a, d)] = newt2
        if tl in ts:
            ts.remove(tl)
        if tr in ts:
            ts.remove(tr)
        ts.add(newt1)
        ts.add(newt2)
        re.extend([(d, b), (b, c), (c, a), (a, d)])
    return list(ts)


def _TriDict(tris):
    """tris is a list of triangles (a,b,c), CCW-oriented indices.
    Return dict mapping all edges in the triangles to the containing
    triangle list."""

    ans = dict()
    for i in range(0, len(tris)):
        (a, b, c) = t = tris[i]
        ans[(a, b)] = t
        ans[(b, c)] = t
        ans[(c, a)] = t
    return ans


def _ReveresedEdges(tris, td, bord, points):
    """Return list of reversed edges in tris.
    Only want edges not in bord, and only need one representative
    of (u,v)/(v,u), so choose the one with u < v.
    td is dictionary from _TriDict, and is used to find left and right
    triangles of edges."""

    ans = []
    for i in range(0, len(tris)):
        (a, b, c) = tris[i]
        for e in [(a, b), (b, c), (c, a)]:
            if e in bord:
                continue
            (u, v) = e
            if u < v:
                if _IsReversed(e, td, points):
                    ans.append(e)
    return ans


def _IsReversed(e, td, points):
    """If e=(a,b) is a non-border edge, with left-face triangle tl and
    right-face triangle tr, then it is 'reversed' if the circle through
    a, b, and (say) the other vertex of tl containts the other vertex of tr.
    td is a _TriDict, for finding triangles containing edges, and points
    gives the coordinates for vertex indices used in edges."""

    tl = td.get(e)
    if not tl:
        return False
    (a, b) = e
    tr = td.get((b, a))
    if not tr:
        return False
    c = _OtherVert(tl, a, b)
    d = _OtherVert(tr, a, b)
    if c is None or d is None:
        return False
    return InCircle(a, b, c, d, points)


def _OtherVert(tri, a, b):
    """tri should be a tuple of 3 vertex indices, two of which are a and b.
    Return the third index, or None if all vertices are a or b"""

    for v in tri:
        if v != a and v != b:
            return v
    return None


def _ClassifyAngles(face, n, points):
    """Return vector of anglekinds of the Angle around each point in face."""

    return [_AngleKind(face[(i - 1) % n], face[i], face[(i + 1) % n], points) \
        for i in list(range(0, n))]


def _AngleKind(a, b, c, points):
    """Return one of the Ang... constants to classify Angle formed by ABC,
    in a counterclockwise traversal of a face,
    where a, b, c are indices into points."""

    if Ccw(a, b, c, points):
        return Angconvex
    elif Ccw(a, c, b, points):
        return Angreflex
    else:
        vb = points.pos[b]
        udotv = Dot2(Sub2(vb, points.pos[a]), Sub2(points.pos[c], vb))
        if udotv > 0.0:
            return Angtangential
        else:
            return Ang0   # to fix: return Ang360 if "inside" spur


def _Quandrangulate(tris, bord, points):
    """Tris is list of triangles, forming a triangulation of region whose
    border edges are in set bord.
    Combine adjacent triangles to make quads, trying for "good" quads where
    possible. Some triangles will probably remain uncombined"""

    (er, td) = _ERGraph(tris, bord, points)
    if len(er) == 0:
        return tris
    if len(er) > GTHRESH:
        match = _GreedyMatch(er)
    else:
        match = _MaxMatch(er)
    return _RemoveEdges(tris, match)


def _RemoveEdges(tris, match):
    """tris is list of triangles.
    er is as returned from _MaxMatch or _GreedyMatch.

    Return list of (A,D,B,C) resulting from deleting edge (A,B) causing a merge
    of two triangles; append to that list the remaining unmatched triangles."""

    ans = []
    triset = set(tris)
    while len(match) > 0:
        (_, e, tl, tr) = match.pop()
        (a, b) = e
        if tl in triset:
            triset.remove(tl)
        if tr in triset:
            triset.remove(tr)
        c = _OtherVert(tl, a, b)
        d = _OtherVert(tr, a, b)
        if c is None or d is None:
            continue
        ans.append((a, d, b, c))
    return ans + list(triset)


def _ERGraph(tris, bord, points):
    """Make an 'Edge Removal Graph'.

    Given a list of triangles, the 'Edge Removal Graph' is a graph whose
    nodes are the triangles (think of a point in the center of them),
    and whose edges go between adjacent triangles (they share a non-border
    edge), such that it would be possible to remove the shared edge
    and form a convex quadrilateral.  Forming a quadrilateralization
    is then a matter of finding a matching (set of edges that don't
    share a vertex - remember, these are the 'face' vertices).
    For better quadrilaterlization, we'll make the Edge Removal Graph
    edges have weights, with higher weights going to the edges that
    are more desirable to remove.  Then we want a maximum weight matching
    in this graph.

    We'll return the graph in a kind of implicit form, using edges of
    the original triangles as a proxy for the edges between the faces
    (i.e., the edge of the triangle is the shared edge). We'll arbitrarily
    pick the triangle graph edge with lower-index start vertex.
    Also, to aid in traversing the implicit graph, we'll keep the left
    and right triangle triples with edge 'ER edge'.
    Finally, since we calculate it anyway, we'll return a dictionary
    mapping edges of the triangles to the triangle triples they're in.

    Args:
      tris: list of (int, int, int) giving a triple of vertex indices for
          triangles, assumed CCW oriented
      bord: set of (int, int) giving vertex indices for border edges
      points: geom.Points - for mapping vertex indices to coords
    Returns:
      (list of (weight,e,tl,tr), dict)
        where edge e=(a,b) is non-border edge
        with left face tl and right face tr (each a triple (i,j,k)),
        where removing the edge would form an "OK" quad (no concave angles),
        with weight representing the desirability of removing the edge
        The dict maps int pairs (a,b) to int triples (i,j,k), that is,
        mapping edges to their containing triangles.
    """

    td = _TriDict(tris)
    dd = _DegreeDict(tris)
    ans = []
    ctris = tris[:]  # copy, so argument not affected
    while len(ctris) > 0:
        (i, j, k) = tl = ctris.pop()
        for e in [(i, j), (j, k), (k, i)]:
            if e in bord:
                continue
            (a, b) = e
            # just consider one of (a,b) and (b,a), to avoid dups
            if a > b:
                continue
            erev = (b, a)
            tr = td.get(erev)
            if not tr:
                continue
            c = _OtherVert(tl, a, b)
            d = _OtherVert(tr, a, b)
            if c is None or d is None:
                continue
            # calculate amax, the max of the new angles that would
            # be formed at a and b if tl and tr were combined
            amax = max(Angle(c, a, b, points) + Angle(d, a, b, points),
                       Angle(c, b, a, points) + Angle(d, b, a, points))
            if amax > 180.0:
                continue
            weight = ANGFAC * (180.0 - amax) + DEGFAC * (dd[a] + dd[b])
            ans.append((weight, e, tl, tr))
    return (ans, td)


def _GreedyMatch(er):
    """er is list of (weight,e,tl,tr).

    Find maximal set so that each triangle appears in at most
    one member of set"""

    # sort in order of decreasing weight
    er.sort(key=lambda v: v[0], reverse=True)
    match = set()
    ans = []
    while len(er) > 0:
        (_, _, tl, tr) = q = er.pop()
        if tl not in match and tr not in match:
            match.add(tl)
            match.add(tr)
            ans.append(q)
    return ans


def _MaxMatch(er):
    """Like _GreedyMatch, but use divide and conquer to find best possible set.

    Args:
      er: list of (weight,e,tl,tr)  - see _ERGraph
    Returns:
      list that is a subset of er giving a maximum weight match
    """

    (ans, _) = _DCMatch(er)
    return ans


def _DCMatch(er):
    """Recursive helper for _MaxMatch.

    Divide and Conquer approach to finding max weight matching.
    If we're lucky, there's an edge in er that separates the edge removal
    graph into (at least) two separate components.  Then the max weight
    is either one that includes that edge or excludes it - and we can
    use a recursive call to _DCMatch to handle each component separately
    on what remains of the graph after including/excluding the separating edge.
    If we're not lucky, we fall back on _EMatch (see below).

    Args:
      er: list of (weight, e, tl, tr) (see _ERGraph)
    Returns:
      (list of (weight, e, tl, tr), float) - the subset forming a maximum
          matching, and the total weight of the match.
    """

    if not er:
        return ([], 0.0)
    if len(er) == 1:
        return (er, er[0][0])
    match = []
    matchw = 0.0
    for i in range(0, len(er)):
        (nc, comp) = _FindComponents(er, i)
        if nc == 1:
            # er[i] doesn't separate er
            continue
        (wi, _, tl, tr) = er[i]
        if comp[tl] != comp[tr]:
            # case 1: er separates graph
            # compare the matches that include er[i] versus
            # those that exclude it
            (a, b) = _PartitionComps(er, comp, i, comp[tl], comp[tr])
            ax = _CopyExcluding(a, tl, tr)
            bx = _CopyExcluding(b, tl, tr)
            (axmatch, wax) = _DCMatch(ax)
            (bxmatch, wbx) = _DCMatch(bx)
            if len(ax) == len(a):
                wa = wax
                amatch = axmatch
            else:
                (amatch, wa) = _DCMatch(a)
            if len(bx) == len(b):
                wb = wbx
                bmatch = bxmatch
            else:
                (bmatch, wb) = _DCMatch(b)
            w = wa + wb
            wx = wax + wbx + wi
            if w > wx:
                match = amatch + bmatch
                matchw = w
            else:
                match = [er[i]] + axmatch + bxmatch
                matchw = wx
        else:
            # case 2: er not needed to separate graph
            (a, b) = _PartitionComps(er, comp, -1, 0, 0)
            (amatch, wa) = _DCMatch(a)
            (bmatch, wb) = _DCMatch(b)
            match = amatch + bmatch
            matchw = wa + wb
        if match:
            break
    if not match:
        return _EMatch(er)
    return (match, matchw)


def _EMatch(er):
    """Exhaustive match helper for _MaxMatch.

    This is the case when we were unable to find a single edge
    separating the edge removal graph into two components.
    So pick a single edge and try _DCMatch on the two cases of
    including or excluding that edge.  We may be lucky in these
    subcases (say, if the graph is currently a simple cycle, so
    only needs one more edge after the one we pick here to separate
    it into components).  Otherwise, we'll end up back in _EMatch
    again, and the worse case will be exponential.

    Pick a random edge rather than say, the first, to hopefully
    avoid some pathological cases.

    Args:
      er: list of (weight, el, tl, tr) (see _ERGraph)
    Returns:
       (list of (weight, e, tl, tr), float) - the subset forming a maximum
          matching, and the total weight of the match.
    """

    if not er:
        return ([], 0.0)
    if len(er) == 1:
        return (er, er[1][1])
    i = random.randint(0, len(er) - 1)
    eri = (wi, _, tl, tr) = er[i]
    # case a: include eri.  exlude other edges that touch tl or tr
    a = _CopyExcluding(er, tl, tr)
    a.append(eri)
    (amatch, wa) = _DCMatch(a)
    wa += wi
    if len(a) == len(er) - 1:
        # if a excludes only eri, then er didn't touch anything else
        # in the graph, and the best match will always include er
        # and we can skip the call for case b
        wb = -1.0
        bmatch = []
    else:
        b = er[:i] + er[i + 1:]
        (bmatch, wb) = _DCMatch(b)
    if wa > wb:
        match = amatch
        match.append(eri)
        matchw = wa
    else:
        match = bmatch
        matchw = wb
    return (match, matchw)


def _FindComponents(er, excepti):
    """Find connected components induced by edges, excluding one edge.

    Args:
      er: list of (weight, el, tl, tr) (see _ERGraph)
      excepti: index in er of edge to be excluded
    Returns:
      (int, dict): int is number of connected components found,
          dict maps triangle triple ->
              connected component index (starting at 1)
     """

    ncomps = 0
    comp = dict()
    for i in range(0, len(er)):
        (_, _, tl, tr) = er[i]
        for t in [tl, tr]:
            if t not in comp:
                ncomps += 1
                _FCVisit(er, excepti, comp, t, ncomps)
    return (ncomps, comp)


def _FCVisit(er, excepti, comp, t, compnum):
    """Helper for _FindComponents depth-first-search."""

    comp[t] = compnum
    for i in range(0, len(er)):
        if i == excepti:
            continue
        (_, _, tl, tr) = er[i]
        if tl == t or tr == t:
            s = tl
            if s == t:
                s = tr
            if s not in comp:
                _FCVisit(er, excepti, comp, s, compnum)


def _PartitionComps(er, comp, excepti, compa, compb):
    """Partition the edges of er by component number, into two lists.

    Generally, put odd components into first list and even into second,
    except that component compa goes in the first and compb goes in the second,
    and we ignore edge er[excepti].

    Args:
      er: list of (weight, el, tl, tr) (see _ERGraph)
      comp: dict - mapping triangle triple -> connected component index
      excepti: int - index in er to ignore (unless excepti==-1)
      compa: int - component to go in first list of answer (unless 0)
      compb: int - component to go in second list of answer (unless 0)
    Returns:
      (list, list) - a partition of er according to above rules
    """

    parta = []
    partb = []
    for i in range(0, len(er)):

        if i == excepti:
            continue
        tl = er[i][2]
        c = comp[tl]
        if c == compa or (c != compb and (c & 1) == 1):
            parta.append(er[i])
        else:
            partb.append(er[i])
    return (parta, partb)


def _CopyExcluding(er, s, t):
    """Return a copy of er, excluding all those involving triangles s and t.

    Args:
      er: list of (weight, e, tl, tr) - see _ERGraph
      s: 3-tuple of int - a triangle
      t: 3-tuple of int - a triangle
    Returns:
      Copy of er excluding those with tl or tr == s or t
    """

    ans = []
    for e in er:
        (_, _, tl, tr) = e
        if tl == s or tr == s or tl == t or tr == t:
            continue
        ans.append(e)
    return ans


def _DegreeDict(tris):
    """Return a dictionary mapping vertices in tris to the number of triangles
    that they are touch."""

    ans = dict()
    for t in tris:
        for v in t:
            if v in ans:
                ans[v] = ans[v] + 1
            else:
                ans[v] = 1
    return ans


def PolygonPlane(face, points):
    """Return a Normal vector for the face with 3d coords given by indexing
    into points."""

    if len(face) < 3:
        return (0.0, 0.0, 1.0)    # arbitrary, we really have no idea
    else:
        coords = [points.pos[i] for i in face]
        return Normal(coords)


# This Normal appears to be on the CCW-traversing side of a polygon
def Normal(coords):
    """Return an average Normal vector for the point list, 3d coords."""

    if len(coords) < 3:
        return (0.0, 0.0, 1.0)    # arbitrary

    (ax, ay, az) = coords[0]
    (bx, by, bz) = coords[1]
    (cx, cy, cz) = coords[2]

    if len(coords) == 3:
        sx = (ay - by) * (az + bz) + \
            (by - cy) * (bz + cz) + \
            (cy - ay) * (cz + az)
        sy = (az - bz) * (ax + bx) + \
            (bz - cz) * (bx + cx) + \
            (cz - az) * (cx + ax)
        sz = (ax - bx) * (by + by) + \
            (bx - cx) * (by + cy) + \
            (cx - ax) * (cy + ay)
        return Norm3(sx, sy, sz)
    else:
        sx = (ay - by) * (az + bz) + (by - cy) * (bz + cz)
        sy = (az - bz) * (ax + bx) + (bz - cz) * (bx + cx)
        sz = (ax - bx) * (ay + by) + (bx - cx) * (by + cy)
        return _NormalAux(coords[3:], coords[0], sx, sy, sz)


def _NormalAux(rest, first, sx, sy, sz):
    (ax, ay, az) = rest[0]
    if len(rest) == 1:
        (bx, by, bz) = first
    else:
        (bx, by, bz) = rest[1]
    nx = sx + (ay - by) * (az + bz)
    ny = sy + (az - bz) * (ax + bx)
    nz = sz + (ax - bx) * (ay + by)
    if len(rest) == 1:
        return Norm3(nx, ny, nz)
    else:
        return _NormalAux(rest[1:], first, nx, ny, nz)


def Norm3(x, y, z):
    """Return vector (x,y,z) normalized by dividing by squared length.
    Return (0.0, 0.0, 1.0) if the result is undefined."""
    sqrlen = x * x + y * y + z * z
    if sqrlen < 1e-100:
        return (0.0, 0.0, 1.0)
    else:
        try:
            d = sqrt(sqrlen)
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


def Dot2(a, b):
    """Return the dot product of two 2d vectors, a . b."""

    return a[0] * b[0] + a[1] * b[1]


def Perp2(a, b):
    """Return a sort of 2d cross product."""

    return a[0] * b[1] - a[1] * b[0]


def Sub2(a, b):
    """Return difference of 2d vectors, a-b."""

    return (a[0] - b[0], a[1] - b[1])


def Add2(a, b):
    """Return the sum of 2d vectors, a+b."""

    return (a[0] + b[0], a[1] + b[1])


def Length2(v):
    """Return length of vector v=(x,y)."""

    return hypot(v[0], v[1])


def LinInterp2(a, b, alpha):
    """Return the point alpha of the way from a to b."""

    beta = 1 - alpha
    return (beta * a[0] + alpha * b[0], beta * a[1] + alpha * b[1])


def Normalized2(p):
    """Return vector p normlized by dividing by its squared length.
    Return (0.0, 1.0) if the result is undefined."""

    (x, y) = p
    sqrlen = x * x + y * y
    if sqrlen < 1e-100:
        return (0.0, 1.0)
    else:
        try:
            d = sqrt(sqrlen)
            return (x / d, y / d)
        except:
            return (0.0, 1.0)


def Angle(a, b, c, points):
    """Return Angle abc in degrees, in range [0,180),
    where a,b,c are indices into points."""

    u = Sub2(points.pos[c], points.pos[b])
    v = Sub2(points.pos[a], points.pos[b])
    n1 = Length2(u)
    n2 = Length2(v)
    if n1 == 0.0 or n2 == 0.0:
        return 0.0
    else:
        costheta = Dot2(u, v) / (n1 * n2)
        if costheta > 1.0:
            costheta = 1.0
        if costheta < - 1.0:
            costheta = - 1.0
        return math.acos(costheta) * 180.0 / math.pi


def SegsIntersect(ixa, ixb, ixc, ixd, points):
    """Return true if segment AB intersects CD,
    false if they just touch.  ixa, ixb, ixc, ixd are indices
    into points."""

    a = points.pos[ixa]
    b = points.pos[ixb]
    c = points.pos[ixc]
    d = points.pos[ixd]
    u = Sub2(b, a)
    v = Sub2(d, c)
    w = Sub2(a, c)
    pp = Perp2(u, v)
    if abs(pp) > TOL:
        si = Perp2(v, w) / pp
        ti = Perp2(u, w) / pp
        return 0.0 < si < 1.0 and 0.0 < ti < 1.0
    else:
        # parallel or overlapping
        if Dot2(u, u) == 0.0 or Dot2(v, v) == 0.0:
            return False
        else:
            pp2 = Perp2(w, v)
            if abs(pp2) > TOL:
                return False  # parallel, not collinear
            z = Sub2(b, c)
            (vx, vy) = v
            (wx, wy) = w
            (zx, zy) = z
            if vx == 0.0:
                (t0, t1) = (wy / vy, zy / vy)
            else:
                (t0, t1) = (wx / vx, zx / vx)
            return 0.0 < t0 < 1.0 and 0.0 < t1 < 1.0


def Ccw(a, b, c, points):
    """Return true if ABC is a counterclockwise-oriented triangle,
    where a, b, and c are indices into points.
    Returns false if not, or if colinear within TOL."""

    (ax, ay) = (points.pos[a][0], points.pos[a][1])
    (bx, by) = (points.pos[b][0], points.pos[b][1])
    (cx, cy) = (points.pos[c][0], points.pos[c][1])
    d = ax * by - bx * ay - ax * cy + cx * ay + bx * cy - cx * by
    return d > TOL


def InCircle(a, b, c, d, points):
    """Return true if circle through points with indices a, b, c
    contains point with index d (indices into points).
    Except: if ABC forms a counterclockwise oriented triangle
    then the test is reversed: return true if d is outside the circle.
    Will get false, no matter what orientation, if d is cocircular, with TOL^2.
      | xa ya xa^2+ya^2 1 |
      | xb yb xb^2+yb^2 1 | > 0
      | xc yc xc^2+yc^2 1 |
      | xd yd xd^2+yd^2 1 |
    """

    (xa, ya, za) = _Icc(points.pos[a])
    (xb, yb, zb) = _Icc(points.pos[b])
    (xc, yc, zc) = _Icc(points.pos[c])
    (xd, yd, zd) = _Icc(points.pos[d])
    det = xa * (yb * zc - yc * zb - yb * zd + yd * zb + yc * zd - yd * zc) \
          - xb * (ya * zc - yc * za - ya * zd + yd * za + yc * zd - yd * zc) \
          + xc * (ya * zb - yb * za - ya * zd + yd * za + yb * zd - yd * zb) \
          - xd * (ya * zb - yb * za - ya * zc + yc * za + yb * zc - yc * zb)
    return det > TOL * TOL


def _Icc(p):
    (x, y) = (p[0], p[1])
    return (x, y, x * x + y * y)
