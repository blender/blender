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

# Helper script which takes care of signing provided location.
#
# The location can either be a directory (in which case all eligible binaries
# will be signed) or a single file (in which case a single file will be signed).
#
# This script takes care of all the complexity of communicating between process
# which requests file to be signed and the code signing server.
#
# NOTE: Signing happens in-place.

import argparse
import sys

from pathlib import Path

from codesign.simple_code_signer import SimpleCodeSigner


def create_argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('path_to_sign', type=Path)
    return parser


def main():
    parser = create_argument_parser()
    args = parser.parse_args()
    path_to_sign = args.path_to_sign.absolute()

    if sys.platform == 'win32':
        # When WIX packed is used to generate .msi on Windows the CPack will
        # install two different projects and install them to different
        # installation prefix:
        #
        # - C:\b\build\_CPack_Packages\WIX\Blender
        # - C:\b\build\_CPack_Packages\WIX\Unspecified
        #
        # Annoying part is: CMake's post-install script will only be run
        # once, with the install prefix which corresponds to a project which
        # was installed last. But we want to sign binaries from all projects.
        # So in order to do so we detect that we are running for a CPack's
        # project used for WIX and force parent directory (which includes both
        # projects) to be signed.
        #
        # Here we force both projects to be signed.
        if path_to_sign.name == 'Unspecified' and 'WIX' in str(path_to_sign):
            path_to_sign = path_to_sign.parent

    code_signer = SimpleCodeSigner()
    code_signer.sign_file_or_directory(path_to_sign)


if __name__ == "__main__":
    main()
