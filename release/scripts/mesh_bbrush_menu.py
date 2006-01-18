#!BPY
"""
Name: 'B-Brush Sculpter'
Blender: 240
Group: 'Mesh'
Tooltip: 'Sculpt the active mesh (adds a scriptlink)'
"""

import Blender
def main():
	name = 'mesh_bbrush.py'
	for t in Blender.Text.Get():
		if t.name.startswith(name):
			Blender.Draw.PupMenu('ERROR: Script "%s" alredy imported, aborting load.' % name)
			return
	
	# Load the text
	datadir = Blender.Get('datadir')
	if not datadir.endswith(Blender.sys.sep):
		datadir += Blender.sys.sep
	path= datadir + name
	try:
		t = Blender.Text.Load(path)
	except:
		Blender.Draw.PupMenu('ERROR: "%s" not found.' % path)
		
	pup_input = [\
	'B-Brush Usage (message only)%t',
	'Enable B-Brush by',
	'selecting the "View" menu, then',
	'"Space Handeler Scripts",',
	'"%s"' % name,
	'LMB to sculpt, RMB for prefs, Shift reverses pressure',
	]
	
	#Blender.Draw.PupBlock('%s loaded' % name ,pup_input)
	Blender.Draw.PupMenu('|'.join(pup_input))
	
if __name__ == '__main__':

	
	
	main()
	