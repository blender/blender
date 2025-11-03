# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

"""
Compare renders or screenshots against reference versions and generate
a HTML report showing the differences, for regression testing.
"""

import glob
import os
import sys
import pathlib
import shutil
import subprocess
import time
import multiprocessing

from . import global_report
from .colored_print import (print_message, use_message_colors)


def blend_list(dirpath, blocklist):
    import re

    for root, dirs, files in os.walk(dirpath):
        for filename in files:
            if not filename.lower().endswith(".blend"):
                continue

            skip = False
            for blocklist_entry in blocklist:
                if re.match(blocklist_entry, filename):
                    skip = True
                    break

            if not skip:
                filepath = os.path.join(root, filename)
                yield filepath


def test_get_name(filepath):
    filename = os.path.basename(filepath)
    return os.path.splitext(filename)[0]


def test_get_images(output_dir, filepath, testname, reference_dir, reference_override_dir):
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
    diff_color_img = os.path.join(diff_dirpath, testname + ".diff_color.png")
    diff_alpha_img = os.path.join(diff_dirpath, testname + ".diff_alpha.png")

    return old_img, ref_img, new_img, diff_color_img, diff_alpha_img


class TestResult:
    def __init__(self, report, filepath, name):
        self.filepath = filepath
        self.name = name
        self.error = None
        self.tmp_out_img_base = os.path.join(report.output_dir, "tmp_" + name)
        self.tmp_out_img = self.tmp_out_img_base + '0001.png'
        self.old_img, self.ref_img, self.new_img, self.diff_color_img, self.diff_alpha_img = test_get_images(
            report.output_dir, filepath, name, report.reference_dir, report.reference_override_dir)


def diff_output(test, oiiotool, fail_threshold, fail_percent, verbose, update):
    # Create reference render directory.
    old_dirpath = os.path.dirname(test.old_img)
    os.makedirs(old_dirpath, exist_ok=True)

    # Copy temporary to new image.
    if os.path.exists(test.new_img):
        os.remove(test.new_img)
    if os.path.exists(test.tmp_out_img):
        shutil.copy(test.tmp_out_img, test.new_img)

    if os.path.exists(test.ref_img):
        # Diff images test with threshold.
        command = (
            oiiotool,
            test.ref_img,
            test.tmp_out_img,
            "--fail", str(fail_threshold),
            "--failpercent", str(fail_percent),
            "--diff",
        )
        try:
            subprocess.check_output(command)
            failed = False
        except subprocess.CalledProcessError as e:
            if verbose:
                print_message(e.output.decode("utf-8", 'ignore'))
            failed = e.returncode != 0
    else:
        if not update:
            test.error = "VERIFY"
            return test

        failed = True

    if failed and update:
        # Update reference image if requested.
        shutil.copy(test.new_img, test.ref_img)
        shutil.copy(test.new_img, test.old_img)
        failed = False

    # Generate color diff image.
    command = (
        oiiotool,
        test.ref_img,
        "--ch", "R,G,B",
        test.tmp_out_img,
        "--ch", "R,G,B",
        "--sub",
        "--abs",
        "--mulc", "16",
        "-o", test.diff_color_img,
    )
    try:
        subprocess.check_output(command, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        if verbose:
            print_message(e.output.decode("utf-8", 'ignore'))

    # Generate alpha diff image.
    command = (
        oiiotool,
        test.ref_img,
        "--ch", "A",
        test.tmp_out_img,
        "--ch", "A",
        "--sub",
        "--abs",
        "--mulc", "16",
        "-o", test.diff_alpha_img,
    )
    try:
        subprocess.check_output(command, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        if verbose:
            msg = e.output.decode("utf-8", 'ignore')
            for line in msg.splitlines():
                # Ignore warnings for images without alpha channel.
                if "--ch: Unknown channel name" not in line:
                    print_message(line)

    if failed:
        test.error = "VERIFY"
    else:
        test.error = None

    return test


def get_gpu_device_vendor(blender):
    command = [
        blender,
        "--background",
        "--factory-startup",
        "--python",
        str(pathlib.Path(__file__).parent / "gpu_info.py")
    ]
    try:
        completed_process = subprocess.run(command, stdout=subprocess.PIPE, universal_newlines=True)
        for line in completed_process.stdout.splitlines():
            if line.startswith("GPU_DEVICE_TYPE:"):
                vendor = line.split(':')[1].upper()
                return vendor
    except Exception:
        return None
    return None


class Report:
    __slots__ = (
        'title',
        'engine_name',
        'output_dir',
        'global_dir',
        'reference_dir',
        'reference_override_dir',
        'oiiotool',
        'pixelated',
        'fail_threshold',
        'fail_percent',
        'verbose',
        'update',
        'failed_tests',
        'passed_tests',
        'compare_tests',
        'compare_engine',
        'blocklist',
    )

    def __init__(self, title, output_dir, oiiotool, variation=None, blocklist=[]):
        self.title = title

        # Normalize the path to avoid output_dir and global_dir being the same when a directory
        # ends with a trailing slash.
        self.output_dir = os.path.normpath(output_dir)
        self.global_dir = os.path.dirname(self.output_dir)

        self.reference_dir = 'reference_renders'
        self.reference_override_dir = None
        self.oiiotool = oiiotool
        self.compare_engine = None
        self.fail_threshold = 0.016
        self.fail_percent = 1
        self.engine_name = self.title.lower().replace(" ", "_")
        self.blocklist = [] if os.getenv('BLENDER_TEST_IGNORE_BLOCKLIST') is not None else blocklist

        if variation:
            self.title = self._engine_title(title, variation)
            self.output_dir = self._engine_path(self.output_dir, variation.lower())

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

    def set_compare_engine(self, other_engine, other_variation=None):
        self.compare_engine = (other_engine, other_variation)

    def set_engine_name(self, engine_name):
        self.engine_name = engine_name

    def run(self, dirpath, blender, arguments_cb, batch=False, fail_silently=False):
        # Run tests and output report.
        dirname = os.path.basename(dirpath)
        ok = self._run_all_tests(dirname, dirpath, blender, arguments_cb, batch, fail_silently)
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

    def _engine_title(self, engine, variation):
        if variation:
            return engine.title() + ' ' + variation
        else:
            return engine.title()

    def _engine_path(self, path, variation):
        if variation:
            variation = variation.replace(' ', '_')
            return os.path.join(path, variation.lower())
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
            message += """<p><tt>BLENDER_TEST_UPDATE=1 ctest -R %s</tt></p>""" % self.engine_name
            message += """<p>This then happens for new and failing tests; reference images of """ \
                       """passing test cases will not be updated. Be sure to commit the new reference """ \
                       """images under the tests/files folder afterwards.</p>"""
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
            columns_html = "<tr><th>Name</th><th>New</th><th>Reference</th><th>Diff Color</th><th>Diff Alpha</th>"

        html = f"""
<html>
<head>
    <title>{title}</title>
    <style>
        div.page_container {{
          text-align: center;
        }}
        div.page_container div {{
          text-align: left;
        }}
        div.page_content {{
          display: inline-block;
        }}
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
    <div class="page_container"><div class="page_content">
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
    </div></div>
</body>
</html>
            """

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

    def _write_test_html(self, test_category, test_result):
        name = test_result.name.replace('_', ' ')

        status = test_result.error if test_result.error else ""
        tr_style = """ class="table-danger" """ if test_result.error else ""

        new_url = self._relative_url(test_result.new_img)
        ref_url = self._relative_url(test_result.ref_img)
        diff_color_url = self._relative_url(test_result.diff_color_img)
        diff_alpha_url = self._relative_url(test_result.diff_alpha_img)

        test_html = f"""
            <tr{tr_style}>
                <td><b>{name}</b><br/>{test_category}<br/>{status}</td>
                <td><img src="{new_url}" onmouseover="this.src='{ref_url}';" onmouseout="this.src='{new_url}';" class="render"></td>
                <td><img src="{ref_url}" onmouseover="this.src='{new_url}';" onmouseout="this.src='{ref_url}';" class="render"></td>
                <td><img src="{diff_color_url}"></td>
                <td><img src="{diff_alpha_url}"></td>
            </tr>"""

        if test_result.error:
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
                                  testname=test_result.name,
                                  status=status,
                                  new_url=new_url,
                                  ref_url=ref_url)

            self.compare_tests += test_html

    def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
        # Each render test can override this method to provide extra functionality.
        # See Cycles render tests for an example.
        # Do not delete.
        return arguments_cb(filepath, base_output_filepath)

    def _get_arguments_suffix(self):
        # Get command line arguments that need to be provided after all file-specific ones.
        # For example the Cycles render device argument needs to be added at the end of
        # the argument list, otherwise tests can't be batched together.
        #
        # Each render test is supposed to override this method.
        return []

    def _get_filepath_tests(self, filepath):
        list_filepath = filepath.replace('.blend', '_permutations.txt')
        if os.path.exists(list_filepath):
            with open(list_filepath, 'r') as file:
                return [TestResult(self, filepath, testname.rstrip('\n')) for testname in file]
        else:
            testname = test_get_name(filepath)
            return [TestResult(self, filepath, testname)]

    def _run_tests(self, filepaths, blender, arguments_cb, batch):
        # Run multiple tests in a single Blender process since startup can be
        # a significant factor. In case of crashes, re-run the remaining tests.
        verbose = os.environ.get("BLENDER_VERBOSE") is not None

        remaining_filepaths = filepaths[:]
        test_results = []
        arguments_suffix = self._get_arguments_suffix()

        while len(remaining_filepaths) > 0:
            command = [blender]
            running_tests = []

            # On Windows, there is a maximum length of 32,767 characters (including the terminating null character)
            # for process command line commands, see:
            # https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
            command_line_length = len(blender)
            for suffix in arguments_suffix:
                # Add 3 for taking into account spaces and quotation marks potentially added by Python.
                command_line_length += len(suffix) + 3

            # Construct output filepaths and command to run
            for filepath in remaining_filepaths:
                testname = test_get_name(filepath)

                base_output_filepath = os.path.join(self.output_dir, "tmp_" + testname)
                command_filepath = self._get_render_arguments(arguments_cb, filepath, base_output_filepath)

                # Check if we have surpassed the command line limit.
                for cmd in command_filepath:
                    command_line_length += len(cmd) + 3
                if sys.platform == 'win32' and command_line_length > 32766 and len(running_tests) > 0:
                    break

                print_message(testname, 'SUCCESS', 'RUN')
                running_tests.append(filepath)
                command.extend(command_filepath)

                output_filepath = base_output_filepath + '0001.png'
                if os.path.exists(output_filepath):
                    os.remove(output_filepath)

                # Only chain multiple commands for batch
                if not batch:
                    break

            command.extend(arguments_suffix)

            # Run process
            crash = False
            output = None
            try:
                completed_process = subprocess.run(command, stdout=subprocess.PIPE)
                if completed_process.returncode != 0:
                    crash = True
                output = completed_process.stdout
            except Exception:
                crash = True

            if verbose:
                def quote_expr_args(cmd):
                    quoted = []
                    quote_next = False
                    for arg in cmd:
                        if quote_next:
                            quoted.append('"{}"'.format(arg))  # wrap the expression in quotes
                            quote_next = False
                        else:
                            quoted.append(arg)
                            if arg == "--python-expr":
                                quote_next = True
                    return quoted
                print(' '.join(quote_expr_args(command)))

            if (verbose or crash) and output:
                print(output.decode("utf-8", 'ignore'))

            tests_to_check = []

            # Detect missing filepaths and consider those errors
            for filepath in running_tests:
                remaining_filepaths.pop(0)
                file_crashed = False
                for test in self._get_filepath_tests(filepath):
                    self.postprocess_test(blender, test)
                    if not os.path.exists(test.tmp_out_img) or os.path.getsize(test.tmp_out_img) == 0:
                        if crash:
                            # In case of crash, stop after missing files and re-render remaining
                            test.error = "CRASH"
                            test_results.append(test)
                            file_crashed = True
                            break
                        else:
                            test.error = "NO OUTPUT"
                            test_results.append(test)
                    else:
                        tests_to_check.append(test)
                if file_crashed:
                    break

            pool = multiprocessing.Pool(multiprocessing.cpu_count())
            test_results.extend(pool.starmap(diff_output,
                                             [(test, self.oiiotool, self.fail_threshold, self.fail_percent, self.verbose, self.update)
                                              for test in tests_to_check]))
            pool.close()

        for test in test_results:
            if test.error == "CRASH":
                print_message("Crash running Blender")
                print_message(test.name, 'FAILURE', 'FAILED')
            elif test.error == "NO OUTPUT":
                print_message("No render result file found")
                print_message(test.tmp_out_img, 'FAILURE', 'FAILED')
            elif test.error == "VERIFY":
                print_message("Render result is different from reference image")
                print_message(test.name, 'FAILURE', 'FAILED')
            else:
                print_message(test.name, 'SUCCESS', 'OK')

            if os.path.exists(test.tmp_out_img):
                os.remove(test.tmp_out_img)

        return test_results

    def postprocess_test(self, blender, test):
        """
        Post-process test result after the Blender has run.
        For example, this function is where conversion from video to a still image suitable for image diffing.
        """

        pass

    def _run_all_tests(self, dirname, dirpath, blender, arguments_cb, batch, fail_silently):
        passed_tests = []
        failed_tests = []
        silently_failed_tests = []
        all_files = list(blend_list(dirpath, self.blocklist))
        all_files.sort()
        if not list(blend_list(dirpath, [])):
            print_message("No .blend files found in '{}'!".format(dirpath), 'FAILURE', 'FAILED')
            return False

        print_message("Running {} tests from 1 test case." .
                      format(len(all_files)),
                      'SUCCESS', "==========")
        time_start = time.time()
        test_results = self._run_tests(all_files, blender, arguments_cb, batch)
        for test in test_results:
            if test.error:
                if test.error == "NO_ENGINE":
                    return False
                elif test.error == "NO_START":
                    return False

                if fail_silently and test.error != 'CRASH':
                    silently_failed_tests.append(test.name)
                else:
                    failed_tests.append(test.name)
            else:
                passed_tests.append(test.name)
            self._write_test_html(dirname, test)
        time_end = time.time()
        elapsed_ms = int((time_end - time_start) * 1000)
        print_message("")
        print_message("{} tests from 1 test case ran. ({} ms total)" .
                      format(len(all_files), elapsed_ms),
                      'SUCCESS', "==========")
        print_message("{} tests." .
                      format(len(passed_tests)),
                      'SUCCESS', 'PASSED')
        all_failed_tests = silently_failed_tests + failed_tests
        if all_failed_tests:
            print_message("{} tests, listed below:" .
                          format(len(all_failed_tests)),
                          'FAILURE', 'FAILED')
            all_failed_tests.sort()
            for test in all_failed_tests:
                print_message("{}" . format(test), 'FAILURE', "FAILED")

        return not bool(failed_tests)
