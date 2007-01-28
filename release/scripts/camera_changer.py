#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Camera Changer'
Blender: 234
Group: 'Animation'
Tip: 'Create script link to change cameras (based on their names) during an animation'
"""

__author__ = '3R - R3gis'
__version__ = '1.2'
__url__ = ["Author's site , http://cybercreator.free.fr", "French Blender support forum, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender"]
__email__=["3R, r3gis@free.fr"]


__bpydoc__ = """\
This script creates an script link to change cameras during an animation.

The created script link (a Blender Text) is linked to Scene Frame Changed events.

Usage:

Run the script, then name the camera Object with the number of the frame(s)
where you want this camera to become active.

For example:<br>
  - a camera called "10" will become active at frame 10.<br>
  - a camera called "10,25,185" will become active at frames 10, 25 and 185.  

Notes:<br>
  - This script creates another script named camera.py, which is linked to the current scene.<br>
  - If there is already a text called "camera.py", but it's from an old version or is not recognized,
you can choose if you want to rename or overwrite it.
  - Script inspired by Jean-Michel (jms) Soler's:<br>
    http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_changerdecamera.htm
"""


# $Id$
#
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004-2005: Regis Montoya
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

#Script inspired of the idea of this one :
#http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_changerdecamera.htm
#
#----------------------------------------------
# R3gis Montoya (3R)
#
# Pout tout probleme a:
# cybercreator@free.fr
# ---------------------------------------------

import Blender
from Blender import *
import string

header = '# camera.py 1.3 scriptlink'

camera_change_scriptlink = header + \
'''
import Blender
def main():
	scn = Blender.Scene.GetCurrent()
	frame = str(Blender.Get('curframe'))

	# change the camera if it has the current frame
	for ob_cam in [ob for ob in scn.objects if ob.type == 'Camera']:
		for number in ob_cam.name.split(','):
			if number == frame:
				scn.setCurrentCamera(ob_cam)
				return
main()
'''

def main():
	
	# Get the text
	try:	cam_text = Blender.Text.Get('camera.py')
	except:	cam_text = None
	
	if cam_text:
		if cam_text.asLines()[0] != header:
			ret = Blender.Draw.PupMenu("WARNING: An old camera.py exists%t|Overwrite|Rename old version text")
			if ret == -1:			return # EXIT DO NOTHING
			elif ret == 1:		Text.unlink(cam_text)
			elif ret == 2:		cam_text.name = 'old_camera.txt'
			cam_text = None

	if not cam_text:
		scripting=Blender.Text.New('camera.py')
		scripting.write(camera_change_scriptlink)
	
	scn=Scene.GetCurrent()
	scriptlinks = scn.getScriptLinks('FrameChanged')
	if not scriptlinks or ('camera.py' not in scriptlinks):
		scn.addScriptLink('camera.py','FrameChanged')
		Blender.Draw.PupMenu('FrameChange Scriptlink Added%t|Name camera objects to their activation frame numbers(s) seperated by commas|valid names are "1,10,46" or "1,10,200" or "200" (without quotation marks)')
		Blender.Window.RedrawAll()

if __name__ == '__main__':
	main()