#!BPY

"""
Name: 'Armature Symmetry'
Blender: 234
Group: 'Animation'
Tooltip: 'Make an armature symetrical'
"""

__author__ = "Jonas Petersen"
__url__ = ("blender", "elysiun", "Script's homepage, http://www.mindfloaters.de/blender/", "thread at blender.org, http://www.blender.org/modules.php?op=modload&name=phpBB2&file=viewtopic&t=4858")
__version__ = "0.9 2004-11-10"

__doc__ = """\
This script creates perfectly symmetrical armatures.

With default configuration it will:<br>
  - Look for bones that have the reference suffix (".L") and
adjust/create the according opposite bone (suffix ".R");<br>
  - Center align all bones that _don't_ have the suffix ".X".

Please check the script's homepage and the thread at blender.org (last link button above) for more info.

For this version users need to edit the script code to change default options.
"""

# --------------------------------------------------------------------------
# "Armature Symmetry" by Jonas Petersen
# Version 0.9 - 10th November 2004 - first public release
# --------------------------------------------------------------------------
#
# A script for creating perfectly symmetrical armatures.
#
# It is available in Object Mode via the menu item:
#
#   Object -> Scripts -> Armature Symmetry
#
# With default configuration it will:
#
#   - Look for bones that have the reference suffix (".L") and
#     adjust/create the according opposite bone (suffix ".R").
#
#   - Center align all bones that _don't_ have the suffix ".X"
#
# Find the latest version at: http://www.mindfloaters.de/blender/
#
# --------------------------------------------------------------------------
# $Id$
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004: Jonas Petersen, jonas at mindfloaters dot de
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
# CONFIGURATION
# --------------------------------------------------------------------------

# Note: Theses values will later be editable via a gui interface
# within Blender.

# CENTER_SUFFIX is the suffix for bones that should (or shouldn't) get
# center aligned. The default is '.X'.
CENTER_SUFFIX = '.X'

# CENTER_SUFFIX_MODE:
#
#   'include'  only bones with the CENTER_SUFFIX appended
#              get center aligned.
#
#   'exclude'  (default)
#              all bones except those with the CENTER_SUFFIX
#              appended get center aligned.
#
#
#   'off'      bones will not get center aligned at all.
#
CENTER_SUFFIX_MODE = 'exclude'

# The suffix for the reference and opposite side of the
# armature. Bone positions of the opposite side will be overwritten by
# the mirrored values of the reference side.
# The default is REF_SUFFIX = '.L' and OPP_SUFFIX = '.R'.
REF_SUFFIX = '.L'
OPP_SUFFIX = '.R'

# MIRROR_AXIS defines the axis in which bones are mirrored/aligned.
# Values:
#   0 for X (default)
#   1 for Y
#   2 for Z
MIRROR_AXIS = 0

# --------------------------------------------------------------------------
# END OF CONFIGURATION
# --------------------------------------------------------------------------

import Blender

def splitName(bone):
	name = bone.getName()
	base = name[0:len(name)-ref_suffix_len]
	rsuff = name[-ref_suffix_len:len(name)]
	csuff = name[-center_suffix_len:len(name)]
	return name, base, rsuff, csuff

ref_suffix_len = len(REF_SUFFIX);
center_suffix_len = len(CENTER_SUFFIX);
armature_selected = False

obj_list = Blender.Object.GetSelected()
for obj in obj_list:
    if obj.getType() == "Armature":
		armature_selected = True
		arm = obj.getData()
		bones = arm.getBones()
		bonehash = {}

		for bone in bones:
			bonehash[bone.getName()] = bone

		for bone in bones:
			name, base, rsuff, csuff = splitName(bone)

			# reference bone?
			if (rsuff == REF_SUFFIX):
				oppname = base + OPP_SUFFIX

				# create opposite bone if necessary
				if not bonehash.has_key(oppname):
					bonehash[oppname]=Blender.Armature.Bone.New(oppname)
					parent = bone.getParent()
					if parent:
						pname, pbase, prsuff, pcsuff = splitName(parent)
						if prsuff == REF_SUFFIX:
							poppname = pbase + OPP_SUFFIX
							if bonehash.has_key(poppname):
								bonehash[oppname].setParent(bonehash[poppname])
						else:
							bonehash[oppname].setParent(parent)
					arm.addBone(bonehash[oppname])

				# mirror bone coords

				tail = bone.getTail()
				tail[MIRROR_AXIS] *= -1;
				bonehash[oppname].setTail(tail)

				head = bone.getHead()
				head[MIRROR_AXIS] *= -1;
				bonehash[oppname].setHead(head)

				roll = -bone.getRoll()
				bonehash[oppname].setRoll(roll)

				# Write access to ik flag not (yet?) supported in Blender (2.34)
				#if bone.hasParent():
				#	bonehash[oppname].setIK(not bone.getIK())

			# center bone?
			elif (rsuff != OPP_SUFFIX) and \
			     (CENTER_SUFFIX_MODE != 'off') and \
			     ((CENTER_SUFFIX_MODE == 'exclude' and csuff != CENTER_SUFFIX) or \
			     (CENTER_SUFFIX_MODE == 'include' and csuff == CENTER_SUFFIX)):

				# center bone coords

				tail = bone.getTail()
				tail[MIRROR_AXIS] = 0.0;
				bone.setTail(tail)

				head = bone.getHead()
				head[MIRROR_AXIS] = 0.0;
				bone.setHead(head)

				# Setting set roll in python rotates all child bones.
				# Not so if set via the Transform Properties in Blender.
				# Bug?
				bone.setRoll(0.0)

if not armature_selected:
	Blender.Draw.PupMenu("Armature Symmetry%t|Please select an Armature object!")
