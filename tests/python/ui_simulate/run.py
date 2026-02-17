#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Run interaction tests using event simulation.

Example usage from Blender's source dir:

This uses ``test_undo.py``, running the ``text_editor_simple`` function.

To run all tests:

   ./tests/python/ui_simulate/run.py --blender=blender.bin --tests '*'

For an editor to follow the tests:

   ./tests/python/ui_simulate/run.py --blender=blender.bin --tests '*' \
       --step-command-pre='gvim --remote-silent +{line} "{file}"'

"""

import os
import sys
import tempfile


def create_parser():
    import argparse
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--blender",
        dest="blender",
        required=True,
        metavar="BLENDER_COMMAND",
        help="Location of the blender command to run (when quoted, may include arguments).",
    )

    parser.add_argument(
        "--tests",
        dest="tests",
        nargs='+',
        required=True,
        metavar="TEST_ID",
        help="Names of tests to run, use '*' to run all tests.",
    )

    parser.add_argument(
        "--jobs", "-j",
        dest="jobs",
        default=1,
        type=int,
        help="Number of tests (and instances of Blender) to run in parallel.",
    )

    parser.add_argument(
        "--keep-open",
        dest="keep_open",
        default=False,
        action='store_true',
        required=False,
        help="Keep the Blender window open after running the test.",
    )

    parser.add_argument(
        "--list-tests",
        dest="list_tests",
        default=False,
        action='store_true',
        required=False,
        help="Show a list of available TEST_ID.",
    )

    parser.add_argument(
        "--step-command-pre",
        dest="step_command_pre",
        required=False,
        metavar="STEP_COMMAND_PRE",
        help=(
            "Command to run that takes the test file and line as arguments. "
            "Literals {file} and {line} will be replaced with the file and line."
            "Called for every event."
            "Called for every event, allows an editor to track which commands run."
        )
    )
    parser.add_argument(
        "--step-command-post",
        dest="step_command_post",
        required=False,
        metavar="STEP_COMMAND_POST",
        help=(
            "Command to run that takes the test file and line as arguments. "
            "Literals {file} and {line} will be replaced with the file and line."
            "Called for every event, allows an editor to track which commands run."
        )
    )

    return parser


def all_test_ids(directory):
    from types import FunctionType
    for f in sorted(os.listdir(directory)):
        if f.startswith("test_") and f.endswith(".py"):
            mod = __import__(f[:-3])
            for k, v in sorted(vars(mod).items()):
                if not k.startswith("_") and isinstance(v, FunctionType):
                    yield f.rpartition(".")[0] + "." + k


def list_tests(directory):
    for test_id in all_test_ids(directory):
        print(test_id)
    sys.exit(0)


def _process_test_id_fn(env, args, test_id):
    import subprocess
    import shlex

    directory = os.path.dirname(__file__)
    cmd = (
        *shlex.split(args.blender),
        "--enable-event-simulate",
        "--factory-startup",
        "--python", os.path.join(directory, "run_blender_setup.py"),
        "--",
        "--tests", test_id,
        *(("--keep-open",) if args.keep_open else ()),
        *(("--step-command-pre", args.step_command_pre) if args.step_command_pre else ()),
        *(("--step-command-post", args.step_command_post) if args.step_command_post else ()),
    )
    callproc = subprocess.run(cmd, env=env)
    return test_id, callproc.returncode == 0


def run(empty_user_dir):
    directory = os.path.dirname(__file__)
    if "--list-tests" in sys.argv:
        list_tests(directory)
        sys.exit(0)

    if "bpy" in sys.modules:
        raise Exception("Cannot run inside Blender")

    parser = create_parser()
    args = parser.parse_args()

    tests = args.tests

    # Validate tests exist
    test_ids = list(all_test_ids(directory))
    if tests[0] == "*":
        tests = test_ids
    else:
        for test_id in tests:
            if test_id not in test_ids:
                print(test_id, "not found in", test_ids)
                return

    env = os.environ.copy()
    env.update({
        "LSAN_OPTIONS": "exitcode=0",
        "BLENDER_USER_RESOURCES": empty_user_dir,
    })

    # We could support multiple tests per Blender session.
    results = []
    results_fail = 0
    if args.jobs <= 1:
        for test_id in tests:
            _, success = _process_test_id_fn(env, args, test_id)
            results.append((test_id, success))
            if not success:
                results_fail += 1
    else:
        from concurrent.futures import ProcessPoolExecutor
        executor = ProcessPoolExecutor(max_workers=args.jobs)
        num_tests = len(tests)
        for test_id, success in executor.map(_process_test_id_fn, (env,) * num_tests, (args,) * num_tests, tests):
            results.append((test_id, success))
            if not success:
                results_fail += 1

    print(len(results), "tests,", results_fail, "failed")
    for test_id, ok in results:
        print("OK:  " if ok else "FAIL:", test_id)

    return 0


def main():
    with tempfile.TemporaryDirectory() as empty_user_dir:
        sys.exit(run(empty_user_dir))


if __name__ == "__main__":
    main()
