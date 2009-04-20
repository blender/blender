#!BPY
"""
Name: 'Find by Data Use'
Blender: 242
Group: 'Object'
Tooltip: 'Find an object by the data it uses'
"""
__author__= "Campbell Barton"
__url__= ["blender.org", "blenderartists.org"]
__version__= "1.0"

__bpydoc__= """
"""

# --------------------------------------------------------------------------
# Find by Data Use v0.1 by Campbell Barton (AKA Ideasman42)
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

from Blender import Image, sys, Draw, Window, Scene, Group
import bpy
import BPyMessages


def get_object_images(ob):
	# Could optimize this
	if ob.type != 'Mesh':
		return []
	
	me = ob.getData(mesh=1)

	if not me.faceUV:
		return []

	unique_images = {}
	
	orig_uvlayer = me.activeUVLayer 
	
	for uvlayer in me.getUVLayerNames():
		me.activeUVLayer = uvlayer
		for f in me.faces:
			i = f.image
			if i: unique_images[i.name] = i
	
	me.activeUVLayer = orig_uvlayer
	
	
	# Now get material images
	for mat in me.materials:
		if mat:
			for mtex in mat.getTextures():
				if mtex:
					tex = mtex.tex
					i = tex.getImage()
					if i: unique_images[i.name] = i
	
	return unique_images.values()
	
	
	
	# Todo, support other object types, materials
	return []
	
	

def main():
	
	NAME_DATA= Draw.Create('')
	NAME_INGROUP= Draw.Create('')
	NAME_DUPGROUP= Draw.Create('')
	NAME_IMAGE= Draw.Create('')
	NAME_MATERIAL= Draw.Create('')
	NAME_TEXTURE= Draw.Create('')
	
	
	PREF_CASESENS= Draw.Create(False)
	PREF_PART_MATCH= Draw.Create(True)
	
	
	# Get USER Options
	pup_block= [\
	('ObData:', NAME_DATA, 0, 32, 'Match with the objects data name'),\
	('InGroup:', NAME_INGROUP, 0, 32, 'Match with the group name to find one of its objects'),\
	('DupGroup:', NAME_DUPGROUP, 0, 32, 'Match with the group name to find an object that instances this group'),\
	('Image:', NAME_IMAGE, 0, 32, 'Match with the image name to find an object that uses this image'),\
	('Material:', NAME_MATERIAL, 0, 32, 'Match with the material name to find an object that uses this material'),\
	('Texture:', NAME_TEXTURE, 0, 32, 'Match with the texture name to find an object that uses this texture'),\
	('Case Sensitive', PREF_CASESENS, 'Do a case sensitive comparison?'),\
	('Partial Match', PREF_PART_MATCH, 'Match when only a part of the text is in the data name'),\
	]
	
	if not Draw.PupBlock('Find object using dataname...', pup_block):
		return
	
	NAME_DATA = NAME_DATA.val
	NAME_INGROUP = NAME_INGROUP.val
	NAME_DUPGROUP = NAME_DUPGROUP.val
	NAME_IMAGE = NAME_IMAGE.val
	NAME_MATERIAL = NAME_MATERIAL.val
	NAME_TEXTURE = NAME_TEXTURE.val
	
	PREF_CASESENS = PREF_CASESENS.val
	PREF_PART_MATCH = PREF_PART_MATCH.val
	
	if not PREF_CASESENS:
		NAME_DATA = NAME_DATA.lower()
		NAME_INGROUP = NAME_INGROUP.lower()
		NAME_DUPGROUP = NAME_DUPGROUP.lower()
		NAME_IMAGE = NAME_IMAGE.lower()
		NAME_MATERIAL = NAME_MATERIAL.lower()
		NAME_TEXTURE = NAME_TEXTURE.lower()
	
	def activate(ob, scn):
		bpy.data.scenes.active = scn
		scn.objects.selected = []
		scn.Layers = ob.Layers & (1<<20)-1
		ob.sel = 1
	
	def name_cmp(name_search, name_found):
		if name_found == None: return False
		if not PREF_CASESENS: name_found = name_found.lower()
		if PREF_PART_MATCH:
			if name_search in name_found:
				# print name_found, name_search
				return True
		else:
			if name_found == name_search:
				# print name_found, name_search
				return True
		
		return False
	
	
	if NAME_INGROUP:
		# Best we speed this up.
		bpy.data.objects.tag = False
		
		ok = False
		for group in bpy.data.groups:
			if name_cmp(NAME_INGROUP, group.name):
				for ob in group.objects:
					ob.tag = True
					ok = True
		if not ok:
			Draw.PupMenu('No Objects Found')
			return
	
	for scn in bpy.data.scenes:
		for ob in scn.objects:
			if NAME_DATA:
				if name_cmp(NAME_DATA, ob.getData(1)):
					activate(ob, scn)
					return
			if NAME_INGROUP:
				# Crap and slow but not much we can do about that
				'''
				for group in bpy.data.groups:
					if name_cmp(NAME_INGROUP, group.name):
						for ob_group in group.objects:
							if ob == ob_group:
								activate(ob, scn)
								return
				'''
				# Use speedup, this is in a group whos name matches.
				if ob.tag:
					activate(ob, scn)
					return
			
			if NAME_DUPGROUP:
				if ob.DupGroup and name_cmp(NAME_DUPGROUP, ob.DupGroup.name):
					activate(ob, scn)
					return
			
			if NAME_IMAGE:
				for img in get_object_images(ob):
					if name_cmp(NAME_IMAGE, img.name) or name_cmp(NAME_IMAGE, img.filename.split('\\')[-1].split('/')[-1]):
						activate(ob, scn)
						return
			if NAME_MATERIAL or NAME_TEXTURE:
				try:	materials = ob.getData(mesh=1).materials
				except:	materials = []
				
				# Add object materials
				materials.extend(ob.getMaterials())
				
				for mat in materials:
					if mat:
						if NAME_MATERIAL:
							if name_cmp(NAME_MATERIAL, mat.name):
								activate(ob, scn)
								return
						if NAME_TEXTURE:
							for mtex in mat.getTextures():
								if mtex:
									tex = mtex.tex
									if tex:
										if name_cmp(NAME_TEXTURE, tex.name):
											activate(ob, scn)
											return
	
	
	Draw.PupMenu('No Objects Found')

if __name__ == '__main__':
	main()
