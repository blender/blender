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


if __name__ == "__main__":
    import sys

    sys.argv = [__file__] + (
        sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    )

    unittest.main()
