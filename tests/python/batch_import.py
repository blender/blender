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

"""
Example Usage:

./blender.bin --background --python tests/python/batch_import.py -- \
    --operator="bpy.ops.import_scene.obj" \
    --path="/fe/obj" \
    --match="*.obj" \
    --start=0 --end=10 \
    --save_path=/tmp/test

./blender.bin --background --python tests/python/batch_import.py -- \
    --operator="bpy.ops.import_scene.autodesk_3ds" \
    --path="/fe/" \
    --match="*.3ds" \
    --start=0 --end=1000 \
    --save_path=/tmp/test

./blender.bin --background --addons io_curve_svg --python tests/python/batch_import.py -- \
    --operator="bpy.ops.import_curve.svg" \
    --path="/usr/" \
    --match="*.svg" \
    --start=0 --end=1000 \
    --save_path=/tmp/test

"""

import os
import sys


def clear_scene():
    import bpy
    unique_obs = set()
    for scene in bpy.data.scenes:
        for obj in scene.objects[:]:
            scene.objects.unlink(obj)
            unique_obs.add(obj)

    # remove obdata, for now only worry about the startup scene
    for bpy_data_iter in (bpy.data.objects, bpy.data.meshes, bpy.data.lamps, bpy.data.cameras):
        for id_data in bpy_data_iter:
            bpy_data_iter.remove(id_data)


def batch_import(operator="",
                 path="",
                 save_path="",
                 match="",
                 start=0,
                 end=sys.maxsize,
                 ):
    import addon_utils
    _reset_all = addon_utils.reset_all  # XXX, hack

    import fnmatch

    path = os.path.normpath(path)
    path = os.path.abspath(path)

    match_upper = match.upper()
    pattern_match = lambda a: fnmatch.fnmatchcase(a.upper(), match_upper)

    def file_generator(path):
        for dirpath, dirnames, filenames in os.walk(path):

            # skip '.svn'
            if dirpath.startswith("."):
                continue

            for filename in filenames:
                if pattern_match(filename):
                    yield os.path.join(dirpath, filename)

    print("Collecting %r files in %s" % (match, path), end="")

    files = list(file_generator(path))
    files_len = len(files)
    end = min(end, len(files))
    print(" found %d" % files_len, end="")

    files.sort()
    files = files[start:end]
    if len(files) != files_len:
        print(" using a subset in (%d, %d), total %d" % (start, end, len(files)), end="")

    import bpy
    op = eval(operator)

    tot_done = 0
    tot_fail = 0

    for i, f in enumerate(files):
        print("    %s(filepath=%r) # %d of %d" % (operator, f, i + start, len(files)))

        # hack so loading the new file doesn't undo our loaded addons
        addon_utils.reset_all = lambda: None  # XXX, hack

        bpy.ops.wm.read_factory_settings()

        addon_utils.reset_all = _reset_all  # XXX, hack
        clear_scene()

        result = op(filepath=f)

        if 'FINISHED' in result:
            tot_done += 1
        else:
            tot_fail += 1

        if save_path:
            fout = os.path.join(save_path, os.path.relpath(f, path))
            fout_blend = os.path.splitext(fout)[0] + ".blend"

            print("\tSaving: %r" % fout_blend)

            fout_dir = os.path.dirname(fout_blend)
            os.makedirs(fout_dir, exist_ok=True)

            bpy.ops.wm.save_as_mainfile(filepath=fout_blend)

    print("finished, done:%d,  fail:%d" % (tot_done, tot_fail))


def main():
    import optparse

    # get the args passed to blender after "--", all of which are ignored by blender specifically
    # so python may receive its own arguments
    argv = sys.argv

    if "--" not in argv:
        argv = []  # as if no args are passed
    else:
        argv = argv[argv.index("--") + 1:]  # get all args after "--"

    # When --help or no args are given, print this help
    usage_text = "Run blender in background mode with this script:"
    usage_text += "  blender --background --python " + __file__ + " -- [options]"

    parser = optparse.OptionParser(usage=usage_text)

    # Example background utility, add some text and renders or saves it (with options)
    # Possible types are: string, int, long, choice, float and complex.
    parser.add_option("-o", "--operator", dest="operator", help="This text will be used to render an image", type="string")
    parser.add_option("-p", "--path", dest="path", help="Path to use for searching for files", type='string')
    parser.add_option("-m", "--match", dest="match", help="Wildcard to match filename", type="string")
    parser.add_option("-s", "--save_path", dest="save_path", help="Save the input file to a blend file in a new location", metavar='string')
    parser.add_option("-S", "--start", dest="start", help="From collected files, start with this index", metavar='int')
    parser.add_option("-E", "--end", dest="end", help="From collected files, end with this index", metavar='int')

    options, args = parser.parse_args(argv)  # In this example we wont use the args

    if not argv:
        parser.print_help()
        return

    if not options.operator:
        print("Error: --operator=\"some string\" argument not given, aborting.")
        parser.print_help()
        return

    if options.start is None:
        options.start = 0

    if options.end is None:
        options.end = sys.maxsize

    # Run the example function
    batch_import(operator=options.operator,
                 path=options.path,
                 save_path=options.save_path,
                 match=options.match,
                 start=int(options.start),
                 end=int(options.end),
                 )

    print("batch job finished, exiting")


if __name__ == "__main__":
    main()
