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


def rgba_bytes_from_packed_int(value):
    return (
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF,
    )


# Test color in linear float, sRGB byte, and the expected conversions between them.
COLOR_FLOAT = (1.0, 0.5, 0.25, 1.0)
COLOR_BYTE = (255, 128, 196, 255)
# sRGB byte -> linear float.
COLOR_BYTE_AS_FLOAT = (1.0, 0.2159, 0.5520, 1.0)
# Linear float -> sRGB byte.
COLOR_FLOAT_AS_BYTE = (255, 188, 137, 255)

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

    def test_file_type(self):
        self.assertEqual(self.ibuf.file_type, 'PNG')
        self.ibuf.file_type = 'PNG'
        self.assertEqual(self.ibuf.file_type, 'PNG')
        with self.assertRaises(ValueError):
            self.ibuf.file_type = 'INVALID'
        with self.assertRaises(ValueError):
            self.ibuf.file_type = ""

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

    def test_planes_roundtrip(self):
        file_type = 'IRIS'
        file_ext = imbuf.file_types[file_type].file_extensions[0]
        # Per-plane pixel data (2x2 = 4 pixels, packed as 0xRRGGBBAA).
        # 8: gray-scale (R=G=B, A ignored on save, becomes 0xFF on load).
        # 16: gray-scale + alpha (R=G=B, A preserved).
        # 24: RGB (A ignored on save, becomes 0xFF on load).
        # 32: RGBA (all channels preserved).
        pixel_data = {
            8: (0x404040FF, 0x808080FF, 0xC0C0C0FF, 0xFFFFFFFF),
            16: (0x404040C0, 0x808080FF, 0xC0C0C080, 0xFFFFFF40),
            24: (0xFF0000FF, 0x00FF00FF, 0x0000FFFF, 0xAABBCCFF),
            32: (0xFF000080, 0x00FF00C0, 0x0000FF40, 0xAABBCCDD),
        }
        with tempfile.TemporaryDirectory() as tempdir:
            for pixel_planes, pixels in pixel_data.items():
                with self.subTest(planes=pixel_planes):
                    ibuf = imbuf.new((2, 2), planes=pixel_planes)
                    ibuf.file_type = file_type
                    with ibuf.with_buffer('BYTE', write=True) as buf:
                        for i, pixel in enumerate(pixels):
                            buf[i * 4:(i + 1) * 4] = bytes(rgba_bytes_from_packed_int(pixel))
                    filepath = os.path.join(tempdir, "test_{:d}{:s}".format(pixel_planes, file_ext))
                    imbuf.write(ibuf, filepath=filepath)
                    ibuf.free()

                    ibuf_loaded = imbuf.load(filepath)
                    self.assertEqual(ibuf_loaded.planes, pixel_planes)
                    with ibuf_loaded.with_buffer('BYTE') as buf:
                        for i, expected in enumerate(pixels):
                            actual = tuple(buf[i * 4:(i + 1) * 4])
                            self.assertEqual(actual, rgba_bytes_from_packed_int(expected))
                    ibuf_loaded.free()


class TestImBufPixelsBytes(unittest.TestCase):

    def test_read_only_by_default(self):
        ibuf = imbuf.new((2, 2))
        with ibuf.with_buffer('BYTE') as buf:
            self.assertEqual(buf.readonly, True)
            with self.assertRaises(TypeError):
                buf[0] = 0xFF
        ibuf.free()

    def test_write_mode(self):
        ibuf = imbuf.new((2, 2))
        with ibuf.with_buffer('BYTE', write=True) as buf:
            self.assertEqual(buf.readonly, False)
            buf[0] = 42
            self.assertEqual(buf[0], 42)
        ibuf.free()

    def test_buffer_length(self):
        w, h = 8, 6
        ibuf = imbuf.new((w, h))
        with ibuf.with_buffer('BYTE') as buf:
            self.assertEqual(len(buf), w * h * 4)
        ibuf.free()

    def test_freed_image(self):
        ibuf = imbuf.new((2, 2))
        ibuf.free()
        with self.assertRaises(ReferenceError):
            ibuf.with_buffer('BYTE')

    def test_exports_block_resize(self):
        ibuf = imbuf.new((4, 4))
        with ibuf.with_buffer('BYTE') as buf:
            with self.assertRaises(BufferError):
                ibuf.resize((2, 2))
        # After context exit, resize works.
        ibuf.resize((2, 2))
        self.assertEqual(ibuf.size, (2, 2))
        ibuf.free()

    def test_exports_block_crop(self):
        ibuf = imbuf.new((4, 4))
        with ibuf.with_buffer('BYTE') as buf:
            with self.assertRaises(BufferError):
                ibuf.crop((0, 0), (1, 1))
        ibuf.free()

    def test_exports_block_free(self):
        ibuf = imbuf.new((4, 4))
        with ibuf.with_buffer('BYTE') as buf:
            with self.assertRaises(BufferError):
                ibuf.free()
        ibuf.free()

    def test_double_enter_blocked(self):
        ibuf = imbuf.new((2, 2))
        ctx = ibuf.with_buffer('BYTE')
        ctx.__enter__()
        with self.assertRaises(RuntimeError):
            ctx.__enter__()
        ctx.__exit__(None, None, None)
        ibuf.free()

    def test_reentry_after_exit(self):
        ibuf = imbuf.new((2, 2))
        ctx = ibuf.with_buffer('BYTE', write=True)
        buf1 = ctx.__enter__()
        buf1[0] = 10
        ctx.__exit__(None, None, None)
        buf2 = ctx.__enter__()
        self.assertEqual(buf2[0], 10)
        ctx.__exit__(None, None, None)
        ibuf.free()


class TestImBufPixelsRegion(unittest.TestCase):

    def setUp(self):
        self.ibuf = imbuf.new((8, 6))
        with self.ibuf.with_buffer('BYTE', write=True) as buf:
            for i in range(8 * 6):
                buf[i * 4] = i % 256

    def tearDown(self):
        self.ibuf.free()

    def test_full_image_1d(self):
        with self.ibuf.with_buffer('BYTE') as buf:
            self.assertEqual(buf.ndim, 1)
            self.assertEqual(len(buf), 8 * 6 * 4)

    def test_region_none_is_1d(self):
        with self.ibuf.with_buffer('BYTE', region=None) as buf:
            self.assertEqual(buf.ndim, 1)

    def test_sub_region_2d(self):
        with self.ibuf.with_buffer('BYTE', region=((2, 1), (6, 4))) as buf:
            self.assertEqual(buf.ndim, 2)
            self.assertEqual(buf.shape, (3, 16))

    def test_region_pixel_values(self):
        with self.ibuf.with_buffer('BYTE', region=((2, 1), (6, 4))) as buf:
            # Pixel (2, 1) has index 1*8+2 = 10.
            self.assertEqual(buf[0, 0], 10)
            # Pixel (2, 2) has index 2*8+2 = 18.
            self.assertEqual(buf[1, 0], 18)

    def test_full_image_region_collapses_to_1d(self):
        with self.ibuf.with_buffer('BYTE', region=((0, 0), (8, 6))) as buf:
            self.assertEqual(buf.ndim, 1)

    def test_oversized_region_collapses_to_1d(self):
        with self.ibuf.with_buffer('BYTE', region=((-5, -5), (100, 100))) as buf:
            self.assertEqual(buf.ndim, 1)

    def test_zero_area_region(self):
        # Fully outside image bounds.
        with self.ibuf.with_buffer('BYTE', region=((100, 100), (200, 200))) as buf:
            self.assertEqual(buf.ndim, 2)
            self.assertEqual(buf.shape[0] * buf.shape[1], 0)

        # Start == end (zero-size).
        with self.ibuf.with_buffer('BYTE', region=((3, 3), (3, 3))) as buf:
            self.assertEqual(buf.shape[0] * buf.shape[1], 0)

    def test_inverted_region_sanitized(self):
        # Inverted region is sanitized (min/max swapped), not treated as empty.
        with self.ibuf.with_buffer('BYTE', region=((5, 5), (2, 2))) as buf:
            self.assertEqual(buf.ndim, 2)
            self.assertEqual(buf.shape, (3, 12))

    def test_region_write_does_not_corrupt_neighbors(self):
        with self.ibuf.with_buffer('BYTE', write=True, region=((1, 1), (3, 3))) as buf:
            for r in range(buf.shape[0]):
                for c in range(buf.shape[1]):
                    buf[r, c] = 0xFF
        with self.ibuf.with_buffer('BYTE') as buf:
            self.assertEqual(buf[0], 0)
            inside = (1 * 8 + 1) * 4
            self.assertEqual(buf[inside], 0xFF)


class TestImBufPixelsFloat(unittest.TestCase):

    def test_no_float_buffer_error(self):
        ibuf = imbuf.new((2, 2))
        with self.assertRaises(RuntimeError):
            ibuf.with_buffer('FLOAT')
        ibuf.free()

    def test_ensure_and_access(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('FLOAT')
        with ibuf.with_buffer('FLOAT') as buf:
            self.assertEqual(buf.format, "f")
        ibuf.free()

    def test_float_write(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('FLOAT')
        with ibuf.with_buffer('FLOAT', write=True) as buf:
            buf[0] = 0.5
            self.assertAlmostEqual(buf[0], 0.5, places=5)
        ibuf.free()


class TestImBufEnsureBuffers(unittest.TestCase):

    def test_ensure_float_from_byte(self):
        ibuf = imbuf.new((2, 2))
        with ibuf.with_buffer('BYTE', write=True) as buf:
            buf[0:4] = bytes(COLOR_BYTE)
        ibuf.ensure_buffer('FLOAT')
        with ibuf.with_buffer('FLOAT') as buf:
            for i, expected in enumerate(COLOR_BYTE_AS_FLOAT):
                self.assertAlmostEqual(buf[i], expected, places=3)
        ibuf.free()

    def test_ensure_byte_from_float(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('FLOAT')
        with ibuf.with_buffer('FLOAT', write=True) as buf:
            for i, v in enumerate(COLOR_FLOAT):
                buf[i] = v
        ibuf.clear_buffer('BYTE')
        self.assertFalse(ibuf.has_buffer('BYTE'))
        ibuf.ensure_buffer('BYTE')
        self.assertTrue(ibuf.has_buffer('BYTE'))
        with ibuf.with_buffer('BYTE') as buf:
            for i, expected in enumerate(COLOR_FLOAT_AS_BYTE):
                self.assertEqual(buf[i], expected)
        ibuf.free()

    def test_ensure_noop_when_present(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('BYTE')
        ibuf.ensure_buffer('FLOAT')
        ibuf.ensure_buffer('FLOAT')
        ibuf.free()

    def test_ensure_blocked_while_exported(self):
        ibuf = imbuf.new((2, 2))
        with ibuf.with_buffer('BYTE') as buf:
            with self.assertRaises(BufferError):
                ibuf.ensure_buffer('FLOAT')
        ibuf.free()


class TestImBufHasAndClearBuffer(unittest.TestCase):

    def test_has_buffer_byte(self):
        ibuf = imbuf.new((2, 2))
        self.assertTrue(ibuf.has_buffer('BYTE'))
        self.assertFalse(ibuf.has_buffer('FLOAT'))
        ibuf.free()

    def test_clear_buffer_byte(self):
        ibuf = imbuf.new((2, 2))
        self.assertTrue(ibuf.has_buffer('BYTE'))
        ibuf.clear_buffer('BYTE')
        self.assertFalse(ibuf.has_buffer('BYTE'))
        ibuf.free()

    def test_clear_buffer_float(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('FLOAT')
        self.assertTrue(ibuf.has_buffer('FLOAT'))
        ibuf.clear_buffer('FLOAT')
        self.assertFalse(ibuf.has_buffer('FLOAT'))
        ibuf.free()

    def test_clear_buffer_blocked_while_exported(self):
        ibuf = imbuf.new((2, 2))
        with ibuf.with_buffer('BYTE') as buf:
            with self.assertRaises(BufferError):
                ibuf.clear_buffer('BYTE')
        ibuf.free()

    def test_clear_already_absent(self):
        ibuf = imbuf.new((2, 2))
        ibuf.clear_buffer('FLOAT')
        ibuf.free()


class TestImBufExitSync(unittest.TestCase):

    def test_byte_write_syncs_to_float(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('FLOAT')
        with ibuf.with_buffer('BYTE', write=True) as buf:
            for i, v in enumerate(COLOR_BYTE):
                buf[i] = v
        with ibuf.with_buffer('FLOAT') as buf:
            for i, expected in enumerate(COLOR_BYTE_AS_FLOAT):
                self.assertAlmostEqual(buf[i], expected, places=3)
        ibuf.free()

    def test_float_write_syncs_to_byte(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('FLOAT')
        with ibuf.with_buffer('FLOAT', write=True) as buf:
            for i, v in enumerate(COLOR_FLOAT):
                buf[i] = v
        with ibuf.with_buffer('BYTE') as buf:
            for i, expected in enumerate(COLOR_FLOAT_AS_BYTE):
                self.assertEqual(buf[i], expected)
        ibuf.free()

    def test_read_only_does_not_sync(self):
        ibuf = imbuf.new((2, 2))
        ibuf.ensure_buffer('FLOAT')
        with ibuf.with_buffer('FLOAT', write=True) as buf:
            for i, v in enumerate(COLOR_FLOAT):
                buf[i] = v
        # Read-only byte access should NOT overwrite the float buffer.
        with ibuf.with_buffer('BYTE') as buf:
            _ = buf[0]
        with ibuf.with_buffer('FLOAT') as buf:
            for i, expected in enumerate(COLOR_FLOAT):
                self.assertAlmostEqual(buf[i], expected, places=3)
        ibuf.free()


class TestImBufFileTypes(unittest.TestCase):

    def test_file_types_have_id_and_extensions(self):
        for type_id, file_type in imbuf.file_types.items():
            with self.subTest(type_id=type_id):
                self.assertEqual(file_type.id, type_id)
                self.assertIsInstance(file_type.file_extensions, tuple)
                if type_id != 'NONE':
                    self.assertGreater(len(file_type.file_extensions), 0)

    def test_none_file_type(self):
        # NONE is accessible in the file_types dict.
        self.assertIn('NONE', imbuf.file_types)
        none_type = imbuf.file_types['NONE']
        self.assertEqual(none_type.id, 'NONE')
        self.assertEqual(none_type.file_extensions, ())
        self.assertFalse(none_type.has_write_file)
        self.assertFalse(none_type.has_write_memory)

        # Can set file_type to NONE and read it back.
        ibuf = imbuf.new(DEFAULT_SIZE)
        ibuf.file_type = 'NONE'
        self.assertEqual(ibuf.file_type, 'NONE')

        # Writing as NONE fails for both file and memory.
        with tempfile.TemporaryDirectory() as tempdir:
            filepath = os.path.join(tempdir, 'test.bin')
            with self.assertRaises(ValueError):
                imbuf.write(ibuf, filepath=filepath)
        with self.assertRaises(ValueError):
            imbuf.write_to_buffer(ibuf, io.BytesIO())

        ibuf.free()

    def test_write_and_detect_all_types(self):
        size = (32, 32)
        with tempfile.TemporaryDirectory() as tempdir:
            for type_id, file_type in imbuf.file_types.items():
                if not (file_type.has_write_file or file_type.has_write_memory):
                    continue
                ext = file_type.file_extensions[0]
                with self.subTest(type_id=type_id):
                    ibuf = imbuf.new(size)
                    ibuf.file_type = type_id

                    image_data = []

                    if file_type.has_write_file:
                        filepath = os.path.join(tempdir, "test" + ext)
                        imbuf.write(ibuf, filepath=filepath)
                        with open(filepath, "rb") as fh:
                            image_data.append(("file", fh.read()))

                    if file_type.has_write_memory:
                        buf = io.BytesIO()
                        imbuf.write_to_buffer(ibuf, buf)
                        image_data.append(("memory", buf.getvalue()))

                    ibuf.free()

                    for kind, data in image_data:
                        msg = "{:s} ({:s})".format(type_id, kind)
                        detected = imbuf.file_type_from_buffer(data)
                        self.assertIsNotNone(detected, msg=msg)
                        self.assertEqual(detected.id, type_id, msg=msg)

                        ibuf_loaded = imbuf.load_from_buffer(data)
                        self.assertEqual(ibuf_loaded.size, size, msg=msg)
                        self.assertEqual(ibuf_loaded.file_type, type_id, msg=msg)
                        ibuf_loaded.free()


def main():
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()


if __name__ == "__main__":
    main()
