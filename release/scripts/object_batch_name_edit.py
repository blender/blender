#!BPY
"""
Name: 'Batch Object Name Edit'
Blender: 240
Group: 'Object'
Tooltip: 'Apply the chosen rule to rename all selected objects at once.'
"""
__author__ = "Campbell Barton"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.0"

__bpydoc__ = """\
"Batch Object Name Edit" allows you to change multiple names of Blender
objects at once.  It provides options to define if you want to: replace text
in the current names, truncate their beginnings or endings or prepend / append
strings to them.

Usage:
Select the objects to be renamed and run this script from the Object->Scripts
menu of the 3d View.
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
import bpy

global renameCount
renameCount = 0
obsel = [ob for ob in bpy.data.scenes.active.objects.context if not ob.lib]

def setDataNameWrapper(ob, newname):
	if ob.getData(name_only=1) == newname:
		return False
	
	data= ob.getData(mesh=1)
	
	if data and not data.lib:
		data.name= newname
		return True
	return False

def main():
	global renameCount
	# Rename the datablocks that are used by the object.
	def renameLinkedDataFromObject():
		
		# Result 1, we want to rename data
		for ob in obsel:
			if ob.name == ob.getData(name_only=1):
				return # Alredy the same name, dont bother.
			
			data = ob.getData(mesh=1) # use mesh so we dont have to update the nmesh.
			if data and not data.lib:
				data.name = ob.name
	
	
	def new():
		global renameCount
		NEW_NAME_STRING = Draw.Create('')
		RENAME_LINKED = Draw.Create(0)
		pup_block = [\
		('New Name: ', NEW_NAME_STRING, 19, 19, 'New Name'),\
		('Rename ObData', RENAME_LINKED, 'Renames objects data to match the obname'),\
		]
		
		if not Draw.PupBlock('Replace in name...', pup_block):
			return 0
		
		NEW_NAME_STRING= NEW_NAME_STRING.val
		
		Window.WaitCursor(1)
		for ob in obsel:
			if ob.name != NEW_NAME_STRING:
				ob.name = NEW_NAME_STRING
				renameCount+=1
		
		return RENAME_LINKED.val
		
	def replace():
		global renameCount
		REPLACE_STRING = Draw.Create('')
		WITH_STRING = Draw.Create('')
		RENAME_LINKED = Draw.Create(0)
		
		pup_block = [\
		('Replace: ', REPLACE_STRING, 19, 19, 'Text to find'),\
		('With:', WITH_STRING, 19, 19, 'Text to replace with'),\
		('Rename ObData', RENAME_LINKED, 'Renames objects data to match the obname'),\
		]
		
		if not Draw.PupBlock('Replace in name...', pup_block) or\
		((not REPLACE_STRING.val) and (not WITH_STRING)):
			return 0
		
		REPLACE_STRING = REPLACE_STRING.val
		WITH_STRING = WITH_STRING.val
		
		Window.WaitCursor(1)
		for ob in obsel:
			newname = ob.name.replace(REPLACE_STRING, WITH_STRING)
			if ob.name != newname:
				ob.name = newname
				renameCount+=1
		return RENAME_LINKED.val


	def prefix():
		global renameCount
		PREFIX_STRING = Draw.Create('')
		RENAME_LINKED = Draw.Create(0)
		
		pup_block = [\
		('Prefix: ', PREFIX_STRING, 19, 19, 'Name prefix'),\
		('Rename ObData', RENAME_LINKED, 'Renames objects data to match the obname'),\
		]
		
		if not Draw.PupBlock('Prefix...', pup_block) or\
		not PREFIX_STRING.val:
			return 0
		
		PREFIX_STRING = PREFIX_STRING.val
		
		Window.WaitCursor(1)
		for ob in obsel:
			ob.name = PREFIX_STRING + ob.name
			renameCount+=1 # we knows these are different.
		return RENAME_LINKED.val

	def suffix():
		global renameCount
		SUFFIX_STRING = Draw.Create('')
		RENAME_LINKED = Draw.Create(0)
		
		pup_block = [\
		('Suffix: ', SUFFIX_STRING, 19, 19, 'Name suffix'),\
		('Rename ObData', RENAME_LINKED, 'Renames objects data to match the obname'),\
		]
		
		if not Draw.PupBlock('Suffix...', pup_block) or\
		not SUFFIX_STRING.val:
			return 0
		
		SUFFIX_STRING = SUFFIX_STRING.val
		
		Window.WaitCursor(1)
		for ob in obsel:
			ob.name =  ob.name + SUFFIX_STRING
			renameCount+=1 # we knows these are different.
		return RENAME_LINKED.val	

	def truncate_start():
		global renameCount
		TRUNCATE_START = Draw.Create(0)
		RENAME_LINKED = Draw.Create(0)
		
		pup_block = [\
		('Truncate Start: ', TRUNCATE_START, 0, 19, 'Truncate chars from the start of the name'),\
		('Rename ObData', RENAME_LINKED, 'Renames objects data to match the obname'),\
		]
		
		if not Draw.PupBlock('Truncate Start...', pup_block) or\
		not TRUNCATE_START.val:
			return 0
			
		Window.WaitCursor(1)
		TRUNCATE_START = TRUNCATE_START.val
		for ob in obsel:
			newname = ob.name[TRUNCATE_START: ]
			ob.name = newname
			renameCount+=1
		
		return RENAME_LINKED.val

	def truncate_end():
		global renameCount
		TRUNCATE_END = Draw.Create(0)
		RENAME_LINKED = Draw.Create(0)
		
		pup_block = [\
		('Truncate End: ', TRUNCATE_END, 0, 19, 'Truncate chars from the end of the name'),\
		('Rename ObData', RENAME_LINKED, 'Renames objects data to match the obname'),\
		]
		
		if not Draw.PupBlock('Truncate End...', pup_block) or\
		not TRUNCATE_END.val:
			return 0
			
		Window.WaitCursor(1)
		TRUNCATE_END = TRUNCATE_END.val
		for ob in obsel:
			newname = ob.name[: -TRUNCATE_END]
			ob.name = newname
			renameCount+=1
		
		return RENAME_LINKED.val

	def renameObjectFromLinkedData():
		global renameCount
		Window.WaitCursor(1)
		
		for ob in obsel:
			newname = ob.getData(name_only=1)
			if newname != None and ob.name != newname:
				ob.name = newname
				renameCount+=1
		return 0
	
	def renameObjectFromDupGroup():
		global renameCount
		Window.WaitCursor(1)
		
		for ob in obsel:
			group= ob.DupGroup
			if group != None:
				newname= group.name
				if newname != ob.name:
					ob.name = newname
					renameCount+=1
		return 0
		
	def renameLinkedDataFromObject():
		global renameCount
		Window.WaitCursor(1)
		
		for ob in obsel:
			if setDataNameWrapper(ob, ob.name):
				renameCount+=1
		return 0
	
	name = "Selected Object Names%t|New Name|Replace Text|Add Prefix|Add Suffix|Truncate Start|Truncate End|Rename Objects to Data Names|Rename Objects to DupGroup Names|Rename Data to Object Names"
	result = Draw.PupMenu(name)
	renLinked = 0 # Rename linked data to the object name?
	if result == -1:
		return
	elif result == 1: renLinked= new()
	elif result == 2: renLinked= replace()
	elif result == 3: renLinked= prefix()
	elif result == 4: renLinked= suffix()
	elif result == 5: renLinked= truncate_start()
	elif result == 6: renLinked= truncate_end()
	elif result == 7: renameObjectFromLinkedData()
	elif result == 8: renameObjectFromDupGroup()
	elif result == 9: renameLinkedDataFromObject()
	
	if renLinked:
		renameLinkedDataFromObject()
	
	Window.WaitCursor(0)
	
	Draw.PupMenu('renamed: %d objects.' % renameCount)

if __name__=='__main__':
	main()
