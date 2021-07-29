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

import os
import re
import subprocess

from netrender.utils import *

class AbstractVCS:
    name = "ABSTRACT VCS"
    def __init__(self):
        pass

    def update(self, info):
        """update(info)
        Update a working copy to the specified revision.
        If working copy doesn't exist, do a full get from server to create it.
        [info] model.VersioningInfo instance, specifies the working path, remote path and version number."""
        pass

    def revision(self, path):
        """revision(path)
        return the current revision of the specified working copy path"""
        pass

    def path(self, path):
        """path(path)
        return the remote path of the specified working copy path"""
        pass

class Subversion(AbstractVCS):
    name = "Subversion"
    description = "Use the Subversion version control system"
    def __init__(self):
        super().__init__()
        self.version_exp = re.compile("([0-9]*)")
        self.path_exp = re.compile("URL: (.*)")

    def update(self, info):
        if not os.path.exists(info.wpath):
            base, folder = os.path.split(info.wpath)

            with DirectoryContext(base):
                subprocess.call(["svn", "co", "%s@%s" % (info.rpath, str(info.revision)), folder])
        else:
            with DirectoryContext(info.wpath):
                subprocess.call(["svn", "up", "--accept", "theirs-full", "-r", str(info.revision)])

    def revision(self, path):
        if not os.path.exists(path):
            return

        with DirectoryContext(path):
            stdout = subprocess.check_output(["svnversion"])

            match = self.version_exp.match(str(stdout, encoding="utf-8"))

            if match:
                return match.group(1)

    def path(self, path):
        if not os.path.exists(path):
            return

        with DirectoryContext(path):
            stdout = subprocess.check_output(["svn", "info"])

            match = self.path_exp.search(str(stdout, encoding="utf-8"))

            if match:
                return match.group(1)

class Git(AbstractVCS):
    name = "Git"
    description = "Use the Git distributed version control system"
    def __init__(self):
        super().__init__()
        self.version_exp = re.compile("^commit (.*)")

    def update(self, info):
        if not os.path.exists(info.wpath):
            base, folder = os.path.split(info.wpath)

            with DirectoryContext(base):
                subprocess.call(["git", "clone", "%s" % (info.rpath), folder])

        with DirectoryContext(info.wpath):
            subprocess.call(["git", "checkout", str(info.revision)])

    def revision(self, path):
        if not os.path.exists(path):
            return

        with DirectoryContext(path):
            stdout = subprocess.check_output(["git", "show"])

            match = self.version_exp.search(str(stdout, encoding="utf-8"))

            if match:
                return match.group(1)

    def path(self, path):
        if not os.path.exists(path):
            return

        # find something that could somehow work for git (fun times)
        return path

SYSTEMS = {
            Subversion.name: Subversion(),
            Git.name: Git()
           }

ITEMS =  (
          (Subversion.name, Subversion.name, Subversion.description),
          (Git.name, Git.name, Git.description),
          )

