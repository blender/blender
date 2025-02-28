# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0


import bpy
import unittest


def process_rna_struct(self, struct, rna_path):
    # These paths are currently known failures of `path_from_id`.
    KNOWN_FAILURES = {
        "render.views[\"left\"]",
        "render.views[\"right\"]",
        "uv_layers[\"UVMap\"]",
        "uv_layers[\"UVMap\"]",
    }
    SKIP_KNOWN_FAILURES = True

    def filter_prop_iter(struct):
        for p in struct.bl_rna.properties:
            # Internal rna meta-data, not expected to support RNA paths generation.
            if p.identifier in {"bl_rna", "rna_type"}:
                continue
            # Only these types can point to sub-structs.
            if p.type not in {'POINTER', 'COLLECTION'}:
                continue
            # TODO: Dynamic typed pointer/collection properties are ignored for now.
            if not p.fixed_type:
                continue
            if bpy.types.ID.bl_rna in {p.fixed_type, p.fixed_type.base}:
                continue
            yield p

    def validate_rna_path(self, struct, rna_path_root, p, p_data, p_keys=[]):
        if not p_data:
            return None
        rna_path_references = [rna_path_root + "." + p.identifier if rna_path_root else p.identifier]
        if p_keys:
            rna_path_reference = rna_path_references[0]
            rna_path_references = [rna_path_reference + '[' + p_key + ']' for p_key in p_keys]
        try:
            rna_path_generated = p_data.path_from_id()
        except ValueError:
            return None
        if SKIP_KNOWN_FAILURES and rna_path_generated in KNOWN_FAILURES:
            return None
        self.assertTrue((rna_path_generated in rna_path_references) or ("..." in rna_path_generated),
                        msg=f"\"{rna_path_generated}\" (from {struct}) failed to match expected paths {rna_path_references}")
        return rna_path_generated

    for p in filter_prop_iter(struct):
        if p.type == 'COLLECTION':
            for p_idx, (p_key, p_data) in enumerate(getattr(struct, p.identifier).items()):
                p_keys = ['"' + p_key + '"', str(p_idx)] if isinstance(p_key, str) else [str(p_key), str(p_idx)]
                rna_path_sub = validate_rna_path(self, struct, rna_path, p, p_data, p_keys)
                if rna_path_sub is not None:
                    process_rna_struct(self, p_data, rna_path_sub)
        else:
            assert (p.type == 'POINTER')
            p_data = getattr(struct, p.identifier)
            rna_path_sub = validate_rna_path(self, struct, rna_path, p, p_data)
            if rna_path_sub is not None:
                process_rna_struct(self, p_data, rna_path_sub)


# Walk over all exposed RNA properties in factory startup file, and compare generated RNA paths to 'actual' paths.
class TestRnaPaths(unittest.TestCase):
    def test_paths_generation(self):
        bpy.ops.wm.read_factory_settings()
        for data_block in bpy.data.user_map().keys():
            process_rna_struct(self, data_block, "")


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
