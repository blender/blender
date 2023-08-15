#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import glob
import pathlib
import subprocess
import tempfile
import urllib.request
import zipfile

parser = argparse.ArgumentParser(description="Check and upload bpy module to PyPI")
parser.add_argument(
    "--version",
    required=True,
    help="Version string in the form of {major}.{minor}.{patch} (e.g. 3.6.0)")
parser.add_argument(
    "--git-hash",
    required=True,
    help="Git hash matching the version")
parser.add_argument(
    "--check",
    action="store_true",
    help="Only check wheels for errors, don't upload")
args = parser.parse_args()

platforms = [
    "darwin.x86_64",
    "darwin.arm64",
    "linux.x86_64",
    "windows.amd64"]

with tempfile.TemporaryDirectory() as tmp_dir:
    tmp_dir = pathlib.Path(tmp_dir)

    print("Download:")
    for platform in platforms:
        # Download from buildbot.
        version = args.version
        version_tokens = version.split(".")
        short_version = version_tokens[0] + version_tokens[1]
        git_hash = args.git_hash

        url = f"https://builder.blender.org/download/daily/bpy-{version}-stable+v{short_version}.{git_hash}-{platform}-release.zip"
        filepath = tmp_dir / f"{platform}.zip"
        print(url)
        urllib.request.urlretrieve(url, filepath)

        # Unzip.
        with zipfile.ZipFile(filepath, "r") as zipf:
            zipf.extractall(path=tmp_dir)
    print("")

    wheels = glob.glob(str(tmp_dir / "*.whl"))
    print("Wheels:")
    print("\n".join(wheels))

    if len(platforms) != len(wheels):
        sys.stderr.write("Unexpected number of whl files.")
        sys.exit(1)
    print("")

    # Check and upload.
    print("Twine:")
    subprocess.run(["twine", "check"] + wheels, check=True)
    if not args.check:
        subprocess.run(["twine", "upload", "--repository", "bpy", "--verbose"] + wheels, check=True)
