# this is the importloader which blender calls on unknown
# file types

import importer

supported= {'wrl': importer.VRMLimporter}

def process(name):
	# run through importerlist and check for magic
	m = None
	for modname in importer.importers:
		mod = getattr(importer, modname)
		if mod.checkmagic(name):
			m = mod
			break
	if not m:
		return 0
	m.importfile(name)
	#except:
		#import sys
		#print "Import failed"sys.exc_value
	return 1

