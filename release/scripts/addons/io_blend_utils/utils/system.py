# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

def colorize_dummy(msg, color=None):
    return msg

_USE_COLOR = True
if _USE_COLOR:
    color_codes = {
        'black':        '\033[0;30m',
        'bright_gray':  '\033[0;37m',
        'blue':         '\033[0;34m',
        'white':        '\033[1;37m',
        'green':        '\033[0;32m',
        'bright_blue':  '\033[1;34m',
        'cyan':         '\033[0;36m',
        'bright_green': '\033[1;32m',
        'red':          '\033[0;31m',
        'bright_cyan':  '\033[1;36m',
        'purple':       '\033[0;35m',
        'bright_red':   '\033[1;31m',
        'yellow':       '\033[0;33m',
        'bright_purple':'\033[1;35m',
        'dark_gray':    '\033[1;30m',
        'bright_yellow':'\033[1;33m',
        'normal':       '\033[0m',
    }

    def colorize(msg, color=None):
        return (color_codes[color] + msg + color_codes['normal'])
else:
    colorize = colorize_dummy


def uuid_from_file(fn, block_size=1 << 20):
    """
    Returns an arbitrary sized unique ASCII string based on the file contents.
    (exact hashing method may change).
    """
    with open(fn, 'rb') as f:
        # first get the size
        import os
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.seek(0, os.SEEK_SET)
        del os
        # done!

        import hashlib
        sha1 = hashlib.new('sha512')
        while True:
            data = f.read(block_size)
            if not data:
                break
            sha1.update(data)
        # skip the '0x'
        return hex(size)[2:] + sha1.hexdigest()


def write_json_to_zip(zip_handle, path, data=None):
    import json
    zip_handle.writestr(
            path,
            json.dumps(
                data,
                check_circular=False,
                # optional (pretty)
                sort_keys=True, indent=4, separators=(',', ': '),
                ).encode('utf-8'))


def write_json_to_file(path, data):
    import json
    with open(path, 'w') as file_handle:
        json.dump(
                data, file_handle, ensure_ascii=False,
                check_circular=False,
                # optional (pretty)
                sort_keys=True, indent=4, separators=(',', ': '),
                )


def is_compressed_filetype(filepath):
    """
    Use to check if we should compress files in a zip.
    """
    # for now, only include files which Blender is likely to reference
    import os
    assert(isinstance(filepath, bytes))
    return os.path.splitext(filepath)[1].lower() in {
        # images
        b'.exr',
        b'.jpg', b'.jpeg',
        b'.png',

        # audio
        b'.aif', b'.aiff',
        b'.mp3',
        b'.ogg', b'.ogv',
        b'.wav',

        # video
        b'.avi',
        b'.mkv',
        b'.mov',
        b'.mpg', b'.mpeg',

        # archives
        # '.bz2', '.tbz',
        # '.gz', '.tgz',
        # '.zip',
        }


def is_subdir(path, directory):
    """
    Returns true if *path* in a subdirectory of *directory*.
    """
    import os
    from os.path import normpath, normcase, sep
    path = normpath(normcase(path))
    directory = normpath(normcase(directory))
    if len(path) > len(directory):
        sep = sep.encode('ascii') if isinstance(directory, bytes) else sep
        if path.startswith(directory.rstrip(sep) + sep):
            return True
    return False

