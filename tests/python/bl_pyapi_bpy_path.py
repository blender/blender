# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_bpy_path.py -- --verbose
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

        # Test case-sensitive behaviour.
        self.assertEqual(ensure_ext('demo', '.blend', True), 'demo.blend')
        self.assertEqual(ensure_ext('demo.BLEND', '.blend', True), 'demo.BLEND.blend')
        self.assertEqual(ensure_ext('demo', 'Blend', True), 'demoBlend')
        self.assertEqual(ensure_ext('demoBlend', 'blend', True), 'demoBlendblend')
        self.assertEqual(ensure_ext('demo', '', True), 'demo')


if __name__ == '__main__':
    import sys

    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
