# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import pathlib
import sys
import unittest

import bpy

sys.path.append(str(pathlib.Path(__file__).parent.absolute()))
from modules.colored_print import print_message
from modules.imbuf_test import AbstractImBufTest


args = None

TEMPLATE_RGBA08 = "template-rgba08.png"
TEMPLATE_RGBA32 = "template-rgba32.exr"


class ImBufTest(AbstractImBufTest):
    @classmethod
    def setUpClass(cls):
        AbstractImBufTest.init(args)

        if cls.update:
            os.makedirs(cls.reference_dir, exist_ok=True)

    def _load_template_image(self, name, template_name):
        image_path = str(self.test_dir.joinpath(template_name))
        bpy.ops.image.open(filepath=image_path)
        img = bpy.data.images[template_name]
        img.name = name
        return img

    def _setup_image(self, src, ext, settings):
        scene = bpy.data.scenes[0]
        image_settings = scene.render.image_settings

        # Make an appropriate filename which embeds all relevant settings and
        # set the file output parameters for the exact configuration we want
        name = ""
        for s in settings:
            if s == "color_depth":
                name += str(settings[s]).rjust(2, '0') + "-"
            else:
                name += str(settings[s]) + "-"

            setattr(image_settings, s, settings[s])

        image_name = name[:-1].lower() + "__from__" + src + "." + ext

        return image_name

    def _save_image(self, src, image_name):
        loaders = {
            "rgba08": lambda name: self._load_template_image(name, TEMPLATE_RGBA08),
            "rgba32": lambda name: self._load_template_image(name, TEMPLATE_RGBA32),
        }

        # Load the template image and assign it the image name
        img = loaders[src](image_name)

        # Save the image in the desired format with the desired settings
        scene = bpy.data.scenes[0]
        ref_image_path = self.reference_dir.joinpath(img.name)
        out_image_path = self.output_dir.joinpath(img.name)
        img.save_render(str(out_image_path), scene=scene)

        # Completely remove image in case it was modified during save
        img.user_clear()
        bpy.data.images.remove(img)
        return ref_image_path, out_image_path

    def _validate(self, ref_image_path, out_image_path):
        return self.call_idiff(ref_image_path, out_image_path)

    def check(self, src, ext, settings):
        image_name = self._setup_image(src, ext, settings)
        print_message(image_name, 'SUCCESS', 'RUN')

        ref_image_path, out_image_path = self._save_image(src, image_name)

        if not self._validate(ref_image_path, out_image_path):
            self.errors += 1
            print_message("Save result is different from reference image")
            print_message(ref_image_path.name, 'FAILURE', 'FAILED')
        else:
            print_message(ref_image_path.name, 'SUCCESS', 'OK')


# autopep8: off
class ImBufSaveTest(ImBufTest):
    def test_save_bmp(self):
        self.check(src="rgba08", ext="bmp", settings={"file_format": "BMP", "color_mode": "BW"})
        self.check(src="rgba08", ext="bmp", settings={"file_format": "BMP", "color_mode": "RGB"})

        self.check(src="rgba32", ext="bmp", settings={"file_format": "BMP", "color_mode": "BW"})
        self.check(src="rgba32", ext="bmp", settings={"file_format": "BMP", "color_mode": "RGB"})

    def test_save_png(self):
        self.check(src="rgba08", ext="png", settings={"file_format": "PNG", "color_mode": "BW", "color_depth": "8", "compression": 15})
        self.check(src="rgba08", ext="png", settings={"file_format": "PNG", "color_mode": "RGB", "color_depth": "8", "compression": 15})
        self.check(src="rgba08", ext="png", settings={"file_format": "PNG", "color_mode": "RGBA", "color_depth": "8", "compression": 15})
        self.check(src="rgba08", ext="png", settings={"file_format": "PNG", "color_mode": "BW", "color_depth": "16", "compression": 25})
        self.check(src="rgba08", ext="png", settings={"file_format": "PNG", "color_mode": "RGB", "color_depth": "16", "compression": 25})
        self.check(src="rgba08", ext="png", settings={"file_format": "PNG", "color_mode": "RGBA", "color_depth": "16", "compression": 25})

        self.check(src="rgba32", ext="png", settings={"file_format": "PNG", "color_mode": "BW", "color_depth": "8", "compression": 15})
        self.check(src="rgba32", ext="png", settings={"file_format": "PNG", "color_mode": "RGB", "color_depth": "8", "compression": 15})
        self.check(src="rgba32", ext="png", settings={"file_format": "PNG", "color_mode": "RGBA", "color_depth": "8", "compression": 15})
        self.check(src="rgba32", ext="png", settings={"file_format": "PNG", "color_mode": "BW", "color_depth": "16", "compression": 25})
        self.check(src="rgba32", ext="png", settings={"file_format": "PNG", "color_mode": "RGB", "color_depth": "16", "compression": 25})
        self.check(src="rgba32", ext="png", settings={"file_format": "PNG", "color_mode": "RGBA", "color_depth": "16", "compression": 25})

    def test_save_exr(self):
        self.skip_if_format_missing("OPENEXR")

        self.check(src="rgba08", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "BW", "color_depth": "16", "exr_codec": "ZIP"})
        self.check(src="rgba08", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGB", "color_depth": "16", "exr_codec": "DWAA"})
        self.check(src="rgba08", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGBA", "color_depth": "16", "exr_codec": "DWAB"})
        self.check(src="rgba08", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "BW", "color_depth": "32", "exr_codec": "DWAB"})
        self.check(src="rgba08", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGB", "color_depth": "32", "exr_codec": "DWAA"})
        self.check(src="rgba08", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGBA", "color_depth": "32", "exr_codec": "ZIP"})

        self.check(src="rgba32", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "BW", "color_depth": "16", "exr_codec": "ZIP"})
        self.check(src="rgba32", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGB", "color_depth": "16", "exr_codec": "DWAA"})
        self.check(src="rgba32", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGBA", "color_depth": "16", "exr_codec": "DWAB"})
        self.check(src="rgba32", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "BW", "color_depth": "32", "exr_codec": "DWAB"})
        self.check(src="rgba32", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGB", "color_depth": "32", "exr_codec": "DWAA"})
        self.check(src="rgba32", ext="exr", settings={"file_format": "OPEN_EXR", "color_mode": "RGBA", "color_depth": "32", "exr_codec": "ZIP"})

    def test_save_hdr(self):
        self.check(src="rgba08", ext="hdr", settings={"file_format": "HDR", "color_mode": "BW"})
        self.check(src="rgba08", ext="hdr", settings={"file_format": "HDR", "color_mode": "RGB"})

        self.check(src="rgba32", ext="hdr", settings={"file_format": "HDR", "color_mode": "BW"})
        self.check(src="rgba32", ext="hdr", settings={"file_format": "HDR", "color_mode": "RGB"})

    def test_save_targa(self):
        self.check(src="rgba08", ext="tga", settings={"file_format": "TARGA", "color_mode": "BW"})
        self.check(src="rgba08", ext="tga", settings={"file_format": "TARGA", "color_mode": "RGB"})
        self.check(src="rgba08", ext="tga", settings={"file_format": "TARGA", "color_mode": "RGBA"})

        self.check(src="rgba32", ext="tga", settings={"file_format": "TARGA", "color_mode": "BW"})
        self.check(src="rgba32", ext="tga", settings={"file_format": "TARGA", "color_mode": "RGB"})
        self.check(src="rgba32", ext="tga", settings={"file_format": "TARGA", "color_mode": "RGBA"})

    def test_save_targa_raw(self):
        self.check(src="rgba08", ext="tga", settings={"file_format": "TARGA_RAW", "color_mode": "BW"})
        self.check(src="rgba08", ext="tga", settings={"file_format": "TARGA_RAW", "color_mode": "RGB"})
        self.check(src="rgba08", ext="tga", settings={"file_format": "TARGA_RAW", "color_mode": "RGBA"})

        self.check(src="rgba32", ext="tga", settings={"file_format": "TARGA_RAW", "color_mode": "BW"})
        self.check(src="rgba32", ext="tga", settings={"file_format": "TARGA_RAW", "color_mode": "RGB"})
        self.check(src="rgba32", ext="tga", settings={"file_format": "TARGA_RAW", "color_mode": "RGBA"})

    def test_save_tiff(self):
        self.check(src="rgba08", ext="tif", settings={"file_format": "TIFF", "color_mode": "BW", "color_depth": "8", "tiff_codec": "DEFLATE"})
        self.check(src="rgba08", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGB", "color_depth": "8", "tiff_codec": "LZW"})
        self.check(src="rgba08", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGBA", "color_depth": "8", "tiff_codec": "PACKBITS"})
        self.check(src="rgba08", ext="tif", settings={"file_format": "TIFF", "color_mode": "BW", "color_depth": "16", "tiff_codec": "PACKBITS"})
        self.check(src="rgba08", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGB", "color_depth": "16", "tiff_codec": "LZW"})
        self.check(src="rgba08", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGBA", "color_depth": "16", "tiff_codec": "DEFLATE"})

        self.check(src="rgba32", ext="tif", settings={"file_format": "TIFF", "color_mode": "BW", "color_depth": "8", "tiff_codec": "DEFLATE"})
        self.check(src="rgba32", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGB", "color_depth": "8", "tiff_codec": "LZW"})
        self.check(src="rgba32", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGBA", "color_depth": "8", "tiff_codec": "PACKBITS"})
        self.check(src="rgba32", ext="tif", settings={"file_format": "TIFF", "color_mode": "BW", "color_depth": "16", "tiff_codec": "PACKBITS"})
        self.check(src="rgba32", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGB", "color_depth": "16", "tiff_codec": "LZW"})
        self.check(src="rgba32", ext="tif", settings={"file_format": "TIFF", "color_mode": "RGBA", "color_depth": "16", "tiff_codec": "DEFLATE"})

    def test_save_jpeg(self):
        self.check(src="rgba08", ext="jpg", settings={"file_format": "JPEG", "color_mode": "BW", "quality": 90})
        self.check(src="rgba08", ext="jpg", settings={"file_format": "JPEG", "color_mode": "RGB", "quality": 90})

        self.check(src="rgba32", ext="jpg", settings={"file_format": "JPEG", "color_mode": "BW", "quality": 70})
        self.check(src="rgba32", ext="jpg", settings={"file_format": "JPEG", "color_mode": "RGB", "quality": 70})

    def test_save_jpeg2000(self):
        self.skip_if_format_missing("OPENJPEG")

        # Is there a better combination of settings we can use so there's not so many?
        # Is this a good mix?
        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "BW", "color_depth": "8", "jpeg2k_codec": "JP2", "quality": 90})
        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "BW", "color_depth": "12", "jpeg2k_codec": "JP2", "quality": 90})
        self.check(src="rgba08", ext="j2c", settings={"file_format": "JPEG2000", "color_mode": "BW", "color_depth": "16", "jpeg2k_codec": "J2K", "quality": 90})
        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGB", "color_depth": "8", "jpeg2k_codec": "JP2", "quality": 90})
        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGB", "color_depth": "12", "jpeg2k_codec": "JP2", "quality": 90})
        self.check(src="rgba08", ext="j2c", settings={"file_format": "JPEG2000", "color_mode": "RGB", "color_depth": "16", "jpeg2k_codec": "J2K", "quality": 90})
        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "8", "jpeg2k_codec": "JP2", "quality": 90})
        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "12", "jpeg2k_codec": "JP2", "quality": 90})
        self.check(src="rgba08", ext="j2c", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "16", "jpeg2k_codec": "J2K", "quality": 90})

        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "BW", "color_depth": "8", "jpeg2k_codec": "JP2", "quality": 70})
        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "BW", "color_depth": "12", "jpeg2k_codec": "JP2", "quality": 70})
        self.check(src="rgba32", ext="j2c", settings={"file_format": "JPEG2000", "color_mode": "BW", "color_depth": "16", "jpeg2k_codec": "J2K", "quality": 70})
        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGB", "color_depth": "8", "jpeg2k_codec": "JP2", "quality": 70})
        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGB", "color_depth": "12", "jpeg2k_codec": "JP2", "quality": 70})
        self.check(src="rgba32", ext="j2c", settings={"file_format": "JPEG2000", "color_mode": "RGB", "color_depth": "16", "jpeg2k_codec": "J2K", "quality": 70})
        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "8", "jpeg2k_codec": "JP2", "quality": 70})
        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "12", "jpeg2k_codec": "JP2", "quality": 70})
        self.check(src="rgba32", ext="j2c", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "16", "jpeg2k_codec": "J2K", "quality": 70})

        # Note: The 'use_jpeg2k_cinema_preset' option mandates very large images
        # self.check(src="rgba08", ext="jpg", settings={"file_format":"JPEG2000", "color_mode":"RGBA", "color_depth":"8", "jpeg2k_codec":"JP2", "use_jpeg2k_cinema_preset":True, "use_jpeg2k_cinema_48":False, "use_jpeg2k_ycc":False, "quality":70})
        # self.check(src="rgba32", ext="jpg", settings={"file_format":"JPEG2000", "color_mode":"RGBA", "color_depth":"8", "jpeg2k_codec":"JP2", "use_jpeg2k_cinema_preset":True, "use_jpeg2k_cinema_48":False, "use_jpeg2k_ycc":False, "quality":70})

        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "12", "jpeg2k_codec": "JP2", "use_jpeg2k_cinema_preset": False, "use_jpeg2k_cinema_48": True, "use_jpeg2k_ycc": False, "quality": 70})
        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "12", "jpeg2k_codec": "JP2", "use_jpeg2k_cinema_preset": False, "use_jpeg2k_cinema_48": True, "use_jpeg2k_ycc": False, "quality": 70})

        self.check(src="rgba08", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "16", "jpeg2k_codec": "JP2", "use_jpeg2k_cinema_preset": False, "use_jpeg2k_cinema_48": False, "use_jpeg2k_ycc": True, "quality": 70})
        self.check(src="rgba32", ext="jp2", settings={"file_format": "JPEG2000", "color_mode": "RGBA", "color_depth": "16", "jpeg2k_codec": "JP2", "use_jpeg2k_cinema_preset": False, "use_jpeg2k_cinema_48": False, "use_jpeg2k_ycc": True, "quality": 70})

    def test_save_dpx(self):
        self.check(src="rgba08", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGB", "color_depth": "8", "use_cineon_log": False})
        self.check(src="rgba08", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGB", "color_depth": "12", "use_cineon_log": False})
        self.check(src="rgba08", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGB", "color_depth": "16", "use_cineon_log": False})
        self.check(src="rgba08", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGBA", "color_depth": "10", "use_cineon_log": True})
        self.check(src="rgba08", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGBA", "color_depth": "12", "use_cineon_log": True})
        self.check(src="rgba08", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGBA", "color_depth": "16", "use_cineon_log": True})

        self.check(src="rgba32", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGB", "color_depth": "8", "use_cineon_log": False})
        self.check(src="rgba32", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGB", "color_depth": "12", "use_cineon_log": False})
        self.check(src="rgba32", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGB", "color_depth": "16", "use_cineon_log": False})
        self.check(src="rgba32", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGBA", "color_depth": "10", "use_cineon_log": True})
        self.check(src="rgba32", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGBA", "color_depth": "12", "use_cineon_log": True})
        self.check(src="rgba32", ext="dpx", settings={"file_format": "DPX", "color_mode": "RGBA", "color_depth": "16", "use_cineon_log": True})

    def test_save_cineon(self):
        self.skip_if_format_missing("CINEON")

        self.check(src="rgba08", ext="cin", settings={"file_format": "CINEON", "color_mode": "RGB"})
        self.check(src="rgba32", ext="cin", settings={"file_format": "CINEON", "color_mode": "RGB"})

    def test_save_webp(self):
        self.skip_if_format_missing("WEBP")

        self.check(src="rgba08", ext="webp", settings={"file_format": "WEBP", "color_mode": "RGB", "quality": 90})
        self.check(src="rgba08", ext="webp", settings={"file_format": "WEBP", "color_mode": "RGBA", "quality": 90})

        # Note: These 2 variations are problematic on MacOS ARM64 (#105006)
        # self.check(src="rgba32", ext="webp", settings={"file_format": "WEBP", "color_mode": "RGB", "quality": 70})
        # self.check(src="rgba32", ext="webp", settings={"file_format": "WEBP", "color_mode": "RGBA", "quality": 70})
# autopep8: on


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('-test_dir', required=True, type=pathlib.Path)
    parser.add_argument('-output_dir', required=True, type=pathlib.Path)
    parser.add_argument('-idiff', required=True, type=pathlib.Path)
    parser.add_argument('-optional_formats', required=True)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == '__main__':
    main()
