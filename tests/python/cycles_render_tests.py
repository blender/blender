#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import glob
import os
import pathlib
import shutil
import subprocess
import sys
import time
import tempfile


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
    print("[{}]" . format(status_text), end="")
    print(COLORS.ENDC, end="")
    print(" {}" . format(message))
    sys.stdout.flush()


def render_file(filepath):
    dirname = os.path.dirname(filepath)
    basedir = os.path.dirname(dirname)
    subject = os.path.basename(dirname)
    if subject == 'opengl':
        command = (
            BLENDER,
            "--window-geometry", "0", "0", "1", "1",
            "-noaudio",
            "--factory-startup",
            "--enable-autoexec",
            filepath,
            "-E", "CYCLES",
            # Run with OSL enabled
            # "--python-expr", "import bpy; bpy.context.scene.cycles.shading_system = True",
            "-o", TEMP_FILE_MASK,
            "-F", "PNG",
            '--python', os.path.join(basedir,
                                     "util",
                                     "render_opengl.py")
        )
    else:
        command = (
            BLENDER,
            "--background",
            "-noaudio",
            "--factory-startup",
            "--enable-autoexec",
            filepath,
            "-E", "CYCLES",
            # Run with OSL enabled
            # "--python-expr", "import bpy; bpy.context.scene.cycles.shading_system = True",
            "-o", TEMP_FILE_MASK,
            "-F", "PNG",
            "-f", "1",
            )
    try:
        output = subprocess.check_output(command)
        if VERBOSE:
            print(output.decode("utf-8"))
        return None
    except subprocess.CalledProcessError as e:
        if os.path.exists(TEMP_FILE):
            os.remove(TEMP_FILE)
        if VERBOSE:
            print(e.output.decode("utf-8"))
        if b"Error: engine not found" in e.output:
            return "NO_CYCLES"
        elif b"blender probably wont start" in e.output:
            return "NO_START"
        return "CRASH"
    except BaseException as e:
        if os.path.exists(TEMP_FILE):
            os.remove(TEMP_FILE)
        if VERBOSE:
            print(e)
        return "CRASH"


def test_get_name(filepath):
    filename = os.path.basename(filepath)
    return os.path.splitext(filename)[0]

def test_get_images(filepath):
    testname = test_get_name(filepath)
    dirpath = os.path.dirname(filepath)
    ref_dirpath = os.path.join(dirpath, "reference_renders")
    ref_img = os.path.join(ref_dirpath, testname + ".png")
    new_dirpath = os.path.join(OUTDIR, os.path.basename(dirpath))
    if not os.path.exists(new_dirpath):
        os.makedirs(new_dirpath)
    new_img = os.path.join(new_dirpath, testname + ".png")
    diff_dirpath = os.path.join(OUTDIR, os.path.basename(dirpath), "diff")
    if not os.path.exists(diff_dirpath):
        os.makedirs(diff_dirpath)
    diff_img = os.path.join(diff_dirpath, testname + ".diff.png")
    return ref_img, new_img, diff_img


class Report:
    def __init__(self, testname):
        self.failed_tests = ""
        self.passed_tests = ""
        self.testname = testname

    def output(self):
        # write intermediate data for single test
        outdir = os.path.join(OUTDIR, self.testname)
        f = open(os.path.join(outdir, "failed.data"), "w")
        f.write(self.failed_tests)
        f.close()

        f = open(os.path.join(outdir, "passed.data"), "w")
        f.write(self.passed_tests)
        f.close()

        # gather intermediate data for all tests
        failed_data = sorted(glob.glob(os.path.join(OUTDIR, "*/failed.data")))
        passed_data = sorted(glob.glob(os.path.join(OUTDIR, "*/passed.data")))

        failed_tests = ""
        passed_tests = ""

        for filename in failed_data:
            failed_tests += open(os.path.join(OUTDIR, filename), "r").read()
        for filename in passed_data:
            passed_tests += open(os.path.join(OUTDIR, filename), "r").read()

        # write html for all tests
        self.html = """
<html>
<head>
    <title>Cycles Test Report</title>
    <style>
        img {{ image-rendering: pixelated; width: 256; background-color: #000; }}
        table td:first-child {{ width: 100%; }}
    </style>
    <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css">
</head>
<body>
    <div class="container">
        <br/>
        <h1>Cycles Test Report</h1>
        <br/>
        <table class="table table-striped">
            <thead class="thead-default">
                <tr><th>Name</th><th>New</th><th>Reference</th><th>Diff</th>
            </thead>
            {}{}
        </table>
        <br/>
    </div>
</body>
</html>
            """ . format(failed_tests, passed_tests)

        filepath = os.path.join(OUTDIR, "report.html")
        f = open(filepath, "w")
        f.write(self.html)
        f.close()

        print_message("Report saved to: " + pathlib.Path(filepath).as_uri())

    def add_test(self, filepath, error):
        name = test_get_name(filepath)

        ref_img, new_img, diff_img = test_get_images(filepath)

        status = error if error else ""
        style = """ style="background-color: #f99;" """ if error else ""

        new_url = pathlib.Path(new_img).as_uri()
        ref_url = pathlib.Path(ref_img).as_uri()
        diff_url = pathlib.Path(diff_img).as_uri()

        test_html = """
            <tr{}>
                <td><b>{}</b><br/>{}<br/>{}</td>
                <td><img src="{}" onmouseover="this.src='{}';" onmouseout="this.src='{}';"></td>
                <td><img src="{}" onmouseover="this.src='{}';" onmouseout="this.src='{}';"></td>
                <td><img src="{}"></td>
            </tr>""" . format(style, name, self.testname, status,
                              new_url, ref_url, new_url,
                              ref_url, new_url, ref_url,
                              diff_url)

        if error:
            self.failed_tests += test_html
        else:
            self.passed_tests += test_html


def verify_output(report, filepath):
    ref_img, new_img, diff_img = test_get_images(filepath)
    if not os.path.exists(ref_img):
        return False

    # diff test with threshold
    command = (
        IDIFF,
        "-fail", "0.016",
        "-failpercent", "1",
        ref_img,
        TEMP_FILE,
        )
    try:
        subprocess.check_output(command)
        failed = False
    except subprocess.CalledProcessError as e:
        if VERBOSE:
            print_message(e.output.decode("utf-8"))
        failed = e.returncode != 1

    # generate diff image
    command = (
        IDIFF,
        "-o", diff_img,
        "-abs", "-scale", "16",
        ref_img,
        TEMP_FILE
        )

    try:
        subprocess.check_output(command)
    except subprocess.CalledProcessError as e:
        if VERBOSE:
            print_message(e.output.decode("utf-8"))

    # copy new image
    if os.path.exists(new_img):
        os.remove(new_img)
    if os.path.exists(TEMP_FILE):
        shutil.copy(TEMP_FILE, new_img)

    return not failed


def run_test(report, filepath):
    testname = test_get_name(filepath)
    spacer = "." * (32 - len(testname))
    print_message(testname, 'SUCCESS', 'RUN')
    time_start = time.time()
    error = render_file(filepath)
    status = "FAIL"
    if not error:
        if not verify_output(report, filepath):
            error = "VERIFY"
    time_end = time.time()
    elapsed_ms = int((time_end - time_start) * 1000)
    if not error:
        print_message("{} ({} ms)" . format(testname, elapsed_ms),
                      'SUCCESS', 'OK')
    else:
        if error == "NO_CYCLES":
            print_message("Can't perform tests because Cycles failed to load!")
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



def blend_list(path):
    for dirpath, dirnames, filenames in os.walk(path):
        for filename in filenames:
            if filename.lower().endswith(".blend"):
                filepath = os.path.join(dirpath, filename)
                yield filepath

def run_all_tests(dirpath):
    passed_tests = []
    failed_tests = []
    all_files = list(blend_list(dirpath))
    all_files.sort()
    report = Report(os.path.basename(dirpath))
    print_message("Running {} tests from 1 test case." .
                  format(len(all_files)),
                  'SUCCESS', "==========")
    time_start = time.time()
    for filepath in all_files:
        error = run_test(report, filepath)
        testname = test_get_name(filepath)
        if error:
            if error == "NO_CYCLES":
                return False
            elif error == "NO_START":
                return False
            failed_tests.append(testname)
        else:
            passed_tests.append(testname)
        report.add_test(filepath, error)
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

    report.output()
    return not bool(failed_tests)


def create_argparse():
    parser = argparse.ArgumentParser()
    parser.add_argument("-blender", nargs="+")
    parser.add_argument("-testdir", nargs=1)
    parser.add_argument("-outdir", nargs=1)
    parser.add_argument("-idiff", nargs=1)
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    global COLORS
    global BLENDER, TESTDIR, IDIFF, OUTDIR
    global TEMP_FILE, TEMP_FILE_MASK, TEST_SCRIPT
    global VERBOSE

    if os.environ.get("CYCLESTEST_COLOR") is not None:
        COLORS = COLORS_ANSI

    BLENDER = args.blender[0]
    TESTDIR = args.testdir[0]
    IDIFF = args.idiff[0]
    OUTDIR = args.outdir[0]

    if not os.path.exists(OUTDIR):
        os.makedirs(OUTDIR)

    TEMP = tempfile.mkdtemp()
    TEMP_FILE_MASK = os.path.join(TEMP, "test")
    TEMP_FILE = TEMP_FILE_MASK + "0001.png"

    TEST_SCRIPT = os.path.join(os.path.dirname(__file__), "runtime_check.py")

    VERBOSE = os.environ.get("BLENDER_VERBOSE") is not None

    ok = run_all_tests(TESTDIR)

    # Cleanup temp files and folders
    if os.path.exists(TEMP_FILE):
        os.remove(TEMP_FILE)
    os.rmdir(TEMP)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
