# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0


"""
Simple test looping over (most of) all RNA API and checking that data can be accessed.

Note that not all RNA is covered, this focuses mainly on data accessible from `bpy.data`.
"""

import bpy
import unittest


# Walk over all exposed RNA properties in factory startup file, and access them.
class TestRnaProperties(unittest.TestCase):
    def process_rna_struct(self, struct, rna_path):
        # These paths are problematic to process, skip for the time being.
        SKIP_PROPERTIES = {
            bpy.types.Depsgraph.bl_rna.properties["object_instances"],
        }

        if repr(struct) in self.processed_data:
            return
        self.processed_data.add(repr(struct))

        def generate_rna_path(struct, rna_path_root, p, p_data, p_keys_str=[]):
            if not p_data:
                return None
            rna_path_references = [rna_path_root + "." + p.identifier if rna_path_root else p.identifier]
            if p_keys_str:
                rna_path_reference = rna_path_references[0]
                rna_path_references = [rna_path_reference + '[' + p_key + ']' for p_key in p_keys_str if p_key]
            return rna_path_references[0]

        def process_pointer_property(self, struct, rna_path, p, p_data, p_keys_str=[]):
            rna_path_sub = generate_rna_path(struct, rna_path, p, p_data, p_keys_str)
            if p.identifier in {"bl_rna", "rna_type"}:
                return
            if isinstance(p_data, bpy.types.ID) and not p_data.is_embedded_data:
                return
            if rna_path_sub is None:
                return
            self.process_rna_struct(p_data, rna_path_sub)

        for p in struct.bl_rna.properties:
            if p in SKIP_PROPERTIES:
                continue
            if p.type == 'COLLECTION':
                try:
                    iter_data = getattr(struct, p.identifier)
                except:
                    self.assertTrue(False, msg=f"Failed to retrieve {rna_path}.{p.identifier} collection")
                for p_idx, (p_key, p_data) in enumerate(iter_data.items()):
                    p_keys_str = ['"' + p_key + '"', str(p_idx)] if isinstance(p_key, str) else [str(p_key), str(p_idx)]
                    process_pointer_property(self, struct, rna_path, p, p_data, p_keys_str)
            elif p.type == 'POINTER':
                try:
                    p_data = getattr(struct, p.identifier)
                except:
                    self.assertTrue(False, msg=f"Failed to retrieve {rna_path}.{p.identifier} pointer")
                process_pointer_property(self, struct, rna_path, p, p_data)
            else:
                try:
                    p_data = getattr(struct, p.identifier)
                except:
                    self.assertTrue(False, msg=f"Failed to retrieve {rna_path}.{p.identifier} value")

    def test_paths_generation(self):
        bpy.ops.wm.read_factory_settings()
        self.processed_data = set()
        for data_block in bpy.data.user_map().keys():
            self.process_rna_struct(data_block, "")


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
