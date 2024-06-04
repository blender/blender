# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from os import path
import re


def split_into_components(fname):
    """
    Split filename into components
    'WallTexture_diff_2k.002.jpg' -> ['Wall', 'Texture', 'diff', 'k']
    """
    # Remove extension
    fname = path.splitext(fname)[0]
    # Remove digits
    fname = "".join(i for i in fname if not i.isdigit())
    # Separate CamelCase by space
    fname = re.sub(r"([a-z])([A-Z])", r"\g<1> \g<2>", fname)
    # Replace common separators with SPACE
    separators = ["_", ".", "-", "__", "--", "#"]
    for sep in separators:
        fname = fname.replace(sep, " ")

    components = fname.split(" ")
    components = [c.lower() for c in components]
    return components


def remove_common_prefix(names_to_tag_lists):
    """
    Accepts a mapping of file names to tag lists that should be used for socket
    matching.

    This function modifies the provided mapping so that any common prefix
    between all the tag lists is removed.

    Returns true if some prefix was removed, false otherwise.
    """
    if not names_to_tag_lists:
        return False
    sample_tags = next(iter(names_to_tag_lists.values()))
    if not sample_tags:
        return False

    common_prefix = sample_tags[0]
    for tag_list in names_to_tag_lists.values():
        if tag_list[0] != common_prefix:
            return False

    for name, tag_list in names_to_tag_lists.items():
        names_to_tag_lists[name] = tag_list[1:]
    return True


def remove_common_suffix(names_to_tag_lists):
    """
    Accepts a mapping of file names to tag lists that should be used for socket
    matching.

    This function modifies the provided mapping so that any common suffix
    between all the tag lists is removed.

    Returns true if some suffix was removed, false otherwise.
    """
    if not names_to_tag_lists:
        return False
    sample_tags = next(iter(names_to_tag_lists.values()))
    if not sample_tags:
        return False

    common_suffix = sample_tags[-1]
    for tag_list in names_to_tag_lists.values():
        if tag_list[-1] != common_suffix:
            return False

    for name, tag_list in names_to_tag_lists.items():
        names_to_tag_lists[name] = tag_list[:-1]
    return True


def files_to_clean_file_names_for_sockets(files, sockets):
    """
    Accepts a list of files and a list of sockets.

    Returns a mapping from file names to tag lists that should be used for
    classification.

    A file is something that we can do x.name on to figure out the file name.

    A socket is a tuple containing:
    * name
    * list of tags
    * a None field where the selected file name will go later. Ignored by us.
    """

    names_to_tag_lists = {}
    for file in files:
        names_to_tag_lists[file.name] = split_into_components(file.name)

    all_tags = set()
    for socket in sockets:
        socket_tags = socket[1]
        all_tags.update(socket_tags)

    while len(names_to_tag_lists) > 1:
        something_changed = False

        # Common prefixes / suffixes provide zero information about what file
        # should go to which socket, but they can confuse the mapping. So we get
        # rid of them here.
        something_changed |= remove_common_prefix(names_to_tag_lists)
        something_changed |= remove_common_suffix(names_to_tag_lists)

        # Names matching zero tags provide no value, remove those
        names_to_remove = set()
        for name, tag_list in names_to_tag_lists.items():
            match_found = False
            for tag in tag_list:
                if tag in all_tags:
                    match_found = True

            if not match_found:
                names_to_remove.add(name)

        for name_to_remove in names_to_remove:
            del names_to_tag_lists[name_to_remove]
            something_changed = True

        if not something_changed:
            break

    return names_to_tag_lists


def match_files_to_socket_names(files, sockets):
    """
    Given a list of files and a list of sockets, match file names to sockets.

    A file is something that you can get a file name out of using x.name.

    After this function returns, all possible sockets have had their file names
    filled in. Sockets without any matches will not get their file names
    changed.

    Sockets list format. Note that all file names are initially expected to be
    None. Tags are strings, as are the socket names: [
        [
            socket_name, [tags], Optional[file_name]
        ]
    ]
    """

    names_to_tag_lists = files_to_clean_file_names_for_sockets(files, sockets)

    for sname in sockets:
        for name, tag_list in names_to_tag_lists.items():
            if sname[0] == "Normal":
                # Blender wants GL normals, not DX (DirectX) ones:
                # https://www.reddit.com/r/blender/comments/rbuaua/texture_contains_normaldx_and_normalgl_files/
                if 'dx' in tag_list:
                    continue
                if 'directx' in tag_list:
                    continue

            matches = set(sname[1]).intersection(set(tag_list))
            if matches:
                sname[2] = name
                break
