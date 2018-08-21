# Apache License, Version 2.0

"""
Example Usage
=============

Command line::

   ./blender.bin \
       icon_file.blend --background --python ./release/datafiles/blender_icons_geom.py -- \
       --output-dir=./release/datafiles/blender_icons_geom

Icon Format
===========

This is a simple binary format (all bytes, so no endian).

The header is 8 bytes:

:0..3: ``VCO``: identifier.
:4: ``0``: icon file version.
:5: icon size-x.
:6: icon size-y.
:7: icon start-x.
:8: icon start-y.

Icon width and height are for icons that don't use the full byte range
(so we don't get bad alignment for 48 pixel grid for eg).

Start values are currently unused.

After the header, the remaining length of the data defines the geometry size.

:6 bytes each: triangle (XY) locations.
:12 bytes each: triangle (RGBA) locations.

All coordinates are written, then all colors.

Since this is a binary format which isn't intended for general use
the ``.dat`` file extension should be used.
"""

# This script writes out geometry-icons.
import bpy

# Generic functions

def area_tri_signed_2x_v2(v1, v2, v3):
	return (v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0])


class TriMesh:
    """
    Triangulate, may apply other changes here too.
    """
    __slots__ = ("object", "mesh")

    def __init__(self, ob):
        self.object = ob
        self.mesh = None

    def __enter__(self):
        self.mesh = self._tri_copy_from_object(self.object)
        return self.mesh

    def __exit__(self, *args):
        bpy.data.meshes.remove(self.mesh)

    @staticmethod
    def _tri_copy_from_object(ob):
        import bmesh
        assert(ob.type == 'MESH')
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.triangulate(bm, faces=bm.faces)
        me = bpy.data.meshes.new(ob.name + ".copy")
        bm.to_mesh(me)
        bm.free()
        return me


def object_material_colors(ob):
    material_colors = []
    color_default = (1.0, 1.0, 1.0, 1.0)
    for slot in ob.material_slots:
        material = slot.material
        color = color_default
        if material is not None and material.use_nodes:
            node_tree = material.node_tree
            if node_tree is not None:
                color = next((
                    node.outputs[0].default_value[:]
                    for node in node_tree.nodes
                    if node.type == 'RGB'
                ), color_default)
        material_colors.append(color)
    return material_colors


def object_child_map(objects):
    objects_children = {}
    for ob in objects:
        ob_parent = ob.parent
        # Get the root.
        if ob_parent is not None:
            while ob_parent and ob_parent.parent:
                ob_parent = ob_parent.parent
        if ob_parent is not None:
            objects_children.setdefault(ob_parent, []).append(ob)
    for ob_all in objects_children.values():
        ob_all.sort(key=lambda ob: ob.name)
    return objects_children


def mesh_data_lists_from_mesh(me, material_colors):
    me_loops = me.loops[:]
    me_loops_color = me.vertex_colors.active.data[:]
    me_verts = me.vertices[:]
    me_polys = me.polygons[:]

    tris_data = []

    for p in me_polys:
        # Note, all faces are handled, backfacing/zero area is checked just before writing.
        material_index = p.material_index
        if material_index < len(material_colors):
            base_color = material_colors[p.material_index]
        else:
            base_color = (1.0, 1.0, 1.0, 1.0)

        l_sta = p.loop_start
        l_len = p.loop_total
        loops_poly = me_loops[l_sta:l_sta + l_len]
        color_poly = me_loops_color[l_sta:l_sta + l_len]
        i0 = 0
        i1 = 1

        # we only write tris now
        assert(len(loops_poly) == 3)

        for i2 in range(2, l_len):
            l0 = loops_poly[i0]
            l1 = loops_poly[i1]
            l2 = loops_poly[i2]

            c0 = color_poly[i0]
            c1 = color_poly[i1]
            c2 = color_poly[i2]

            v0 = me_verts[l0.vertex_index]
            v1 = me_verts[l1.vertex_index]
            v2 = me_verts[l2.vertex_index]

            tris_data.append((
                # float depth
                p.center.z,
                # XY coords.
                (
                    v0.co.xy[:],
                    v1.co.xy[:],
                    v2.co.xy[:],
                ),
                # RGBA color.
                tuple((
                    [min(max(int(c * b * 255), 0), 255) for c, b in zip(cn.color, base_color)]
                    for cn in (c0, c1, c2)
                )),
            ))
            i1 = i2
    return tris_data


def mesh_data_lists_from_objects(ob_parent, ob_children):
    tris_data = []

    has_parent = False
    if ob_children:
        parent_matrix = ob_parent.matrix_world.copy()
        parent_matrix_inverted = parent_matrix.inverted()

    for ob in (ob_parent, *ob_children):
        with TriMesh(ob) as me:
            if has_parent:
                me.transform(parent_matrix_inverted @ ob.matrix_world)

            tris_data.extend(
                mesh_data_lists_from_mesh(
                    me,
                    object_material_colors(ob),
                )
            )
        has_parent = True
    return tris_data


def write_mesh_to_py(fh, ob, ob_children):

    def float_as_byte(f, axis_range):
        assert(axis_range <= 255)
        # -1..1 -> 0..255
        f = (f + 1.0) * 0.5
        f = int(round(f * axis_range))
        return min(max(f, 0), axis_range)

    def vert_as_byte_pair(v):
        return (
            float_as_byte(v[0], coords_range_align[0]),
            float_as_byte(v[1], coords_range_align[1]),
        )

    tris_data = mesh_data_lists_from_objects(ob, ob_children)

    # 100 levels of Z depth, round to avoid differences from precision error
    # causing different computers to write triangles in more or less random order.
    tris_data.sort(key=lambda data: int(data[0] * 100))

    if 0:
        # make as large as we can, keeping alignment
        def size_scale_up(size):
            assert(size != 0)
            while size * 2 <= 255:
                size *= 2
            return size

        coords_range = (
            size_scale_up(ob.get("size_x")) or 255,
            size_scale_up(ob.get("size_y")) or 255,
        )
    else:
        # disable for now
        coords_range = 255, 255

    # Pixel size needs to be increased since a pixel needs one extra geom coordinate,
    # if we're writing 32 pixel, align verts to 33.
    coords_range_align = tuple(min(c + 1, 255) for c in coords_range)

    print("Writing:", fh.name, coords_range)

    fw = fh.write

    # Header (version 0).
    fw(b'VCO\x00')
    # Width, Height
    fw(bytes(coords_range))
    # X, Y
    fw(bytes((0, 0)))

    # Once converted into bytes, the triangle might become zero area
    tri_skip = [False] * len(tris_data)
    for i, (_, tri_coords, _) in enumerate(tris_data):
        tri_coords_as_byte = [vert_as_byte_pair(vert) for vert in tri_coords]
        if area_tri_signed_2x_v2(*tri_coords_as_byte) <= 0:
            tri_skip[i] = True
            continue
        for vert_byte in tri_coords_as_byte:
            fw(bytes(vert_byte))
    for i, (_, _, tri_color) in enumerate(tris_data):
        if tri_skip[i]:
            continue
        for color in tri_color:
            fw(bytes(color))


def create_argparse():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        default=".",
        type=str,
        metavar="DIR",
        required=False,
        help="Directory to write icons to.",
    )
    parser.add_argument(
        "--group",
        dest="group",
        default="",
        type=str,
        metavar="GROUP",
        required=False,
        help="Group name to export from (otherwise export all objects).",
    )
    return parser


def main():
    import os
    import sys
    parser = create_argparse()
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1:]
    else:
        argv = []
    args = parser.parse_args(argv)

    objects = []

    if args.group:
        group = bpy.data.collections.get(args.group)
        if group is None:
            print(f"Group {args.group!r} not found!")
            return
        objects_source = group.objects
        del group
    else:
        objects_source = bpy.data.objects

    for ob in objects_source:

        # Skip non-mesh objects
        if ob.type != 'MESH':
            continue
        name = ob.name

        # Skip copies of objects
        if name.rpartition(".")[2].isdigit():
            continue

        if not ob.data.vertex_colors:
            print("Skipping:", name, "(no vertex colors)")
            continue

        objects.append((name, ob))

    objects.sort(key=lambda a: a[0])

    objects_children = object_child_map(bpy.data.objects)

    for name, ob in objects:
        if ob.parent:
            continue
        filename = os.path.join(args.output_dir, name + ".dat")
        with open(filename, 'wb') as fh:
            write_mesh_to_py(fh, ob, objects_children.get(ob, []))


if __name__ == "__main__":
    main()
