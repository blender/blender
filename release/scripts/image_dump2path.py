#!BPY
"""
Name: 'Dump All Images to Path'
Blender: 242
Group: 'Image'
Tooltip: 'Copy and reference all images to a new path.'
"""
__author__= "Campbell Barton"
__url__= ["blender.org", "blenderartists.org"]
__version__= "1.0"

__bpydoc__= """

This script copies all the images used by 1 blend to a spesified path and references the new images from Blender
Usefull for moving projects between computers or when you reference many images. naming collisions and multiple images using the same image path are delt with properly only creating new image names when needed.

Blender images will reference the newly copied files - So be mindfull when you save your blend after running the script.

Notes, images with the path "Untitled will be ignored"

Image path collisions are managed by enumerating the path names so images will never be overwritten at the target path.
"""

from Blender import Image, sys, Draw, Window
import BPyMessages

def copy_file(srcpath, destpath):
	f=open(srcpath, 'rb')
	data= f.read()
	f.close()
	f= open(destpath, 'wb')
	f.write(data)
	f.close()

# Makes the pathe relative to the blend file path.
def makeRelative(path, blendBasePath):
	if path.startswith(blendBasePath):
		path = path.replace(blendBasePath, '//')
		path = path.replace('//\\', '//')
	return path

def makeUnique(path):
	
	if not sys.exists(path):
		return path
	
	orig_path = path
	orig_path_noext, ext= sys.splitext(path)
	
	i= 1
	while sys.exists(path):
		path = '%s_%.3d%s' % (orig_path_noext, i, ext)
		i+= 1
	
	return path


def main():
	#images= [(img, img.name, sys.expandpath(img.filename)) for img in Image.Get() if img.filename != 'Untitled' and img.name not in  ("Render Result", "Compositor")]
	
	# remove double paths so we dont copy twice
	image_dict= {}
	image_missing = []
	
	
	# Make a dict of images with thir file name as a key
	for img in Image.Get():
		name= img.name
		filename= sys.expandpath(img.filename)
		
		if filename== 'Untitled' or name == "Render Result" or name == "Compositor":
			continue
		
		if not sys.exists(filename):
			#continue # ignore missing images.
			image_missing.append(name)
		
		
		try:	image_dict[filename].append(img)
		except: image_dict[filename]= [img]
	
	
	if image_missing:
		ret= Draw.PupMenu( 'Aborting, Image file(s) missing%t|' + '|'.join(image_missing) )
		if ret != -1:
			Image.Get(image_missing[ret-1]).makeCurrent()
		return
	
	# Chech done - select a dir
	def dump_images(dump_path):
		
		tot = len(image_dict)
		count = 0
		
		print 'starting the image dump'
		
		dump_path = sys.dirname(dump_path)
		base_blen_path = sys.expandpath('//')
		
		if BPyMessages.Error_NoDir(dump_path): return
		
		# ahh now were free to copy the images.
		for filename, imgs in image_dict.iteritems():
			count +=1
			file= filename.split('\\')[-1].split('/')[-1]
			new_filename= makeUnique( sys.join(dump_path, file) )
			
			print ' copying image "%s" %d of %d' % (dump_path, count, tot)
			print '   source path:', filename
			print '   target path:', new_filename
			
			copy_fail= False
			try:
				copy_file( filename, new_filename)
			except:
				copy_fail = True
			
			if copy_fail or sys.exists(new_filename)==0:
				print '\tERROR could not copy the file above!'
				Draw.PupMenu('Error%t|Copy Failed, do not save this Blend file|"' + filename + '", see console for details.')
				return
			
			# account for 
			for img in imgs:
				img.filename = makeRelative(new_filename, base_blen_path)
			
		msg= 'Relinking %d images done' % len(image_dict)
		Draw.PupMenu(msg)
		
		Window.RedrawAll()
		
		print 'done'
	
	Window.FileSelector(dump_images, 'IMG DUMP DIR', '')

if __name__ == '__main__':
	main()
