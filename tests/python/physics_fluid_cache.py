# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

import bpy


def mesh_counts(mesh):
    return (
        len(mesh.vertices),
        len(mesh.edges),
        len(mesh.polygons),
    )


class FluidCacheTest(unittest.TestCase):
    def test_missing_mesh_cache_does_not_reuse_previous_frame(self):
        scene = bpy.context.scene
        domain = bpy.data.objects["Cube"]
        original_counts = mesh_counts(domain.data)

        # Frame 1 has a cached liquid surface.
        scene.frame_set(1)
        depsgraph = bpy.context.evaluated_depsgraph_get()
        cached_counts = mesh_counts(domain.evaluated_get(depsgraph).data)

        self.assertNotEqual(cached_counts, original_counts)

        # Frame 2 intentionally has no liquid mesh cache.
        scene.frame_set(2)
        depsgraph = bpy.context.evaluated_depsgraph_get()
        missing_cache_counts = mesh_counts(domain.evaluated_get(depsgraph).data)

        self.assertEqual(missing_cache_counts, original_counts)


if __name__ == "__main__":
    import sys

    sys.argv = [__file__]
    unittest.main()
