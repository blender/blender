# Apache License, Version 2.0
#
# Compare renders or screenshots against reference versions and generate
# a HTML report showing the differences, for regression testing.

import glob
import os
import pathlib
import shutil
import subprocess
import sys
import time


class COLORS_ANSI:
    RED = '\033[00;31m'
    GREEN = '\033[00;32m'
    ENDC = '\033[0m'


class COLORS_DUMMY:
    RED = ''
    GREEN = ''
    ENDC = ''


COLORS = COLORS_DUMMY


def print_message(message, type=None, status=''):
    if type == 'SUCCESS':
        print(COLORS.GREEN, end="")
    elif type == 'FAILURE':
        print(COLORS.RED, end="")
    status_text = ...
    if status == 'RUN':
        status_text = " RUN      "
    elif status == 'OK':
        status_text = "       OK "
    elif status == 'PASSED':
        status_text = "  PASSED  "
    elif status == 'FAILED':
        status_text = "  FAILED  "
    else:
        status_text = status
    if status_text:
        print("[{}]" . format(status_text), end="")
    print(COLORS.ENDC, end="")
    print(" {}" . format(message))
    sys.stdout.flush()


def blend_list(dirpath):
    for root, dirs, files in os.walk(dirpath):
        for filename in files:
            if filename.lower().endswith(".blend"):
                filepath = os.path.join(root, filename)
                yield filepath


def test_get_name(filepath):
    filename = os.path.basename(filepath)
    return os.path.splitext(filename)[0]


def test_get_images(output_dir, filepath):
    testname = test_get_name(filepath)
    dirpath = os.path.dirname(filepath)

    old_dirpath = os.path.join(dirpath, "reference_renders")
    old_img = os.path.join(old_dirpath, testname + ".png")

    ref_dirpath = os.path.join(output_dir, os.path.basename(dirpath), "ref")
    ref_img = os.path.join(ref_dirpath, testname + ".png")
    if not os.path.exists(ref_dirpath):
        os.makedirs(ref_dirpath)
    if os.path.exists(old_img):
        shutil.copy(old_img, ref_img)

    new_dirpath = os.path.join(output_dir, os.path.basename(dirpath))
    if not os.path.exists(new_dirpath):
        os.makedirs(new_dirpath)
    new_img = os.path.join(new_dirpath, testname + ".png")

    diff_dirpath = os.path.join(output_dir, os.path.basename(dirpath), "diff")
    if not os.path.exists(diff_dirpath):
        os.makedirs(diff_dirpath)
    diff_img = os.path.join(diff_dirpath, testname + ".diff.png")

    return old_img, ref_img, new_img, diff_img


class Report:
    __slots__ = (
        'title',
        'output_dir',
        'idiff',
        'pixelated',
        'verbose',
        'update',
        'failed_tests',
        'passed_tests'
    )

    def __init__(self, title, output_dir, idiff):
        self.title = title
        self.output_dir = output_dir
        self.idiff = idiff

        self.pixelated = False
        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self.update = os.getenv('BLENDER_TEST_UPDATE') is not None

        if os.environ.get("BLENDER_TEST_COLOR") is not None:
            global COLORS, COLORS_ANSI
            COLORS = COLORS_ANSI

        self.failed_tests = ""
        self.passed_tests = ""

        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

    def set_pixelated(self, pixelated):
        self.pixelated = pixelated

    def run(self, dirpath, render_cb):
        # Run tests and output report.
        dirname = os.path.basename(dirpath)
        ok = self._run_all_tests(dirname, dirpath, render_cb)
        self._write_html(dirname)
        return ok

    def _write_html(self, dirname):
        # Write intermediate data for single test.
        outdir = os.path.join(self.output_dir, dirname)
        if not os.path.exists(outdir):
            os.makedirs(outdir)

        filepath = os.path.join(outdir, "failed.data")
        pathlib.Path(filepath).write_text(self.failed_tests)

        filepath = os.path.join(outdir, "passed.data")
        pathlib.Path(filepath).write_text(self.passed_tests)

        # Gather intermediate data for all tests.
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

        if len(failed_tests) > 0:
            message = "<p>Run <tt>BLENDER_TEST_UPDATE=1 ctest</tt> to create or update reference images for failed tests.</p>"
        else:
            message = ""

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
            -webkit-background-size:50px 51px; /* override value for shitty webkit */

            background-position:0 0, 25px 0, 25px -25px, 0px 25px;
        }}
        table td:first-child {{ width: 256px; }}
    </style>
    <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css">
</head>
<body>
    <div class="container">
        <br/>
        <h1>{title}</h1>
        {message}
        <br/>
        <table class="table table-striped">
            <thead class="thead-default">
                <tr><th>Name</th><th>New</th><th>Reference</th><th>Diff</th>
            </thead>
            {tests_html}
        </table>
        <br/>
    </div>
</body>
</html>
            """ . format(title=self.title,
                         message=message,
                         image_rendering=image_rendering,
                         tests_html=tests_html)

        filepath = os.path.join(self.output_dir, "report.html")
        pathlib.Path(filepath).write_text(html)

        print_message("Report saved to: " + pathlib.Path(filepath).as_uri())

    def _relative_url(self, filepath):
        relpath = os.path.relpath(filepath, self.output_dir)
        return pathlib.Path(relpath).as_posix()

    def _write_test_html(self, testname, filepath, error):
        name = test_get_name(filepath)
        name = name.replace('_', ' ')

        old_img, ref_img, new_img, diff_img = test_get_images(self.output_dir, filepath)

        status = error if error else ""
        tr_style = """ style="background-color: #f99;" """ if error else ""

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

    def _diff_output(self, filepath, tmp_filepath):
        old_img, ref_img, new_img, diff_img = test_get_images(self.output_dir, filepath)

        # Create reference render directory.
        old_dirpath = os.path.dirname(old_img)
        if not os.path.exists(old_dirpath):
            os.makedirs(old_dirpath)

        # Copy temporary to new image.
        if os.path.exists(new_img):
            os.remove(new_img)
        if os.path.exists(tmp_filepath):
            shutil.copy(tmp_filepath, new_img)

        if os.path.exists(ref_img):
            # Diff images test with threshold.
            command = (
                self.idiff,
                "-fail", "0.016",
                "-failpercent", "1",
                ref_img,
                tmp_filepath,
            )
            try:
                subprocess.check_output(command)
                failed = False
            except subprocess.CalledProcessError as e:
                if self.verbose:
                    print_message(e.output.decode("utf-8"))
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
                print_message(e.output.decode("utf-8"))

        return not failed

    def _run_test(self, filepath, render_cb):
        testname = test_get_name(filepath)
        print_message(testname, 'SUCCESS', 'RUN')
        time_start = time.time()
        tmp_filepath = os.path.join(self.output_dir, "tmp_" + testname)

        error = render_cb(filepath, tmp_filepath)
        status = "FAIL"
        if not error:
            if not self._diff_output(filepath, tmp_filepath):
                error = "VERIFY"

        if os.path.exists(tmp_filepath):
            os.remove(tmp_filepath)

        time_end = time.time()
        elapsed_ms = int((time_end - time_start) * 1000)
        if not error:
            print_message("{} ({} ms)" . format(testname, elapsed_ms),
                          'SUCCESS', 'OK')
        else:
            if error == "NO_ENGINE":
                print_message("Can't perform tests because the render engine failed to load!")
                return error
            elif error == "NO_START":
                print_message('Can not perform tests because blender fails to start.',
                              'Make sure INSTALL target was run.')
                return error
            elif error == 'VERIFY':
                print_message("Rendered result is different from reference image")
            else:
                print_message("Unknown error %r" % error)
            print_message("{} ({} ms)" . format(testname, elapsed_ms),
                          'FAILURE', 'FAILED')
        return error

    def _run_all_tests(self, dirname, dirpath, render_cb):
        passed_tests = []
        failed_tests = []
        all_files = list(blend_list(dirpath))
        all_files.sort()
        print_message("Running {} tests from 1 test case." .
                      format(len(all_files)),
                      'SUCCESS', "==========")
        time_start = time.time()
        for filepath in all_files:
            error = self._run_test(filepath, render_cb)
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
