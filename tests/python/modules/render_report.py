# SPDX-License-Identifier: Apache-2.0

"""
Compare renders or screenshots against reference versions and generate
a HTML report showing the differences, for regression testing.
"""

import glob
import os
import pathlib
import shutil
import subprocess
import sys
import time

from . import global_report
from .colored_print import (print_message, use_message_colors)


def blend_list(dirpath, device, blacklist):
    import re

    for root, dirs, files in os.walk(dirpath):
        for filename in files:
            if not filename.lower().endswith(".blend"):
                continue

            skip = False
            for blacklist_entry in blacklist:
                if re.match(blacklist_entry, filename):
                    skip = True
                    break

            if not skip:
                filepath = os.path.join(root, filename)
                yield filepath


def test_get_name(filepath):
    filename = os.path.basename(filepath)
    return os.path.splitext(filename)[0]


def test_get_images(output_dir, filepath, reference_dir, reference_override_dir):
    testname = test_get_name(filepath)
    dirpath = os.path.dirname(filepath)

    old_dirpath = os.path.join(dirpath, reference_dir)
    old_img = os.path.join(old_dirpath, testname + ".png")
    if reference_override_dir:
        override_dirpath = os.path.join(dirpath, reference_override_dir)
        override_img = os.path.join(override_dirpath, testname + ".png")
        if os.path.exists(override_img):
            old_dirpath = override_dirpath
            old_img = override_img

    ref_dirpath = os.path.join(output_dir, os.path.basename(dirpath), "ref")
    ref_img = os.path.join(ref_dirpath, testname + ".png")
    os.makedirs(ref_dirpath, exist_ok=True)
    if os.path.exists(old_img):
        shutil.copy(old_img, ref_img)

    new_dirpath = os.path.join(output_dir, os.path.basename(dirpath))
    os.makedirs(new_dirpath, exist_ok=True)
    new_img = os.path.join(new_dirpath, testname + ".png")

    diff_dirpath = os.path.join(output_dir, os.path.basename(dirpath), "diff")
    os.makedirs(diff_dirpath, exist_ok=True)
    diff_img = os.path.join(diff_dirpath, testname + ".diff.png")

    return old_img, ref_img, new_img, diff_img


class Report:
    __slots__ = (
        'title',
        'output_dir',
        'global_dir',
        'reference_dir',
        'reference_override_dir',
        'idiff',
        'pixelated',
        'fail_threshold',
        'fail_percent',
        'verbose',
        'update',
        'failed_tests',
        'passed_tests',
        'compare_tests',
        'compare_engine',
        'device',
        'blacklist',
    )

    def __init__(self, title, output_dir, idiff, device=None, blacklist=[]):
        self.title = title
        self.output_dir = output_dir
        self.global_dir = os.path.dirname(output_dir)
        self.reference_dir = 'reference_renders'
        self.reference_override_dir = None
        self.idiff = idiff
        self.compare_engine = None
        self.fail_threshold = 0.016
        self.fail_percent = 1
        self.device = device
        self.blacklist = blacklist

        if device:
            self.title = self._engine_title(title, device)
            self.output_dir = self._engine_path(self.output_dir, device.lower())

        self.pixelated = False
        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self.update = os.getenv('BLENDER_TEST_UPDATE') is not None

        if os.environ.get("BLENDER_TEST_COLOR") is not None:
            use_message_colors()

        self.failed_tests = ""
        self.passed_tests = ""
        self.compare_tests = ""

        os.makedirs(output_dir, exist_ok=True)

    def set_pixelated(self, pixelated):
        self.pixelated = pixelated

    def set_fail_threshold(self, threshold):
        self.fail_threshold = threshold

    def set_fail_percent(self, percent):
        self.fail_percent = percent

    def set_reference_dir(self, reference_dir):
        self.reference_dir = reference_dir

    def set_reference_override_dir(self, reference_override_dir):
        self.reference_override_dir = reference_override_dir

    def set_compare_engine(self, other_engine, other_device=None):
        self.compare_engine = (other_engine, other_device)

    def run(self, dirpath, blender, arguments_cb, batch=False):
        # Run tests and output report.
        dirname = os.path.basename(dirpath)
        ok = self._run_all_tests(dirname, dirpath, blender, arguments_cb, batch)
        self._write_data(dirname)
        self._write_html()
        if self.compare_engine:
            self._write_html(comparison=True)
        return ok

    def _write_data(self, dirname):
        # Write intermediate data for single test.
        outdir = os.path.join(self.output_dir, dirname)
        os.makedirs(outdir, exist_ok=True)

        filepath = os.path.join(outdir, "failed.data")
        pathlib.Path(filepath).write_text(self.failed_tests)

        filepath = os.path.join(outdir, "passed.data")
        pathlib.Path(filepath).write_text(self.passed_tests)

        if self.compare_engine:
            filepath = os.path.join(outdir, "compare.data")
            pathlib.Path(filepath).write_text(self.compare_tests)

    def _navigation_item(self, title, href, active):
        if active:
            return """<li class="breadcrumb-item active" aria-current="page">%s</li>""" % title
        else:
            return """<li class="breadcrumb-item"><a href="%s">%s</a></li>""" % (href, title)

    def _engine_title(self, engine, device):
        if device:
            return engine.title() + ' ' + device
        else:
            return engine.title()

    def _engine_path(self, path, device):
        if device:
            return os.path.join(path, device.lower())
        else:
            return path

    def _navigation_html(self, comparison):
        html = """<nav aria-label="breadcrumb"><ol class="breadcrumb">"""
        base_path = os.path.relpath(self.global_dir, self.output_dir)
        global_report_path = os.path.join(base_path, "report.html")
        html += self._navigation_item("Test Reports", global_report_path, False)
        html += self._navigation_item(self.title, "report.html", not comparison)
        if self.compare_engine:
            compare_title = "Compare with %s" % self._engine_title(*self.compare_engine)
            html += self._navigation_item(compare_title, "compare.html", comparison)
        html += """</ol></nav>"""

        return html

    def _write_html(self, comparison=False):
        # Gather intermediate data for all tests.
        if comparison:
            failed_data = []
            passed_data = sorted(glob.glob(os.path.join(self.output_dir, "*/compare.data")))
        else:
            failed_data = sorted(glob.glob(os.path.join(self.output_dir, "*/failed.data")))
            passed_data = sorted(glob.glob(os.path.join(self.output_dir, "*/passed.data")))

        failed_tests = ""
        passed_tests = ""

        for filename in failed_data:
            filepath = os.path.join(self.output_dir, filename)
            failed_tests += pathlib.Path(filepath).read_text()
        for filename in passed_data:
            filepath = os.path.join(self.output_dir, filename)
            passed_tests += pathlib.Path(filepath).read_text()

        tests_html = failed_tests + passed_tests

        # Write html for all tests.
        if self.pixelated:
            image_rendering = 'pixelated'
        else:
            image_rendering = 'auto'

        # Navigation
        menu = self._navigation_html(comparison)

        failed = len(failed_tests) > 0
        if failed:
            message = """<div class="alert alert-danger" role="alert">"""
            message += """<p>Run this command to regenerate reference (ground truth) images:</p>"""
            message += """<p><tt>BLENDER_TEST_UPDATE=1 ctest -R %s</tt></p>""" % self.title.lower()
            message += """<p>This then happens for new and failing tests; reference images of """ \
                       """passing test cases will not be updated. Be sure to commit the new reference """ \
                       """images to the SVN repository afterwards.</p>"""
            message += """</div>"""
        else:
            message = ""

        if comparison:
            title = self.title + " Test Compare"
            engine_self = self.title
            engine_other = self._engine_title(*self.compare_engine)
            columns_html = "<tr><th>Name</th><th>%s</th><th>%s</th>" % (engine_self, engine_other)
        else:
            title = self.title + " Test Report"
            columns_html = "<tr><th>Name</th><th>New</th><th>Reference</th><th>Diff</th>"

        html = """
<html>
<head>
    <title>{title}</title>
    <style>
        img {{ image-rendering: {image_rendering}; width: 256px; background-color: #000; }}
        img.render {{
            background-color: #fff;
            background-image:
              -moz-linear-gradient(45deg, #eee 25%, transparent 25%),
              -moz-linear-gradient(-45deg, #eee 25%, transparent 25%),
              -moz-linear-gradient(45deg, transparent 75%, #eee 75%),
              -moz-linear-gradient(-45deg, transparent 75%, #eee 75%);
            background-image:
              -webkit-gradient(linear, 0 100%, 100% 0, color-stop(.25, #eee), color-stop(.25, transparent)),
              -webkit-gradient(linear, 0 0, 100% 100%, color-stop(.25, #eee), color-stop(.25, transparent)),
              -webkit-gradient(linear, 0 100%, 100% 0, color-stop(.75, transparent), color-stop(.75, #eee)),
              -webkit-gradient(linear, 0 0, 100% 100%, color-stop(.75, transparent), color-stop(.75, #eee));

            -moz-background-size:50px 50px;
            background-size:50px 50px;
            -webkit-background-size:50px 51px; /* Override value for silly webkit. */

            background-position:0 0, 25px 0, 25px -25px, 0px 25px;
        }}
        table td:first-child {{ width: 256px; }}
        p {{ margin-bottom: 0.5rem; }}
    </style>
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css" integrity="sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T" crossorigin="anonymous">
</head>
<body>
    <div class="container">
        <br/>
        <h1>{title}</h1>
        {menu}
        {message}
        <table class="table table-striped">
            <thead class="thead-dark">
                {columns_html}
            </thead>
            {tests_html}
        </table>
        <br/>
    </div>
</body>
</html>
            """ . format(title=title,
                         menu=menu,
                         message=message,
                         image_rendering=image_rendering,
                         tests_html=tests_html,
                         columns_html=columns_html)

        filename = "report.html" if not comparison else "compare.html"
        filepath = os.path.join(self.output_dir, filename)
        pathlib.Path(filepath).write_text(html)

        print_message("Report saved to: " + pathlib.Path(filepath).as_uri())

        # Update global report
        if not comparison:
            global_failed = failed if not comparison else None
            global_report.add(self.global_dir, "Render", self.title, filepath, global_failed)

    def _relative_url(self, filepath):
        relpath = os.path.relpath(filepath, self.output_dir)
        return pathlib.Path(relpath).as_posix()

    def _write_test_html(self, testname, filepath, error):
        name = test_get_name(filepath)
        name = name.replace('_', ' ')

        old_img, ref_img, new_img, diff_img = test_get_images(
            self.output_dir, filepath, self.reference_dir, self.reference_override_dir)

        status = error if error else ""
        tr_style = """ class="table-danger" """ if error else ""

        new_url = self._relative_url(new_img)
        ref_url = self._relative_url(ref_img)
        diff_url = self._relative_url(diff_img)

        test_html = """
            <tr{tr_style}>
                <td><b>{name}</b><br/>{testname}<br/>{status}</td>
                <td><img src="{new_url}" onmouseover="this.src='{ref_url}';" onmouseout="this.src='{new_url}';" class="render"></td>
                <td><img src="{ref_url}" onmouseover="this.src='{new_url}';" onmouseout="this.src='{ref_url}';" class="render"></td>
                <td><img src="{diff_url}"></td>
            </tr>""" . format(tr_style=tr_style,
                              name=name,
                              testname=testname,
                              status=status,
                              new_url=new_url,
                              ref_url=ref_url,
                              diff_url=diff_url)

        if error:
            self.failed_tests += test_html
        else:
            self.passed_tests += test_html

        if self.compare_engine:
            base_path = os.path.relpath(self.global_dir, self.output_dir)
            ref_url = os.path.join(base_path, self._engine_path(*self.compare_engine), new_url)

            test_html = """
                <tr{tr_style}>
                    <td><b>{name}</b><br/>{testname}<br/>{status}</td>
                    <td><img src="{new_url}" onmouseover="this.src='{ref_url}';" onmouseout="this.src='{new_url}';" class="render"></td>
                    <td><img src="{ref_url}" onmouseover="this.src='{new_url}';" onmouseout="this.src='{ref_url}';" class="render"></td>
                </tr>""" . format(tr_style=tr_style,
                                  name=name,
                                  testname=testname,
                                  status=status,
                                  new_url=new_url,
                                  ref_url=ref_url)

            self.compare_tests += test_html

    def _diff_output(self, filepath, tmp_filepath):
        old_img, ref_img, new_img, diff_img = test_get_images(
            self.output_dir, filepath, self.reference_dir, self.reference_override_dir)

        # Create reference render directory.
        old_dirpath = os.path.dirname(old_img)
        os.makedirs(old_dirpath, exist_ok=True)

        # Copy temporary to new image.
        if os.path.exists(new_img):
            os.remove(new_img)
        if os.path.exists(tmp_filepath):
            shutil.copy(tmp_filepath, new_img)

        if os.path.exists(ref_img):
            # Diff images test with threshold.
            command = (
                self.idiff,
                "-fail", str(self.fail_threshold),
                "-failpercent", str(self.fail_percent),
                ref_img,
                tmp_filepath,
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
            shutil.copy(new_img, ref_img)
            shutil.copy(new_img, old_img)
            failed = False

        # Generate diff image.
        command = (
            self.idiff,
            "-o", diff_img,
            "-abs", "-scale", "16",
            ref_img,
            tmp_filepath
        )

        try:
            subprocess.check_output(command)
        except subprocess.CalledProcessError as e:
            if self.verbose:
                print_message(e.output.decode("utf-8", 'ignore'))

        return not failed

    def _run_tests(self, filepaths, blender, arguments_cb, batch):
        # Run multiple tests in a single Blender process since startup can be
        # a significant factor. In case of crashes, re-run the remaining tests.
        verbose = os.environ.get("BLENDER_VERBOSE") is not None

        remaining_filepaths = filepaths[:]
        errors = []

        while len(remaining_filepaths) > 0:
            command = [blender]
            output_filepaths = []

            # Construct output filepaths and command to run
            for filepath in remaining_filepaths:
                testname = test_get_name(filepath)
                print_message(testname, 'SUCCESS', 'RUN')

                base_output_filepath = os.path.join(self.output_dir, "tmp_" + testname)
                output_filepath = base_output_filepath + '0001.png'
                output_filepaths.append(output_filepath)

                if os.path.exists(output_filepath):
                    os.remove(output_filepath)

                command.extend(arguments_cb(filepath, base_output_filepath))

                # Only chain multiple commands for batch
                if not batch:
                    break

            if self.device:
                command.extend(['--', '--cycles-device', self.device])

            # Run process
            crash = False
            output = None
            try:
                completed_process = subprocess.run(command, stdout=subprocess.PIPE)
                if completed_process.returncode != 0:
                    crash = True
                output = completed_process.stdout
            except BaseException as e:
                crash = True

            if verbose:
                print(" ".join(command))
            if (verbose or crash) and output:
                print(output.decode("utf-8", 'ignore'))

            # Detect missing filepaths and consider those errors
            for filepath, output_filepath in zip(remaining_filepaths[:], output_filepaths):
                remaining_filepaths.pop(0)

                if crash:
                    # In case of crash, stop after missing files and re-render remaining
                    if not os.path.exists(output_filepath):
                        errors.append("CRASH")
                        print_message("Crash running Blender")
                        print_message(testname, 'FAILURE', 'FAILED')
                        break

                testname = test_get_name(filepath)

                if not os.path.exists(output_filepath) or os.path.getsize(output_filepath) == 0:
                    errors.append("NO OUTPUT")
                    print_message("No render result file found")
                    print_message(testname, 'FAILURE', 'FAILED')
                elif not self._diff_output(filepath, output_filepath):
                    errors.append("VERIFY")
                    print_message("Render result is different from reference image")
                    print_message(testname, 'FAILURE', 'FAILED')
                else:
                    errors.append(None)
                    print_message(testname, 'SUCCESS', 'OK')

                if os.path.exists(output_filepath):
                    os.remove(output_filepath)

        return errors

    def _run_all_tests(self, dirname, dirpath, blender, arguments_cb, batch):
        passed_tests = []
        failed_tests = []
        all_files = list(blend_list(dirpath, self.device, self.blacklist))
        all_files.sort()
        print_message("Running {} tests from 1 test case." .
                      format(len(all_files)),
                      'SUCCESS', "==========")
        time_start = time.time()
        errors = self._run_tests(all_files, blender, arguments_cb, batch)
        for filepath, error in zip(all_files, errors):
            testname = test_get_name(filepath)
            if error:
                if error == "NO_ENGINE":
                    return False
                elif error == "NO_START":
                    return False
                failed_tests.append(testname)
            else:
                passed_tests.append(testname)
            self._write_test_html(dirname, filepath, error)
        time_end = time.time()
        elapsed_ms = int((time_end - time_start) * 1000)
        print_message("")
        print_message("{} tests from 1 test case ran. ({} ms total)" .
                      format(len(all_files), elapsed_ms),
                      'SUCCESS', "==========")
        print_message("{} tests." .
                      format(len(passed_tests)),
                      'SUCCESS', 'PASSED')
        if failed_tests:
            print_message("{} tests, listed below:" .
                          format(len(failed_tests)),
                          'FAILURE', 'FAILED')
            failed_tests.sort()
            for test in failed_tests:
                print_message("{}" . format(test), 'FAILURE', "FAILED")

        return not bool(failed_tests)
