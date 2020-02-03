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
import sys

from pathlib import Path
from typing import Optional

import codesign.config_builder
import codesign.util as util
from codesign.base_code_signer import BaseCodeSigner


class SimpleCodeSigner:
    code_signer: Optional[BaseCodeSigner]

    def __init__(self):
        platform = util.get_current_platform()
        if platform == util.Platform.LINUX:
            from codesign.linux_code_signer import LinuxCodeSigner
            self.code_signer = LinuxCodeSigner(codesign.config_builder)
        elif platform == util.Platform.MACOS:
            from codesign.macos_code_signer import MacOSCodeSigner
            self.code_signer = MacOSCodeSigner(codesign.config_builder)
        elif platform == util.Platform.WINDOWS:
            from codesign.windows_code_signer import WindowsCodeSigner
            self.code_signer = WindowsCodeSigner(codesign.config_builder)
        else:
            self.code_signer = None

    def sign_file_or_directory(self, path: Path) -> None:
        logging.config.dictConfig(codesign.config_builder.LOGGING)
        self.code_signer.run_buildbot_path_sign_pipeline(path)
