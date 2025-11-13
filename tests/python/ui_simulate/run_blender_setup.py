# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Utility script, called by ``run.py`` or ``blender_headless.py`` to run inside Blender,
to avoid boilerplate code having to be added into each test.
"""

import os
import sys


def create_parser():
    import argparse
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
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
        "--step-command-pre",
        dest="step_command_pre",
        default=None,
        required=False,
        help="See 'run.py'",
    )
    parser.add_argument(
        "--step-command-post",
        dest="step_command_post",
        default=None,
        required=False,
        help="See 'run.py'",
    )

    parser.add_argument(
        "--tests",
        dest="tests",
        nargs='+',
        required=True,
        metavar="TEST_ID",
        help="Names of tests to run.",
    )

    return parser


def main():
    directory = os.path.dirname(__file__)
    sys.path.insert(0, directory)
    if "bpy" not in sys.modules:
        raise Exception("This must run inside Blender")
    import bpy
    import gpu

    parser = create_parser()
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    verbose = os.getenv('BLENDER_VERBOSE') is not None

    # Check if `bpy.app.use_event_simulate` has been enabled by the test itself.
    # When writing tests, it's useful if the test can temporarily be set to keep the window open.

    def on_error():
        if not bpy.app.use_event_simulate:
            args.keep_open = True

        if not args.keep_open:
            sys.exit(1)

    def on_exit():
        if not bpy.app.use_event_simulate:
            args.keep_open = True

        if not args.keep_open:
            sys.exit(0)
        else:
            bpy.app.use_event_simulate = False

    gpu_device = gpu.platform.device_type_get()

    BLOCKLIST = []
    if os.getenv("BLENDER_TEST_IGNORE_BLOCKLIST") is None:
        if sys.platform == "win32" and gpu_device == "INTEL":
            # See #149084 for the tracking issue
            BLOCKLIST = ["test_workspace"]

    is_first = True
    for test_id in args.tests:
        mod_name, fn_name = test_id.partition(".")[0::2]

        if mod_name in BLOCKLIST or test_id in BLOCKLIST:
            if not args.keep_open:
                sys.exit(0)

        if not is_first:
            bpy.ops.wm.read_homefile()
        is_first = False

        mod = __import__(mod_name)
        test_fn = getattr(mod, fn_name)

        from modules import easy_keys

        # So we can get the operator ID's.
        bpy.context.preferences.view.show_developer_ui = True

        # Hack back in operator search.

        easy_keys.setup_default_preferences(bpy.context.preferences)
        easy_keys.run(
            test_fn(),
            on_error=on_error,
            on_exit=on_exit,
            # Optional.
            on_step_command_pre=args.step_command_pre,
            on_step_command_post=args.step_command_post,
        )


if __name__ == "__main__":
    main()
