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

# Utilities to detect the next matching element (vert/edge/face)
# based on an existing pair of elements.

import bmesh

__all__ = (
    "select_prev",
    "select_next",
)


def other_edges_over_face(e):
    # Can yield same edge multiple times, its fine.
    for l in e.link_loops:
        yield l.link_loop_next.edge
        yield l.link_loop_prev.edge


def other_edges_over_edge(e):
    # Can yield same edge multiple times, its fine.
    for v in e.verts:
        for e_other in v.link_edges:
            if e_other is not e:
                if not e.is_wire:
                    yield e_other


def verts_from_elem(ele):
    ele_type = type(ele)
    if ele_type is bmesh.types.BMFace:
        return [l.vert for l in ele.loops]
    elif ele_type is bmesh.types.BMEdge:
        return [v for v in ele.verts]
    elif ele_type is bmesh.types.BMVert:
        return [ele]
    else:
        raise TypeError("wrong type")


def edges_from_elem(ele):
    ele_type = type(ele)
    if ele_type is bmesh.types.BMFace:
        return [l.edge for l in ele.loops]
    elif ele_type is bmesh.types.BMEdge:
        return [ele]
    elif ele_type is bmesh.types.BMVert:
        return [e for e in ele.link_edges]
    else:
        raise TypeError("wrong type")


def elems_depth_search(ele_init, depths, other_edges_over_cb, results_init=None):
    """
    List of depths -> List of elems that match those depths.
    """

    depth_max = max(depths)
    depth_min = min(depths)
    depths_sorted = tuple(sorted(depths))

    stack_old = edges_from_elem(ele_init)
    stack_new = []

    stack_visit = set(stack_old)

    vert_depths = {}
    vert_depths_setdefault = vert_depths.setdefault

    depth = 0
    while stack_old and depth <= depth_max:
        for ele in stack_old:
            for v in verts_from_elem(ele):
                vert_depths_setdefault(v, depth)
            for ele_other in other_edges_over_cb(ele):
                stack_visit_len = len(stack_visit)
                stack_visit.add(ele_other)
                if stack_visit_len != len(stack_visit):
                    stack_new.append(ele_other)
        stack_new, stack_old = stack_old, stack_new
        stack_new[:] = []
        depth += 1

    # now we have many verts in vert_depths which are attached to elements
    # which are candidates for matching with depths
    if type(ele_init) is bmesh.types.BMFace:
        test_ele = {
            l.face for v, depth in vert_depths.items()
            if depth >= depth_min for l in v.link_loops}
    elif type(ele_init) is bmesh.types.BMEdge:
        test_ele = {
            e for v, depth in vert_depths.items()
            if depth >= depth_min for e in v.link_edges if not e.is_wire}
    else:
        test_ele = {
            v for v, depth in vert_depths.items()
            if depth >= depth_min}

    result_ele = set()

    vert_depths_get = vert_depths.get
    # re-used each time, will always be the same length
    depths_test = [None] * len(depths)

    for ele in test_ele:
        verts_test = verts_from_elem(ele)
        if len(verts_test) != len(depths):
            continue
        if results_init is not None and ele not in results_init:
            continue
        if ele in result_ele:
            continue

        ok = True
        for i, v in enumerate(verts_test):
            depth = vert_depths_get(v)
            if depth is not None:
                depths_test[i] = depth
            else:
                ok = False
                break

        if ok:
            if depths_sorted == tuple(sorted(depths_test)):
                # Note, its possible the order of sorted items moves the values out-of-order.
                # for this we could do a circular list comparison,
                # however - this is such a rare case that we're ignoring it.
                result_ele.add(ele)

    return result_ele


def elems_depth_measure(ele_dst, ele_src, other_edges_over_cb):
    """
    Returns·ele_dst vert depths from ele_src, aligned with ele_dst verts.
    """

    stack_old = edges_from_elem(ele_src)
    stack_new = []

    stack_visit = set(stack_old)

    # continue until we've reached all verts in the destination
    ele_dst_verts = verts_from_elem(ele_dst)
    all_dst = set(ele_dst_verts)
    all_dst_discard = all_dst.discard

    vert_depths = {}

    depth = 0
    while stack_old and all_dst:
        for ele in stack_old:
            for v in verts_from_elem(ele):
                len_prev = len(all_dst)
                all_dst_discard(v)
                if len_prev != len(all_dst):
                    vert_depths[v] = depth

            for ele_other in other_edges_over_cb(ele):
                stack_visit_len = len(stack_visit)
                stack_visit.add(ele_other)
                if stack_visit_len != len(stack_visit):
                    stack_new.append(ele_other)
        stack_new, stack_old = stack_old, stack_new
        stack_new[:] = []
        depth += 1

    if not all_dst:
        return [vert_depths[v] for v in ele_dst_verts]
    else:
        return None


def find_next(ele_dst, ele_src):
    depth_src_a = elems_depth_measure(ele_dst, ele_src, other_edges_over_edge)
    depth_src_b = elems_depth_measure(ele_dst, ele_src, other_edges_over_face)

    # path not found
    if depth_src_a is None or depth_src_b is None:
        return []

    depth_src = tuple(zip(depth_src_a, depth_src_b))

    candidates = elems_depth_search(ele_dst, depth_src_a, other_edges_over_edge)
    candidates = elems_depth_search(ele_dst, depth_src_b, other_edges_over_face, candidates)
    candidates.discard(ele_src)
    candidates.discard(ele_dst)
    if not candidates:
        return []

    # Now we have to pick which is the best next-element,
    # do this by calculating the element with the largest
    # variation in depth from the relationship to the source.
    # ... So we have the highest chance of stepping onto the opposite element.
    diff_best = 0
    ele_best = None
    ele_best_ls = []
    for ele_test in candidates:
        depth_test_a = elems_depth_measure(ele_dst, ele_test, other_edges_over_edge)
        depth_test_b = elems_depth_measure(ele_dst, ele_test, other_edges_over_face)
        if depth_test_a is None or depth_test_b is None:
            continue
        depth_test = tuple(zip(depth_test_a, depth_test_b))
        # square so a few high values win over many small ones
        diff_test = sum((abs(a[0] - b[0]) ** 2) +
                        (abs(a[1] - b[1]) ** 2) for a, b in zip(depth_src, depth_test))
        if diff_test > diff_best:
            diff_best = diff_test
            ele_best = ele_test
            ele_best_ls[:] = [ele_best]
        elif diff_test == diff_best:
            if ele_best is None:
                ele_best = ele_test
            ele_best_ls.append(ele_test)

    if len(ele_best_ls) > 1:
        ele_best_ls_init = ele_best_ls
        ele_best_ls = []
        depth_accum_max = -1
        for ele_test in ele_best_ls_init:
            depth_test_a = elems_depth_measure(ele_src, ele_test, other_edges_over_edge)
            depth_test_b = elems_depth_measure(ele_src, ele_test, other_edges_over_face)
            if depth_test_a is None or depth_test_b is None:
                continue
            depth_accum_test = (
                sum(depth_test_a) + sum(depth_test_b))

            if depth_accum_test > depth_accum_max:
                depth_accum_max = depth_accum_test
                ele_best = ele_test
                ele_best_ls[:] = [ele_best]
            elif depth_accum_test == depth_accum_max:
                # we have multiple bests, don't return any
                ele_best_ls.append(ele_test)

    return ele_best_ls


# expose for operators
def select_next(bm, report):
    import bmesh
    ele_pair = [None, None]
    for i, ele in enumerate(reversed(bm.select_history)):
        ele_pair[i] = ele
        if i == 1:
            break

    if ele_pair[-1] is None:
        report({'INFO'}, "Selection pair not found")
        return False

    ele_pair_next = find_next(*ele_pair)

    if len(ele_pair_next) > 1:
        # We have multiple options,
        # check topology around the element and find the closest match
        # (allow for sloppy comparison if exact checks fail).

        def ele_uuid(ele):
            ele_type = type(ele)
            if ele_type is bmesh.types.BMFace:
                ret = [len(f.verts) for l in ele.loops for f in l.edge.link_faces if f is not ele]
            elif ele_type is bmesh.types.BMEdge:
                ret = [len(l.face.verts) for l in ele.link_loops]
            elif ele_type is bmesh.types.BMVert:
                ret = [len(l.face.verts) for l in ele.link_loops]
            else:
                raise TypeError("wrong type")
            return tuple(sorted(ret))

        def ele_uuid_filter():

            def pass_fn(seq):
                return seq

            def sum_set(seq):
                return sum(set(seq))

            uuid_cmp = ele_uuid(ele_pair[0])
            ele_pair_next_uuid = [(ele, ele_uuid(ele)) for ele in ele_pair_next]

            # Attempt to find the closest match,
            # start specific, use increasingly more approximate comparisons.
            for fn in (pass_fn, set, sum_set, len):
                uuid_cmp_test = fn(uuid_cmp)
                ele_pair_next_uuid_test = [
                    (ele, uuid) for (ele, uuid) in ele_pair_next_uuid
                    if uuid_cmp_test == fn(uuid)
                ]
                if len(ele_pair_next_uuid_test) > 1:
                    ele_pair_next_uuid = ele_pair_next_uuid_test
                elif len(ele_pair_next_uuid_test) == 1:
                    return [ele for (ele, uuid) in ele_pair_next_uuid_test]
            return []

        ele_pair_next[:] = ele_uuid_filter()

        del ele_uuid, ele_uuid_filter

    if len(ele_pair_next) != 1:
        report({'INFO'}, "No single next item found")
        return False

    ele = ele_pair_next[0]
    if ele.hide:
        report({'INFO'}, "Next element is hidden")
        return False

    ele.select_set(False)
    ele.select_set(True)
    bm.select_history.discard(ele)
    bm.select_history.add(ele)
    if type(ele) is bmesh.types.BMFace:
        bm.faces.active = ele
    return True


def select_prev(bm, report):
    import bmesh
    for ele in reversed(bm.select_history):
        break
    else:
        report({'INFO'}, "Last selected not found")
        return False

    ele.select_set(False)

    for i, ele in enumerate(reversed(bm.select_history)):
        if i == 1:
            if type(ele) is bmesh.types.BMFace:
                bm.faces.active = ele
            break

    return True
