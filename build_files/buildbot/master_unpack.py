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

# Runs on Buildbot master, to unpack incoming unload.zip into latest
# builds directory and remove older builds.
 
import os
import shutil
import sys
import zipfile

# extension stripping
def strip_extension(filename):
    extensions = ['.zip', '.tar', '.bz2', '.gz', '.tgz', '.tbz', '.exe']

    for ext in extensions:
        if filename.endswith(ext):
            filename = filename[:-len(ext)]

    return filename

# extract platform from package name
def get_platform(filename):
    # name is blender-version-platform.extension. we want to get the
    # platform out, but there may be some variations, so we fiddle a
    # bit to handle current and hopefully future names
    filename = strip_extension(filename)
    filename = strip_extension(filename)

    tokens = filename.split("-")
    platforms = ['osx', 'mac', 'bsd', 'win', 'linux', 'source', 'irix', 'solaris']
    platform_tokens = []
    found = False

    for i, token in enumerate(tokens):
        if not found:
            for platform in platforms:
                if token.lower().find(platform) != -1:
                    found = True

        if found:
            platform_tokens += [token]

    return '-'.join(platform_tokens)

# get filename
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting file to unpack\n")
    sys.exit(1)

filename = sys.argv[1]

# open zip file
if not os.path.exists(filename):
    sys.stderr.write("File " + filename + " not found.\n")
    sys.exit(1)

try:
    z = zipfile.ZipFile(filename, "r")
except Exception, ex:
    sys.stderr.write('Failed to open zip file: ' + str(ex) + '\n')
    sys.exit(1)

if len(z.namelist()) != 1:
    sys.stderr.write("Expected on file in " + filename + ".")
    sys.exit(1)

package = z.namelist()[0]
packagename = os.path.basename(package)

# detect platform
platform = get_platform(packagename)

if platform == '':
    sys.stderr.write('Failed to detect platform from package: ' + packagename + '\n')
    sys.exit(1)

# extract
dir = 'public_html/download'

try:
    zf = z.open(package)
    f = file(os.path.join(dir, packagename), "wb")

    shutil.copyfileobj(zf, f)

    zf.close()
    z.close()
    
    os.remove(filename)
except Exception, ex:
    sys.stderr.write('Failed to unzip package: ' + str(ex) + '\n')
    sys.exit(1)

# remove other files from the same platform
try:
    for f in os.listdir(dir):
        if f.lower().find(platform.lower()) != -1:
            if f != packagename:
                os.remove(os.path.join(dir, f))
except Exception, ex:
    sys.stderr.write('Failed to remove old packages: ' + str(ex) + '\n')
    sys.exit(1)

