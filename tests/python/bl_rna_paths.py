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


# Walk over all exposed RNA Collection and Pointer properties in factory startup file, and compare generated RNA
# ancestors to 'actual' ones.
class TestRnaAncestors(unittest.TestCase):
    def process_rna_struct(self, struct, ancestors):
        def filter_prop_iter(struct):
            # These paths are problematic to process, skip for the time being.
            SKIP_PROPERTIES = {
                bpy.types.Depsgraph.bl_rna.properties["object_instances"],
                # XXX To be removed once #133551 is fixed.
                bpy.types.ToolSettings.bl_rna.properties["uv_sculpt"],
            }

            for p in struct.bl_rna.properties:
                # Internal rna meta-data, not expected to support RNA paths generation.
                if p.identifier in {"bl_rna", "rna_type"}:
                    continue
                if p in SKIP_PROPERTIES:
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

        def process_pointer_property(self, p_data, ancestors_sub):
            if not p_data:
                return
            rna_ancestors = p_data.rna_ancestors()
            if not rna_ancestors:
                # Do not error for now. Only ensure that if there is a rna_ancestors array, it is valid.
                return
            if repr(rna_ancestors[0]) != ancestors_sub[0]:
                # Do not error for now. There are valid cases where the data is 'rebased' on a new 'root' ID.
                return
            if repr(p_data) in ancestors_sub:
                # Loop back onto itself, skip.
                # E.g. `Scene.view_layer.depsgraph.view_layer`.
                return
            self.process_rna_struct(p_data, ancestors_sub)

        print(struct, "from", ancestors)
        self.assertEqual([repr(a) for a in struct.rna_ancestors()], ancestors)

        ancestors_sub = ancestors + [repr(struct)]

        for p in filter_prop_iter(struct):
            if p.type == 'COLLECTION':
                for p_key, p_data in getattr(struct, p.identifier).items():
                    process_pointer_property(self, p_data, ancestors_sub)
            else:
                assert (p.type == 'POINTER')
                p_data = getattr(struct, p.identifier)
                process_pointer_property(self, p_data, ancestors_sub)

    def test_ancestors(self):
        bpy.ops.wm.read_factory_settings()
        for data_block in bpy.data.user_map().keys():
            self.process_rna_struct(data_block, [])


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
