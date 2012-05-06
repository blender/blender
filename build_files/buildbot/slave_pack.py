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

# Runs on buildbot slave, creating a release package using the build
# system and zipping it into buildbot_upload.zip. This is then uploaded
# to the master in the next buildbot step.

import os
import subprocess
import sys
import zipfile

# get builder name
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting builder name\n")
    sys.exit(1)

builder = sys.argv[1]
branch = ''

if len(sys.argv) >= 3:
    branch = sys.argv[2]

# scons does own packaging
if builder.find('scons') != -1:
    os.chdir('../blender')
    scons_options = ['BF_QUICK=slnt', 'BUILDBOT_BRANCH=' + branch, 'buildslave', 'BF_FANCY=False']

    if builder.find('linux') != -1:
        buildbot_dir = os.path.dirname(os.path.realpath(__file__))
        config_dir = os.path.join(buildbot_dir, 'config')
        build_dir = os.path.join('..', 'build', builder)
        install_dir = os.path.join('..', 'install', builder)

        scons_options += ['WITH_BF_NOBLENDER=True', 'WITH_BF_PLAYER=False',
            'BF_BUILDDIR=' + build_dir,
            'BF_INSTALLDIR=' + install_dir,
            'WITHOUT_BF_INSTALL=True']

        config = None

        if builder.endswith('linux_x86_64_scons'):
            config = 'user-config-x86_64.py'
        elif builder.endswith('linux_i386_scons'):
            config = 'user-config-x86_64.py'

        if config is not None:
            config_fpath = os.path.join(config_dir, config)
            scons_options.append('BF_CONFIG=' + config_fpath)

        blender = os.path.join(install_dir, 'blender')
        blenderplayer = os.path.join(install_dir, 'blenderplayer')
        subprocess.call(['strip', '--strip-all', blender, blenderplayer])

        retcode = subprocess.call(['python', 'scons/scons.py'] + scons_options)
        sys.exit(retcode)
    else:
        if builder.find('win') != -1:
            bitness = '32'

            if builder.find('win64') != -1:
                bitness = '64'

            scons_options.append('BF_BITNESS=' + bitness)
            scons_options.append('WITH_BF_CYCLES_CUDA_BINARIES=True')
            scons_options.append('BF_CYCLES_CUDA_NVCC=nvcc.exe')
            if builder.find('mingw') != -1:
                scons_options.append('BF_TOOLSET=mingw')

        retcode = subprocess.call(['python', 'scons/scons.py'] + scons_options)
        sys.exit(retcode)

# clean release directory if it already exists
release_dir = 'release'

if os.path.exists(release_dir):
    for f in os.listdir(release_dir):
        if os.path.isfile(os.path.join(release_dir, f)):
            os.remove(os.path.join(release_dir, f))

# create release package
try:
    subprocess.call(['make', 'package_archive'])
except Exception, ex:
    sys.stderr.write('Make package release failed' + str(ex) + '\n')
    sys.exit(1)

# find release directory, must exist this time
if not os.path.exists(release_dir):
    sys.stderr.write("Failed to find release directory %r.\n" % release_dir)
    sys.exit(1)

# find release package
file = None
filepath = None

for f in os.listdir(release_dir):
    rf = os.path.join(release_dir, f)
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
