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

# Use for validating our wiki interlinking.
#  ./blender.bin --background -noaudio --python tests/python/bl_rna_wiki_reference.py
#
# 1) test_data()              -- ensure the data we have is correct format
# 2) test_lookup_coverage()   -- ensure that we have lookups for _every_ RNA path
# 3) test_urls()              -- ensure all the URL's are correct
# 4) test_language_coverage() -- ensure language lookup table is complete
#

import bpy


def test_data():
    import rna_wiki_reference

    assert(isinstance(rna_wiki_reference.url_manual_mapping, tuple))
    for i, value in enumerate(rna_wiki_reference.url_manual_mapping):
        try:
            assert(len(value) == 2)
            assert(isinstance(value[0], str))
            assert(isinstance(value[1], str))
        except:
            print("Expected a tuple of 2 strings, instead item %d is a %s: %r" % (i, type(value), value))
            import traceback
            traceback.print_exc()
            raise


# a stripped down version of api_dump() in rna_info_dump.py
def test_lookup_coverage():

    def rna_ids():
        import rna_info
        struct = rna_info.BuildRNAInfo()[0]
        for struct_id, v in sorted(struct.items()):
            props = [(prop.identifier, prop) for prop in v.properties]
            struct_path = "bpy.types.%s" % struct_id[1]
            for prop_id, prop in props:
                yield (struct_path, "%s.%s" % (struct_path, prop_id))

        for submod_id in dir(bpy.ops):
            op_path = "bpy.ops.%s" % submod_id
            for op_id in dir(getattr(bpy.ops, submod_id)):
                yield (op_path, "%s.%s" % (op_path, op_id))

    # check coverage
    from bl_operators import wm

    set_group_all = set()
    set_group_doc = set()

    for rna_group, rna_id in rna_ids():
        url = wm.WM_OT_doc_view_manual._lookup_rna_url(rna_id, verbose=False)
        print(rna_id, "->", url)

        set_group_all.add(rna_group)
        if url is not None:
            set_group_doc.add(rna_group)

    # finally report undocumented groups
    print("")
    print("---------------------")
    print("Undocumented Sections")

    for rna_group in sorted(set_group_all):
        if rna_group not in set_group_doc:
            print("%s.*" % rna_group)


def test_language_coverage():
    pass  # TODO


def test_urls():
    import sys
    import rna_wiki_reference

    import urllib.error
    from urllib.request import urlopen

    prefix = rna_wiki_reference.url_manual_prefix
    urls = {suffix for (rna_id, suffix) in rna_wiki_reference.url_manual_mapping}

    urls_len = "%d" % len(urls)
    print("")
    print("-------------" + "-" * len(urls_len))
    print("Testing URLS %s" % urls_len)
    print("")

    color_red = '\033[0;31m'
    color_green = '\033[1;32m'
    color_normal = '\033[0m'

    urls_fail = []

    for url in sorted(urls):
        url_full = prefix + url
        print("  %s ... " % url_full, end="")
        sys.stdout.flush()
        try:
            urlopen(url_full)
            print(color_green + "OK" + color_normal)
        except urllib.error.HTTPError:
            print(color_red + "FAIL!" + color_normal)
            urls_fail.append(url)

    if urls_fail:
        urls_len = "%d" % len(urls_fail)
        print("")
        print("------------" + "-" * len(urls_len))
        print("Failed URLS %s" % urls_len)
        print("")
        for url in urls_fail:
            print("  %s%s%s" % (color_red, url, color_normal))


def main():
    test_data()
    test_lookup_coverage()
    test_language_coverage()
    test_urls()

if __name__ == "__main__":
    main()
