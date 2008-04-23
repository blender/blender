#!BPY
"""
Name: 'Edit Externaly'
Blender: 242a
Group: 'Image'
Tooltip: 'Open in an application for editing. (hold Shift to configure)'
"""

__author__ = "Campbell Barton"
__url__ = ["blender", "blenderartists.org"]
__version__ = "1.0"

__bpydoc__ = """\
This script opens the current image in an external application for editing.

Useage:
Choose an image for editing in the UV/Image view.

To configure the application to open the image with, hold Shift as you click on
this menu item.

For first time users try running the default application for your operating system.
If the application does not open you can type in the full path.
You can choose that the last entered application will be saved as a default.

* Note, default commants for opening an image are "start" for win32 and "open" for macos.
This will use the system default assosiated application.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton 2006
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
# --------------------------------------------------------------------------


try:
	import os
	import sys as py_sys
	platform = py_sys.platform
except:
	Draw.PupMenu('Error, python not installed')
	os=None

import Blender
from Blender import Image, sys, Draw, Registry

def edit_extern(image=None):
	
	if not image:
		image = Image.GetCurrent()
	
	if not image: # Image is None
		Draw.PupMenu('ERROR: You must select an active Image.')
		return
	if image.packed:
		Draw.PupMenu('ERROR: Image is packed, unpack before editing.')
		return
	
	imageFileName = sys.expandpath( image.filename )
	
	if not sys.exists(imageFileName):
		Draw.PupMenu('ERROR: Image path does not exist.')
		return
	
	pupblock = [imageFileName.split('/')[-1].split('\\')[-1]]
	
	new_text= False
	try:
		appstring = Registry.GetKey('ExternalImageEditor', True)
		appstring = appstring['path']
		
		# for ZanQdo if he removed the path from the textbox totaly. ;) - Cam
		if not appstring or appstring.find('%f')==-1:
			new_text= True
	except:
		new_text= True
	
	if new_text:
		pupblock.append('first time, set path.')
		if platform == 'win32':
			appstring = 'start "" /B "%f"'
		elif platform == 'darwin':
			appstring = 'open "%f"'
		else:
			appstring = 'gimp-remote "%f"'
	
	appstring_but = Draw.Create(appstring)
	save_default_but = Draw.Create(0)
	
	pupblock.append(('editor: ', appstring_but, 0, 48, 'Path to application, %f will be replaced with the image path.'))
	pupblock.append(('Set Default', save_default_but, 'Store this path in the blender registry.'))
	
	# Only configure if Shift is held,
	if Blender.Window.GetKeyQualifiers() & Blender.Window.Qual.SHIFT:
		if not Draw.PupBlock('External Image Editor...', pupblock):
			return
	
	appstring = appstring_but.val
	save_default= save_default_but.val
	
	if save_default:
		Registry.SetKey('ExternalImageEditor', {'path':appstring}, True)
	
	if appstring.find('%f') == -1:
		Draw.PupMenu('ERROR: The comment you entered did not contain the filename ("%f")')
		return
	
	# -------------------------------
	
	appstring = appstring.replace('%f', imageFileName)
	print '\tediting image with command "%s"' % appstring
	os.system(appstring)


def main():
	edit_extern()
	

if __name__ == '__main__' and os != None:
	main()