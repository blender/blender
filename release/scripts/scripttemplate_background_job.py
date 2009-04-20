#!BPY
"""
Name: 'Background Job Example'
Blender: 248
Group: 'ScriptTemplate'
Tooltip: 'Script template for automating tasks from the command line with blender'
"""

from Blender import Window
import bpy

script_data = \
'''# This script is an example of how you can run blender from the command line (in background mode with no interface)
# to automate tasks, in this example it creates a text object, camera and light, then renders and/or saves it.
# This example also shows how you can parse command line options to python scripts.
# 
# Example usage for this test.
#  blender -b -P $HOME/background_job.py -- --text="Hello World" --render="/tmp/hello" --save="/tmp/hello.blend"
# 
# Notice all python args are after the '--' argument.

import Blender
import bpy

def example_function(body_text, save_path, render_path):
	
	sce= bpy.data.scenes.active

	txt_data= bpy.data.curves.new('MyText', 'Text3d')
	
	# Text Object
	txt_ob = sce.objects.new(txt_data)			# add the data to the scene as an object
	txt_data.setText(body_text)					# set the body text to the command line arg given
	txt_data.setAlignment(Blender.Text3d.MIDDLE)# center text
	
	# Camera
	cam_data= bpy.data.cameras.new('MyCam')		# create new camera data
	cam_ob= sce.objects.new(cam_data)			# add the camera data to the scene (creating a new object)
	sce.objects.camera= cam_ob					# set the active camera
	cam_ob.loc= 0,0,10
	
	# Lamp
	lamp_data= bpy.data.lamps.new('MyLamp')
	lamp_ob= sce.objects.new(lamp_data)
	lamp_ob.loc= 2,2,5

	if save_path:
		try:
			f= open(save_path, 'w')
			f.close()
			ok= True
		except:
			print 'Cannot save to path "%s"' % save_path
			ok= False
		
		if ok:
			Blender.Save(save_path, 1)
	
	if render_path:
		render= sce.render
		render.extensions= True
		render.renderPath = render_path
		render.sFrame= 1
		render.eFrame= 1
		render.renderAnim()



import sys		# to get command line args
import optparse	# to parse options for us and print a nice help message

script_name= 'background_job.py'

def main():
	
	# get the args passed to blender after "--", all of which are ignored by blender specifically
	# so python may receive its own arguments
	argv= sys.argv

	if '--' not in argv:
		argv = [] # as if no args are passed
	else:	
		argv = argv[argv.index('--')+1: ] # get all args after "--"
	
	# When --help or no args are given, print this help
	usage_text =  'Run blender in background mode with this script:\n'
	usage_text += '  blender -b -P ' + script_name + ' -- [options]'
			
	parser = optparse.OptionParser(usage = usage_text)
	

	# Example background utility, add some text and renders or saves it (with options)
	# Possible types are: string, int, long, choice, float and complex.
	parser.add_option('-t', '--text', dest='body_text', help='This text will be used to render an image', type='string')

	parser.add_option('-s', '--save', dest='save_path', help='Save the generated file to the specified path', metavar='FILE')
	parser.add_option('-r', '--render', dest='render_path', help='Render an image to the specified path', metavar='FILE')

	options, args = parser.parse_args(argv) # In this example we wont use the args
	
	if not argv:
		parser.print_help()
		return

	if not options.body_text:
		print 'Error: --text="some string" argument not given, aborting.\n'
		parser.print_help()
		return
	
	# Run the example function
	example_function(options.body_text, options.save_path, options.render_path)

	print 'batch job finished, exiting'


if __name__ == '__main__':
	main()
'''

new_text = bpy.data.texts.new('background_job.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()

