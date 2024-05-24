# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import os
from typing import Optional, Tuple
import numpy as np
import tempfile
import enum


class Channel(enum.IntEnum):
    R = 0
    G = 1
    B = 2
    A = 3

# These describe how an ExportImage's channels should be filled.


class FillImage:
    """Fills a channel with the channel src_chan from a Blender image."""

    def __init__(self, image: bpy.types.Image, src_chan: Channel):
        self.image = image
        self.src_chan = src_chan


class FillImageTile:
    """Fills a channel with the channel src_chan from a Blender UDIM image."""

    def __init__(self, image: bpy.types.Image, tile, src_chan: Channel):
        self.image = image
        self.tile = tile
        self.src_chan = src_chan


class FillWhite:
    """Fills a channel with all ones (1.0)."""
    pass


class FillWith:
    """Fills a channel with all same values"""

    def __init__(self, value):
        self.value = value


class StoreData:
    def __init__(self, data):
        """Store numeric data (not an image channel"""
        self.data = data


class StoreImage:
    """
    Store a channel with the channel src_chan from a Blender image.
    This channel will be used for numpy calculation (no direct channel mapping)
    """

    def __init__(self, image: bpy.types.Image):
        self.image = image


class ExportImage:
    """Custom image class.

    An image is represented by giving a description of how to fill its red,
    green, blue, and alpha channels. For example:

        self.fills = {
            Channel.R: FillImage(image=bpy.data.images['Im1'], src_chan=Channel.B),
            Channel.G: FillWhite(),
        }

    This says that the ExportImage's R channel should be filled with the B
    channel of the Blender image 'Im1', and the ExportImage's G channel
    should be filled with all 1.0s. Undefined channels mean we don't care
    what values that channel has.

    This is flexible enough to handle the case where eg. the user used the R
    channel of one image as the metallic value and the G channel of another
    image as the roughness, and we need to synthesize an ExportImage that
    packs those into the B and G channels for glTF.

    Storing this description (instead of raw pixels) lets us make more
    intelligent decisions about how to encode the image.
    """

    def __init__(self, original=None):
        self.fills = {}
        self.stored = {}

        self.original = original  # In case of keeping original texture images
        self.numpy_calc = None

    def set_calc(self, numpy_calc):
        self.numpy_calc = numpy_calc  # In case of numpy calculation (no direct channel mapping)

    @staticmethod
    def from_blender_image(image: bpy.types.Image):
        export_image = ExportImage()
        for chan in range(image.channels):
            export_image.fill_image(image, dst_chan=chan, src_chan=chan)
        return export_image

    @staticmethod
    def from_blender_image_tile(export_settings):
        export_image = ExportImage()
        original_udim = export_settings['current_udim_info']['image']
        for chan in range(original_udim.channels):
            export_image.fill_image_tile(
                original_udim,
                export_settings['current_udim_info']['tile'],
                dst_chan=chan,
                src_chan=chan)

        return export_image

    @staticmethod
    def from_original(image: bpy.types.Image):
        return ExportImage(image)

    def fill_image(self, image: bpy.types.Image, dst_chan: Channel, src_chan: Channel):
        self.fills[dst_chan] = FillImage(image, src_chan)

    def fill_image_tile(self, image: bpy.types.Image, tile, dst_chan: Channel, src_chan: Channel):
        self.fills[dst_chan] = FillImageTile(image, tile, src_chan)

    def store_data(self, identifier, data, type='Image'):
        if type == "Image":  # This is an image
            self.stored[identifier] = StoreImage(data)
        else:  # This is a numeric value
            self.stored[identifier] = StoreData(data)

    def fill_white(self, dst_chan: Channel):
        self.fills[dst_chan] = FillWhite()

    def fill_with(self, dst_chan, value):
        self.fills[dst_chan] = FillWith(value)

    def is_filled(self, chan: Channel) -> bool:
        return chan in self.fills

    def empty(self) -> bool:
        if self.original is None:
            return not (self.fills or self.stored)
        else:
            return False

    def blender_image(self, export_settings) -> Optional[bpy.types.Image]:
        """If there's an existing Blender image we can use,
        returns it. Otherwise (if channels need packing),
        returns None.
        """
        if self.__on_happy_path():
            # Store that this image is fully exported (used to export or not not used images)
            for fill in self.fills.values():
                export_settings['exported_images'][fill.image.name] = 1  # Fully used
                break

            for fill in self.fills.values():
                return fill.image
        return None

    def __on_happy_path(self) -> bool:
        # All src_chans match their dst_chan and come from the same image
        return (
            all(isinstance(fill, FillImage) for fill in self.fills.values()) and
            all(dst_chan == fill.src_chan for dst_chan, fill in self.fills.items()) and
            len(set(fill.image.name for fill in self.fills.values())) == 1
        )

    def __on_happy_path_udim(self) -> bool:
        # All src_chans match their dst_chan and come from the same udim image

        return (
            all(isinstance(fill, FillImageTile) for fill in self.fills.values()) and
            all(dst_chan == fill.src_chan for dst_chan, fill in self.fills.items()) and
            len(set(fill.image.name for fill in self.fills.values())) == 1 and
            all(fill.tile == self.fills[list(self.fills.keys())[0]].tile for fill in self.fills.values())
        )

    def encode(self, mime_type: Optional[str], export_settings) -> Tuple[bytes, bool]:
        self.file_format = {
            "image/jpeg": "JPEG",
            "image/png": "PNG",
            "image/webp": "WEBP"
        }.get(mime_type, "PNG")

        # Happy path = we can just use an existing Blender image
        if self.__on_happy_path():
            # Store that this image is fully exported (used to export or not not used images)
            for fill in self.fills.values():
                export_settings['exported_images'][fill.image.name] = 1  # Fully used
                break
            return self.__encode_happy(export_settings), None

        if self.__on_happy_path_udim():
            return self.__encode_happy_tile(export_settings), None

        # Unhappy path = we need to create the image self.fills describes or self.stores describes
        if self.numpy_calc is None:
            if self.__unhappy_is_udim():
                return self.__encode_unhappy_udim(export_settings), None
            else:
                return self.__encode_unhappy(export_settings), None
        else:
            pixels, width, height, factor = self.numpy_calc(self.stored, export_settings)
            return self.__encode_from_numpy_array(pixels, (width, height), export_settings), factor

    def __encode_happy(self, export_settings) -> bytes:
        return self.__encode_from_image(self.blender_image(export_settings), export_settings)

    def __encode_happy_tile(self, export_settings) -> bytes:
        return self.__encode_from_image_tile(
            self.fills[list(self.fills.keys())[0]].image, export_settings['current_udim_info']['tile'], export_settings)

    def __unhappy_is_udim(self):
        return any(isinstance(fill, FillImageTile) for fill in self.fills.values())

    def __encode_unhappy_udim(self, export_settings) -> bytes:
        # We need to assemble the image out of channels.
        # Do it with numpy and image.pixels of the right UDIM tile.

        images = []
        for fill in self.fills.values():
            if isinstance(fill, FillImageTile):
                if fill.image not in images:
                    images.append((fill.image, fill.tile))
                    export_settings['exported_images'][fill.image.name] = 2  # 2 = partially used

        if not images:
            # No ImageFills; use a 1x1 white pixel
            pixels = np.array([1.0, 1.0, 1.0, 1.0], np.float32)
            return self.__encode_from_numpy_array(pixels, (1, 1), export_settings)

        # We need to open the original UDIM image tile to get size & pixel data
        original_image_sizes = []
        for image, tile in images:
            src_path = bpy.path.abspath(image.filepath_raw).replace("<UDIM>", tile)
            with TmpImageGuard() as guard:
                guard.image = bpy.data.images.load(
                    src_path,
                )
                original_image_sizes.append((guard.image.size[0], guard.image.size[1]))

        width = max(image_size[0] for image_size in original_image_sizes)
        height = max(image_size[1] for image_size in original_image_sizes)

        out_buf = np.ones(width * height * 4, np.float32)
        tmp_buf = np.empty(width * height * 4, np.float32)

        for idx, (image, tile) in enumerate(images):
            if original_image_sizes[idx][0] == width and original_image_sizes[idx][1] == height:
                src_path = bpy.path.abspath(image.filepath_raw).replace("<UDIM>", tile)
                with TmpImageGuard() as guard:
                    guard.image = bpy.data.images.load(
                        src_path,
                    )
                    guard.image.pixels.foreach_get(tmp_buf)
            else:
                # Image is the wrong size; make a temp copy and scale it.
                src_path = bpy.path.abspath(image.filepath_raw).replace("<UDIM>", tile)
                with TmpImageGuard() as guard:
                    guard.image = bpy.data.images.load(
                        src_path,
                    )
                    tmp_image = guard.image
                    tmp_image.scale(width, height)
                    tmp_image.pixels.foreach_get(tmp_buf)

            # Copy any channels for this image to the output
            for dst_chan, fill in self.fills.items():
                if isinstance(fill, FillImageTile) and fill.image == image:
                    out_buf[int(dst_chan)::4] = tmp_buf[int(fill.src_chan)::4]
                elif isinstance(fill, FillWith):
                    out_buf[int(dst_chan)::4] = fill.value

        tmp_buf = None  # GC this

        return self.__encode_from_numpy_array(out_buf, (width, height), export_settings)

    def __encode_unhappy(self, export_settings) -> bytes:
        # We need to assemble the image out of channels.
        # Do it with numpy and image.pixels.

        # Find all Blender images used
        images = []
        for fill in self.fills.values():
            if isinstance(fill, FillImage):
                if fill.image not in images:
                    images.append(fill.image)
                    export_settings['exported_images'][fill.image.name] = 2  # 2 = partially used

        if not images:
            # No ImageFills; use a 1x1 white pixel
            pixels = np.array([1.0, 1.0, 1.0, 1.0], np.float32)
            return self.__encode_from_numpy_array(pixels, (1, 1), export_settings)

        width = max(image.size[0] for image in images)
        height = max(image.size[1] for image in images)

        out_buf = np.ones(width * height * 4, np.float32)
        tmp_buf = np.empty(width * height * 4, np.float32)

        for image in images:
            if image.size[0] == width and image.size[1] == height:
                image.pixels.foreach_get(tmp_buf)
            else:
                # Image is the wrong size; make a temp copy and scale it.
                with TmpImageGuard() as guard:
                    make_temp_image_copy(guard, src_image=image)
                    tmp_image = guard.image
                    tmp_image.scale(width, height)
                    tmp_image.pixels.foreach_get(tmp_buf)

            # Copy any channels for this image to the output
            for dst_chan, fill in self.fills.items():
                if isinstance(fill, FillImage) and fill.image == image:
                    out_buf[int(dst_chan)::4] = tmp_buf[int(fill.src_chan)::4]
                elif isinstance(fill, FillWith):
                    out_buf[int(dst_chan)::4] = fill.value

        tmp_buf = None  # GC this

        return self.__encode_from_numpy_array(out_buf, (width, height), export_settings)

    def __encode_from_numpy_array(self, pixels: np.ndarray, dim: Tuple[int, int], export_settings) -> bytes:
        with TmpImageGuard() as guard:
            guard.image = bpy.data.images.new(
                "##gltf-export:tmp-image##",
                width=dim[0],
                height=dim[1],
                alpha=Channel.A in self.fills,
            )
            tmp_image = guard.image

            tmp_image.pixels.foreach_set(pixels)

            return _encode_temp_image(tmp_image, self.file_format, export_settings)

    def __encode_from_image(self, image: bpy.types.Image, export_settings) -> bytes:
        # See if there is an existing file we can use.
        data = None
        # Sequence image can't be exported, but it avoid to crash to check that default image exists
        # Else, it can crash when trying to access a non existing image
        if image.source in ['FILE', 'SEQUENCE'] and not image.is_dirty:
            if image.packed_file is not None:
                data = image.packed_file.data
            else:
                src_path = bpy.path.abspath(image.filepath_raw)
                if os.path.isfile(src_path):
                    with open(src_path, 'rb') as f:
                        data = f.read()
        # Check magic number is right
        if data:
            if self.file_format == 'PNG':
                if data.startswith(b'\x89PNG'):
                    return data
            elif self.file_format == 'JPEG':
                if data.startswith(b'\xff\xd8\xff'):
                    return data
            elif self.file_format == 'WEBP':
                if data[8:12] == b'WEBP':
                    return data

        # Copy to a temp image and save.
        with TmpImageGuard() as guard:
            make_temp_image_copy(guard, src_image=image)
            tmp_image = guard.image
            return _encode_temp_image(tmp_image, self.file_format, export_settings)

    def __encode_from_image_tile(self, udim_image, tile, export_settings):
        src_path = bpy.path.abspath(udim_image.filepath_raw).replace("<UDIM>", tile)

        if os.path.isfile(src_path):
            with open(src_path, 'rb') as f:
                data = f.read()

        if data:
            if self.file_format == 'PNG':
                if data.startswith(b'\x89PNG'):
                    return data
            elif self.file_format == 'JPEG':
                if data.startswith(b'\xff\xd8\xff'):
                    return data
            elif self.file_format == 'WEBP':
                if data[8:12] == b'WEBP':
                    return data

        # We don't manage UDIM packed image, so this could not happen to be here


def _encode_temp_image(tmp_image: bpy.types.Image, file_format: str, export_settings) -> bytes:
    with tempfile.TemporaryDirectory() as tmpdirname:
        tmpfilename = tmpdirname + '/img'
        tmp_image.filepath_raw = tmpfilename

        tmp_image.file_format = file_format

        # if image is jpeg, use quality export settings
        if file_format in ["JPEG", "WEBP"]:
            tmp_image.save(quality=export_settings['gltf_image_quality'])
        else:
            tmp_image.save()

        with open(tmpfilename, "rb") as f:
            return f.read()


class TmpImageGuard:
    """Guard to automatically clean up temp images (use it with `with`)."""

    def __init__(self):
        self.image = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.image is not None:
            bpy.data.images.remove(self.image, do_unlink=True)


def make_temp_image_copy(guard: TmpImageGuard, src_image: bpy.types.Image):
    """Makes a temporary copy of src_image. Will be cleaned up with guard."""
    guard.image = src_image.copy()
    tmp_image = guard.image

    tmp_image.update()
    # See #1564 and T95616
    tmp_image.scale(*src_image.size)

    if src_image.is_dirty:  # Warning, img size change doesn't make it dirty, see T95616
        # Unsaved changes aren't copied by .copy(), so do them ourselves
        tmp_buf = np.empty(src_image.size[0] * src_image.size[1] * 4, np.float32)
        src_image.pixels.foreach_get(tmp_buf)
        tmp_image.pixels.foreach_set(tmp_buf)
