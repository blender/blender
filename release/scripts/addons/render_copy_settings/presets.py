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


class CopyPreset(object):
    def __init__(self, ui_name, rna_enum, elements):
        self.ui_name = ui_name
        self.rna_enum = rna_enum
        self.elements = elements


presets = (CopyPreset("Resolution",
                      ("resolution", "Render Resolution", "Render resolution and aspect ratio settings"),
                      {"resolution_x", "resolution_y", "pixel_aspect_x", "pixel_aspect_y"}),
           CopyPreset("Scale",
                      ("scale", "Render Scale", "The “Render Scale” setting"),
                      {"resolution_percentage"}),
           CopyPreset("OSA",
                      ("osa", "Render OSA", "The OSA toggle and sample settings"),
                      {"use_antialiasing", "antialiasing_samples"}),
           CopyPreset("Threads",
                      ("threads", "Render Threads", "The thread mode and number settings"),
                      {"threads_mode", "threads"}),
           CopyPreset("Fields",
                      ("fields", "Render Fields", "The Fields settings"),
                      {"use_fields", "field_order", "use_fields_still"}),
           CopyPreset("Stamp",
                      ("stamp", "Render Stamp", "The Stamp toggle"),
                      {"use_stamp"})
          )
