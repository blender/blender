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

import logging.config
from pathlib import Path
from typing import List

from codesign.macos_code_signer import MacOSCodeSigner
import codesign.config_server

if __name__ == "__main__":
    entitlements_file = codesign.config_server.MACOS_ENTITLEMENTS_FILE
    if not entitlements_file.exists():
        raise SystemExit(
            'Entitlements file {entitlements_file} does not exist.')
    if not entitlements_file.is_file():
        raise SystemExit(
            'Entitlements file {entitlements_file} is not a file.')

    logging.config.dictConfig(codesign.config_server.LOGGING)
    code_signer = MacOSCodeSigner(codesign.config_server)
    code_signer.run_signing_server()
