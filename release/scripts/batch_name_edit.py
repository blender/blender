#!BPY

"""
Name: 'Batch Object Name Edit'
Blender: 232
Group: 'Object'
Tooltip: 'Apply the chosen rule to rename all selected objects at once.'
"""

# $Id$
#
# --------------------------------------------------------------------------
# Batch Name Edit by Campbell Barton (AKA Ideasman)
# --------------------------------------------------------------------------
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
# --------------------------------------------------------------------------

from Blender import *

def replace():
	replace = Draw.PupStrInput('replace: ', '', 32)
	if replace == None: return
		
	with = Draw.PupStrInput('with: ', '', 32)
	if with == None: return
	
	for ob in Object.GetSelected():
		
		if replace in ob.name:
			chIdx = ob.name.index(replace)
			
			# Remove the offending word and replace it with - 'with'
			ob.name = ob.name[ :chIdx] + with + ob.name[chIdx + len(replace):]
			

def prefix():
	prefix = Draw.PupStrInput('prefix: ', '', 32)
	if prefix == None: return
	
	for ob in Object.GetSelected():
		ob.name = prefix + ob.name


def suffix():
	suffix = Draw.PupStrInput('suffix: ', '', 32)
	if suffix == None: return
	
	for ob in Object.GetSelected():
		ob.name = ob.name + suffix

def truncate_start():
	truncate = Draw.PupIntInput('truncate start: ', 0, 0, 31)
	if truncate != None:
		for ob in Object.GetSelected():
			ob.name = ob.name[truncate: ]

def truncate_end():
	truncate = Draw.PupIntInput('truncate end: ', 0, 0, 31)
	if truncate == None: return
	
	for ob in Object.GetSelected():
		ob.name = ob.name[ :-truncate]




name = "Selected Object Names%t|Replace Text|Add Prefix|Add Suffix|Truncate Start|Truncate End"
result = Draw.PupMenu(name)

if result == -1:
	pass
elif result == 1:
	replace()
elif result == 2:
	prefix()
elif result == 3:
	suffix()
elif result == 4:
	truncate_start()
elif result == 5:
	truncate_end()


