# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import pathlib
import sys


def argparse_create():
    import argparse

    parser = argparse.ArgumentParser(
        prog=pathlib.Path(sys.argv[0]).name + " --command maketx",
        description=(
            "Generate Cycles texture cache (.tx) files.\n"
            "\n"
            "When a file path is given, generate a tx file for that image.\n"
            "When no file path is given, a blend file must be loaded and tx\n"
            "files are generated for all images used by shader node trees."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "filepath",
        type=str,
        nargs="?",
        default=None,
        help="Source image file path. If omitted, generate for all images in the blend file.",
    )

    parser.add_argument(
        "--colorspace",
        dest="colorspace",
        type=str,
        default="auto",
        help="Color space name. Auto-detected if omitted. Only used with a file path.",
    )

    parser.add_argument(
        "--alpha-type",
        dest="alpha_type",
        type=str,
        default="auto",
        choices=["straight", "premultiplied", "channel_packed", "none", "auto"],
        help="Alpha type. Auto-detected if omitted. Only used with a file path.",
    )

    parser.add_argument(
        "--cache-dir",
        dest="cache_dir",
        type=str,
        default="",
        help="Output directory for tx files, relative to source image or absolute.",
    )

    return parser


def maketx_command(argv):
    parser = argparse_create()
    args = parser.parse_args(argv)

    if args.filepath is not None:
        return maketx_file(args)
    else:
        return maketx_blend(args, parser)


def maketx_file(args):
    import _cycles

    filepath = str(pathlib.Path(args.filepath).absolute())

    try:
        out_filepath = _cycles.maketx(
            filepath,
            colorspace=args.colorspace,
            alpha_type=args.alpha_type,
            cache_dir=args.cache_dir,
        )
    except RuntimeError as ex:
        sys.stderr.write("Error: {:s}\n".format(str(ex)))
        return 1

    print(out_filepath)
    return 0


def maketx_blend(args, parser):
    import bpy

    if not bpy.data.filepath:
        parser.error("No image file path given and no blend file loaded.")

    if args.colorspace != "auto":
        parser.error("--colorspace is only used with an image file path argument.")

    if args.alpha_type != "auto":
        parser.error("--alpha-type is only used with an image file path argument.")

    if args.cache_dir:
        bpy.context.preferences.filepaths.texture_cache_directory = args.cache_dir

    bpy.ops.render.generate_texture_cache()
    return 0
