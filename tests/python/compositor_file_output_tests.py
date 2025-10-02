# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import sys
import os
from shutil import copyfile, rmtree
import argparse
import pathlib
import OpenImageIO as oiio
import unittest

import bpy

# Test utils are not accessible when run inside blender.
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(current_dir)
from modules.colored_print import print_message


class FileOutputTest(unittest.TestCase):
    """
    This class extends the compositor tests by supporting the File Output node.
    Unlike the `render_test` framework, this framework supports multiple output files per blend file.

    For consistency, all test cases for the file output node are part of a single ctest test case.
    To run such a test for CPU, run `ctest -R compositor_cpu_file_output --verbose`.
    To update a failing test, run `BLENDER_TEST_UPDATE=1 ctest -R compositor_cpu_file_output`
    """

    @classmethod
    def setUpClass(cls):
        cls.testdir = pathlib.Path(args.testdir)
        cls.outdir = pathlib.Path(args.outdir)
        cls.execution_device = "GPU" if args.gpu_backend else "CPU"
        cls.update = os.getenv("BLENDER_TEST_UPDATE") is not None

        # Images that look similar enough should pass the test
        cls.fail_threshold = 0.001

        comp_outdir = os.path.dirname(cls.outdir)
        if not os.path.exists(comp_outdir):
            os.mkdir(comp_outdir)

    def compare_images(self, ref_img_path, out_img_path, verbose=True):
        """
        Compare content and metadata of two images.
        """
        img_name = os.path.basename(ref_img_path)

        ref_input = oiio.ImageInput.open(ref_img_path)
        out_input = oiio.ImageInput.open(out_img_path)

        # We'll compare all subimages, so that every pass in multilayer, multiview
        # and multipart EXR files is checked.
        ref_subimages = ref_input.spec().get_int_attribute("oiio:subimages", 1)
        out_subimages = out_input.spec().get_int_attribute("oiio:subimages", 1)
        if ref_subimages != out_subimages:
            if verbose:
                print_message("Image subimages mismatch for '{:s}': {} -> {}".
                              format(img_name, ref_subimages, out_subimages),
                              'FAILURE', 'FAILED')
            return False

        ok = True

        for subimage in range(0, ref_subimages):
            ref_img = oiio.ImageBuf(ref_img_path, subimage, 0, oiio.ImageSpec())
            out_img = oiio.ImageBuf(out_img_path, subimage, 0, oiio.ImageSpec())

            # Compare image content
            comp = oiio.ImageBufAlgo.compare(ref_img, out_img, self.fail_threshold, 0)
            if comp.nfail != 0:
                if verbose:
                    print_message("Image content mismatch for '{:s}'".format(img_name),
                                  'FAILURE', 'FAILED')
                ok = False

            # Compare Metadata
            metadata_ignore = set(("Time", "File", "Date", "RenderTime", "Software"))

            ref_meta = ref_img.spec().extra_attribs
            out_meta = out_img.spec().extra_attribs

            for attrib in ref_meta:
                if attrib.name in metadata_ignore:
                    continue
                if attrib.name not in out_meta:
                    if verbose:
                        print_message(
                            "Image metadata mismatch: metadata '{:s}' does not exist in output image '{:s}'".format(
                                attrib.name, img_name), 'FAILURE', 'FAILED')
                    ok = False
                    continue
                if attrib.value != out_meta[attrib.name]:
                    if verbose:
                        print_message(
                            "Image metadata mismatch for metadata '{:s}' in image '{:s}'".format(attrib.name, img_name),
                            'FAILURE',
                            'FAILED')
                    ok = False

        return ok

    def update_tests(self, testdir, outdir):
        """
        Update tests by copying changed output images to the test directory.
        """
        print_message("Updating test {:s}...".format(os.path.basename(outdir)), 'SUCCESS', 'RUN')
        # The directory usually does not exist when a new test case (i.e. new blend file) is added,
        # So add the new reference test directory and copy all files from the output folder.
        if not os.path.exists(testdir):
            os.mkdir(testdir)
            for filename in os.listdir(outdir):
                copyfile(os.path.join(outdir, filename),
                         os.path.join(testdir, filename))
            return

        for filename in os.listdir(outdir):
            ref_img = os.path.join(testdir, filename)
            out_img = os.path.join(outdir, filename)
            if filename not in os.listdir(testdir):
                # A newly created image is found in the output directory,
                # so copy it to the reference test directory.
                copyfile(out_img, ref_img)
            else:
                if not self.compare_images(ref_img, out_img, verbose=False):
                    # An image in the output directory is found to be modified,
                    # so update the reference image by overwriting it with the output image.
                    copyfile(out_img, ref_img)

        # Delete reference images that have no corresponding output image.
        for filename in os.listdir(testdir):
            if filename not in os.listdir(outdir):
                pathlib.Path.unlink(os.path.join(testdir, filename))

    def compare_dirs(self, curr_testdir, curr_outdir):
        """
        Compare all images in both directories. Missing or too many output images will cause the test to fail.
        """
        ok = True
        ref_images = set()
        out_images = set()
        if not os.path.exists(curr_testdir):
            print_message("Test directory {:s} does not exist".format(curr_testdir), 'FAILURE', 'FAILED')
            if self.update:
                self.update_tests(curr_testdir, curr_outdir)
                return True
            else:
                return False

        for filename in os.listdir(curr_testdir):
            ref_images.add(filename)

        for filename in os.listdir(curr_outdir):
            out_images.add(filename)

        for img in out_images:
            if img not in ref_images:
                print_message("Output image '{:s}' has no corresponding test image".format(img),
                              'FAILURE', 'FAILED')
                ok = False

        for img in ref_images:
            if img not in out_images:
                print_message("Test image '{:s}' not found in output images".format(img), 'FAILURE', 'FAILED')
                ok = False
                continue

            if not self.compare_images(os.path.join(curr_testdir, img), os.path.join(curr_outdir, img), verbose=True):
                ok = False

        if not ok and self.update:
            self.update_tests(curr_testdir, curr_outdir)
            ok = True

        if ok:
            print_message("Passed", 'SUCCESS', 'PASSED')

        return ok

    def test_file_output_node(self):
        if not os.path.exists(self.testdir):
            print_message("Test directory '{:s}' does not exist.".format(self.testdir), 'FAILURE', 'FAILED')
            return False
        if not os.listdir(self.testdir):
            print_message("Test directory '{:s}' is empty.".format(self.testdir), 'FAILURE', 'FAILED')
            return False

        if not os.path.exists(self.outdir):
            os.mkdir(self.outdir)

        ok = True
        for filename in os.listdir(self.testdir):
            test_name, ext = os.path.splitext(filename)
            if ext != '.blend':
                continue
            curr_out_dir = os.path.join(self.outdir, test_name)
            curr_test_dir = os.path.join(self.testdir, test_name)
            blendfile = os.path.join(self.testdir, filename)

            # A test may fail because there are too many outputs.
            # So start with a fresh folder on every run to avoid false positives.
            if os.path.exists(curr_out_dir):
                rmtree(curr_out_dir)
            os.mkdir(curr_out_dir)

            print_message("Running test {:s}... ".format(os.path.basename(curr_out_dir)), 'SUCCESS', 'RUN')
            self.run_test_script(blendfile, curr_out_dir)

            if not self.compare_dirs(curr_test_dir, curr_out_dir):
                ok = False

        self.assertTrue(ok)

    def run_test_script(self, blendfile, curr_out_dir):
        def set_directory(node_tree, base_path):
            for node in node_tree.nodes:
                if node.type == 'OUTPUT_FILE':
                    node.directory = f'{curr_out_dir}/'
                    # Can't use empty filename for multilayer, as multiple such
                    # nodes will write to the same file name then.
                    if node.format.media_type != 'MULTI_LAYER_IMAGE':
                        node.file_name = ""
                elif node.type == 'GROUP' and node.node_tree:
                    set_directory(node.node_tree, base_path)

        bpy.ops.wm.open_mainfile(filepath=blendfile)
        # Set output directory for all existing file output nodes.
        set_directory(bpy.data.scenes[0].compositing_node_group, f'{curr_out_dir}/')
        bpy.data.scenes[0].render.compositor_device = f'{self.execution_device}'
        bpy.ops.render.render(animation=True, frame_start=1, frame_end=1)


if __name__ == "__main__":
    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser(
        description="Run test script for each blend file containing a File Output node in TESTDIR, "
        "comparing all render outputs with known outputs."
    )
    parser.add_argument("--testdir", required=True)
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--gpu-backend", required=False)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)
