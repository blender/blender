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

# internal blender C module
import _bpy
from _bpy import types, props, app

data = _bpy.data
context = _bpy.context

# python modules
from . import utils, path
from . import ops as _ops_module

# fake operator module
ops = _ops_module.ops_fake_module

import sys as _sys


def _main():

    # Possibly temp. addons path
    from os.path import join, dirname, normpath
    _sys.path.append(normpath(join(dirname(__file__), "..", "..", "addons", "modules")))

    # if "-d" in sys.argv: # Enable this to measure startup speed
    if 0:
        import cProfile
        cProfile.run('import bpy; bpy.utils.load_scripts()', 'blender.prof')

        import pstats
        p = pstats.Stats('blender.prof')
        p.sort_stats('cumulative').print_stats(100)

    else:
        utils.load_scripts()

    import os

    # test for X11
    if os.environ.get("DISPLAY"):

        # BSD licenced code copied from python, temp fix for bug
        # http://bugs.python.org/issue11432, XXX == added code
        def _invoke(self, args, remote, autoraise):
            # XXX, added imports
            import io
            import subprocess
            import time

            raise_opt = []
            if remote and self.raise_opts:
                # use autoraise argument only for remote invocation
                autoraise = int(autoraise)
                opt = self.raise_opts[autoraise]
                if opt:
                    raise_opt = [opt]

            cmdline = [self.name] + raise_opt + args

            if remote or self.background:
                inout = io.open(os.devnull, "r+")
            else:
                # for TTY browsers, we need stdin/out
                inout = None
            # if possible, put browser in separate process group, so
            # keyboard interrupts don't affect browser as well as Python
            setsid = getattr(os, 'setsid', None)
            if not setsid:
                setsid = getattr(os, 'setpgrp', None)

            p = subprocess.Popen(cmdline, close_fds=True,  # XXX, stdin=inout,
                                 stdout=(self.redirect_stdout and inout or None),
                                 stderr=inout, preexec_fn=setsid)
            if remote:
                # wait five secons. If the subprocess is not finished, the
                # remote invocation has (hopefully) started a new instance.
                time.sleep(1)
                rc = p.poll()
                if rc is None:
                    time.sleep(4)
                    rc = p.poll()
                    if rc is None:
                        return True
                # if remote call failed, open() will try direct invocation
                return not rc
            elif self.background:
                if p.poll() is None:
                    return True
                else:
                    return False
            else:
                return not p.wait()

        import webbrowser
        webbrowser.UnixBrowser._invoke = _invoke


_main()
