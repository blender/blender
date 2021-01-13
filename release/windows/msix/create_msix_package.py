#!/usr/bin/env python3

import argparse
import os
import pathlib
import requests
import shutil
import subprocess
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument(
    "--version",
    required=True,
    help="Version string in the form of 2.83.3.0",
)
parser.add_argument(
    "--url",
    required=True,
    help="Location of the release ZIP archive to download",
)
parser.add_argument(
    "--publisher",
    required=True,
    help="A string in the form of 'CN=PUBLISHER'",
)
parser.add_argument(
    "--pfx",
    required=False,
    help="Absolute path to the PFX file used for signing the resulting MSIX package",
)
parser.add_argument(
    "--password",
    required=False,
    default="blender",
    help="Password for the PFX file",
)
parser.add_argument(
    "--lts",
    required=False,
    help="If set this MSIX is for an LTS release",
    action='store_const',
    const=1,
)
parser.add_argument(
    "--skipdl",
    required=False,
    help="If set skip downloading of the specified URL as blender.zip. The tool assumes blender.zip exists",
    action='store_const',
    const=1,
)
parser.add_argument(
    "--leavezip",
    required=False,
    help="If set don't clean up the downloaded blender.zip",
    action='store_const',
    const=1,
)
parser.add_argument(
    "--overwrite",
    required=False,
    help="If set remove Content folder if it already exists",
    action='store_const',
    const=1,
)
args = parser.parse_args()


def execute_command(cmd: list, name: str, errcode: int):
    """
    Execute given command in cmd. Output is captured. If an error
    occurs name is used to print ERROR message, along with stderr and
    stdout of the process if either was captured.
    """
    cmd_process = subprocess.run(cmd, capture_output=True, encoding="UTF-8")
    if cmd_process.returncode != 0:
        print(f"ERROR: {name} failed.")
        if cmd_process.stdout:
            print(cmd_process.stdout)
        if cmd_process.stderr:
            print(cmd_process.stderr)
        exit(errcode)


LTSORNOT = ""
PACKAGETYPE = ""
if args.lts:
    versionparts = args.version.split(".")
    LTSORNOT = f" {versionparts[0]}.{versionparts[1]} LTS"
    PACKAGETYPE = f"{versionparts[0]}.{versionparts[1]}LTS"

blender_package_msix = pathlib.Path(".", f"blender-{args.version}-windows64.msix").absolute()
content_folder = pathlib.Path(".", "Content")
content_blender_folder = pathlib.Path(content_folder, "Blender").absolute()
content_assets_folder = pathlib.Path(content_folder, "Assets")
assets_original_folder = pathlib.Path(".", "Assets")

pri_config_file = pathlib.Path(".", "priconfig.xml")
pri_resources_file = pathlib.Path(content_folder, "resources.pri")

local_blender_zip = pathlib.Path(".", "blender.zip")

if args.pfx:
    pfx_path = pathlib.Path(args.pfx)
    if not pfx_path.exists():
        print("ERROR: PFX file not found. Please ensure you give the correct path to the PFX file on the command-line.")
        exit(1)
    print(f"Creating MSIX package with signing using PFX file at {pfx_path}")
else:
    pfx_path = None
    print("Creating MSIX package without signing.")

pri_command = ["makepri",
                   "new",
                   "/pr", f"{content_folder.absolute()}",
                   "/cf", f"{pri_config_file.absolute()}",
                   "/of", f"{pri_resources_file.absolute()}"
                   ]

msix_command = ["makeappx",
                "pack",
                "/h", "SHA256",
                "/d", f"{content_folder.absolute()}",
                "/p", f"{blender_package_msix}"
                ]
if pfx_path:
    sign_command = ["signtool",
                    "sign",
                    "/fd", "sha256",
                    "/a", "/f", f"{pfx_path.absolute()}",
                    "/p", f"{args.password}",
                    f"{blender_package_msix}"
                    ]

if args.overwrite:
    if content_folder.joinpath("Assets").exists():
        shutil.rmtree(content_folder)
content_folder.mkdir(exist_ok=True)
shutil.copytree(assets_original_folder, content_assets_folder)

manifest_text = pathlib.Path("AppxManifest.xml.template").read_text()
manifest_text = manifest_text.replace("[VERSION]", args.version)
manifest_text = manifest_text.replace("[PUBLISHER]", args.publisher)
manifest_text = manifest_text.replace("[LTSORNOT]", LTSORNOT)
manifest_text = manifest_text.replace("[PACKAGETYPE]", PACKAGETYPE)
pathlib.Path(content_folder, "AppxManifest.xml").write_text(manifest_text)

if not args.skipdl:
    print(f"Downloading blender archive {args.url} to {local_blender_zip}...")

    with open(local_blender_zip, "wb") as download_zip:
        response = requests.get(args.url)
        download_zip.write(response.content)

    print("... download complete.")
else:
    print("Skipping download")

print(f"Extracting files from ZIP to {content_blender_folder}...")

# Extract the files from the ZIP archive, but skip the leading part of paths
# in the ZIP. We want to write the files to the content_blender_folder where
# blender.exe ends up as ./Content/Blender/blender.exe, and not
# ./Content/Blender/blender-2.83.3-windows64/blender.exe
with zipfile.ZipFile(local_blender_zip, "r") as blender_zip:
    for entry in blender_zip.infolist():
        if entry.is_dir():
            continue
        entry_location = pathlib.Path(entry.filename)
        target_location = content_blender_folder.joinpath(*entry_location.parts[1:])
        pathlib.Path(target_location.parent).mkdir(parents=True, exist_ok=True)
        extracted_entry = blender_zip.read(entry)
        target_location.write_bytes(extracted_entry)

print("... extraction complete.")


print(f"Generating Package Resource Index (PRI) file using command: {' '.join(pri_command)}")
execute_command(pri_command, "MakePri", 4)

print(f"Creating MSIX package using command: {' '.join(msix_command)}")

# Remove MSIX file if it already exists. Otherwise the MakeAppX tool
# will hang.
if blender_package_msix.exists():
    os.remove(blender_package_msix)
execute_command(msix_command, "MakeAppX", 2)

if args.pfx:
    print(f"Signing MSIX package using command: {' '.join(sign_command)}")
    execute_command(sign_command, "SignTool", 3)

if not args.leavezip:
    os.remove(local_blender_zip)
shutil.rmtree(content_folder)

print("Done.")
