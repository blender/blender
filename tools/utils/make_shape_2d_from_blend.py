# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

'''
Created compact byte arrays which can be decoded into 2D shapes.
(See 'GPU_batch_from_poly_2d_encoded').

- Objects must use the prefix "shape_"
- Meshes and Curves are supported as input.
- C and Python output is written to "output/"

The format is simple: a series of  (X, Y) locations one byte each.
Repeating the same value terminates the polygon, moving onto the next.

Example Use::

   blender.bin -b --factory-startup my_shapes.blend --python make_shape_2d_from_blend.py
'''
import bpy
import os

USE_C_STYLE = True
USE_PY_STYLE = True

WRAP_LIMIT = 79
TAB_WIDTH = 4

SUBDIR = "output"


def float_to_ubyte(f):
    return max(0, min(255, int(round(f * 255.0))))


def curve_to_loops(ob):
    import bmesh
    cu = ob.data

    me = ob.to_mesh()
    bm = bmesh.new()
    bm.from_mesh(me)
    me = ob.to_mesh_clear()

    bmesh.ops.beautify_fill(bm, faces=bm.faces, edges=bm.edges)

    edges = bm.edges[:]
    edges.sort(key=lambda e: e.calc_length(), reverse=True)

    for e in edges:
        if e.is_manifold:
            f_a, f_b = [f for f in e.link_faces]
            bmesh.utils.face_join((f_a, f_b), False)

    edges = bm.edges[:]
    for e in edges:
        if e.is_wire:
            bm.edges.remove(e)

    bm.normal_update()

    data_all = []
    for f in bm.faces:
        points = []
        # Ensure all faces are pointing the correct direction
        # Note, we may want to use polygon sign for a second color
        # (via the material index).
        if f.normal.z > 0.0:
            loops = f.loops
        else:
            loops = reversed(f.loops)
        for l in loops:
            points.append(
                tuple(float_to_ubyte(axis) for axis in l.vert.co.xy)
            )
        data_all.append((points, f.material_index))
    bm.free()
    return data_all


def write_c(ob):
    cu = ob.data
    name = ob.name
    with open(os.path.join(SUBDIR, name + ".c"), 'w') as fh:
        fw = fh.write
        fw(f"/* {name} */\n")
        fw(f"const uchar {name}[] = {{")
        line_len = WRAP_LIMIT
        line_is_first = True
        array_len = 0
        data_all = curve_to_loops(ob)
        for (points, material_index) in data_all:
            # TODO, material_index
            for p in points + [points[-1]]:
                line_len += 12
                if line_len >= WRAP_LIMIT:
                    fw("\n\t")
                    line_len = TAB_WIDTH
                    line_is_first = True
                if not line_is_first:
                    fw(" ")
                fw(", ".join([f"0x{axis:02x}" for axis in p]) + ",")
                line_is_first = False
            array_len += (len(points) + 1) * 2
        fw("\n};\n")
        # fw(f"const int data_len = {array_len}\n")


def write_py(ob):
    cu = ob.data
    name = ob.name
    with open(os.path.join(SUBDIR, name + ".py"), 'w') as fh:
        fw = fh.write
        fw(f"# {name}\n")
        fw("data = (")
        line_len = WRAP_LIMIT
        fw = fh.write
        data_all = curve_to_loops(ob)
        for (points, material_index) in data_all:
            # TODO, material_index
            for p in points + [points[-1]]:
                line_len += 8
                if line_len >= WRAP_LIMIT:
                    if p is not points[0]:
                        fw("'")
                    fw("\n    b'")
                    line_len = 6
                fw("".join([f"\\x{axis:02x}" for axis in p]))
        fw("'\n)\n")


def main():
    os.makedirs(SUBDIR, exist_ok=True)
    for ob in bpy.data.objects:
        if ob.type not in {'MESH', 'CURVE'}:
            continue
        if not ob.name.startswith('shape_'):
            continue
        if USE_C_STYLE:
            write_c(ob)
        if USE_PY_STYLE:
            write_py(ob)


if __name__ == "__main__":
    main()
