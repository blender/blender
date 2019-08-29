#!/usr/bin/env python3
#
# Utility functions for make update and make tests.

import re
import subprocess
import sys

def call(cmd):
    print(" ".join(cmd))

    # Flush to ensure correct order output on Windows.
    sys.stdout.flush()
    sys.stderr.flush()

    retcode = subprocess.call(cmd)
    if retcode != 0:
      sys.exit(retcode)

def git_branch_release_version(git_command):
    # Test if we are building a specific release version.
    try:
        branch = subprocess.check_output([git_command, "rev-parse", "--abbrev-ref", "HEAD"])
    except subprocess.CalledProcessError as e:
        sys.stderr.write("Failed to get Blender git branch\n")
        sys.exit(1)

    branch = branch.strip().decode('utf8')
    release_version = re.search("^blender-v(.*)-release$", branch)
    if release_version:
        release_version = release_version.group(1)
    return release_version

def svn_libraries_base_url(release_version):
    if release_version:
        svn_branch = "tags/blender-" + release_version + "-release"
    else:
        svn_branch = "trunk"
    return "https://svn.blender.org/svnroot/bf-blender/" + svn_branch + "/lib/"
