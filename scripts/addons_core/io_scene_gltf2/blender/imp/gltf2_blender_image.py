# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import os
from os.path import dirname, join, basename

from ...io.com.gltf2_io_path import uri_to_path
from ...io.imp.gltf2_io_binary import BinaryData
from ...io.imp.gltf2_io_user_extensions import import_user_extensions


# Note that Image is not a glTF2.0 object
class BlenderImage():
    """Manage Image."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def create(gltf, img_idx):
        """Image creation."""
        img = gltf.data.images[img_idx]

        if img.blender_image_name is not None:
            # Image is already used somewhere
            return

        import_user_extensions('gather_import_image_before_hook', gltf, img)

        if img.uri is not None and not img.uri.startswith('data:'):
            blender_image = create_from_file(gltf, img_idx)
        else:
            blender_image = create_from_data(gltf, img_idx)

        if blender_image:
            blender_image.alpha_mode = 'CHANNEL_PACKED'
            img.blender_image_name = blender_image.name

        import_user_extensions('gather_import_image_after_hook', gltf, img, blender_image)


def create_from_file(gltf, img_idx):
    # Image stored in a file

    num_images = len(bpy.data.images)

    img = gltf.data.images[img_idx]

    path = join(dirname(gltf.filename), uri_to_path(img.uri))
    path = os.path.abspath(path)
    if bpy.data.is_saved and bpy.context.preferences.filepaths.use_relative_paths:
        try:
            path = bpy.path.relpath(path)
        except:
            # May happen on Windows if on different drives, eg. C:\ and D:\
            pass

    img_name = img.name or basename(path)

    try:
        blender_image = bpy.data.images.load(
            path,
            check_existing=True,
        )

        needs_pack = gltf.import_settings['import_pack_images']
        if needs_pack:
            blender_image.pack()

    except RuntimeError:
        gltf.log.error("Missing image file (index %d): %s" % (img_idx, path))
        blender_image = _placeholder_image(img_name, os.path.abspath(path))

    if len(bpy.data.images) != num_images:  # If created a new image
        blender_image.name = img_name

    return blender_image


def create_from_data(gltf, img_idx):
    # Image stored as data => pack
    img_data = BinaryData.get_image_data(gltf, img_idx)
    if img_data is None:
        return
    img_name = gltf.data.images[img_idx].name or 'Image_%d' % img_idx

    # Create image, width and height are dummy values
    blender_image = bpy.data.images.new(img_name, 8, 8)
    # Set packed file data
    blender_image.pack(data=img_data.tobytes(), data_len=len(img_data))
    blender_image.source = 'FILE'

    return blender_image


def _placeholder_image(name, path):
    image = bpy.data.images.new(name, 128, 128)
    # allow the path to be resolved later
    image.filepath = path
    image.source = 'FILE'
    return image
