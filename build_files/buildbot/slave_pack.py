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

# Runs on buildbot slave, creating a release package using the build
# system and zipping it into buildbot_upload.zip. This is then uploaded
# to the master in the next buildbot step.

import os
import subprocess
import sys
import zipfile


# find release directory
def find_release_directory():
    for d in os.listdir('.'):
        if os.path.isdir(d):
            rd = os.path.join(d, 'release')
            if os.path.exists(rd):
                return rd

    return None

# clean release directory if it already exists
dir = find_release_directory()

if dir:
    for f in os.listdir(dir):
        if os.path.isfile(os.path.join(dir, f)):
            os.remove(os.path.join(dir, f))

# create release package
try:
    os.chdir('../blender')
    subprocess.call(['make', 'package_archive'])
    os.chdir('../build')
except Exception, ex:
    sys.stderr.write('Make package release failed' + str(ex) + '\n')
    sys.exit(1)

# find release directory, must exist this time
dir = find_release_directory()

if not dir:
    sys.stderr.write("Failed to find release directory.\n")
    sys.exit(1)

# find release package
file = None
filepath = None

for f in os.listdir(dir):
    rf = os.path.join(dir, f)
    if os.path.isfile(rf) and f.startswith('blender'):
        file = f
        filepath = rf

if not file:
    sys.stderr.write("Failed to find release package.\n")
    sys.exit(1)

# create zip file
try:
    upload_zip = "buildbot_upload.zip"
    if os.path.exists(upload_zip):
        os.remove(upload_zip)
    z = zipfile.ZipFile(upload_zip, "w", compression=zipfile.ZIP_STORED)
    z.write(filepath, arcname=file)
    z.close()
except Exception, ex:
    sys.stderr.write('Create buildbot_upload.zip failed' + str(ex) + '\n')
    sys.exit(1)
