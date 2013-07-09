#!/usr/bin/python

# <pep8 compliant>

import os
import shutil
import subprocess
import sys

# todo:
# strip executables

# get parameters
if len(sys.argv) < 5:
    sys.stderr.write('Excepted arguments: ./build_archive.py name extension install_dir output_dir')
    sys.exit(1)

package_name = sys.argv[1]
extension = sys.argv[2]
install_dir = sys.argv[3]
output_dir = sys.argv[4]

package_archive = os.path.join(output_dir, package_name + '.' + extension)
package_dir = package_name

# remove existing package with the same name
try:
    if os.path.exists(package_archive):
        os.remove(package_archive)
    if os.path.exists(package_dir):
        shutil.rmtree(package_dir)
except Exception, ex:
    sys.stderr.write('Failed to clean up old package files: ' + str(ex) + '\n')
    sys.exit(1)

# create temporary package dir
try:
    shutil.copytree(install_dir, package_dir)

    for f in os.listdir(package_dir):
        if f.startswith('makes'):
            os.remove(os.path.join(package_dir, f))
except Exception, ex:
    sys.stderr.write('Failed to copy install directory: ' + str(ex) + '\n')
    sys.exit(1)

# create archive
try:
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)

    if extension == 'zip':
        archive_cmd = ['zip', '-9', '-r', package_archive, package_dir]
    elif extension == 'tar.bz2':
        archive_cmd = ['tar', 'cjf', package_archive, package_dir]
    else:
        sys.stderr.write('Unknown archive extension: ' + extension)
        sys.exit(-1)

    subprocess.call(archive_cmd)
except Exception, ex:
    sys.stderr.write('Failed to create package archive: ' + str(ex) + '\n')
    sys.exit(1)

# empty temporary package dir
try:
    shutil.rmtree(package_dir)
except Exception, ex:
    sys.stderr.write('Failed to clean up package directory: ' + str(ex) + '\n')
    sys.exit(1)
