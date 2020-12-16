#!/usr/bin/env python3

import argparse
import pathlib
import requests
import shutil
import subprocess
from typing import Callable, Iterator, List, Tuple

# supported archive and platform endings, used to create actual archive names
archive_endings = ["windows64.zip", "linux64.tar.xz", "macOS.dmg"]


def add_optional_argument(option: str, help: str) -> None:
    global parser
    """Add an optional argument

    Args:
        option (str): Option to add
        help (str): Help description for the argument
    """
    parser.add_argument(option, help=help, action='store_const', const=1)


def blender_archives(version: str) -> Iterator[str]:
    """Generator for Blender archives for version.

    Yields for items in archive_endings an archive name in the form of
    blender-{version}-{ending}.

    Args:
        version (str): Version string of the form 2.83.2


    Yields:
        Iterator[str]: Name in the form of blender-{version}-{ending}
    """
    global archive_endings

    for ending in archive_endings:
        yield f"blender-{version}-{ending}"


def get_archive_type(archive_type: str, version: str) -> str:
    """Return the archive of given type and version.

    Args:
        archive_type (str): extension for archive type to check for
        version (str): Version string in the form 2.83.2

    Raises:
        Exception: Execption when archive type isn't found

    Returns:
        str: archive name for given type
    """

    for archive in blender_archives(version):
        if archive.endswith(archive_type):
            return archive
    raise Exception("Unknown archive type")


def execute_command(cmd: List[str], name: str, errcode: int, cwd=".", capture_output=True) -> str:
    """Execute the given command.

    Returns the process stdout upon success if any.

    On error print message the command with name that has failed. Print stdout
    and stderr of the process if any, and then exit with given error code.

    Args:
        cmd (List[str]): Command in list format, each argument as their own item
        name (str): Name of command to use when printing to command-line
        errcode (int): Error code to use in case of exit()
        cwd (str, optional): Folder to use as current work directory for command
                             execution. Defaults to ".".
        capture_output (bool, optional): Whether to capture command output or not.
                                         Defaults to True.

    Returns:
        str: stdout if any, or empty string
    """
    cmd_process = subprocess.run(
        cmd, capture_output=capture_output, encoding="UTF-8", cwd=cwd)
    if cmd_process.returncode == 0:
        if cmd_process.stdout:
            return cmd_process.stdout
        else:
            return ""
    else:
        print(f"ERROR: {name} failed.")
        if cmd_process.stdout:
            print(cmd_process.stdout)
        if cmd_process.stderr:
            print(cmd_process.stderr)
        exit(errcode)
        return ""


def download_archives(base_url: str, archives: Callable[[str], Iterator[str]], version: str, dst_dir: pathlib.Path):
    """Download archives from the given base_url.

    Archives is a generator for Blender archive names based on version.

    Archive names are appended to the base_url to load from, and appended to
    dst_dir to save to.

    Args:
        base_url (str): Base URL to load archives from
        archives (Callable[[str], Iterator[str]]): Generator for Blender archive
                                                   names based on version
        version (str): Version string in the form of 2.83.2
        dst_dir (pathlib.Path): Download destination
    """

    if base_url[-1] != '/':
        base_url = base_url + '/'

    for archive in archives(version):
        download_url = f"{base_url}{archive}"
        target_file = dst_dir.joinpath(archive)
        download_file(download_url, target_file)


def download_file(from_url: str, to_file: pathlib.Path) -> None:
    """Download from_url as to_file.

    Actual downloading will be skipped if --skipdl is given on the command-line.

    Args:
        from_url (str): Full URL to resource to download
        to_file (pathlib.Path): Full path to save downloaded resource as
    """
    global args

    if not args.skipdl or not to_file.exists():
        print(f"Downloading {from_url}")
        with open(to_file, "wb") as download_zip:
            response = requests.get(from_url)
            if response.status_code != requests.codes.ok:
                print(f"ERROR: failed to download {from_url} (status code: {response.status_code})")
                exit(1313)
            download_zip.write(response.content)
    else:
        print(f"Downloading {from_url} skipped")
    print("   ... OK")


def copy_contents_from_dmg_to_path(dmg_file: pathlib.Path, dst: pathlib.Path) -> None:
    """Copy the contents of the given DMG file to the destination folder.

    Args:
        dmg_file (pathlib.Path): Full path to DMG archive to extract from
        dst (pathlib.Path): Full path to destination to extract to
    """
    hdiutil_attach = ["hdiutil",
                      "attach",
                      "-readonly",
                      f"{dmg_file}"
                      ]
    attached = execute_command(hdiutil_attach, "hdiutil attach", 1)

    # Last line of output is what we want, it is of the form
    # /dev/somedisk    Apple_HFS     /Volumes/Blender
    # We want to retain the mount point, and the folder the mount is
    # created on. The mounted disk we need for detaching, the folder we
    # need to be able to copy the contents to where we can use them
    attachment_items = attached.splitlines()[-1].split()
    mounted_disk = attachment_items[0]
    source_location = pathlib.Path(attachment_items[2], "Blender.app")

    print(f"{source_location} -> {dst}")

    shutil.copytree(source_location, dst)

    hdiutil_detach = ["hdiutil",
                      "detach",
                      f"{mounted_disk}"
                      ]
    execute_command(hdiutil_detach, "hdiutil detach", 2)


def create_build_script(template_name: str, vars: List[Tuple[str, str]]) -> pathlib.Path:
    """
    Create the Steam build script

    Use the given template and template variable tuple list.

    Returns pathlib.Path to the created script.

    Args:
        template_name (str): [description]
        vars (List[Tuple[str, str]]): [description]

    Returns:
        pathlib.Path: Full path to the generated script
    """
    build_script = pathlib.Path(".", template_name).read_text()
    for var in vars:
        build_script = build_script.replace(var[0], var[1])
    build_script_file = template_name.replace(".template", "")
    build_script_path = pathlib.Path(".", build_script_file)
    build_script_path.write_text(build_script)
    return build_script_path


def clean_up() -> None:
    """Remove intermediate files depending on given command-line arguments
    """
    global content_location, args

    if not args.leavearch and not args.leaveextracted:
        shutil.rmtree(content_location)

    if args.leavearch and not args.leaveextracted:
        shutil.rmtree(content_location.joinpath(zip_extract_folder))
        shutil.rmtree(content_location.joinpath(tarxz_extract_folder))
        shutil.rmtree(content_location.joinpath(dmg_extract_folder))

    if args.leaveextracted and not args.leavearch:
        import os
        os.remove(content_location.joinpath(zipped_blender))
        os.remove(content_location.joinpath(tarxz_blender))
        os.remove(content_location.joinpath(dmg_blender))


def extract_archive(archive: str, extract_folder_name: str,
                    cmd: List[str], errcode: int) -> None:
    """Extract all files from archive to given folder name.

    Will not extract if
    target folder already exists, or if --skipextract was given on the
    command-line.

    Args:
        archive (str): Archive name to extract
        extract_folder_name (str): Folder name to extract to
        cmd (List[str]): Command with arguments to use
        errcode (int): Error code to use for exit()
    """
    global args, content_location

    extract_location = content_location.joinpath(extract_folder_name)

    pre_extract = set(content_location.glob("*"))

    if not args.skipextract or not extract_location.exists():
        print(f"Extracting files from {archive}...")
        cmd.append(content_location.joinpath(archive))
        execute_command(cmd, cmd[0], errcode, cwd=content_location)
        # in case we use a non-release archive the naming will be incorrect.
        # simply rename to expected target name
        post_extract = set(content_location.glob("*"))
        diff_extract = post_extract - pre_extract
        if not extract_location in diff_extract:
            folder_to_rename = list(diff_extract)[0]
            folder_to_rename.rename(extract_location)
        print("   OK")
    else:
        print(f"Skipping extraction {archive}!")

# ==============================================================================


parser = argparse.ArgumentParser()

parser.add_argument("--baseurl", required=True,
                    help="The base URL for files to download, "
                    "i.e. https://download.blender.org/release/Blender2.83/")

parser.add_argument("--version", required=True,
                    help="The Blender version to release, in the form 2.83.3")

parser.add_argument("--appid", required=True,
                    help="The Blender App ID on Steam")
parser.add_argument("--winid", required=True,
                    help="The Windows depot ID")
parser.add_argument("--linuxid", required=True,
                    help="The Linux depot ID")
parser.add_argument("--macosid", required=True,
                    help="The MacOS depot ID")

parser.add_argument("--steamcmd", required=True,
                    help="Path to the steamcmd")
parser.add_argument("--steamuser", required=True,
                    help="The login for the Steam builder user")
parser.add_argument("--steampw", required=True,
                    help="Login password for the Steam builder user")

add_optional_argument("--dryrun",
                      "If set the Steam files will not be uploaded")
add_optional_argument("--leavearch",
                      help="If set don't clean up the downloaded archives")
add_optional_argument("--leaveextracted",
                      help="If set don't clean up the extraction folders")
add_optional_argument("--skipdl",
                      help="If set downloading the archives is skipped if it already exists locally.")
add_optional_argument("--skipextract",
                      help="If set skips extracting of archives. The tool assumes the archives"
                      "have already been extracted to their correct locations")

args = parser.parse_args()

VERSIONNODOTS = args.version.replace('.', '')
OUTPUT = f"output{VERSIONNODOTS}"
CONTENT = f"content{VERSIONNODOTS}"

# ===== set up main locations

content_location = pathlib.Path(".", CONTENT).absolute()
output_location = pathlib.Path(".", OUTPUT).absolute()

content_location.mkdir(parents=True, exist_ok=True)
output_location.mkdir(parents=True, exist_ok=True)

# ===== login

# Logging into Steam once to ensure the SDK updates itself properly. If we don't
# do that the combined +login and +run_app_build_http at the end of the tool
# will fail.
steam_login = [args.steamcmd,
               "+login",
               args.steamuser,
               args.steampw,
               "+quit"
               ]
print("Logging in to Steam...")
execute_command(steam_login, "Login to Steam", 10)
print("   OK")

# ===== prepare Steam build scripts

template_vars = [
    ("[APPID]", args.appid),
    ("[OUTPUT]", OUTPUT),
    ("[CONTENT]", CONTENT),
    ("[VERSION]", args.version),
    ("[WINID]", args.winid),
    ("[LINUXID]", args.linuxid),
    ("[MACOSID]", args.macosid),
    ("[DRYRUN]", f"{args.dryrun}" if args.dryrun else "0")
]

blender_app_build = create_build_script(
    "blender_app_build.vdf.template", template_vars)
create_build_script("depot_build_win.vdf.template", template_vars)
create_build_script("depot_build_linux.vdf.template", template_vars)
create_build_script("depot_build_macos.vdf.template", template_vars)

# ===== download archives

download_archives(args.baseurl, blender_archives,
                  args.version, content_location)

# ===== set up file and folder names

zipped_blender = get_archive_type("zip", args.version)
zip_extract_folder = zipped_blender.replace(".zip", "")
tarxz_blender = get_archive_type("tar.xz", args.version)
tarxz_extract_folder = tarxz_blender.replace(".tar.xz", "")
dmg_blender = get_archive_type("dmg", args.version)
dmg_extract_folder = dmg_blender.replace(".dmg", "")

# ===== extract

unzip_cmd = ["unzip", "-q"]
extract_archive(zipped_blender, zip_extract_folder,  unzip_cmd, 3)

untarxz_cmd = ["tar", "-xf"]
extract_archive(tarxz_blender, tarxz_extract_folder, untarxz_cmd, 4)

if not args.skipextract or not content_location.joinpath(dmg_extract_folder).exists():
    print("Extracting files from Blender MacOS archive...")
    blender_dmg = content_location.joinpath(dmg_blender)
    target_location = content_location.joinpath(
        dmg_extract_folder, "Blender.app")
    copy_contents_from_dmg_to_path(blender_dmg, target_location)
    print("   OK")
else:
    print("Skipping extraction of .dmg!")

# ===== building

print("Build Steam game files...")
steam_build = [args.steamcmd,
               "+login",
               args.steamuser,
               args.steampw,
               "+run_app_build_http",
               blender_app_build.absolute(),
               "+quit"
               ]
execute_command(steam_build, "Build with steamcmd", 13)
print("   OK")

clean_up()
