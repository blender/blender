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

# Simple script to check mash validate code.
# XXX Should be extended with many more "wrong cases"!

import bpy

import sys
import random


MESHES = {
    "test1": (
        (
            (  # Verts
             (-1.0, -1.0, 0.0),
             (-1.0, 0.0, 0.0),
             (-1.0, 1.0, 0.0),
             (0.0, -1.0, 0.0),
             (0.0, 0.0, 0.0),
             (0.0, 1.0, 0.0),
             (1.0, -1.0, 0.0),
             (1.0, 0.0, 0.0),
             (1.5, 0.5, 0.0),
             (1.0, 1.0, 0.0),
            ),
            (  # Edges
            ),
            (  # Loops
                0, 1, 4, 3,
                3, 4, 6,
                1, 2, 5, 4,
                3, 4, 6,
                4, 7, 6,
                4, 5, 9, 4, 8, 7,
            ),
            (  # Polygons
                (0, 4),
                (4, 3),
                (7, 4),
                (11, 3),
                (14, 3),
                (16, 6),
            ),
        ),
    ),
}


BUILTINS = (
    "primitive_plane_add",
    "primitive_cube_add",
    "primitive_circle_add",
    "primitive_uv_sphere_add",
    "primitive_ico_sphere_add",
    "primitive_cylinder_add",
    "primitive_cone_add",
    "primitive_grid_add",
    "primitive_monkey_add",
    "primitive_torus_add",
)
BUILTINS_NBR = 4
BUILTINS_NBRCHANGES = 5


def test_meshes():
    for m in MESHES["test1"]:
        bpy.ops.object.add(type="MESH")
        data = bpy.context.active_object.data

        # Vertices.
        data.vertices.add(len(m[0]))
        for idx, v in enumerate(m[0]):
            data.vertices[idx].co = v
        # Edges.
        data.edges.add(len(m[1]))
        for idx, e in enumerate(m[1]):
            data.edges[idx].vertices = e
        # Loops.
        data.loops.add(len(m[2]))
        for idx, v in enumerate(m[2]):
            data.loops[idx].vertex_index = v
        # Polygons.
        data.polygons.add(len(m[3]))
        for idx, l in enumerate(m[3]):
            data.polygons[idx].loop_start = l[0]
            data.polygons[idx].loop_total = l[1]

        while data.validate(verbose=True):
            pass


def test_builtins():
    for x, func in enumerate(BUILTINS):
        for y in range(BUILTINS_NBR):
            getattr(bpy.ops.mesh, func)(location=(x * 2.5, y * 2.5, 0))
            data = bpy.context.active_object.data
            try:
                for n in range(BUILTINS_NBRCHANGES):
                    rnd = random.randint(1, 3)
                    if rnd == 1:
                        # Make fun with some edge.
                        e = random.randrange(0, len(data.edges))
                        data.edges[e].vertices[random.randint(0, 1)] = \
                            random.randrange(0, len(data.vertices) * 2)
                    elif rnd == 2:
                        # Make fun with some loop.
                        l = random.randrange(0, len(data.loops))
                        if random.randint(0, 1):
                            data.loops[l].vertex_index = \
                                random.randrange(0, len(data.vertices) * 2)
                        else:
                            data.loops[l].edge_index = \
                                random.randrange(0, len(data.edges) * 2)
                    elif rnd == 3:
                        # Make fun with some polygons.
                        p = random.randrange(0, len(data.polygons))
                        if random.randint(0, 1):
                            data.polygons[p].loop_start = \
                                random.randrange(0, len(data.loops))
                        else:
                            data.polygons[p].loop_total = \
                                random.randrange(0, 10)
            except:
                pass

            while data.validate(verbose=True):
                pass


def main():
    test_builtins()
    test_meshes()


if __name__ == "__main__":
    # So a python error exits(1)
    try:
        main()
    except:
        import traceback
        traceback.print_exc()
        sys.exit(1)
