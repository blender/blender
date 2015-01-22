#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import os
import subprocess
import sys
import tempfile


def render_file(filepath):
    command = (
        BLENDER,
        "--background",
        "-noaudio",
        "--factory-startup",
        filepath,
        "-E", "CYCLES",
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
    except:
        if os.path.exists(TEMP_FILE):
            os.remove(TEMP_FILE)
        if VERBOSE:
            print(e.output.decode("utf-8"))
        return "CRASH"


def test_get_name(filepath):
    filename = os.path.basename(filepath)
    return os.path.splitext(filename)[0]


def verify_output(filepath):
    testname = test_get_name(filepath)
    dirpath = os.path.dirname(filepath)
    reference_dirpath = os.path.join(dirpath, "reference_renders")
    reference_image = os.path.join(reference_dirpath, testname + ".png")
    if not os.path.exists(reference_image):
        return False
    command = (
        IDIFF,
        "-fail", "0.01",
        "-failpercent", "1",
        reference_image,
        TEMP_FILE,
        )
    try:
        subprocess.check_output(command)
        return True
    except subprocess.CalledProcessError as grepexc:
        return grepexc.returncode == 1


def run_test(filepath):
    testname = test_get_name(filepath)
    spacer = "." * (32 - len(testname))
    print(testname, spacer, end="")
    sys.stdout.flush()
    error = render_file(filepath)
    if not error:
        if verify_output(filepath):
            print("PASS")
        else:
            error = "VERIFY"
    if error:
        print("FAIL", error)
    return error


def blend_list(path):
    for dirpath, dirnames, filenames in os.walk(path):
        for filename in filenames:
            if filename.lower().endswith(".blend"):
                filepath = os.path.join(dirpath, filename)
                yield filepath


def run_all_tests(dirpath):
    failed_tests = []
    all_files = list(blend_list(dirpath))
    all_files.sort()
    for filepath in all_files:
        error = run_test(filepath)
        if error:
            if error == "NO_CYCLES":
                print("Can't perform tests because Cycles failed to load!")
                return False
            elif error == "NO_START":
                print('Can not perform tests because blender fails to start.',
                      'Make sure INSTALL target was run.')
                return False
            else:
                print("Unknown error %r" % error)
            testname = test_get_name(filepath)
            failed_tests.append(testname)
    if failed_tests:
        failed_tests.sort()
        print("\n\nFAILED tests:")
        for test in failed_tests:
            print("   ", test)
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

    global BLENDER, ROOT, IDIFF
    global TEMP_FILE, TEMP_FILE_MASK, TEST_SCRIPT
    global VERBOSE

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
