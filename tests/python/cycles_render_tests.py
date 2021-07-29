#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import os
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


def printMessage(type, status, message):
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


def verify_output(filepath):
    testname = test_get_name(filepath)
    dirpath = os.path.dirname(filepath)
    reference_dirpath = os.path.join(dirpath, "reference_renders")
    reference_image = os.path.join(reference_dirpath, testname + ".png")
    failed_image = os.path.join(reference_dirpath, testname + ".fail.png")
    if not os.path.exists(reference_image):
        return False
    command = (
        IDIFF,
        "-fail", "0.015",
        "-failpercent", "1",
        reference_image,
        TEMP_FILE,
        )
    try:
        subprocess.check_output(command)
        failed = False
    except subprocess.CalledProcessError as e:
        if VERBOSE:
            print(e.output.decode("utf-8"))
        failed = e.returncode != 1
    if failed:
        shutil.copy(TEMP_FILE, failed_image)
    elif os.path.exists(failed_image):
        os.remove(failed_image)
    return not failed


def run_test(filepath):
    testname = test_get_name(filepath)
    spacer = "." * (32 - len(testname))
    printMessage('SUCCESS', 'RUN', testname)
    time_start = time.time()
    error = render_file(filepath)
    status = "FAIL"
    if not error:
        if not verify_output(filepath):
            error = "VERIFY"
    time_end = time.time()
    elapsed_ms = int((time_end - time_start) * 1000)
    if not error:
        printMessage('SUCCESS', 'OK', "{} ({} ms)" .
                     format(testname, elapsed_ms))
    else:
        if error == "NO_CYCLES":
            print("Can't perform tests because Cycles failed to load!")
            return False
        elif error == "NO_START":
            print('Can not perform tests because blender fails to start.',
                  'Make sure INSTALL target was run.')
            return False
        elif error == 'VERIFY':
            print("Rendered result is different from reference image")
        else:
            print("Unknown error %r" % error)
        printMessage('FAILURE', 'FAILED', "{} ({} ms)" .
                     format(testname, elapsed_ms))
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
    printMessage('SUCCESS', "==========",
                 "Running {} tests from 1 test case." . format(len(all_files)))
    time_start = time.time()
    for filepath in all_files:
        error = run_test(filepath)
        testname = test_get_name(filepath)
        if error:
            if error == "NO_CYCLES":
                return False
            elif error == "NO_START":
                return False
            failed_tests.append(testname)
        else:
            passed_tests.append(testname)
    time_end = time.time()
    elapsed_ms = int((time_end - time_start) * 1000)
    print("")
    printMessage('SUCCESS', "==========",
                 "{} tests from 1 test case ran. ({} ms total)" .
                 format(len(all_files), elapsed_ms))
    printMessage('SUCCESS', 'PASSED', "{} tests." .
                 format(len(passed_tests)))
    if failed_tests:
        printMessage('FAILURE', 'FAILED', "{} tests, listed below:" .
                     format(len(failed_tests)))
        failed_tests.sort()
        for test in failed_tests:
            printMessage('FAILURE', "FAILED", "{}" . format(test))
        return False
    return True


def create_argparse():
    parser = argparse.ArgumentParser()
    parser.add_argument("-blender", nargs="+")
    parser.add_argument("-testdir", nargs=1)
    parser.add_argument("-idiff", nargs=1)
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    global COLORS
    global BLENDER, ROOT, IDIFF
    global TEMP_FILE, TEMP_FILE_MASK, TEST_SCRIPT
    global VERBOSE

    if os.environ.get("CYCLESTEST_COLOR") is not None:
        COLORS = COLORS_ANSI

    BLENDER = args.blender[0]
    ROOT = args.testdir[0]
    IDIFF = args.idiff[0]

    TEMP = tempfile.mkdtemp()
    TEMP_FILE_MASK = os.path.join(TEMP, "test")
    TEMP_FILE = TEMP_FILE_MASK + "0001.png"

    TEST_SCRIPT = os.path.join(os.path.dirname(__file__), "runtime_check.py")

    VERBOSE = os.environ.get("BLENDER_VERBOSE") is not None

    ok = run_all_tests(ROOT)

    # Cleanup temp files and folders
    if os.path.exists(TEMP_FILE):
        os.remove(TEMP_FILE)
    os.rmdir(TEMP)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
