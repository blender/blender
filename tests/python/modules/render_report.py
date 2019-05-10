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

from . import global_report


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


def test_get_images(output_dir, filepath, reference_dir):
    testname = test_get_name(filepath)
    dirpath = os.path.dirname(filepath)

    old_dirpath = os.path.join(dirpath, reference_dir)
    old_img = os.path.join(old_dirpath, testname + ".png")

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
        'reference_dir',
        'idiff',
        'pixelated',
        'verbose',
        'update',
        'failed_tests',
        'passed_tests',
        'compare_tests',
        'compare_engines'
    )

    def __init__(self, title, output_dir, idiff):
        self.title = title
        self.output_dir = output_dir
        self.reference_dir = 'reference_renders'
        self.idiff = idiff
        self.compare_engines = None

        self.pixelated = False
        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self.update = os.getenv('BLENDER_TEST_UPDATE') is not None

        if os.environ.get("BLENDER_TEST_COLOR") is not None:
            global COLORS, COLORS_ANSI
            COLORS = COLORS_ANSI

        self.failed_tests = ""
        self.passed_tests = ""
        self.compare_tests = ""

        os.makedirs(output_dir, exist_ok=True)

    def set_pixelated(self, pixelated):
        self.pixelated = pixelated

    def set_reference_dir(self, reference_dir):
        self.reference_dir = reference_dir

    def set_compare_engines(self, engine, other_engine):
        self.compare_engines = (engine, other_engine)

    def run(self, dirpath, render_cb):
        # Run tests and output report.
        dirname = os.path.basename(dirpath)
        ok = self._run_all_tests(dirname, dirpath, render_cb)
        self._write_data(dirname)
        self._write_html()
        if self.compare_engines:
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

        if self.compare_engines:
            filepath = os.path.join(outdir, "compare.data")
            pathlib.Path(filepath).write_text(self.compare_tests)

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

        failed = len(failed_tests) > 0
        if failed:
            message = "<p>Run <tt>BLENDER_TEST_UPDATE=1 ctest</tt> to create or update reference images for failed tests.</p>"
        else:
            message = ""

        if comparison:
            title = "Render Test Compare"
            columns_html = "<tr><th>Name</th><th>%s</th><th>%s</th>" % self.compare_engines
        else:
            title = self.title
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
                {columns_html}
            </thead>
            {tests_html}
        </table>
        <br/>
    </div>
</body>
</html>
            """ . format(title=title,
                         message=message,
                         image_rendering=image_rendering,
                         tests_html=tests_html,
                         columns_html=columns_html)

        filename = "report.html" if not comparison else "compare.html"
        filepath = os.path.join(self.output_dir, filename)
        pathlib.Path(filepath).write_text(html)

        print_message("Report saved to: " + pathlib.Path(filepath).as_uri())


        # Update global report
        link_name = "Renders" if not comparison else "Comparison"
        global_output_dir = os.path.dirname(self.output_dir)
        global_failed = failed if not comparison else None
        global_report.add(global_output_dir, self.title, link_name, filepath, global_failed)

    def _relative_url(self, filepath):
        relpath = os.path.relpath(filepath, self.output_dir)
        return pathlib.Path(relpath).as_posix()

    def _write_test_html(self, testname, filepath, error):
        name = test_get_name(filepath)
        name = name.replace('_', ' ')

        old_img, ref_img, new_img, diff_img = test_get_images(self.output_dir, filepath, self.reference_dir)

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

        if self.compare_engines:
            ref_url = os.path.join("..", self.compare_engines[1], new_url)

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
        old_img, ref_img, new_img, diff_img = test_get_images(self.output_dir, filepath, self.reference_dir)

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

    def _run_tests(self, filepaths, render_cb):
        # Run all tests together for performance, since Blender
        # startup time is a significant factor.
        tmp_filepaths = []
        for filepath in filepaths:
            testname = test_get_name(filepath)
            print_message(testname, 'SUCCESS', 'RUN')
            tmp_filepaths.append(os.path.join(self.output_dir, "tmp_" + testname))

        run_errors = render_cb(filepaths, tmp_filepaths)
        errors = []

        for error, filepath, tmp_filepath in zip(run_errors, filepaths, tmp_filepaths):
            if not error:
                if os.path.getsize(tmp_filepath) == 0:
                    error = "VERIFY"
                elif not self._diff_output(filepath, tmp_filepath):
                    error = "VERIFY"

            if os.path.exists(tmp_filepath):
                os.remove(tmp_filepath)

            errors.append(error)

            testname = test_get_name(filepath)
            if not error:
                print_message(testname, 'SUCCESS', 'OK')
            else:
                if error == "SKIPPED":
                    print_message("Skipped after previous render caused error")
                elif error == "NO_ENGINE":
                    print_message("Can't perform tests because the render engine failed to load!")
                elif error == "NO_START":
                    print_message('Can not perform tests because blender fails to start.',
                                  'Make sure INSTALL target was run.')
                elif error == 'VERIFY':
                    print_message("Rendered result is different from reference image")
                else:
                    print_message("Unknown error %r" % error)
                print_message(testname, 'FAILURE', 'FAILED')

        return errors

    def _run_all_tests(self, dirname, dirpath, render_cb):
        passed_tests = []
        failed_tests = []
        all_files = list(blend_list(dirpath))
        all_files.sort()
        print_message("Running {} tests from 1 test case." .
                      format(len(all_files)),
                      'SUCCESS', "==========")
        time_start = time.time()
        errors = self._run_tests(all_files, render_cb)
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
