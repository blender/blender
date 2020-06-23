#!/usr/bin/env python3

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
This is a helper script to generate Blender Python API documentation (using Sphinx), and update server data using rsync.

You'll need to specify your user login and password, obviously.

Example usage:

   ./sphinx_doc_update.py --jobs 16 --mirror ../../../docs/remote_api_backup/ --source ../.. --blender ../../../build_cmake/bin/blender --user foobar --password barfoo

"""

import os
import shutil
import subprocess
import sys
import tempfile
import zipfile


DEFAULT_RSYNC_SERVER = "docs.blender.org"
DEFAULT_RSYNC_ROOT = "/api/"
DEFAULT_SYMLINK_ROOT = "/data/www/vhosts/docs.blender.org/api"


def argparse_create():
    import argparse
    global __doc__

    # When --help or no args are given, print this help
    usage_text = __doc__

    parser = argparse.ArgumentParser(description=usage_text,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument(
        "--mirror", dest="mirror_dir",
        metavar='PATH', required=True,
        help="Path to local rsync mirror of api doc server")
    parser.add_argument(
        "--source", dest="source_dir",
        metavar='PATH', required=True,
        help="Path to Blender git repository")
    parser.add_argument(
        "--blender", dest="blender",
        metavar='PATH', required=True,
        help="Path to Blender executable")
    parser.add_argument(
        "--rsync-server", dest="rsync_server", default=DEFAULT_RSYNC_SERVER,
        metavar='RSYNCSERVER', type=str, required=False,
        help=("rsync server address"))
    parser.add_argument(
        "--rsync-root", dest="rsync_root", default=DEFAULT_RSYNC_ROOT,
        metavar='RSYNCROOT', type=str, required=False,
        help=("Root path of API doc on rsync server"))
    parser.add_argument(
        "--user", dest="user",
        metavar='USER', type=str, required=True,
        help=("User to login on rsync server"))
    parser.add_argument(
        "--password", dest="password",
        metavar='PASSWORD', type=str, required=True,
        help=("Password to login on rsync server"))
    parser.add_argument(
        "--jobs", dest="jobs_nr",
        metavar='NR', type=int, required=False, default=1,
        help="Number of sphinx building jobs to launch in parallel")

    return parser


def main():
    # ----------
    # Parse Args

    args = argparse_create().parse_args()

    rsync_base = "rsync://%s@%s:%s" % (args.user, args.rsync_server, args.rsync_root)

    blenver = api_blenver = api_blenver_zip = ""
    api_name = ""
    branch = ""
    is_release = is_beta = False

    # I) Update local mirror using rsync.
    rsync_mirror_cmd = ("rsync", "--delete-after", "-avzz", rsync_base, args.mirror_dir)
    subprocess.run(rsync_mirror_cmd, env=dict(os.environ, RSYNC_PASSWORD=args.password))

    with tempfile.TemporaryDirectory() as tmp_dir:
        # II) Generate doc source in temp dir.
        doc_gen_cmd = (
            args.blender, "--background", "-noaudio", "--factory-startup", "--python-exit-code", "1",
            "--python", "%s/doc/python_api/sphinx_doc_gen.py" % args.source_dir, "--",
            "--output", tmp_dir
        )
        subprocess.run(doc_gen_cmd)

        # III) Get Blender version info.
        getver_file = os.path.join(tmp_dir, "blendver.txt")
        getver_script = (r"""import sys, bpy
with open(sys.argv[-1], 'w') as f:
    is_release = bpy.app.version_cycle in {'rc', 'release'}
    is_beta = bpy.app.version_cycle in {'beta'}
    branch = bpy.app.build_branch.split()[0].decode()
    f.write('%d\n' % is_release)
    f.write('%d\n' % is_beta)
    f.write('%s\n' % branch)
    f.write('%d.%d\n' % (bpy.app.version[0], bpy.app.version[1]))
    f.write('%d.%d\n' % (bpy.app.version[0], bpy.app.version[1])
            if (is_release or is_beta) else '%s\n' % branch)
    f.write('%d_%d' % (bpy.app.version[0], bpy.app.version[1]))
""")
        get_ver_cmd = (args.blender, "--background", "-noaudio", "--factory-startup", "--python-exit-code", "1",
                       "--python-expr", getver_script, "--", getver_file)
        subprocess.run(get_ver_cmd)
        with open(getver_file) as f:
            is_release, is_beta, branch, blenver, api_blenver, api_blenver_zip = f.read().split("\n")
            is_release = bool(int(is_release))
            is_beta = bool(int(is_beta))
        os.remove(getver_file)

        # IV) Build doc.
        curr_dir = os.getcwd()
        os.chdir(tmp_dir)
        sphinx_cmd = ("sphinx-build", "-j", str(args.jobs_nr), "-b", "html", "sphinx-in", "sphinx-out")
        subprocess.run(sphinx_cmd)
        shutil.rmtree(os.path.join("sphinx-out", ".doctrees"))
        os.chdir(curr_dir)

        # V) Cleanup existing matching dir in server mirror (if any), and copy new doc.
        api_name = api_blenver
        api_dir = os.path.join(args.mirror_dir, api_name)
        if os.path.exists(api_dir):
            if os.path.islink(api_dir):
                os.remove(api_dir)
            else:
                shutil.rmtree(api_dir)
        os.rename(os.path.join(tmp_dir, "sphinx-out"), api_dir)

    # VI) Create zip archive.
    zip_name = "blender_python_reference_%s" % api_blenver_zip  # We can't use 'release' postfix here...
    zip_path = os.path.join(args.mirror_dir, zip_name)
    with zipfile.ZipFile(zip_path, 'w') as zf:
        for dirname, _, filenames in os.walk(api_dir):
            for filename in filenames:
                filepath = os.path.join(dirname, filename)
                zip_filepath = os.path.join(zip_name, os.path.relpath(filepath, api_dir))
                zf.write(filepath, arcname=zip_filepath)
    os.rename(zip_path, os.path.join(api_dir, "%s.zip" % zip_name))

    # VII) Create symlinks and html redirects.
    if is_release:
        symlink = os.path.join(args.mirror_dir, "current")
        if os.path.exists(symlink):
            if os.path.islink(symlink):
                os.remove(symlink)
            else:
                shutil.rmtree(symlink)
        os.symlink("./%s" % api_name, symlink)
        with open(os.path.join(args.mirror_dir, "250PythonDoc/index.html"), 'w') as f:
            f.write("<html><head><title>Redirecting...</title><meta http-equiv=\"REFRESH\""
                    "content=\"0;url=../%s/\"></head><body>Redirecting...</body></html>" % api_name)
    elif is_beta:
        # We do not have any particular symlink for that stage.
        pass
    elif branch == "master":
        # Also create a symlink from version number to actual master api doc.
        symlink = os.path.join(args.mirror_dir, blenver)
        if os.path.exists(symlink):
            if os.path.islink(symlink):
                os.remove(symlink)
            else:
                shutil.rmtree(symlink)
        os.symlink("./%s" % api_name, symlink)
        with open(os.path.join(args.mirror_dir, "blender_python_api/index.html"), 'w') as f:
            f.write("<html><head><title>Redirecting...</title><meta http-equiv=\"REFRESH\""
                    "content=\"0;url=../%s/\"></head><body>Redirecting...</body></html>" % api_name)

    # VIII) Upload (first do a dry-run so user can ensure everything is OK).
    print("Doc generated in local mirror %s, please check it before uploading "
          "(hit [Enter] to continue, [Ctrl-C] to exit):" % api_dir)
    sys.stdin.read(1)

    rsync_mirror_cmd = ("rsync", "--dry-run", "--delete-after", "-avzz", args.mirror_dir, rsync_base)
    subprocess.run(rsync_mirror_cmd, env=dict(os.environ, RSYNC_PASSWORD=args.password))

    print("Rsync upload simulated, please check every thing is OK (hit [Enter] to continue, [Ctrl-C] to exit):")
    sys.stdin.read(1)

    rsync_mirror_cmd = ("rsync", "--delete-after", "-avzz", args.mirror_dir, rsync_base)
    subprocess.run(rsync_mirror_cmd, env=dict(os.environ, RSYNC_PASSWORD=args.password))


if __name__ == "__main__":
    main()
