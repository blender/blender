#!/usr/bin/python3

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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Update all branches:
# * Generate a temp messages.txt file.
# * Use it to generate a temp .pot file.
# * Use it to update all .po’s in /branches.

import subprocess
import os
import sys
import tempfile

try:
    import settings
except:
    from . import settings


PY3 = settings.PYTHON3_EXEC

def main():
    import argparse
    parser = argparse.ArgumentParser(description="" \
                                 "Update all branches:\n" \
                                 "* Generate a temp messages.txt file.\n" \
                                 "* Use it to generate a temp .pot file.\n" \
                                 "* Use it to update all .po’s in /branches.")
    parser.add_argument('--pproc-contexts', action="store_true",
                        help="Pre-process po’s to avoid having plenty of "
                             "fuzzy msgids just because a context was "
                             "added/changed!")
    parser.add_argument('-c', '--no_checks', default=True,
                        action="store_false",
                        help="No checks over UI messages.")
    parser.add_argument('-a', '--add', action="store_true",
                        help="Add missing po’s (useful only when one or " \
                             "more languages are given!).")
    parser.add_argument('langs', metavar='ISO_code', nargs='*',
                        help="Restrict processed languages to those.")
    args = parser.parse_args()


    ret = 0

    # Generate a temp messages file.
    dummy, msgfile = tempfile.mkstemp(suffix=".txt",
                                      prefix="blender_messages_")
    os.close(dummy)
    cmd = (PY3, "./update_msg.py", "-o", msgfile)
    t = subprocess.call(cmd)
    if t:
        ret = t

    # Regenerate POTFILES.in.
#    cmd = (PY3, "./update_potinput.py")
#    t = subprocess.call(cmd)
#    if t:
#        ret = t

    # Generate a temp pot file.
    dummy, potfile = tempfile.mkstemp(suffix=".pot",
                                      prefix="blender_pot_")
    os.close(dummy)
    cmd = [PY3, "./update_pot.py", "-i", msgfile, "-o", potfile]
    if not args.no_checks:
        cmd.append("-c")
    t = subprocess.call(cmd)
    if t:
        ret = t

    # Update branches’ po files.
    cmd = [PY3, "./update_po.py", "-i", potfile]
    if args.langs:
        if args.add:
            cmd.append("-a")
        cmd += args.langs
    if args.pproc_contexts:
        cmd.append("--pproc-contexts")
    t = subprocess.call(cmd)
    if t:
        ret = t

    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    sys.exit(main())
