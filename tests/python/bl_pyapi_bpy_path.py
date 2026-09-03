# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_pyapi_bpy_path.py -- --verbose
import unittest


class TestBpyPath(unittest.TestCase):
    def test_ensure_ext(self):
        from bpy.path import ensure_ext

        # Should work with both strings and bytes.
        self.assertEqual(ensure_ext('demo', '.blend'), 'demo.blend')
        self.assertEqual(ensure_ext(b'demo', b'.blend'), b'demo.blend')

        # Test different cases.
        self.assertEqual(ensure_ext('demo.blend', '.blend'), 'demo.blend')
        self.assertEqual(ensure_ext('demo.BLEND', '.blend'), 'demo.BLEND')
        self.assertEqual(ensure_ext('demo.blend', '.BLEND'), 'demo.blend')

        # Test empty extensions, compound extensions etc.
        self.assertEqual(ensure_ext('demo', 'blend'), 'demoblend')
        self.assertEqual(ensure_ext('demo', ''), 'demo')
        self.assertEqual(ensure_ext('demo', '.json.gz'), 'demo.json.gz')
        self.assertEqual(ensure_ext('demo.json.gz', '.json.gz'), 'demo.json.gz')
        self.assertEqual(ensure_ext('demo.json', '.json.gz'), 'demo.json.json.gz')
        self.assertEqual(ensure_ext('', ''), '')
        self.assertEqual(ensure_ext('', '.blend'), '.blend')

        # Test case-sensitive behavior.
        self.assertEqual(ensure_ext('demo', '.blend', case_sensitive=True), 'demo.blend')
        self.assertEqual(ensure_ext('demo.BLEND', '.blend', case_sensitive=True), 'demo.BLEND.blend')
        self.assertEqual(ensure_ext('demo', 'Blend', case_sensitive=True), 'demoBlend')
        self.assertEqual(ensure_ext('demoBlend', 'blend', case_sensitive=True), 'demoBlendblend')
        self.assertEqual(ensure_ext('demo', '', case_sensitive=True), 'demo')

    def test_is_autoexec(self):
        import bpy
        from bpy.path import is_autoexec

        prefs = bpy.context.preferences

        use_scripts_auto_execute = prefs.filepaths.use_scripts_auto_execute
        self.addCleanup(setattr, prefs.filepaths, "use_scripts_auto_execute", use_scripts_auto_execute)

        path_cmp = prefs.autoexec_paths.new()
        self.addCleanup(prefs.autoexec_paths.remove, path_cmp)
        path_cmp.path = "/untrusted/"

        self.assertFalse(is_autoexec("/untrusted/"))
        self.assertFalse(is_autoexec(b"/untrusted/"))
        self.assertTrue(is_autoexec("/trusted/"))

        path_cmp.use_glob = True
        path_cmp.path = "*/download*"
        self.assertFalse(is_autoexec("/home/user/downloads/"))
        self.assertTrue(is_autoexec("/home/user/projects/"))

        # The preference to enable auto-execution isn't taken into account.
        prefs.filepaths.use_scripts_auto_execute = False
        self.assertFalse(is_autoexec("/home/user/downloads/"))


if __name__ == '__main__':
    import sys

    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
