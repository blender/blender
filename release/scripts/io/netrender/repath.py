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

import sys, os
import subprocess

import bpy

from netrender.utils import *
import netrender.model

BLENDER_PATH = sys.argv[0]

def reset(job):
    main_file = job.files[0]
    
    job_full_path = main_file.filepath

    if os.path.exists(job_full_path + ".bak"):
        os.remove(job_full_path) # repathed file
        os.renames(job_full_path + ".bak", job_full_path)

def update(job):
    paths = []
    
    main_file = job.files[0]
    
    job_full_path = main_file.filepath

        
    path, ext = os.path.splitext(job_full_path)
    
    new_path = path + ".remap" + ext 
    
    all = main_file.filepath == main_file.original_path 
    
    for rfile in job.files[1:]:
        if all or rfile.original_path != rfile.filepath:
            paths.append(rfile.original_path)
            paths.append(rfile.filepath)
    
    # Only update if needed
    if paths:        
        process = subprocess.Popen([BLENDER_PATH, "-b", "-noaudio", job_full_path, "-P", __file__, "--", new_path] + paths, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        process.wait()
        
        os.renames(job_full_path, job_full_path + ".bak")
        os.renames(new_path, job_full_path)

def process(paths):
    def processPointCache(point_cache):
        point_cache.external = False

    def processFluid(fluid):
        new_path = path_map.get(fluid.path, None)
        if new_path:
            fluid.path = new_path
    
    path_map = {}
    for i in range(0, len(paths), 2):
        # special case for point cache
        if paths[i].endswith(".bphys"):
            pass # Don't need them in the map, they all use the default external path
            # NOTE: This is probably not correct all the time, need to be fixed.
        # special case for fluids
        elif paths[i].endswith(".bobj.gz"):
            path_map[os.path.split(paths[i])[0]] = os.path.split(paths[i+1])[0]
        else:
            path_map[paths[i]] = paths[i+1]
    
    ###########################
    # LIBRARIES
    ###########################
    for lib in bpy.data.libraries:
        file_path = bpy.utils.expandpath(lib.filepath)
        new_path = path_map.get(file_path, None)
        if new_path:
            lib.filepath = new_path

    ###########################
    # IMAGES
    ###########################
    for image in bpy.data.images:
        if image.source == "FILE" and not image.packed_file:
            file_path = bpy.utils.expandpath(image.filepath)
            new_path = path_map.get(file_path, None)
            if new_path:
                image.filepath = new_path
            

    ###########################
    # FLUID + POINT CACHE
    ###########################
    for object in bpy.data.objects:
        for modifier in object.modifiers:
            if modifier.type == 'FLUID_SIMULATION' and modifier.settings.type == "DOMAIN":
                processFluid(settings)
            elif modifier.type == "CLOTH":
                processPointCache(modifier.point_cache)
            elif modifier.type == "SOFT_BODY":
                processPointCache(modifier.point_cache)
            elif modifier.type == "SMOKE" and modifier.smoke_type == "TYPE_DOMAIN":
                processPointCache(modifier.domain_settings.point_cache_low)
                if modifier.domain_settings.highres:
                    processPointCache(modifier.domain_settings.point_cache_high)
            elif modifier.type == "MULTIRES" and modifier.external:
                file_path = bpy.utils.expandpath(modifier.filepath)
                new_path = path_map.get(file_path, None)
                if new_path:
                    modifier.filepath = new_path

        # particles modifier are stupid and don't contain data
        # we have to go through the object property
        for psys in object.particle_systems:
            processPointCache(psys.point_cache)
                

if __name__ == "__main__":
    try:
        i = sys.argv.index("--")
    except:
        i = 0
    
    if i:
        new_path = sys.argv[i+1]
        args = sys.argv[i+2:]
        
        process(args)
        
        bpy.ops.wm.save_as_mainfile(path=new_path, check_existing=False)
