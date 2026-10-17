# SPDX-FileCopyrightText: opyrightText: 2026 The glTF-Blender-IO author
#
# SPDX-License-Identifier: Apache-2.0

import array
import ctypes
from .gltf2_io_gltf import ImportError
from ..com.library import dll_path
import bpy
from os.path import basename


class KtxDecoder:
    """KTX2 decoder."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def find_library():
        """Find the KTX decoder library."""
        path = dll_path('bf_intern_ktx_bridge', 'KTX')
        if path is not None and path.exists() and path.is_file():
            return path
        else:
            raise ImportError("KTX decoder library not found at {}".format(path))

    @staticmethod
    def load_library(gltf):
        """Load the KTX decoder library."""
        if hasattr(gltf, 'ktx_decoder'):
            return
        lib_path = KtxDecoder.find_library()
        try:
            lib = ctypes.CDLL(lib_path.resolve())
        except Exception as e:
            raise ImportError("Failed to load KTX decoder library: {}".format(e))

        gltf.ktx_decoder = lib

        # ktx_error_code_e decodeFromFile(const char* filename, ktxTexture2** newTex)
        lib.decodeFromFile.restype = ctypes.c_int
        lib.decodeFromFile.argtypes = [
            ctypes.c_char_p,                  # const char* filename
            ctypes.POINTER(ctypes.c_void_p),  # ktxTexture2** newTex
        ]

        # ktx_error_code_e getImageInfo(ktxTexture2* texture, unsigned int* width,
        # unsigned int* height, size_t* dataSize)
        lib.getImageInfo.restype = ctypes.c_int
        lib.getImageInfo.argtypes = [
            ctypes.c_void_p,                  # ktxTexture2* texture
            ctypes.POINTER(ctypes.c_uint32),  # unsigned int* width
            ctypes.POINTER(ctypes.c_uint32),  # unsigned int* height
            ctypes.POINTER(ctypes.c_size_t),  # size_t* dataSize
        ]

        # ktx_error_code_e getImageDataFloat(ktxTexture2* texture, float* destination, size_t destinationSize)
        lib.getImageDataFloat.restype = ctypes.c_int
        lib.getImageDataFloat.argtypes = [
            ctypes.c_void_p,                 # ktxTexture2* texture
            ctypes.POINTER(ctypes.c_float),  # float* destination
            ctypes.c_size_t,                 # size_t destinationSize
        ]

        # void destroyTexture(ktxTexture2* texture)
        lib.destroyTexture.restype = None
        lib.destroyTexture.argtypes = [ctypes.c_void_p]

    @staticmethod
    def read_file(gltf, filepath):
        width, height, pixels = KtxDecoder.decode_file(gltf, filepath)
        img_name = basename(filepath)

        blender_image = bpy.data.images.new(
            img_name,
            width,
            height,
            alpha=True,
            float_buffer=True,
        )
        blender_image.pixels.foreach_set(pixels)
        blender_image.pack()
        blender_image.update()

        return blender_image

    @staticmethod
    def decode_file(gltf, filepath):
        """Decodes a KTX2 file into a flat RGBA float buffer.

        Returns (width, height, pixels), with `pixels` an `array.array('f')` of
        width * height * 4 values in [0, 1], row order already bottom-to-top,
        ready to assign directly to `bpy.types.Image.pixels.foreach_set()`.
        """
        KtxDecoder.load_library(gltf)
        lib = gltf.ktx_decoder

        texture = ctypes.c_void_p()
        error_code = lib.decodeFromFile(
            str(filepath).encode('utf-8'),
            ctypes.byref(texture),
        )
        if error_code != 0:
            raise ImportError("KTX decoding failed with error code {}".format(error_code))

        try:
            width = ctypes.c_uint32()
            height = ctypes.c_uint32()
            data_size = ctypes.c_size_t()
            error_code = lib.getImageInfo(
                texture, ctypes.byref(width), ctypes.byref(height), ctypes.byref(data_size))
            if error_code != 0:
                raise ImportError("Failed to get KTX image info, error code {}".format(error_code))

            # One float per source RGBA8 byte.
            float_count = data_size.value
            pixels = (ctypes.c_float * float_count)()
            error_code = lib.getImageDataFloat(texture, pixels, float_count * ctypes.sizeof(ctypes.c_float))
            if error_code != 0:
                raise ImportError("Failed to get KTX image data, error code {}".format(error_code))

            # `ctypes` arrays report their buffer format with an explicit byte-order prefix
            # (e.g. "<f"), which `foreach_set()` rejects. `array.array('f')` reports the plain
            # "f" format it expects, so convert via a fast bulk byte copy.
            pixels_array = array.array('f')
            pixels_array.frombytes(bytes(pixels))

            return width.value, height.value, pixels_array
        finally:
            lib.destroyTexture(texture)
