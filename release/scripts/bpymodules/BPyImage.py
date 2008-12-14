# --------------------------------------------------------------------------
# BPyImage.py version 0.15
# --------------------------------------------------------------------------
# helper functions to be used by other scripts
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

#===========================================================================#
# Comprehensive image loader, will search and find the image                #
# Will return a blender image or a new image if the image is missing        #
#===========================================================================#
import bpy
from Blender import sys
try:
	import os
except:
	os=None

#==============================================#
# Return directory, where the file is          #
#==============================================#
def stripFile(path):
	lastSlash = max(path.rfind('\\'), path.rfind('/'))
	if lastSlash != -1:
		path = path[:lastSlash]
		newpath= '%s%s' % (path, sys.sep)
	else:
		newpath= path
	return newpath

#==============================================#
# Strips the slashes from the back of a string #
#==============================================#
def stripPath(path):
	return path.split('/')[-1].split('\\')[-1]

#====================================================#
# Strips the prefix off the name before writing      #
#====================================================#
def stripExt(name): # name is a string
	index = name.rfind('.')
	if index != -1:
		return name[ : index ]
	else:
		return name

def getExt(name):
	index = name.rfind('.')
	if index != -1:
		return name[index+1:]
	return name

#====================================================#
# Adds a slash to the end of a path if its not there #
#====================================================#
def addSlash(path):
	if not path:
		return ''
	
	elif path.endswith('\\') or path.endswith('/'):
		return path
	return path + sys.sep


def comprehensiveImageLoad(imagePath, filePath, PLACE_HOLDER= True, RECURSIVE=True, VERBOSE=False, CONVERT_CALLBACK=None):
	'''
	imagePath: The image filename
		If a path precedes it, this will be searched as well.
		
	filePath: is the directory where the image may be located - any file at teh end will be ignored.
	
	PLACE_HOLDER: if True a new place holder image will be created.
		this is usefull so later you can relink the image to its original data.
	
	VERBOSE: If True debug info will be printed.
	
	RECURSIVE: If True, directories will be recursivly searched.
		Be carefull with this if you have files in your root directory because it may take a long time.
	
	CASE_INSENSITIVE: for non win32 systems, find the correct case for the file.
	
	CONVERT_CALLBACK: a function that takes an existing path and returns a new one.
		Use this when loading image formats blender may not support, the CONVERT_CALLBACK
		can take the path for a GIF (for example), convert it to a PNG and return the PNG's path.
		For formats blender can read, simply return the path that is given.
	'''
	
	# VERBOSE = True
	
	if VERBOSE: print 'img:', imagePath, 'file:', filePath
	
	if os == None and CASE_INSENSITIVE:
		CASE_INSENSITIVE = True
	
	# When we have the file load it with this. try/except niceness.
	def imageLoad(path):
		#if path.endswith('\\') or path.endswith('/'):
		#	raise 'INVALID PATH'
		
		if CONVERT_CALLBACK:
			path = CONVERT_CALLBACK(path)
		
		try:
			img = bpy.data.images.new(filename=path)
			if VERBOSE: print '\t\tImage loaded "%s"' % path
			return img
		except:
			if VERBOSE:
				if sys.exists(path): print '\t\tImage failed loading "%s", mabe its not a format blender can read.' % (path)
				else: print '\t\tImage not found, making a place holder "%s"' % (path)
			if PLACE_HOLDER:
				img= bpy.data.images.new(stripPath(path),4,4)
				img.filename= path
				return img #blank image
			else:
				return None
			
	# Image formats blender can read
	IMAGE_EXT = ['jpg', 'jpeg', 'png', 'tga', 'bmp', 'rgb', 'sgi', 'bw', 'iff', 'lbm', # Blender Internal
	'gif', 'psd', 'tif', 'tiff', 'pct', 'pict', 'pntg', 'qtif'] # Quacktime, worth a try.
	
	imageFileName =  stripPath(imagePath) # image path only
	imageFileName_lower =  imageFileName.lower() # image path only
	
	if VERBOSE: print '\tSearchingExisting Images for "%s"' % imagePath
	for i in bpy.data.images:
		if stripPath(i.filename.lower()) == imageFileName_lower:
			if VERBOSE: print '\t\tUsing existing image.'
			return i
	
	
	if VERBOSE: print '\tAttempting to load "%s"' % imagePath
	if sys.exists(imagePath):
		if VERBOSE: print '\t\tFile found where expected "%s".' % imagePath
		return imageLoad(imagePath)
	
	
	
	imageFileName_noext = stripExt(imageFileName) # With no extension.
	imageFileName_noext_lower = stripExt(imageFileName_lower) # With no extension.
	imageFilePath = stripFile(imagePath)
	
	# Remove relative path from image path
	if imageFilePath.startswith('./') or imageFilePath.startswith('.\\'):
		imageFilePath = imageFilePath[2:]
	
	
	# Attempt to load from obj path.
	tmpPath = stripFile(filePath) + stripPath(imageFileName)
	if sys.exists(tmpPath):
		if VERBOSE: print '\t\tFile found in path (1)"%s".' % tmpPath
		return imageLoad(tmpPath)
	
	
	# os needed if we go any further.
	if not os:
		if VERBOSE: print '\t\tCreating a placeholder with a face path: "%s".' % imagePath
		return imageLoad(imagePath) # Will jus treturn a placeholder.
	
	
	# We have os.
	# GATHER PATHS.
	paths = {} # Store possible paths we may use, dict for no doubles.
	tmpPath = addSlash(sys.expandpath('//')) # Blenders path
	if sys.exists(tmpPath):
		if VERBOSE: print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
	else:
		if VERBOSE: print '\tNo Path: "%s"' % tmpPath
	
	tmpPath = imageFilePath
	if sys.exists(tmpPath):
		if VERBOSE: print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
	else:
		if VERBOSE: print '\tNo Path: "%s"' % tmpPath

	tmpPath = stripFile(filePath)
	if sys.exists(tmpPath):
		if VERBOSE: print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
	else:
		if VERBOSE: print '\tNo Path: "%s"' % tmpPath

	tmpPath = addSlash(bpy.config.textureDir)
	if tmpPath and sys.exists(tmpPath):
		if VERBOSE: print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
	else:
		if VERBOSE: print '\tNo Path: "%s"' % tmpPath
	
	# Add path if relative image patrh was given.
	tmp_paths= paths.keys()
	for k in tmp_paths:
		tmpPath = k + imageFilePath
		if sys.exists(tmpPath):
			paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
			paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
			paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
		else:
			if VERBOSE: print '\tNo Path: "%s"' % tmpPath
	# DONE
	# 
	for path, files in paths.iteritems():
		if sys.exists(path + imageFileName):
			if VERBOSE: print '\tFound image at path: "%s" file" "%s"' % (path, imageFileName)
			return imageLoad(path + imageFileName)
		
		# If the files not there then well do a case insensitive seek.
		filesOrigCase = files[0]
		filesLower = files[1]
		filesLowerNoExt = files[2]
		
		# We are going to try in index the file directly, if its not there just keep on
		
		index = None
		try:
			# Is it just a case mismatch?
			index = filesLower.index(imageFileName_lower)
		except:
			try:
				# Have the extensions changed?
				index = filesLowerNoExt.index(imageFileName_noext_lower)
				
				ext = getExt( filesLower[index] ) # Get the extension of the file that matches all but ext.
				
				# Check that the ext is useable eg- not a 3ds file :)
				if ext.lower() not in IMAGE_EXT:
					index = None
			
			except:
				index = None
		
		if index != None:
			tmpPath = path + filesOrigCase[index]
			img = imageLoad( tmpPath )
			if img != None:
				if VERBOSE: print '\t\tImage Found "%s"' % tmpPath
				return img
	
	if RECURSIVE:
		# IMAGE NOT FOUND IN ANY OF THE DIRS!, DO A RECURSIVE SEARCH.
		if VERBOSE: print '\t\tImage Not Found in any of the dirs, doing a recusrive search'
		for path in paths.iterkeys():
			# Were not going to use files
			if path == '/' or len(path) == 3 and path[1:] == ':\\':
				continue
			
			# print path , 'ASS'
			
			#------------------
			# finds the file starting at the root.
			#	def findImage(findRoot, imagePath):
			#W---------------
			
			# ROOT, DIRS, FILES
			pathWalk = os.walk(path)
			pathList = [True]
			
			matchList = [] # Store a list of (match, size), choose the biggest.
			while True:
				try:
					pathList  = pathWalk.next()
				except:
					break
				
				for file in pathList[2]:
					file_lower = file.lower()
					# FOUND A MATCH
					if (file_lower == imageFileName_lower) or\
					(stripExt(file_lower) == imageFileName_noext_lower and getExt(file_lower) in IMAGE_EXT):
						name = pathList[0] + sys.sep + file
						size = os.path.getsize(name)
						if VERBOSE: print '\t\t\tfound:', name 
						matchList.append( (name, size) )
			
			if matchList:
				# Sort by file size
				matchList.sort(lambda A, B: cmp(B[1], A[1]) )
				
				if VERBOSE: print '\t\tFound "%s"' % matchList[0][0]
				
				# Loop through all we have found
				img = None
				for match in matchList:
					img = imageLoad(match[0]) # 0 - first, 0 - pathname
					if img != None:
						break
				return img
	
	# No go.
	if VERBOSE: print '\t\tImage Not Found after looking everywhere! "%s"' % imagePath
	return imageLoad(imagePath) # Will jus treturn a placeholder.
