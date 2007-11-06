#!BPY

"""
Name: 'Fix Broken Paths'
Blender: 242
Group: 'Image'
Tooltip: 'Search for new image paths to make relative links to'
"""

__author__ = "Campbell Barton AKA Ideasman"
__url__ = ["blenderartist.org"]

__bpydoc__ = """\
Find image target paths

This script searches for images whos
file paths do not point to an existing image file,
all image paths are made relative where possible.
usefull when moving projects between computers, when absolute paths links are broken.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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

try:
	import os
except:
	Draw.PupMenu('You need a full python install to use this script')
	os= None


#==============================================#
# Strips the slashes from the back of a string #
#==============================================#
def stripPath(path):
	return path.split('/')[-1].split('\\')[-1]

# finds the file starting at the root.
def findImage(findRoot, imagePath):
	newImageFile = None
	
	imageFile = imagePath.split('/')[-1].split('\\')[-1]
	
	# ROOT, DIRS, FILES
	pathWalk = os.walk(findRoot)
	pathList = [True]
	
	matchList = [] # Store a list of (match, size), choose the biggest.
	while True:
		try:
			pathList  = pathWalk.next()
		except:
			break
		
		for file in pathList[2]:
			# FOUND A MATCH
			if file.lower() == imageFile.lower():
				name = pathList[0] + sys.sep + file
				try:
					size = os.path.getsize(name)
				except:
					size = 0
					
				if size:
					print '   found:', name 
					matchList.append( (name, size) )
		
	if matchList == []:
		print 'no match for:', imageFile
		return None
	else:
		# Sort by file size
		matchList.sort(lambda A, B: cmp(B[1], A[1]) )
		
		print 'using:', matchList[0][0]
		# First item is the largest
		return matchList[0][0] # 0 - first, 0 - pathname
		

# Makes the pathe relative to the blend file path.
def makeRelative(path, blendBasePath):
	if path.startswith(blendBasePath):
		path = path.replace(blendBasePath, '//')
		path = path.replace('//\\', '//')
	return path

def find_images(findRoot):
	print findRoot
	
	# findRoot = Draw.PupStrInput ('find in: ', '', 100)
	
	if findRoot == '':
		Draw.PupMenu('No Directory Selected')
		return
	
	# Account for //
	findRoot = sys.expandpath(findRoot)
	
	# Strip filename
	while findRoot[-1] != '/' and findRoot[-1] != '\\':
		findRoot = findRoot[:-1]
	
	
	if not findRoot.endswith(sys.sep):
		findRoot += sys.sep
	
	
	if findRoot != '/' and not sys.exists(findRoot[:-1]):
		Draw.PupMenu('Directory Dosent Exist')
	
	blendBasePath = sys.expandpath('//')
	
	
	Window.WaitCursor(1)
	# ============ DIR DONE\
	images = Image.Get()
	len_images = float(len(images))
	for idx, i in enumerate(images):

		progress = idx / len_images
		Window.DrawProgressBar(progress, 'searching for images')
		
		# If files not there?
		if not sys.exists(sys.expandpath(i.filename )):	
			newImageFile = findImage(findRoot, i.filename)
			if newImageFile != None:
				newImageFile= makeRelative(newImageFile, blendBasePath)
				print 'newpath relink:', newImageFile
				i.filename = newImageFile
				i.reload()
		else:
			# Exists
			newImageFile= makeRelative(i.filename, blendBasePath)
			if newImageFile!=i.filename:
				print 'newpath relative:', newImageFile
				i.filename = newImageFile
			
	
	Window.RedrawAll()
	Window.DrawProgressBar(1.0, '')
	Window.WaitCursor(0)

if __name__ == '__main__' and os:
	Window.FileSelector(find_images, 'SEARCH ROOT DIR', sys.expandpath('//'))


