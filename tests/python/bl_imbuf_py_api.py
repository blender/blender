# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ./blender.bin --background --python tests/python/bl_imbuf_py_api.py -- --verbose

__all__ = (
    "main",
)

import copy
import io
import os
import tempfile
import unittest

import imbuf
from imbuf.types import ImBuf

DEFAULT_SIZE = 32, 32

# NOTE: an invalid file format & missing files is aborting on the build-bot.
# Disable as it makes tests fail, although we could consider demoting this to a warning
# since it prevents valid code-paths from being tested.
USE_TESTS_THAT_ABORT = False


class TestImBufNew(unittest.TestCase):

    def test_new_basic(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        self.assertIsInstance(ibuf, ImBuf)
        self.assertEqual(ibuf.size, DEFAULT_SIZE)
        ibuf.free()

    def test_new_single_pixel(self):
        size = (1, 1)
        ibuf = imbuf.new(size)
        self.assertEqual(ibuf.size, size)
        ibuf.free()

    def test_new_default_properties(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        self.assertEqual(ibuf.planes, 32)
        self.assertEqual(ibuf.channels, 4)
        self.assertEqual(ibuf.filepath, "")
        ibuf.free()

    def test_new_large(self):
        size = (4096, 4096)
        ibuf = imbuf.new(size)
        self.assertEqual(ibuf.size, size)
        ibuf.free()

    def test_new_invalid_size(self):
        with self.assertRaises(ValueError):
            imbuf.new((0, 10))
        with self.assertRaises(ValueError):
            imbuf.new((-1, 10))


class TestImBufProperties(unittest.TestCase):

    def setUp(self):
        self.ibuf = imbuf.new(DEFAULT_SIZE)

    def tearDown(self):
        self.ibuf.free()

    def test_readonly_properties(self):
        readonly_attrs = (
            ("size", DEFAULT_SIZE, (10, 10)),
            ("planes", 32, 24),
            ("channels", 4, 3),
        )
        for attr, expected, new_value in readonly_attrs:
            self.assertTrue(hasattr(self.ibuf, attr))
            self.assertEqual(getattr(self.ibuf, attr), expected)
            with self.assertRaises(AttributeError):
                setattr(self.ibuf, attr, new_value)

    def test_filepath_read_write(self):
        filepath = "/tmp/test.png"
        self.ibuf.filepath = filepath
        self.assertEqual(self.ibuf.filepath, filepath)

    def test_filepath_read_write_bytes(self):
        filepath = b"/tmp/test.png"
        self.ibuf.filepath = filepath
        self.assertEqual(self.ibuf.filepath, filepath.decode('ascii'))

    def test_filepath_empty_default(self):
        self.assertEqual(self.ibuf.filepath, "")

    def test_filepath_type_error(self):
        with self.assertRaises(TypeError):
            self.ibuf.filepath = 123

    def test_ppm_read_write(self):
        ppm = (3780.0, 3780.0)
        self.ibuf.ppm = ppm
        self.assertEqual(self.ibuf.ppm, ppm)

    def test_ppm_invalid(self):
        with self.assertRaises(ValueError):
            self.ibuf.ppm = (0.0, 0.0)
        with self.assertRaises(ValueError):
            self.ibuf.ppm = (-1.0, 100.0)

    def test_quality(self):
        self.assertEqual(self.ibuf.quality, 90)
        self.ibuf.quality = 50
        self.assertEqual(self.ibuf.quality, 50)
        self.ibuf.quality = -1
        self.assertEqual(self.ibuf.quality, 0)
        self.ibuf.quality = 101
        self.assertEqual(self.ibuf.quality, 100)

    def test_compress(self):
        self.assertEqual(self.ibuf.compress, 15)
        self.ibuf.compress = 50
        self.assertEqual(self.ibuf.compress, 50)
        self.ibuf.compress = -1
        self.assertEqual(self.ibuf.compress, 0)
        self.ibuf.compress = 101
        self.assertEqual(self.ibuf.compress, 100)


class TestImBufResize(unittest.TestCase):

    def test_resize_basic(self):
        ibuf = imbuf.new((64, 64))
        ibuf.resize(DEFAULT_SIZE)
        self.assertEqual(ibuf.size, DEFAULT_SIZE)
        ibuf.free()

    def test_resize_upscale(self):
        size = (128, 128)
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf.resize(size)
        self.assertEqual(ibuf.size, size)
        ibuf.free()

    def test_resize_non_square(self):
        size = (100, 50)
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf.resize(size)
        self.assertEqual(ibuf.size, size)
        ibuf.free()

    def test_resize_method_bilinear(self):
        ibuf = imbuf.new((64, 64))
        ibuf.resize(DEFAULT_SIZE, method='BILINEAR')
        self.assertEqual(ibuf.size, DEFAULT_SIZE)
        ibuf.free()

    def test_resize_invalid(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with self.assertRaises(ValueError):
            ibuf.resize((0, 64))
        with self.assertRaises(ValueError):
            ibuf.resize((-1, 64))
        with self.assertRaises(ValueError):
            ibuf.resize(DEFAULT_SIZE, method='INVALID')
        ibuf.free()


class TestImBufCrop(unittest.TestCase):

    def test_crop_basic(self):
        size = (64, 64)
        ibuf = imbuf.new(size)
        crop_max = (DEFAULT_SIZE[0] - 1, DEFAULT_SIZE[1] - 1)
        ibuf.crop((0, 0), crop_max)
        self.assertEqual(ibuf.size, DEFAULT_SIZE)
        ibuf.free()

    def test_crop_partial(self):
        crop_min = (10, 20)
        crop_max = (49, 79)
        expected_size = (crop_max[0] - crop_min[0] + 1, crop_max[1] - crop_min[1] + 1)
        ibuf = imbuf.new((100, 100))
        ibuf.crop(crop_min, crop_max)
        self.assertEqual(ibuf.size, expected_size)
        ibuf.free()

    def test_crop_single_pixel(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf.crop((0, 0), (0, 0))
        self.assertEqual(ibuf.size, (1, 1))
        ibuf.free()

    def test_crop_full_image(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        crop_max = (DEFAULT_SIZE[0] - 1, DEFAULT_SIZE[1] - 1)
        ibuf.crop((0, 0), crop_max)
        self.assertEqual(ibuf.size, DEFAULT_SIZE)
        ibuf.free()

    def test_crop_invalid(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with self.assertRaises(ValueError):
            ibuf.crop((0, 0), DEFAULT_SIZE)
        with self.assertRaises(ValueError):
            ibuf.crop((20, 20), (10, 10))
        with self.assertRaises(ValueError):
            ibuf.crop((-1, 0), (10, 10))
        ibuf.free()


class TestImBufCopy(unittest.TestCase):

    def test_copy(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf_copy = ibuf.copy()
        self.assertIsInstance(ibuf_copy, ImBuf)
        self.assertEqual(ibuf_copy.size, DEFAULT_SIZE)
        # Copy should be independent.
        ibuf.free()
        self.assertEqual(ibuf_copy.size, DEFAULT_SIZE)
        ibuf_copy.free()

    def test_copy_and_deepcopy(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf_copy = copy.copy(ibuf)
        ibuf_deepcopy = copy.deepcopy(ibuf)
        self.assertEqual(ibuf_copy.size, ibuf.size)
        self.assertEqual(ibuf_deepcopy.size, ibuf.size)
        ibuf.free()
        ibuf_copy.free()
        ibuf_deepcopy.free()


class TestImBufFree(unittest.TestCase):

    def test_free_then_access(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf.free()
        with self.assertRaises(ReferenceError):
            _ = ibuf.size
        with self.assertRaises(ReferenceError):
            ibuf.resize(DEFAULT_SIZE)
        with self.assertRaises(ReferenceError):
            ibuf.crop((0, 0), (10, 10))
        with self.assertRaises(ReferenceError):
            ibuf.copy()
        with self.assertRaises(ReferenceError):
            imbuf.write_to_buffer(ibuf, io.BytesIO())
        with self.assertRaises(ReferenceError):
            ibuf.filepath = "/tmp/test.png"

    def test_double_free(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf.free()
        # Double free should not crash.
        ibuf.free()


class TestImBufPythonProtocols(unittest.TestCase):

    def test_repr(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        r = repr(ibuf)
        self.assertIn("imbuf", r)
        self.assertIn("size=({:d}, {:d})".format(*DEFAULT_SIZE), r)
        ibuf.free()

    def test_repr_after_free(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf.free()
        r = repr(ibuf)
        self.assertIn("address=0x0", r)

    def test_hash(self):
        ibuf1 = imbuf.new(DEFAULT_SIZE)
        ibuf2 = imbuf.new(DEFAULT_SIZE)
        self.assertIsInstance(hash(ibuf1), int)
        # Different image buffers should (almost certainly) have different hashes.
        self.assertNotEqual(hash(ibuf1), hash(ibuf2))
        ibuf1.free()
        ibuf2.free()

    def test_isinstance(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        self.assertIsInstance(ibuf, ImBuf)
        self.assertEqual(type(ibuf).__name__, "ImBuf")
        ibuf.free()


class TestImBufIO(unittest.TestCase):

    def test_write_and_load(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with tempfile.TemporaryDirectory() as tempdir:
            filepath = os.path.join(tempdir, "test.png")
            imbuf.write(ibuf, filepath=filepath)
            ibuf.free()

            ibuf_loaded = imbuf.load(filepath)
            self.assertIsInstance(ibuf_loaded, ImBuf)
            self.assertEqual(ibuf_loaded.size, DEFAULT_SIZE)
            ibuf_loaded.free()

    def test_write_uses_image_filepath(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with tempfile.TemporaryDirectory() as tempdir:
            filepath = os.path.join(tempdir, "test.png")
            ibuf.filepath = filepath
            imbuf.write(ibuf)
            ibuf.free()

            ibuf_loaded = imbuf.load(filepath)
            self.assertEqual(ibuf_loaded.size, DEFAULT_SIZE)
            ibuf_loaded.free()

    def test_load_nonexistent(self):
        import bpy
        # The binary path is a file, not a directory, so this sub-path will not exist in practice.
        filepath = os.path.join(bpy.app.binary_path, "image.png")
        with self.assertRaises(IOError):
            imbuf.load(filepath)

    def test_load_from_buffer(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with tempfile.TemporaryDirectory() as tempdir:
            filepath = os.path.join(tempdir, "test.png")
            imbuf.write(ibuf, filepath=filepath)
            ibuf.free()

            with open(filepath, "rb") as f:
                data = f.read()
            ibuf_loaded = imbuf.load_from_buffer(data)
            self.assertIsInstance(ibuf_loaded, ImBuf)
            self.assertEqual(ibuf_loaded.size, DEFAULT_SIZE)
            ibuf_loaded.free()

    def test_load_from_buffer_bytearray(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with tempfile.TemporaryDirectory() as tempdir:
            filepath = os.path.join(tempdir, "test.png")
            imbuf.write(ibuf, filepath=filepath)
            ibuf.free()

            with open(filepath, "rb") as f:
                data = bytearray(f.read())
            ibuf_loaded = imbuf.load_from_buffer(data)
            self.assertEqual(ibuf_loaded.size, DEFAULT_SIZE)
            ibuf_loaded.free()

    def test_load_from_buffer_invalid(self):
        if USE_TESTS_THAT_ABORT:
            with self.assertRaises(ValueError):
                imbuf.load_from_buffer(b"not an image")
        with self.assertRaises(TypeError):
            imbuf.load_from_buffer(12345)

    def test_write_to_invalid_path(self):
        import bpy
        ibuf = imbuf.new(DEFAULT_SIZE)
        # The binary path is a file, not a directory, so this sub-path will not exist in practice.
        filepath = os.path.join(bpy.app.binary_path, "test.png")
        if USE_TESTS_THAT_ABORT:
            with self.assertRaises(IOError):
                imbuf.write(ibuf, filepath=filepath)
        ibuf.free()

    def test_write_to_buffer(self):
        size = (16, 24)
        ibuf = imbuf.new(size)
        buf = io.BytesIO()
        imbuf.write_to_buffer(ibuf, buf)
        ibuf.free()
        # Round-trip: the written data should be loadable.
        ibuf_loaded = imbuf.load_from_buffer(buf.getvalue())
        self.assertEqual(ibuf_loaded.size, size)
        ibuf_loaded.free()

    def test_write_to_buffer_not_writable(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with self.assertRaises(AttributeError):
            imbuf.write_to_buffer(ibuf, object())
        ibuf.free()

    def test_load_sets_filepath(self):
        ibuf = imbuf.new(DEFAULT_SIZE)
        with tempfile.TemporaryDirectory() as tempdir:
            filepath = os.path.join(tempdir, "test.png")
            imbuf.write(ibuf, filepath=filepath)
            ibuf.free()

            ibuf_loaded = imbuf.load(filepath)
            self.assertEqual(ibuf_loaded.filepath, filepath)
            ibuf_loaded.free()


class TestImBufPlanes(unittest.TestCase):

    def test_new_planes_values(self):
        for p in (8, 16, 24, 32):
            ibuf = imbuf.new((2, 2), planes=p)
            self.assertEqual(ibuf.planes, p)
            ibuf.free()

    def test_new_planes_invalid(self):
        for p in (0, 4, 12, 64):
            with self.assertRaises(ValueError):
                imbuf.new((2, 2), planes=p)


def main():
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()


if __name__ == "__main__":
    main()
