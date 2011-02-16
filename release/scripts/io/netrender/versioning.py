import sys, os
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

SYSTEMS = {
            Subversion.name: Subversion()
           }
