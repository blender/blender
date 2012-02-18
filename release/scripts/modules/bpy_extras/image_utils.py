# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

__all__ = (
    "load_image",
    )


# limited replacement for BPyImage.comprehensiveImageLoad
def load_image(imagepath,
               dirname="",
               place_holder=False,
               recursive=False,
               ncase_cmp=True,
               convert_callback=None,
               verbose=False,
               ):
    """
    Return an image from the file path with options to search multiple paths
    and return a placeholder if its not found.

    :arg filepath: The image filename
       If a path precedes it, this will be searched as well.
    :type filepath: string
    :arg dirname: is the directory where the image may be located - any file at
       the end will be ignored.
    :type dirname: string
    :arg place_holder: if True a new place holder image will be created.
       this is useful so later you can relink the image to its original data.
    :type place_holder: bool
    :arg recursive: If True, directories will be recursively searched.
       Be careful with this if you have files in your root directory because
       it may take a long time.
    :type recursive: bool
    :arg ncase_cmp: on non windows systems, find the correct case for the file.
    :type ncase_cmp: bool
    :arg convert_callback: a function that takes an existing path and returns
       a new one. Use this when loading image formats blender may not support,
       the CONVERT_CALLBACK can take the path for a GIF (for example),
       convert it to a PNG and return the PNG's path.
       For formats blender can read, simply return the path that is given.
    :type convert_callback: function
    :return: an image or None
    :rtype: :class:`bpy.types.Image`
    """
    import os
    import bpy

    # TODO: recursive

    # -------------------------------------------------------------------------
    # Utility Functions

    def _image_load_placeholder(path):
        name = bpy.path.basename(path)
        if type(name) == bytes:
            name = name.decode('utf-8', "replace")
        image = bpy.data.images.new(name, 128, 128)
        # allow the path to be resolved later
        image.filepath = path
        image.source = 'FILE'
        return image

    def _image_load(path):
        import bpy

        if convert_callback:
            path = convert_callback(path)

        try:
            image = bpy.data.images.load(path)
        except RuntimeError:
            image = None

        if verbose:
            if image:
                print("    image loaded '%s'" % path)
            else:
                print("    image load failed '%s'" % path)

        # image path has been checked so the path could not be read for some
        # reason, so be sure to return a placeholder
        if place_holder and image is None:
            image = _image_load_placeholder(path)

        return image

    # -------------------------------------------------------------------------

    if verbose:
        print("load_image('%s', '%s', ...)" % (imagepath, dirname))

    if os.path.exists(imagepath):
        return _image_load(imagepath)

    variants = [imagepath]

    if dirname:
        variants += [os.path.join(dirname, imagepath),
                     os.path.join(dirname, bpy.path.basename(imagepath)),
                     ]

    for filepath_test in variants:
        if ncase_cmp:
            ncase_variants = (filepath_test,
                              bpy.path.resolve_ncase(filepath_test),
                              )
        else:
            ncase_variants = (filepath_test, )

        for nfilepath in ncase_variants:
            if os.path.exists(nfilepath):
                return _image_load(nfilepath)

    # None of the paths exist so return placeholder
    if place_holder:
        return _image_load_placeholder(imagepath)

    # TODO comprehensiveImageLoad also searched in bpy.config.textureDir
    return None
