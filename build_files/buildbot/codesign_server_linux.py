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

# NOTE: This is a no-op signer (since there isn't really a procedure to sign
# Linux binaries yet). Used to debug and verify the code signing routines on
# a Linux environment.

import logging.config
from pathlib import Path
from typing import List

from codesign.linux_code_signer import LinuxCodeSigner
import codesign.config_server

if __name__ == "__main__":
    logging.config.dictConfig(codesign.config_server.LOGGING)
    code_signer = LinuxCodeSigner(codesign.config_server)
    code_signer.run_signing_server()
