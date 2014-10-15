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

# <pep8 compliant>

import os
import shutil
import sys
import zipfile


# extension stripping
def strip_extension(filename):
    extensions = '.zip', '.tar', '.bz2', '.gz', '.tgz', '.tbz', '.exe'
    filename_noext, ext = os.path.splitext(filename)
    if ext in extensions:
        return strip_extension(filename_noext)  # may have .tar.bz2
    else:
        return filename


# extract platform from package name
def get_platform(filename):
    # name is blender-version-platform.extension. we want to get the
    # platform out, but there may be some variations, so we fiddle a
    # bit to handle current and hopefully future names
    filename = strip_extension(filename)
    filename = strip_extension(filename)

    tokens = filename.split("-")
    platforms = ('osx', 'mac', 'bsd',
                 'win', 'linux', 'source',
                 'solaris',
                 'mingw')
    platform_tokens = []
    found = False

    for token in tokens:
        if not found:
            for platform in platforms:
                if platform in token.lower():
                    found = True
                    break

        if found:
            platform_tokens += [token]

    return '-'.join(platform_tokens)


def get_branch(filename):
    tokens = filename.split("-")
    branch = ""

    for token in tokens:
        if token == "blender":
            return branch

        if branch == "":
            branch = token
        else:
            branch = branch + "-" + token

    return ""

# get filename
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting file to unpack\n")
    sys.exit(1)

filename = sys.argv[1]

# open zip file
if not os.path.exists(filename):
    sys.stderr.write("File %r not found.\n" % filename)
    sys.exit(1)

try:
    z = zipfile.ZipFile(filename, "r")
except Exception, ex:
    sys.stderr.write('Failed to open zip file: %s\n' % str(ex))
    sys.exit(1)

if len(z.namelist()) != 1:
    sys.stderr.write("Expected one file in %r." % filename)
    sys.exit(1)

package = z.namelist()[0]
packagename = os.path.basename(package)

# detect platform and branch
platform = get_platform(packagename)
branch = get_branch(packagename)

if platform == '':
    sys.stderr.write('Failed to detect platform ' +
                     'from package: %r\n' % packagename)
    sys.exit(1)

# extract
directory = 'public_html/download'
if not branch or branch == 'master':
    directory = 'public_html/download'
elif branch == 'experimental-build':
    directory = 'public_html/experimental'
# else: put 'official' branches in their own public subdir of download/ ?

try:
    zf = z.open(package)
    f = file(os.path.join(directory, packagename), "wb")

    shutil.copyfileobj(zf, f)

    zf.close()
    z.close()

    os.remove(filename)
except Exception, ex:
    sys.stderr.write('Failed to unzip package: %s\n' % str(ex))
    sys.exit(1)

# remove other files from the same platform and branch
try:
    for f in os.listdir(directory):
        if get_platform(f) == platform and get_branch(f) == branch:
            if f != packagename:
                os.remove(os.path.join(directory, f))
except Exception, ex:
    sys.stderr.write('Failed to remove old packages: %s\n' % str(ex))
    sys.exit(1)
