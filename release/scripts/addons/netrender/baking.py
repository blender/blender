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

import bpy
import sys, subprocess, re

from netrender.utils import *


def commandToTask(command):
    i = command.index("|")
    ri = command.rindex("|")
    return (command[:i], command[i+1:ri], command[ri+1:])

def taskToCommand(task):
    return "|".join(task)

def bake(job, tasks):
    main_file = job.files[0]
    job_full_path = main_file.filepath

    task_commands = []
    for task in tasks:
        task_commands.extend(task)

    process = subprocess.Popen(
        [bpy.app.binary_path,
         "-b",
         "-y",
         "-noaudio",
         job_full_path,
         "-P", __file__,
         "--",
         ] + task_commands,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )

    return process

result_pattern = re.compile("BAKE FILE\[ ([0-9]+) \]: (.*)")
def resultsFromOuput(lines):
    results = []
    for line in lines:
        match = result_pattern.match(line)

        if match:
            task_id = int(match.groups()[0])
            task_filename = match.groups()[1]

            results.append((task_id, task_filename))

    return results

def bake_cache(obj, point_cache, task_index):
    if point_cache.is_baked:
        bpy.ops.ptcache.free_bake({"point_cache": point_cache})

    point_cache.use_disk_cache = True
    point_cache.use_external = False

    bpy.ops.ptcache.bake({"point_cache": point_cache}, bake=True)

    results = cache_results(obj, point_cache)

    print()

    for filename in results:
        print("BAKE FILE[", task_index, "]:", filename)


def cache_results(obj, point_cache):
    name = cacheName(obj, point_cache)
    default_path = cachePath(bpy.data.filepath)

    cache_path = bpy.path.abspath(point_cache.filepath) if point_cache.use_external else default_path

    index = "%02i" % point_cache.index

    if os.path.exists(cache_path):
        pattern = re.compile(name + "_([0-9]+)_" + index + "\.bphys")

        cache_files = []

        for cache_file in sorted(os.listdir(cache_path)):
            match = pattern.match(cache_file)

            if match:
                cache_files.append(os.path.join(cache_path, cache_file))

        cache_files.sort()

        return cache_files

    return []

def process_generic(obj, index, task_index):
    modifier = obj.modifiers[index]
    point_cache = modifier.point_cache
    bake_cache(obj, point_cache, task_index)

def process_smoke(obj, index, task_index):
    modifier = obj.modifiers[index]
    point_cache = modifier.domain_settings.point_cache
    bake_cache(obj, point_cache, task_index)

def process_particle(obj, index, task_index):
    psys = obj.particle_systems[index]
    point_cache = psys.point_cache
    bake_cache(obj, point_cache, task_index)

def process_paint(obj, index, task_index):
    modifier = obj.modifiers[index]
    for surface in modifier.canvas_settings.canvas_surfaces:
        bake_cache(obj, surface.point_cache, task_index)

def process_null(obj, index, task_index):
    raise ValueException("No baking possible with arguments: " + " ".join(sys.argv))

process_funcs = {}
process_funcs["CLOTH"] = process_generic
process_funcs["SOFT_BODY"] = process_generic
process_funcs["PARTICLE_SYSTEM"] = process_particle
process_funcs["SMOKE"] = process_smoke
process_funcs["DYNAMIC_PAINT"] = process_paint

if __name__ == "__main__":
    try:
        i = sys.argv.index("--")
    except:
        i = 0

    if i:
        task_args = sys.argv[i+1:]
        for i in range(0, len(task_args), 3):
            bake_type = task_args[i]
            obj = bpy.data.objects[task_args[i+1]]
            index = int(task_args[i+2])

            process_funcs.get(bake_type, process_null)(obj, index, i)
