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


class TestGpuStorageBuf(unittest.TestCase):
    def test_create_update_read(self):
        import struct

        gpu.init()

        data = struct.pack("4f", 1.0, 2.0, 3.0, 4.0)
        ssbo = gpu.types.GPUStorageBuf(data)

        self.assertEqual(struct.unpack("4f", bytes(ssbo.read())), (1.0, 2.0, 3.0, 4.0))

        new_data = struct.pack("4f", 10.0, 20.0, 30.0, 40.0)
        ssbo.update(new_data)
        self.assertEqual(struct.unpack("4f", bytes(ssbo.read())), (10.0, 20.0, 30.0, 40.0))

        ssbo.clear_to_zero()
        self.assertEqual(struct.unpack("4f", bytes(ssbo.read())), (0.0, 0.0, 0.0, 0.0))

    def test_compute_shader_binding(self):
        import struct

        gpu.init()

        info = gpu.types.GPUShaderCreateInfo()
        info.storage_buf(0, {"READ", "WRITE"}, "float", "data[]")
        info.local_group_size(4)
        info.compute_source(
            """
            void main() {
              uint i = gl_GlobalInvocationID.x;
              data[i] = data[i] * 2.0;
            }
            """
        )

        shader = gpu.shader.create_from_info(info)
        ssbo = gpu.types.GPUStorageBuf(struct.pack("4f", 1.0, 2.0, 3.0, 4.0))

        shader.bind()
        shader.storage_block("data", ssbo)
        gpu.compute.dispatch(shader, 1, 1, 1)

        self.assertEqual(struct.unpack("4f", bytes(ssbo.read())), (2.0, 4.0, 6.0, 8.0))

    def test_typedef_source_struct_array(self):
        import struct

        gpu.init()

        # std430 struct layout: `vec4` forces 16-byte alignment, so the struct's own
        # alignment (and therefore its array stride) is rounded up to a multiple of 16.
        # `vec4 pos` (16 bytes) + `float weight` (4 bytes) = 20 bytes, padded to 32.
        item_fmt = "<4ff12x"
        self.assertEqual(struct.calcsize(item_fmt), 32)

        items = [
            (1.0, 2.0, 3.0, 4.0, 10.0),
            (5.0, 6.0, 7.0, 8.0, 20.0),
        ]
        data = b"".join(struct.pack(item_fmt, *pos, weight) for *pos, weight in items)

        info = gpu.types.GPUShaderCreateInfo()
        info.typedef_source("struct Item { vec4 pos; float weight; };")
        info.storage_buf(0, {"READ", "WRITE"}, "Item", "items[]")
        info.local_group_size(len(items))
        info.compute_source(
            """
            void main() {
              uint i = gl_GlobalInvocationID.x;
              items[i].pos = items[i].pos * 2.0;
              items[i].weight = items[i].weight + 1.0;
            }
            """
        )

        shader = gpu.shader.create_from_info(info)
        ssbo = gpu.types.GPUStorageBuf(data)

        shader.bind()
        shader.storage_block("items", ssbo)
        gpu.compute.dispatch(shader, 1, 1, 1)

        result = bytes(ssbo.read())
        for i, (x, y, z, w, weight) in enumerate(items):
            item = struct.unpack_from(item_fmt, result, i * 32)
            self.assertEqual(item[:4], (x * 2.0, y * 2.0, z * 2.0, w * 2.0))
            self.assertEqual(item[4], weight + 1.0)


if __name__ == "__main__":
    import sys

    sys.argv = [__file__] + (
        sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    )

    unittest.main()
