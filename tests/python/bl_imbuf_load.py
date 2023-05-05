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


class ImBufTest(AbstractImBufTest):
    @classmethod
    def setUpClass(cls):
        AbstractImBufTest.init(args)

        if cls.update:
            os.makedirs(cls.reference_load_dir, exist_ok=True)

    def _get_image_files(self, file_pattern):
        return [f for f in pathlib.Path(self.reference_dir).glob(file_pattern)]

    def _validate_metadata(self, img, ref_metadata_path, out_metadata_path):
        channels = img.channels
        is_float = img.is_float
        colorspace = img.colorspace_settings.name
        alpha_mode = img.alpha_mode
        actual_metadata = f"{channels=} {is_float=} {colorspace=} {alpha_mode=}"
        expected_metadata = ""

        # Save actual metadata
        out_metadata_path.write_text(actual_metadata, encoding="utf-8")

        if ref_metadata_path.exists():
            # Compare with expected
            try:
                expected_metadata = ref_metadata_path.read_text(encoding="utf-8")

                failed = not (actual_metadata == expected_metadata)
            except BaseException as e:
                if self.verbose:
                    print_message(e.output.decode("utf-8", 'ignore'))
                failed = True
        else:
            if not self.update:
                return False

            failed = True

        if failed:
            if self.update:
                # Update reference if requested.
                ref_metadata_path.write_text(actual_metadata, encoding="utf-8")
                failed = False
            else:
                print_message(
                    "Expected [{}] but got [{}]".format(expected_metadata, actual_metadata))

        return not failed

    def _save_exr(self, img, out_exr_path):
        scene = bpy.data.scenes[0]
        image_settings = scene.render.image_settings
        image_settings.file_format = "OPEN_EXR"
        image_settings.color_mode = "RGBA"
        image_settings.color_depth = "32"
        image_settings.exr_codec = "ZIP"

        img.save_render(str(out_exr_path), scene=scene)

    def _validate_pixels(self, img, ref_exr_path, out_exr_path):
        self._save_exr(img, out_exr_path)

        return self.call_idiff(ref_exr_path, out_exr_path)

    def check(self, file_pattern):
        image_files = self._get_image_files(file_pattern)
        if len(image_files) == 0:
            self.fail(f"No images found for pattern {file_pattern}")

        for image_path in image_files:
            print_message(image_path.name, 'SUCCESS', 'RUN')

            # Load the image under test
            bpy.ops.image.open(filepath=str(image_path))
            img = bpy.data.images[image_path.name]

            # Compare the image with our exr/metadata references
            exr_filename = image_path.with_suffix(".exr").name
            metadata_filename = image_path.with_suffix(".txt").name

            ref_exr_path = self.reference_load_dir.joinpath(exr_filename)
            ref_metadata_path = self.reference_load_dir.joinpath(metadata_filename)
            out_exr_path = self.output_dir.joinpath(exr_filename)
            out_metadata_path = self.output_dir.joinpath(metadata_filename)

            res1 = self._validate_metadata(img, ref_metadata_path, out_metadata_path)
            res2 = self._validate_pixels(img, ref_exr_path, out_exr_path)

            if not res1 or not res2:
                self.errors += 1
                print_message("Results are different from reference data")
                print_message(image_path.name, 'FAILURE', 'FAILED')
            else:
                print_message(image_path.name, 'SUCCESS', 'OK')


class ImBufLoadTest(ImBufTest):
    def test_load_bmp(self):
        self.check("*.bmp")

    def test_load_png(self):
        self.check("*.png")

    def test_load_exr(self):
        self.skip_if_format_missing("OPENEXR")

        self.check("*.exr")

    def test_load_hdr(self):
        self.check("*.hdr")

    def test_load_targa(self):
        self.check("*.tga")

    def test_load_tiff(self):
        self.check("*.tif")

    def test_load_jpeg(self):
        self.check("*.jpg")

    def test_load_jpeg2000(self):
        self.skip_if_format_missing("OPENJPEG")

        self.check("*.jp2")
        self.check("*.j2c")

    def test_load_dpx(self):
        self.check("*.dpx")

    def test_load_cineon(self):
        self.skip_if_format_missing("CINEON")

        self.check("*.cin")

    def test_load_webp(self):
        self.skip_if_format_missing("WEBP")

        self.check("*.webp")


class ImBufBrokenTest(AbstractImBufTest):
    @classmethod
    def setUpClass(cls):
        AbstractImBufTest.init(args)

    def _get_image_files(self, file_pattern):
        return [f for f in (self.test_dir / "broken_images").glob(file_pattern)]

    def check(self, file_pattern):
        image_files = self._get_image_files(file_pattern)
        print(image_files)
        if len(image_files) == 0:
            self.fail(f"No images found for pattern {file_pattern}")

        for image_path in image_files:
            print_message(image_path.name, 'SUCCESS', 'RUN')

            bpy.ops.image.open(filepath=str(image_path))


class ImBufLoadBrokenTest(ImBufBrokenTest):
    def test_load_exr(self):
        self.skip_if_format_missing("OPENEXR")

        self.check("*.exr")


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
