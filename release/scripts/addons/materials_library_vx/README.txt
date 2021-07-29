# #####BEGIN GPL LICENSE BLOCK #####
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
# #####END GPL LICENSE BLOCK #####

bl_info = {
	"name": "Material Library",
	"author": "Mackraken (mackraken2023@hotmail.com)",
	"version": (0, 5, 61),
	"blender": (2, 7, 2),
	"api": 60995,
	"location": "Properties > Material",
	"description": "Material Library VX",
	"warning": "",
	"wiki_url": "https://sites.google.com/site/aleonserra/home/scripts/matlib-vx",
	"tracker_url": "",
	"category": "System"}

MATLIB 5.6.1

Installation:
- AVOID USING THE INSTALL ADDON BUTTON
- Copy the matlib folder inside Blender's addons.
Example: D:\Blender\2.72\scripts\addon\

- Start Blender.
- Goto File->User Preferences->Addons
- Enable "Material Library"


Updates:
v 0.5.61
- Libraries arent read on each draw call, only on startup or when added. This fixes potential crashes and is less stressful, but  when a library is deleted blender should be restarted.
-Moved the addon from "System" category to "Materials"

v 0.5.6
- Create new libraries.
	Libraries are read from the matlib folder. If you want to change this behaviour, edit the variable "matlib_path" at line 40. (Untested)
	
	To delete a library delete the blend file within the matlib folder.

- Apply material to all selected objects.

- Apply material in edit mesh mode.

- Improved Material preview.
	You can apply a material to the last selected object/s while you are previewing.

- Categories are saved within the library blend file.

- More warnings when things goes wrong.

- Options Added:
	- Force Import. False By default.
		This option helps to avoid material duplicates when the same material its applied several times.
		When this option is disabled the script will try to find the selected material within the working scene, instead of importing a new one from the library. 
	- Linked.
		Import the material by making a link to the library.
	- Hide search.
		Shows or hides the search box.


