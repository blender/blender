### WARNING:
# I don't have a clue what I'm doing here!

import win32api
### Following is the "normal" approach,
### but it requires loading the entire win32con file (which is big)
### for two values...
##import win32con
##HKEY_CLASSES_ROOT = win32con.HKEY_CLASSES_ROOT
##REG_SZ = win32con.REG_SZ

### These are the hard-coded values, should work everywhere as far as I know...
HKEY_CLASSES_ROOT = 0x80000000
REG_SZ= 1

def associate( extension, filetype, description="", commands=(), iconfile="" ):
	'''Warning: I don't have a clue what I'm doing here!
	extension -- extension including "." character, e.g. .proc
	filetype -- formal name, no spaces allowed, e.g. SkeletonBuilder.RulesFile
	description -- human-readable description of the file type
	commands -- sequence of (command, commandline), e.g. (("Open", "someexe.exe %1"),)
	iconfile -- optional default icon file for the filetype
	'''
	win32api.RegSetValue(
		HKEY_CLASSES_ROOT,
		extension,
		REG_SZ,
		filetype
	)
	if description:
		win32api.RegSetValue(
			HKEY_CLASSES_ROOT ,
			filetype,
			REG_SZ,
			description
		)
	if iconfile:
		win32api.RegSetValue(
			HKEY_CLASSES_ROOT ,
			"%(filetype)s\\DefaultIcon" % locals(),
			REG_SZ,
			iconfile
		)
	for (command, commandline) in commands:
		win32api.RegSetValue(
			HKEY_CLASSES_ROOT ,
			"%(filetype)s\\Shell\\%(command)s" % locals(),
			REG_SZ,
			command,
		)
		win32api.RegSetValue(
			HKEY_CLASSES_ROOT ,
			"%(filetype)s\\Shell\\%(command)s\\Command" % locals(),
			REG_SZ,
			commandline
		)

if __name__ == "__main__":
	associate(
		".proc",
		"SkeletonBuilder.Processing",
		"SkeletonBuilder Processing File",
		(("Open", '''z:\\skeletonbuilder\\skeletonbuilder.exe "%1" %*'''),),
		'''z:\\skeletonbuilder\\bitmaps\\skeletonbuildericon.ico''',
	)