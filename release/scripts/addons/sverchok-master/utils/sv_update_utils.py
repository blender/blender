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

import requests
import os
import urllib
import urllib.request
from zipfile import ZipFile

import bpy
import sverchok

# pylint: disable=w0141

def sv_get_local_path():
    script_paths = os.path.normpath(os.path.dirname(__file__))
    addons_path = os.path.split(os.path.dirname(script_paths))[0]
    version_local = ".".join(map(str, sverchok.bl_info["version"]))
    return script_paths, addons_path, version_local

sv_script_paths, bl_addons_path, sv_version_local = sv_get_local_path()



def get_sha_filepath(filename='sv_shafile.sv'):
    """ the act if calling this function should produce a file called 
        ../datafiles/sverchok/sv_shafile.sv (or sv_sha_downloaded.sv)

    the location of datafiles is common for Blender apps and defined internally for each OS.
    returns: the path of this file

    """
    dirpath = os.path.join(bpy.utils.user_resource('DATAFILES', path='sverchok', create=True))
    fullpath = os.path.join(dirpath, filename)
    
    # create fullpath if it doesn't exist
    if not os.path.exists(fullpath):
        with open(fullpath, 'w') as _:
            pass
    
    return fullpath

def latest_github_sha():
    """ get sha produced by latest commit on github

        sha = latest_github_sha()
        print(sha)

    """
    r = requests.get('https://api.github.com/repos/nortikin/sverchok/commits')
    json_obj = r.json()
    return os.path.basename(json_obj[0]['commit']['url'])


def latest_local_sha(filename='sv_shafile.sv'):
    """ get previously stored sha, if any. finding no local sha will return empty string

        reads from  ../datafiles/sverchok/sv_shafile.sv

    """
    filepath = get_sha_filepath(filename)
    with open(filepath) as p:
        return p.read()

def write_latest_sha_to_local(sha_value='', filename='sv_shafile.sv'):
    """ write the content of sha_value to 

        ../datafiles/sverchok/sv_shafile.sv

    """
    filepath = get_sha_filepath(filename)
    with open(filepath, 'w') as p:
        p.write(sha_value)


def make_version_sha():
    """ Generate a string to represent sverchok version including sha if found

        returns:   0.5.9.13 (a3bcd34)   (or something like that)
    """
    sha_postfix = ''
    sha = latest_local_sha(filename='sv_sha_downloaded.sv')
    if sha:
        sha_postfix = " (" + sha[:7] + ")"

    return sv_version_local + sha_postfix

version_and_sha = make_version_sha()


class SverchokCheckForUpgradesSHA(bpy.types.Operator):
    """ Check if there new version on github (using sha) """
    bl_idname = "node.sverchok_check_for_upgrades_wsha"
    bl_label = "Sverchok check for new minor version"
    bl_options = {'REGISTER'}
            

    def execute(self, context):
        report = self.report
        context.scene.sv_new_version = False
        
        local_sha = latest_local_sha()
        latest_sha = latest_github_sha()
        
        # this logic can be simplified.
        if not local_sha:
            context.scene.sv_new_version = True
        else:
            if not local_sha == latest_sha:
                context.scene.sv_new_version = True

        write_latest_sha_to_local(sha_value=latest_sha)
        downloaded_sha = latest_local_sha(filename='sv_sha_downloaded.sv')

        if not downloaded_sha == latest_sha:
            context.scene.sv_new_version = True

        if context.scene.sv_new_version:
            report({'INFO'}, "New commits available, update at own risk ({0})".format(latest_sha[:7]))
        else:
            report({'INFO'}, "No new commits to download")
        return {'FINISHED'}


class SverchokUpdateAddon(bpy.types.Operator):
    """ Update Sverchok addon. After completion press F8 to reload addons or restart Blender """
    bl_idname = "node.sverchok_update_addon"
    bl_label = "Sverchok update addon"
    bl_options = {'REGISTER'}

    def execute(self, context):

        os.curdir = bl_addons_path
        os.chdir(os.curdir)

        # wm = bpy.context.window_manager  should be this i think....
        wm = bpy.data.window_managers[0]
        wm.progress_begin(0, 100)
        wm.progress_update(20)

        try:
            branch_name = 'master'
            zipname = '{0}.zip'.format(branch_name)
            url = 'https://github.com/nortikin/sverchok/archive/' + zipname
            to_path = os.path.normpath(os.path.join(os.curdir, zipname))
            file = urllib.request.urlretrieve(url, to_path)
            wm.progress_update(50)
        except:
            self.report({'ERROR'}, "Cannot get archive from Internet")
            wm.progress_end()
            return {'CANCELLED'}
        
        try:
            err = 0
            ZipFile(file[0]).extractall(path=os.curdir, members=None, pwd=None)
            wm.progress_update(90)
            err = 1
            os.remove(file[0])
            err = 2
            bpy.context.scene.sv_new_version = False
            wm.progress_update(100)
            wm.progress_end()
            self.report({'INFO'}, "Unzipped, reload addons with F8 button, maybe restart Blender")
        except:
            self.report({'ERROR'}, "Cannot extract files errno {0}".format(str(err)))
            wm.progress_end()
            os.remove(file[0])
            return {'CANCELLED'}

        # write to both sv_sha_download and sv_shafile.sv
        write_latest_sha_to_local(sha_value=latest_local_sha(), filename='sv_sha_downloaded.sv')
        write_latest_sha_to_local(sha_value=latest_local_sha())
        return {'FINISHED'}


class SvPrintCommits(bpy.types.Operator):
    """ show latest commits in info panel, and terminal """
    bl_idname = "node.sv_show_latest_commits"
    bl_label = "Show latest commits"

    def execute(self, context):
        r = requests.get('https://api.github.com/repos/nortikin/sverchok/commits')
        json_obj = r.json()
        for i in range(5):
            commit = json_obj[i]['commit']
            comment = commit['message'].split('\n')

            # display on report window
            message_dict = {
                'sha': os.path.basename(json_obj[i]['commit']['url'])[:7],
                'user': commit['committer']['name'],
                'comment': comment[0] + '...' if len(comment) else ''
            }

            self.report({'INFO'}, '{sha} : by {user}  :  {comment}'.format(**message_dict))

            # display on terminal
            print('{sha} : by {user}'.format(**message_dict))
            for line in comment:
                print('    ' + line)

        return {'FINISHED'}




def register():
    bpy.utils.register_class(SverchokCheckForUpgradesSHA)
    bpy.utils.register_class(SverchokUpdateAddon)
    bpy.utils.register_class(SvPrintCommits)

def unregister():
    bpy.utils.unregister_class(SverchokCheckForUpgradesSHA)
    bpy.utils.unregister_class(SverchokUpdateAddon)
    bpy.utils.unregister_class(SvPrintCommits)
