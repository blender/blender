#!BPY
"""
Name: 'Metaball Generation'
Blender: 245
Group: 'ScriptTemplate'
Tooltip: 'Script template to make metaballs from a mesh'
"""

from Blender import Window
import bpy

script_data = \
'''#!BPY
"""
Name: 'My Metaball Script'
Blender: 245
Group: 'Misc'
Tooltip: 'Put some useful info here'
"""

# Add a license here if you wish to re-disribute, we recommend the GPL

from Blender import Metaball, Mesh, Window
import bpy

def makeMetaSculpture(sce):
    #Create a base mesh for our sculpture to use
    monkey = Mesh.Primitives.Monkey()
    
    #Create a new meta datablock to use and give it a name
    metaObj = Metaball.New()
    metaObj.name = "MetaSuzanne"
    
    #Increase the resolution so it looks better
    metaObj.wiresize = 0.2
    metaObj.rendersize = 0.1
    
    #The radius for our new meta objects to take
    metaRadius = 2.0
    
    for f in monkey.faces:
        
        #Create a new metaball as part of the Meta Object Data
        newBall = metaObj.elements.add()
        
        #Make the new ball have the same coordinates as a vertex on our Mesh
        newBall.co = f.cent
        
        #Assign the same radius to all balls
        newBall.radius = f.area * metaRadius
    
    #Create the new object and put our meta data there
    sce.objects.new(metaObj, "MetaSuzanne")
        

def main():
    scene = bpy.data.scenes.active #Get the active scene
    
    Window.WaitCursor(1)
    
    #Call the sculpture making function
    makeMetaSculpture(scene)
    
    Window.WaitCursor(0)
    
    #Redraw the Screen When Finished
    Window.RedrawAll(1)

if __name__ == '__main__':
    main() 
'''

new_text = bpy.data.texts.new('metaball_template.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
