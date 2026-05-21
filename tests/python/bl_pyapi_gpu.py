# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# blender -b --factory-startup --python tests/python/bl_pyapi_gpu.py

import unittest

import gpu


class TestGpuInit(unittest.TestCase):
    def test_gpu_module_is_usable_after_init(self):
        gpu.init()

        # GPU module functions will raise SystemError if the GPU has not been initialized.
        gpu.platform.vendor_get()
        gpu.platform.renderer_get()
        gpu.platform.version_get()
        gpu.platform.device_type_get()
        gpu.types.GPUTexture(size=(1024, 1024), format="RGBA8")


class TestGpuFrameBuffer(unittest.TestCase):
    def test_read_color_bounds_check(self):
        gpu.init()

        # Setup a framebuffer.
        tex = gpu.types.GPUTexture(size=(16, 16), format="RGBA8")
        fb = gpu.types.GPUFrameBuffer(color_slots=[tex])

        with fb.bind():
            # Reading within bounds should succeed.
            buf = fb.read_color(0, 0, 1, 1, 4, 0, "UBYTE")

            # Reading outside bounds should raise ValueError.
            with self.assertRaises(ValueError):
                fb.read_color(-1, 0, 1, 1, 4, 0, "UBYTE")


if __name__ == "__main__":
    import sys

    sys.argv = [__file__] + (
        sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    )

    unittest.main()
