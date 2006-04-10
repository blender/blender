#!BPY

"""
Name: 'Find Image Target Paths'
Blender: 241
Group: 'UV'
Tooltip: 'Finds all image paths from this blend and references the new paths'
"""

__author__ = "Campbell Barton AKA Ideasman"
__url__ = ["http://members.iinet.net.au/~cpbarton/ideasman/", "blender", "elysiun"]

__bpydoc__ = """\
"""
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
def makeRelative(path):
	blendBasePath = sys.expandpath('//')
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
				newImageFile = makeRelative(newImageFile)
				print 'newpath:', newImageFile
				i.filename = newImageFile
				i.reload()
	
	Window.RedrawAll()
	Window.DrawProgressBar(1.0, '')
	Window.WaitCursor(0)

if __name__ == '__main__' and os != None:
	Window.FileSelector(find_images, 'SEARCH ROOT DIR', sys.expandpath('//'))


