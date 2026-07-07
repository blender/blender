# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# this script updates XML themes once new settings are added
#
#  ./blender.bin --background --python ./tools/utils_maintenance/blender_update_themes.py

__all__ = (
    "main",
)

import bpy
import os


def update(filepath):
    import _rna_xml as rna_xml
    context = bpy.context

    print("Updating theme: {!r}".format(filepath))
    preset_xml_map = (
        ("preferences.themes[0]", "Theme"),
        ("preferences.ui_styles[0]", "Theme"),
    )
    rna_xml.xml_file_run(
        context,
        filepath,
        preset_xml_map,
    )

    rna_xml.xml_file_write(
        context,
        filepath,
        preset_xml_map,
    )


def update_default(filepath):
    with open(filepath, 'w', encoding='utf-8') as fh:
        fh.write("""<bpy>
  <Theme>
  </Theme>
  <ThemeStyle>
  </ThemeStyle>
</bpy>
""")


def main():
    for path in bpy.utils.preset_paths("interface_theme"):
        for fn in os.listdir(path):
            if fn.endswith(".xml"):
                fn_full = os.path.join(path, fn)
                if fn == "blender_dark.xml":
                    update_default(fn_full)
                else:
                    update(fn_full)


if __name__ == "__main__":
    main()
