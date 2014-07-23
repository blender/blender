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

# Runs on buildbot slave, rsync zip directly to buildbot server rather
# than using upload which is much slower

import os
import sys

# get builder name
if len(sys.argv) < 3:
    sys.stderr.write("Not enough arguments, expecting builder and branch names\n")
    sys.exit(1)

builder = sys.argv[1]
branch = sys.argv[2]

# rsync, this assumes ssh keys are setup so no password is needed
local_zip = "buildbot_upload.zip"
remote_folder = "builder.blender.org:/data/buildbot-master/uploaded/"
remote_zip = remote_folder + "buildbot_upload_" + builder + "_" + branch + ".zip"
command = "rsync -avz %s %s" % (local_zip, remote_zip)

print(command)

ret = os.system(command)
sys.exit(ret)
