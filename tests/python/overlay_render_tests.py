#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import sys
from pathlib import Path

from modules import render_report


class OverlayReport(render_report.Report):
    def __init__(
        self,
        title: str,
        output_dir: Path,
        oiiotool: Path,
        variation: str | None = None,
        blocklist: list[str] = [],
    ) -> None:
        super().__init__(title, output_dir, oiiotool, variation=variation, blocklist=blocklist)
        self.gpu_backend = variation

    def _get_render_arguments(
        self,
        arguments_cb: render_report.ArgumentsCallback,
        filepath: Path,
        base_output_filepath: Path,
    ) -> list[str | Path]:
        return arguments_cb(filepath, base_output_filepath, gpu_backend=self.gpu_backend)


def get_arguments(
    filepath: Path,
    output_filepath: Path,
    gpu_backend: str | None,
) -> list[str | Path]:
    arguments: list[str | Path] = [
        "--no-window-focus",
        "--window-geometry",
        "0", "0", "128", "128",
        "-noaudio",
        "--factory-startup",
        "--no-native-pixels",
        "--enable-autoexec",
        "--debug-memory",
        "--console-crash-handler",
        "--debug-exit-on-error"]

    if gpu_backend:
        arguments.extend(["--gpu-backend", gpu_backend, "--debug-gpu-backend-no-fallback"])

    # Windows separators get messed up when passing them inside the python expression
    output_filepath_posix = output_filepath.as_posix()

    script_dir = Path(__file__).resolve().parent / "overlay"
    script_filepath = script_dir / (filepath.stem + ".py")

    arguments.extend([
        filepath,
        "--python-expr",
        f'import bpy; bpy.context.scene.render.filepath = "{output_filepath_posix}"',
        "-P",
        script_filepath])

    return arguments


def create_argparse():
    parser = argparse.ArgumentParser(
        description="Run test script for each blend file in TESTDIR, comparing the render result with known output."
    )
    parser.add_argument("--blender", required=True, type=Path)
    parser.add_argument("--testdir", required=True, type=Path)
    parser.add_argument("--outdir", required=True, type=Path)
    parser.add_argument("--oiiotool", required=True, type=Path)
    parser.add_argument('--batch', default=False, action='store_true')
    parser.add_argument('--gpu-backend')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    report = OverlayReport("Overlay", args.outdir, args.oiiotool, variation=args.gpu_backend)
    if args.gpu_backend == "vulkan":
        report.set_compare_engine('overlay', 'opengl')
    else:
        report.set_compare_engine('workbench', 'opengl')
    report.set_pixelated(True)
    report.set_reference_dir("overlay_renders")

    gpu_vendor = render_report.get_gpu_device_vendor(args.blender, args.gpu_backend)

    if gpu_vendor == 'INTEL':
        # Intel shows larger differences in Point Primitive coordinates,
        # affecting the coverage of FaceDots and similar overlays.
        # This means reference images should not be rendered on Intel.
        report.set_fail_threshold(0.05)
    elif gpu_vendor == "AMD" and args.gpu_backend == "opengl":
        report.set_fail_threshold(0.22)
        report.set_fail_percent(2.0)
    else:
        report.set_fail_threshold(0.02)

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
