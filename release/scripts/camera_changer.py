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

Run the script, then name the camera Object with the number of the frame(s) where you
want this camera to become active.

For example:<br>
  - a camera called "10" will become active at frame 10.<br>
  - a camera called "10,25,185" will become active at frames 10, 25 and 185.  

Notes:<br>
  - This script creates another script named camera.py, which is linked to the current scene.<br>
  - If there is already a text called "camera.py", but it's from an old version or is not recognized,
you can choose if you want to rename or overwrite it.
"""


# $Id$
#
#Script in the same idea that this one :
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

sc=Scene.GetCurrent()

#import du texte
lestext=Blender.Text.Get()
Ntexts=[]
for txt in lestext:
	Ntexts.append(txt.getName())
ecrire=0

if 'camera.py' not in Ntexts:
	ecrire=1
else :
	if lestext[Ntexts.index('camera.py')].asLines()[0] != "# camera.py 1.2 link python #":
		reecrire=Blender.Draw.PupMenu("WARNING: Text camera.py already exists but is outdated%t|Overwrite|Rename old version text")
		if reecrire == 1:
			Text.unlink(lestext[Ntexts.index('camera.py')])
			ecrire=1
		if reecrire == 2:
			lestext[Ntexts.index('camera.py')].name="old_camera.txt"
			ecrire=1



if ecrire == 1:
	scripting=Blender.Text.New('camera.py')
	scripting.write("# camera.py 1.2 link python #\nimport Blender\nfrom Blender import *\nfrom math import *\nimport string\n")
	scripting.write("sc=Scene.GetCurrent()\n#Changement camera\nlescam=[]\nobjets=Blender.Object.Get()\n")
	scripting.write("for ob in objets:\n	if type(ob.getData())==Blender.Types.CameraType:\n		try:")
	scripting.write("\n			lesfram=string.split(ob.name,',')\n			for fr in lesfram:\n				lescam.append(ob.name)\n				lescam.append(int(fr))\n		except:\n			pass")
	scripting.write("\nframe = Blender.Get('curframe')\nif frame in lescam:\n	nom=lescam.index(frame)\n	sc.setCurrentCamera(Blender.Object.Get(lescam[nom-1]))\n")


#Linkage
list=[]
try:
	for script in sc.getScriptLinks('FrameChanged'):
		list.append(script)
except:
	pass
if 'camera.py' not in list:
	sc.addScriptLink('camera.py','FrameChanged')
	Blender.Draw.PupMenu("Done! Remember:%t|Name cameras as (a comma separated list of) their activation frame number(s)")
	Blender.Redraw(-1)

