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

import argparse
import os
import subprocess
import sys

class Builder:
    def __init__(self, name, branch):
        self.name = name
        self.branch = branch

        # Buildbot runs from build/ directory
        self.blender_dir = os.path.abspath(os.path.join('..', 'blender.git'))
        self.build_dir = os.path.abspath(os.path.join('..', 'build', name))
        self.install_dir = os.path.abspath(os.path.join('..', 'install', name))
        self.upload_dir = os.path.abspath(os.path.join('..', 'install'))

        # Detect platform
        if name.startswith('mac'):
            self.platform = 'mac'
            self.command_prefix =  []
        elif name.startswith('linux'):
            self.platform = 'linux'
            self.command_prefix =  ['scl', 'enable', 'devtoolset-6', '--']
        elif name.startswith('win'):
            self.platform = 'win'
            self.command_prefix =  []
        else:
            raise ValueError('Unkonw platform for builder ' + self.platform)

        # Always 64 bit now
        self.bits = 64

def create_builder_from_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument('builder_name')
    parser.add_argument('branch', default='master', nargs='?')
    args = parser.parse_args()
    return Builder(args.builder_name, args.branch)


class VersionInfo:
    def __init__(self, builder):
        # Get version information
        buildinfo_h = os.path.join(builder.build_dir, "source", "creator", "buildinfo.h")
        blender_h = os.path.join(builder.blender_dir, "source", "blender", "blenkernel", "BKE_blender_version.h")

        version_number = int(self._parse_header_file(blender_h, 'BLENDER_VERSION'))
        self.version = "%d.%d" % (version_number // 100, version_number % 100)
        self.hash = self._parse_header_file(buildinfo_h, 'BUILD_HASH')[1:-1]

    def _parse_header_file(self, filename, define):
        import re
        regex = re.compile("^#\s*define\s+%s\s+(.*)" % define)
        with open(filename, "r") as file:
            for l in file:
                match = regex.match(l)
                if match:
                    return match.group(1)
        return None


def call(cmd, env=None, exit_on_error=True):
    print(' '.join(cmd))

    # Flush to ensure correct order output on Windows.
    sys.stdout.flush()
    sys.stderr.flush()

    retcode = subprocess.call(cmd, env=env)
    if exit_on_error and retcode != 0:
        sys.exit(retcode)
    return retcode
