# SPDX-License-Identifier: GPL-2.0-or-later

import os
import pathlib
import shutil
import subprocess
import unittest

from .colored_print import (print_message, use_message_colors)


class AbstractImBufTest(unittest.TestCase):
    @classmethod
    def init(cls, args):
        cls.test_dir = pathlib.Path(args.test_dir)
        cls.reference_dir = pathlib.Path(args.test_dir).joinpath("reference")
        cls.reference_load_dir = pathlib.Path(args.test_dir).joinpath("reference_load")
        cls.output_dir = pathlib.Path(args.output_dir)
        cls.diff_dir = pathlib.Path(args.output_dir).joinpath("diff")
        cls.idiff = pathlib.Path(args.idiff)
        cls.optional_formats = args.optional_formats

        os.makedirs(cls.diff_dir, exist_ok=True)

        cls.errors = 0
        cls.fail_threshold = 0.016
        cls.fail_percent = 1
        cls.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        cls.update = os.getenv('BLENDER_TEST_UPDATE') is not None
        if os.environ.get("BLENDER_TEST_COLOR") is not None:
            use_message_colors()

    def setUp(self):
        self.errors = 0
        print_message("")

    def tearDown(self):
        if self.errors > 0:
            self.fail("{} errors encountered" . format(self.errors))

    def skip_if_format_missing(self, format):
        if self.optional_formats.find(format) < 0:
            self.skipTest("format not available")

    def call_idiff(self, ref_path, out_path):
        ref_filepath = str(ref_path)
        out_filepath = str(out_path)
        out_name = out_path.name
        if os.path.exists(ref_filepath):
            # Diff images test with threshold.
            command = (
                str(self.idiff),
                "-fail", str(self.fail_threshold),
                "-failpercent", str(self.fail_percent),
                ref_filepath,
                out_filepath,
            )
            try:
                subprocess.check_output(command)
                failed = False
            except subprocess.CalledProcessError as e:
                if self.verbose:
                    print_message(e.output.decode("utf-8", 'ignore'))
                failed = e.returncode != 1
        else:
            if not self.update:
                return False

            failed = True

        if failed and self.update:
            # Update reference image if requested.
            shutil.copy(out_filepath, ref_filepath)
            failed = False

        # Generate diff image (set fail thresholds high to reduce output spam).
        diff_img = str(self.diff_dir.joinpath(out_name + ".diff.png"))
        command = (
            str(self.idiff),
            "-fail", "1",
            "-failpercent", "100",
            "-abs", "-scale", "16",
            "-o", diff_img,
            ref_filepath,
            out_filepath
        )

        try:
            subprocess.check_output(command)
        except subprocess.CalledProcessError as e:
            if self.verbose:
                print_message(e.output.decode("utf-8", 'ignore'))

        return not failed
