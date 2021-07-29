# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

bl_info = {
    "name": "Blend File Utils",
    "author": "Campbell Barton and Sybren A. Stüvel",
    "version": (1, 1, 7),
    "blender": (2, 76, 0),
    "location": "File > External Data > Blend Utils",
    "description": "Utility for packing blend files",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Import-Export/BlendFile_Utils",
    "support": 'OFFICIAL',
    "category": "Import-Export",
}

BAM_WHEEL_PATH = 'blender_bam-unpacked.whl'

import logging

import bpy
from bpy.types import Operator
from bpy_extras.io_utils import ExportHelper

from .bl_utils.subprocess_helper import SubprocessHelper


class ExportBlendPack(Operator, ExportHelper, SubprocessHelper):
    """Packs a blend file and all its dependencies into an archive for easy redistribution"""
    bl_idname = "export_blend.pack"
    bl_label = "Pack Blend to Archive"
    log = logging.getLogger('%s.ExportBlendPack' % __name__)

    # ExportHelper
    filename_ext = ".zip"

    # SubprocessHelper
    report_interval = 0.25

    temp_dir = None

    @classmethod
    def poll(cls, context):
        return bpy.data.is_saved

    def process_pre(self):
        import tempfile

        self.temp_dir = tempfile.TemporaryDirectory()

        self.environ = {'PYTHONPATH': pythonpath()}
        self.outfname = bpy.path.ensure_ext(self.filepath, ".zip")
        self.command = (
            bpy.app.binary_path_python,
            '-m', 'bam.pack',
            # file to pack
            "--input", bpy.data.filepath,
            # file to write
            "--output", self.outfname,
            "--temp", self.temp_dir.name,
        )

        if self.log.isEnabledFor(logging.INFO):
            import shlex
            cmd_to_log = ' '.join(shlex.quote(s) for s in self.command)
            self.log.info('Executing %s', cmd_to_log)

    def process_post(self, returncode):
        if self.temp_dir is None:
            return

        try:
            self.log.debug('Cleaning up temp dir %s', self.temp_dir)
            self.temp_dir.cleanup()
        except FileNotFoundError:
            # This is expected, the directory was already removed by BAM.
            pass
        except Exception:
            self.log.exception('Unable to clean up temp dir %s', self.temp_dir)

        self.log.info('Written to %s', self.outfname)


def menu_func(self, context):
    layout = self.layout
    layout.separator()
    layout.operator(ExportBlendPack.bl_idname)


classes = (
    ExportBlendPack,
)


def pythonpath() -> str:
    """Returns the value of a PYTHONPATH environment variable needed to run BAM from its wheel file.
    """

    import os
    import pathlib

    log = logging.getLogger('%s.pythonpath' % __name__)

    # Find the wheel to run.
    wheelpath = pathlib.Path(__file__).with_name(BAM_WHEEL_PATH)
    if not wheelpath.exists():
        raise EnvironmentError('Wheel %s does not exist!' % wheelpath)

    log.info('Using wheel %s to run BAM-Pack', wheelpath)

    # Update the PYTHONPATH to include that wheel.
    existing_pypath = os.environ.get('PYTHONPATH', '')
    if existing_pypath:
        return os.pathsep.join((existing_pypath, str(wheelpath)))

    return str(wheelpath)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.INFO_MT_file_external_data.append(menu_func)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    bpy.types.INFO_MT_file_external_data.remove(menu_func)


if __name__ == "__main__":
    register()
