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

# <pep8-80 compliant>

__all__ = (
    "mesh_linked_uv_islands",
    "mesh_linked_triangles",
    "edge_face_count_dict",
    "edge_face_count",
    "edge_loops_from_edges",
    "ngon_tessellate",
    "triangle_random_points",
)


def mesh_linked_uv_islands(mesh):
    """
    Splits the mesh into connected polygons, use this for separating cubes from
    other mesh elements within 1 mesh datablock.

    :arg mesh: the mesh used to group with.
    :type mesh: :class:`bpy.types.Mesh`
    :return: lists of lists containing polygon indices
    :rtype: list
    """
    uv_loops = [luv.uv[:] for luv in mesh.uv_layers.active.data]
    poly_loops = [poly.loop_indices for poly in mesh.polygons]
    luv_hash = {}
    luv_hash_get = luv_hash.get
    luv_hash_ls = [None] * len(uv_loops)
    for pi, poly_indices in enumerate(poly_loops):
        for li in poly_indices:
            uv = uv_loops[li]
            uv_hub = luv_hash_get(uv)
            if uv_hub is None:
                uv_hub = luv_hash[uv] = [pi]
            else:
                uv_hub.append(pi)
            luv_hash_ls[li] = uv_hub

    poly_islands = []

    # 0 = none, 1 = added, 2 = searched
    poly_tag = [0] * len(poly_loops)

    while True:
        poly_index = -1
        for i in range(len(poly_loops)):
            if poly_tag[i] == 0:
                poly_index = i
                break

        if poly_index != -1:
            island = [poly_index]
            poly_tag[poly_index] = 1
            poly_islands.append(island)
        else:
            break  # we're done

        added = True
        while added:
            added = False
            for poly_index in island[:]:
                if poly_tag[poly_index] == 1:
                    for li in poly_loops[poly_index]:
                        for poly_index_shared in luv_hash_ls[li]:
                            if poly_tag[poly_index_shared] == 0:
                                added = True
                                poly_tag[poly_index_shared] = 1
                                island.append(poly_index_shared)
                    poly_tag[poly_index] = 2

    return poly_islands


def mesh_linked_triangles(mesh):
    """
    Splits the mesh into connected triangles, use this for separating cubes from
    other mesh elements within 1 mesh datablock.

    :arg mesh: the mesh used to group with.
    :type mesh: :class:`bpy.types.Mesh`
    :return: lists of lists containing triangles.
    :rtype: list
    """

    # Build vert face connectivity
    vert_tris = [[] for i in range(len(mesh.vertices))]
    for t in mesh.loop_triangles:
        for v in t.vertices:
            vert_tris[v].append(t)

    # sort triangles into connectivity groups
    tri_groups = [[t] for t in mesh.loop_triangles]
    # map old, new tri location
    tri_mapping = list(range(len(mesh.loop_triangles)))

    # Now clump triangles iteratively
    ok = True
    while ok:
        ok = False

        for t in mesh.loop_triangles:
            mapped_index = tri_mapping[t.index]
            mapped_group = tri_groups[mapped_index]

            for v in t.vertices:
                for nxt_t in vert_tris[v]:
                    if nxt_t != t:
                        nxt_mapped_index = tri_mapping[nxt_t.index]

                        # We are not a part of the same group
                        if mapped_index != nxt_mapped_index:
                            ok = True

                            # Assign mapping to this group so they
                            # all map to this group
                            for grp_t in tri_groups[nxt_mapped_index]:
                                tri_mapping[grp_t.index] = mapped_index

                            # Move triangles into this group
                            mapped_group.extend(tri_groups[nxt_mapped_index])

                            # remove reference to the list
                            tri_groups[nxt_mapped_index] = None

    # return all tri groups that are not null
    # this is all the triangles that are connected in their own lists.
    return [tg for tg in tri_groups if tg]


def edge_face_count_dict(mesh):
    """
    :return: dict of edge keys with their value set to the number of
       faces using each edge.
    :rtype: dict
    """

    face_edge_count = {}
    loops = mesh.loops
    edges = mesh.edges
    for poly in mesh.polygons:
        for i in poly.loop_indices:
            key = edges[loops[i].edge_index].key
            try:
                face_edge_count[key] += 1
            except:
                face_edge_count[key] = 1

    return face_edge_count


def edge_face_count(mesh):
    """
    :return: list face users for each item in mesh.edges.
    :rtype: list
    """
    edge_face_count = edge_face_count_dict(mesh)
    get = dict.get
    return [get(edge_face_count, ed.key, 0) for ed in mesh.edges]


def edge_loops_from_edges(mesh, edges=None):
    """
    Edge loops defined by edges

    Takes me.edges or a list of edges and returns the edge loops

    return a list of vertex indices.
    [ [1, 6, 7, 2], ...]

    closed loops have matching start and end values.
    """
    line_polys = []

    # Get edges not used by a face
    if edges is None:
        edges = mesh.edges

    if not hasattr(edges, "pop"):
        edges = edges[:]

    while edges:
        current_edge = edges.pop()
        vert_end, vert_start = current_edge.vertices[:]
        line_poly = [vert_start, vert_end]

        ok = True
        while ok:
            ok = False
            # for i, ed in enumerate(edges):
            i = len(edges)
            while i:
                i -= 1
                ed = edges[i]
                v1, v2 = ed.vertices
                if v1 == vert_end:
                    line_poly.append(v2)
                    vert_end = line_poly[-1]
                    ok = 1
                    del edges[i]
                    # break
                elif v2 == vert_end:
                    line_poly.append(v1)
                    vert_end = line_poly[-1]
                    ok = 1
                    del edges[i]
                    # break
                elif v1 == vert_start:
                    line_poly.insert(0, v2)
                    vert_start = line_poly[0]
                    ok = 1
                    del edges[i]
                    # break
                elif v2 == vert_start:
                    line_poly.insert(0, v1)
                    vert_start = line_poly[0]
                    ok = 1
                    del edges[i]
                    # break
        line_polys.append(line_poly)

    return line_polys


def ngon_tessellate(from_data, indices, fix_loops=True, debug_print=True):
    """
    Takes a polyline of indices (fgon) and returns a list of face
    index lists. Designed to be used for importers that need indices for an
    fgon to create from existing verts.

    :arg from_data: either a mesh, or a list/tuple of vectors.
    :type from_data: list or :class:`bpy.types.Mesh`
    :arg indices: a list of indices to use this list
       is the ordered closed polyline
       to fill, and can be a subset of the data given.
    :type indices: list
    :arg fix_loops: If this is enabled polylines
       that use loops to make multiple
       polylines are delt with correctly.
    :type fix_loops: bool
    """

    from mathutils.geometry import tessellate_polygon
    from mathutils import Vector
    vector_to_tuple = Vector.to_tuple

    if not indices:
        return []

    def mlen(co):
        # manhatten length of a vector, faster then length
        return abs(co[0]) + abs(co[1]) + abs(co[2])

    def vert_treplet(v, i):
        return v, vector_to_tuple(v, 6), i, mlen(v)

    def ed_key_mlen(v1, v2):
        if v1[3] > v2[3]:
            return v2[1], v1[1]
        else:
            return v1[1], v2[1]

    if not fix_loops:
        """
        Normal single concave loop filling
        """
        if type(from_data) in {tuple, list}:
            verts = [Vector(from_data[i]) for ii, i in enumerate(indices)]
        else:
            verts = [from_data.vertices[i].co for ii, i in enumerate(indices)]

        # same as reversed(range(1, len(verts))):
        for i in range(len(verts) - 1, 0, -1):
            if verts[i][1] == verts[i - 1][0]:
                verts.pop(i - 1)

        fill = tessellate_polygon([verts])

    else:
        """
        Separate this loop into multiple loops be finding edges that are
        used twice. This is used by lightwave LWO files a lot
        """

        if type(from_data) in {tuple, list}:
            verts = [vert_treplet(Vector(from_data[i]), ii)
                     for ii, i in enumerate(indices)]
        else:
            verts = [vert_treplet(from_data.vertices[i].co, ii)
                     for ii, i in enumerate(indices)]

        edges = [(i, i - 1) for i in range(len(verts))]
        if edges:
            edges[0] = (0, len(verts) - 1)

        if not verts:
            return []

        edges_used = set()
        edges_doubles = set()
        # We need to check if any edges are used twice location based.
        for ed in edges:
            edkey = ed_key_mlen(verts[ed[0]], verts[ed[1]])
            if edkey in edges_used:
                edges_doubles.add(edkey)
            else:
                edges_used.add(edkey)

        # Store a list of unconnected loop segments split by double edges.
        # will join later
        loop_segments = []

        v_prev = verts[0]
        context_loop = [v_prev]
        loop_segments = [context_loop]

        for v in verts:
            if v != v_prev:
                # Are we crossing an edge we removed?
                if ed_key_mlen(v, v_prev) in edges_doubles:
                    context_loop = [v]
                    loop_segments.append(context_loop)
                else:
                    if context_loop and context_loop[-1][1] == v[1]:
                        pass
                    else:
                        context_loop.append(v)

                v_prev = v
        # Now join loop segments

        def join_seg(s1, s2):
            if s2[-1][1] == s1[0][1]:
                s1, s2 = s2, s1
            elif s1[-1][1] == s2[0][1]:
                pass
            else:
                return False

            # If were stuill here s1 and s2 are 2 segments in the same polyline
            s1.pop()  # remove the last vert from s1
            s1.extend(s2)  # add segment 2 to segment 1

            if s1[0][1] == s1[-1][1]:  # remove endpoints double
                s1.pop()

            del s2[:]  # Empty this segment s2 so we don't use it again.
            return True

        joining_segments = True
        while joining_segments:
            joining_segments = False
            segcount = len(loop_segments)

            for j in range(segcount - 1, -1, -1):  # reversed(range(segcount)):
                seg_j = loop_segments[j]
                if seg_j:
                    for k in range(j - 1, -1, -1):  # reversed(range(j)):
                        if not seg_j:
                            break
                        seg_k = loop_segments[k]

                        if seg_k and join_seg(seg_j, seg_k):
                            joining_segments = True

        loop_list = loop_segments

        for verts in loop_list:
            while verts and verts[0][1] == verts[-1][1]:
                verts.pop()

        loop_list = [verts for verts in loop_list if len(verts) > 2]
        # DONE DEALING WITH LOOP FIXING

        # vert mapping
        vert_map = [None] * len(indices)
        ii = 0
        for verts in loop_list:
            if len(verts) > 2:
                for i, vert in enumerate(verts):
                    vert_map[i + ii] = vert[2]
                ii += len(verts)

        fill = tessellate_polygon([[v[0] for v in loop] for loop in loop_list])
        # draw_loops(loop_list)
        #raise Exception("done loop")
        # map to original indices
        fill = [[vert_map[i] for i in reversed(f)] for f in fill]

    if not fill:
        if debug_print:
            print('Warning Cannot scanfill, fallback on a triangle fan.')
        fill = [[0, i - 1, i] for i in range(2, len(indices))]
    else:
        # Use real scanfill.
        # See if its flipped the wrong way.
        flip = None
        for fi in fill:
            if flip is not None:
                break
            for i, vi in enumerate(fi):
                if vi == 0 and fi[i - 1] == 1:
                    flip = False
                    break
                elif vi == 1 and fi[i - 1] == 0:
                    flip = True
                    break

        if not flip:
            for i, fi in enumerate(fill):
                fill[i] = tuple([ii for ii in reversed(fi)])

    return fill


def triangle_random_points(num_points, loop_triangles):
    """
    Generates a list of random points over mesh loop triangles.

    :arg num_points: the number of random points to generate on each triangle.
    :type int:
    :arg loop_triangles: list of the triangles to generate points on.
    :type loop_triangles: :class:`bpy.types.MeshLoopTriangle`, sequence
    :return: list of random points over all triangles.
    :rtype: list
    """

    from random import random

    # For each triangle, generate the required number of random points
    sampled_points = [None] * (num_points * len(loop_triangles))
    for i, lt in enumerate(loop_triangles):
        # Get triangle vertex coordinates
        verts = lt.id_data.vertices
        ltv = lt.vertices[:]
        tv = (verts[ltv[0]].co, verts[ltv[1]].co, verts[ltv[2]].co)

        for k in range(num_points):
            u1 = random()
            u2 = random()
            u_tot = u1 + u2

            if u_tot > 1:
                u1 = 1.0 - u1
                u2 = 1.0 - u2

            side1 = tv[1] - tv[0]
            side2 = tv[2] - tv[0]

            p = tv[0] + u1 * side1 + u2 * side2

            sampled_points[num_points * i + k] = p

    return sampled_points
