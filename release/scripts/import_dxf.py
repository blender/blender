#!BPY

# """
# Name: 'Drawing eXchange Format (.dxf)'
# Blender: 243
# Group: 'Import'
# Tooltip: 'Import DXF file.'
# """
__author__ = 'Kitsu (Ed Blake)'
__version__ = '0.8 1/2007'
__url__ = ["elysiun.com", "BlenderArtists.org"]
__email__ = ["Kitsune_e@yahoo.com"]
__bpydoc__ = """\
This is a Blender import script for dxf files.

This script imports the dxf Geometery from dxf versions 2007 and earlier.

Supported:<br>
   At this time only mesh based imports are supported.<br>
   Future support for all curve import is planned.<br>
  <br>
Currently Supported DXF Ojects:<br>
     Lines<br>
     LightWeight polylines<br>
     True polylines<br>
     Text<br>
     Mtext<br>
     Circles<br>
     Arcs<br>
     Ellipses<br>
     Blocks<br>
     3Dfaces<br>

Known issues:<br>
   Does not convert perfectly between Object Coordinate System (OCS) 
   and World Coordinate System (WCS).  Only rudimentary support for
   true polylines have been implimented - splines/fitted curves/
   3d plines/polymeshes are not supported.
   No support for most 3d entities. Doesn't support the new style object 
   visability.  There are problems importing some curves/arcs/circles.

Notes:<br>
   This is primarally a 2d drawing release.  Currently only support for
   3d faces has been added.
   Blocks are created on layer 19 then referenced at each insert point.  The
   insert point is designated with a small 3d crosshair.  This handle does not render.

"""

# --------------------------------------------------------------------------
# DXF Import v0.8 by Ed Blake (AKA Kitsu)
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

import Blender
from Blender import *
Sys = sys
try:
    from dxfReader import readDXF
except ImportError:
    import sys
    curdir = Sys.dirname(Blender.Get('filename'))
    sys.path.append(curdir)

# development
#import dxfReader
#reload(dxfReader)

from dxfReader import readDXF
from dxfColorMap import color_map
from math import *



try:
    import os
    if os.name:# != 'mac':
        import psyco
        psyco.log()
        psyco.full(memory=100)
        psyco.profile(0.05, memory=100)
        psyco.profile(0.2)
except ImportError:
    pass

SCENE = Scene.GetCurrent()
WORLDX = Mathutils.Vector((1,0,0))
AUTO = BezTriple.HandleTypes.AUTO
BYLAYER=256

class Layer:
    """Dummy layer object."""
    def __init__(self, name, color, frozen):
        self.name = name
        self.color = color
        self.frozen = frozen
    

class MatColors:
    """A smart container for color based materials.
    
    This class is a wrapper around a dictionary mapping color indicies to materials.
    When called with a color index it returns a material corrisponding to that index.
    Behind the scenes it checks if that index is in its keys, and if not it creates
    a new material.  It then adds the new index:material pair to its dict and returns
    the material.
    """
    
    def __init__(self, map):
        """Expects a dictionary mapping layer names to color idices."""
        self.map = map
        self.colors = {}
    
    
    def __call__(self, color=None):
        """Return the material associated with color.
        
        If a layer name is provided the color of that layer is used.
        """
        if not color:
            color = 0
        if type(color) == str: # Layer name
            try:
                color = self.map[color].color # color = layer_map[name].color
            except KeyError:
                layer = Layer(name=color, color=0, frozen=False)
                self.map[color] = layer
                color = 0
        color = abs(color)
        if color not in self.colors: # .keys()
            self.add(color)
        return self.colors[color]
    
    
            
    
    def add(self, color):
        """Create a new material using the provided color index."""
        global color_map
        mat = Material.New('ColorIndex-%s' %color)
        mat.setRGBCol(color_map[color])
        mat.setMode("Shadeless", "Wire")
        self.colors[color] = mat
    



class Blocks:
    """A smart container for blocks.
    
    This class is a wrapper around a dictionary mapping block names to Blender data blocks.
    When called with a name string it returns a block corrisponding to that name.
    Behind the scenes it checks if that name is in its keys, and if not it creates
    a new data block.  It then adds the new name:block pair to its dict and returns
    the block.
    """
    
    def __init__(self, map, settings):
        """Expects a dictionary mapping block names to block objects."""
        self.map = map
        self.settings = settings
        self.blocks = {}
    
    
    def __call__(self, name=None):
        """Return the data block associated with name.
        
        If no name is provided return self.blocks.
        """
        if not name:
            return self.blocks
        if name not in self.blocks: # .keys():
            self.add(name)
        return self.blocks[name]
    
    
                
    def add(self, name):
        """Create a new block group for the block with name."""
        optimization = self.settings.optimization
        group = Group.New(name)
        block = self.map[name]
        if optimization <= 1:
            print "\nDrawing %s block entities..." %name
        drawEntities(block.entities, self.settings, group)
        if optimization <= 1:
            print "Done!"
        self.blocks[name] = group
    
    



class Settings:
    """A container for all the import settings and objects used by the draw functions.
    
    This is like a collection of globally accessable persistant properties and functions.
    """
    # Optimization constants
    MIN = 0
    MID = 1
    MAX = 2
    
    def __init__(self, drawing, curves):
        """Given the drawing initialize all the important settings used by the draw functions."""
        self.curves = curves
        self.layers = True
        self.blocks = True
        self.optimization = self.getOpt()
        
        # First sort out all the sections
        sections = dict([(item.name, item) for item in drawing.data])
        
        # The header section may be omited
        if self.optimization <= self.MID:
            if 'header' in sections: #.keys():
                print "Found header!"
            else:
                print "File contains no header!"
                
        # The tables section may be partialy or completely missing.
        if 'tables' in sections: # .keys():
            if self.optimization <= self.MID:
                print "Found tables!"
            tables = dict([(item.name, item) for item in sections["tables"].data])
            if 'layer' in tables: # .keys():
                if self.optimization <= self.MID:
                    print "Found layers!"
                # Read the layers table and get the layer colors
                self.colors = getLayers(drawing)
            else:
                if self.optimization <= self.MID:
                    print "File contains no layers table!"
                self.layers = False
                self.colors = MatColors({})
        else:
            if self.optimization <= self.MID:
                print "File contains no tables!"
                print "File contains no layers table!"
            self.layers = False
            self.colors = MatColors({})
            
        # The blocks section may be omited
        if 'blocks' in sections: #.keys():
            if self.optimization <= self.MID:
                print "Found blocks!"
            # Read the block definitions and build our block object
            self.blocks = getBlocks(drawing, self)
        else:
            if self.optimization <= self.MID:
                print "File contains no blocks!"
            self.blocks = False
    
    
    def getOpt(self):
        """Ask the user for update optimization level."""
        Window.WaitCursor(False)
        
        retval = Draw.PupIntInput('optimization: ', 1, 0, 2)
        print "Setting optimization level %s!" %retval
        
        Window.WaitCursor(True)
        return retval
    
    
    def isOff(self, name):
        """Given a layer name look up the layer object and return its visable status."""
        # colors are negative if layer is off
        try:
            layer = self.colors.map[name]
        except KeyError:
            return False
            
        if layer.frozen or layer.color < 0:
            return True
        else:
            return False
        
    
    

class Drawer:
    """Super 'function' for all the entitiy drawing functions.
    
        The code for the drawing functions was very repetitive, each differing
        by only a few lines at most.  So here is a callable class with methods 
        for each part of the import proccess.
    """
    
    def __init__(self, block=False):
        self.block = block
    
    
    
    def __call__(self, entities, settings, group=None):
        """Call with a list of entities and a settings object to generate Blender geometry."""
        if entities and settings.optimization <= settings.MID:
            print "Drawing %ss..." %entities[0].type,
            
        if self.block:
            # create one 'handle' data block to use with all blocks
            handle = Mesh.New('insert')
            handle.verts.extend(
                [(-0.01,0,0),
                (0.01,0,0),
                (0,-0.01,0),
                (0,0.01,0),
                (0,0,-0.01),
                (0,0,0.01)]
            )
            handle.edges.extend([(0,1),(2,3),(4,5)])
            
        # For now we only want model-space objects
        entities = [entity for entity in entities if entity.space == 0]
        
        if group:
            block_def = True
        else:
            block_def = False
        
        for entity in entities:
            if settings.optimization <= settings.MID:
                print '\b.',
            # First get the layer group
            if not block_def:
                group = self.getGroup('layer %s' %entity.layer) # add overhead just to make things a little cleaner
            
            if not self.block:
                ob = self.draw(entity, settings.curves)
            else:
                ob = self.draw(entity, handle, settings)
            
            self.setColor(entity, ob, settings)
            # Link it to the scene and add it to the correct group
            SCENE.link(ob)
            self.setGroup(group, ob)
            
            # Set the visability
            if settings.isOff(entity.layer):
                ob.Layers = 1<<19 # [20]
            elif block_def:
                ob.Layers = (1<<18) # [19]
            else:
                ob.Layers = (1<<20)-1 # [i+1 for i in xrange(20)] # all layers
            
            # # Set the visability
            # if settings.isOff(entity.layer) or block_def:
                # ob.restrictDisplay = True
                # ob.restrictRender = True
            
            if settings.optimization == settings.MIN:
                # I know it's slow to have Blender redraw after each entity type is drawn
                # But is it really slower than the progress bar?
                Blender.Redraw()
        if entities and settings.optimization <= settings.MID:
            print "\nFinished drawing %ss!" %entities[0].type    
    def getGroup(self, name):
        """Returns a Blender group object."""
        try:
            group = Group.Get(name)
        except: # What is the exception?
            group = Group.New(name)
        return group    
    def draw(self, entity):
        """Dummy method to be over written in subclasses."""
        pass
    
    
    def setColor(self, entity, ob, settings):
        # Set the color
        if entity.color_index == BYLAYER:
            mat = settings.colors(entity.layer)
        else:
            mat = settings.colors(entity.color_index)
        try:
            ob.setMaterials([mat])
        except ValueError:
            print "material error - %s!" %mat
        ob.colbits = 0x01 # Set OB materials.    
    def setGroup(self, group, it):
        try:
            group.objects.link(it)
        except:
            group.objects.append(it)

    
def main(filename):
    editmode = Window.EditMode()    # are we in edit mode?  If so ...
    if editmode: Window.EditMode(0) # leave edit mode before
    Window.WaitCursor(True)         # Let the user know we are thinking
    
    try:
        if not filename:
            print "DXF import: error, no file selected.  Attempting to load default file."
            try:
                filename = Sys.expandpath(r".\examples\big-test.dxf")
            except IOError:
                print "DXF import: error finding default test file, exiting..."
                return None
        if filename:
            drawing = readDXF(filename)
            drawDrawing(drawing)
    finally:
        # restore state even if things didn't work
        Window.WaitCursor(False)
        if editmode: Window.EditMode(1) # and put things back how we fond them
    
def getOCS(az):
    """An implimentation of the Arbitrary Axis Algorithm."""
    # world x, y, and z axis
    wx = WORLDX
    wy = Mathutils.Vector((0,1,0))
    wz = Mathutils.Vector((0,0,1))
    
    #decide if we need to transform our coords
    if az[0] == 0 and az[1] == 0:
        return False
    # elif abs(az[0]) < 0.0001 or abs(az[1]) < 0.0001:
        # return False
    az = Mathutils.Vector(az)
        
    cap = 0.015625 # square polar cap value (1/64.0)
    if abs(az.x) < cap and abs(az.y) < cap:
        ax = Mathutils.CrossVecs(wy, az)
    else:
        ax = Mathutils.CrossVecs(wz, az)
    ax = ax.normalize()
    ay = Mathutils.CrossVecs(az, ax)
    ay = ay.normalize()
    return ax, ay, az
    
def transform(normal, obj):
    """Use the calculated ocs to determine the objects location/orientation in space.
    
    Quote from dxf docs:
        The elevation value stored with an entity and output in DXF files is a sum
    of the Z-coordinate difference between the UCS XY plane and the OCS XY
    plane, and the elevation value that the user specified at the time the entity
    was drawn.
    """
    ocs = getOCS(normal)
    if ocs:
        #print ocs
        x, y, z = ocs
        x = x.resize4D()
        y = y.resize4D()
        z = -z.resize4D()
        x.w = 0
        y.w = 0
        z.w = 0
        o = Mathutils.Vector(obj.loc)
        o = o.resize4D()
        mat = Mathutils.Matrix(x, y, z, o)
        obj.setMatrix(mat)
    
def getLayers(drawing):
    """Build a dictionary of name:color pairs for the given drawing."""
    tables = drawing.tables
    for table in tables.data:
        if table.name == 'layer':
            layers = table
            break
    map = {}
    for item in layers.data:
        if type(item) != list and item.type == 'layer':
            map[item.name] = item
    colors = MatColors(map)
    return colors
def getBlocks(drawing, settings):
    """Build a dictionary of name:block pairs for the given drawing."""
    map = {}
    for item in drawing.blocks.data:
        if type(item) != list and item.type == 'block':
            try:
                map[item.name] = item
            except KeyError:
                # annon block
                print "Cannot map %s - %s!" %(item.name, item)
    blocks = Blocks(map, settings)
    return blocks
def drawDrawing(drawing):
    """Given a drawing object recreate the drawing in Blender."""
    print "Getting settings..."
    # The settings object controls how dxf entities are drawn
    settings = Settings(drawing, curves=False)
    
    if settings.optimization <= settings.MID:
        print "Drawings entities..."
    # Draw all the know entity types in the current scene
    drawEntities(drawing.entities, settings)
    
    # Set the visable layers
    SCENE.setLayers([i+1 for i in xrange(18)]) # SCENE.Layers = 262143 or (1<<18) 
    Blender.Redraw(-1)
    if settings.optimization <= settings.MID:
        print "Done!"
def drawEntities(entities, settings, group=None):
    """Draw every kind of thing in the entity list.
    
    If provided 'group' is the Blender group new entities are to be added to.
    """
    for _type, drawer in type_map.iteritems():
        # for each known type get a list of that type and call the associated draw function
        drawer(entities.get_type(_type), settings, group)
    

drawLines = Drawer()
def drawLine(line, curves=False):
    """Do all the specific things needed to import lines into Blender."""
    # Generate the geometery
    points = line.points
    edges = [[0, 1]]
    
    me = Mesh.New('line')          # create a new mesh
    
    me.verts.extend(points)          # add vertices to mesh
    me.edges.extend(edges)           # add edges to the mesh
    
    # Now Create an object
    ob = Object.New('Mesh', 'line')  # link mesh to an object
    ob.link(me)
    
    return ob
drawLines.draw = drawLine


drawLWpolylines = Drawer()
def drawLWpolyline(pline, curves=False):
    """Do all the specific things needed to import plines into Blender."""
    # Generate the geometery
    points = []
    for i in xrange(len(pline.points)):
        point = pline.points[i]
        if not point.bulge:
            points.append(point.loc)
        elif point.bulge and i < len(pline.points)-1:# > 0:
            center, radius, start, end = solveBulge(point, pline.points[i+1])
            #print center, radius, start, end
            verts, nosense = drawArc(center, radius, start, end)
            verts.pop(0) # remove first
            verts.pop() #remove last
            if point.bulge >= 0:
                verts.reverse()
            points.extend(verts)
    edges = [[num, num+1] for num in xrange(len(points)-1)]
    if pline.closed:
        edges.append([len(pline.points)-1, 0])

    me = Mesh.New('lwpline')          # create a new mesh
    
    me.verts.extend(points)          # add vertices to mesh
    me.edges.extend(edges)           # add edges to the mesh
    
    # Now Create an object
    ob = Object.New('Mesh', 'lwpline')  # link mesh to an object
    ob.link(me)
    transform(pline.extrusion, ob)
    ob.LocZ = pline.elevation
    
    return ob
drawLWpolylines.draw = drawLWpolyline

drawPolylines = Drawer()
def drawPolyline(pline, curves=False):
    """Do all the specific things needed to import plines into Blender."""
    # Generate the geometery
    points = []
    for i in xrange(len(pline.points)):
        point = pline.points[i]
        if not point.bulge:
            points.append(point.loc)
        elif point.bulge and i < len(pline.points)-1:# > 0:
            center, radius, start, end = solveBulge(point, pline.points[i+1])
            #print center, radius, start, end
            verts, nosense = drawArc(center, radius, start, end)
            verts.pop(0) # remove first
            verts.pop() #remove last
            if point.bulge >= 0:
                verts.reverse()
            points.extend(verts)
    edges = [[num, num+1] for num in xrange(len(points)-1)]
    if pline.closed:
        edges.append([len(pline.points)-1, 0])

    me = Mesh.New('pline')          # create a new mesh
    
    me.verts.extend(points)          # add vertices to mesh
    me.edges.extend(edges)           # add edges to the mesh
    
    # Now Create an object
    ob = Object.New('Mesh', 'pline')  # link mesh to an object
    ob.link(me)
    transform(pline.extrusion, ob)
    ob.LocZ = pline.elevation
    
    return ob
drawPolylines.draw = drawPolyline


def solveBulge(p1, p2):
    """return the center, radius, start angle, and end angle given two points.
    
    Needs to take into account bulge sign.
    negative = clockwise
    positive = counter-clockwise
    
    to find center given two points, and arc angle
    calculate radius
        Cord = sqrt(start^2 + end^2)
        S = (bulge*Cord)/2
        radius = ((Cord/2)^2+S^2)/2*S
    angle of arc = 4*atan( bulge )
    angle from p1 to center is (180-angle)/2
    get vector pointing from p1 to p2 (p2 - p1)
    normalize it and multiply by radius
    rotate around p1 by angle to center point to center.
    
    start angle = angle between (center - p1) and worldX
    end angle = start angle + angle of arc
    """
    bulge = p1.bulge
    p2 = Mathutils.Vector(p2.loc)
    p1 = Mathutils.Vector(p1.loc)
    cord = p2 - p1 # vector from p1 to p2
    clength = cord.length
    s = (bulge * clength)/2 # sagitta (height)
    radius = abs(((clength/2)**2 + s**2)/(2*s)) # magic formula
    angle = abs(degrees(4*atan(bulge))) # theta (included angle)
    delta = (180 - angle)/2 # the angle from cord to center
    if bulge > 0:
        delta = -delta
    radial = cord.normalize() * radius # a radius length vector aligned with cord
    rmat = Mathutils.RotationMatrix(delta, 3, 'Z')
    center = p1 + (rmat * radial) # rotate radial by delta degrees, then add to p1 to find center
    if bulge < 0:
        sv = (p1 - center) # start from point 2
    else:
        sv = (p2 - center) # start from point 1
    start = Mathutils.AngleBetweenVecs(sv, WORLDX) # start angle is the angle between the first leg of the section and the x axis
    # The next bit is my cludge to figure out if start should be negative
    rmat = Mathutils.RotationMatrix(start, 3, 'Z')
    rstart = rmat * sv
    if Mathutils.AngleBetweenVecs(rstart, WORLDX) < start:
        start = -start
    # the end angle is just 'angle' more than start angle
    end = start + angle
    return list(center), radius, start, end
drawTexts = Drawer()
def drawText(text, curves=False):
    """Do all the specific things needed to import texts into Blender."""
    # Generate the geometery
    txt = Text3d.New("text")
    txt.setSize(1)
    txt.setShear(text.oblique/90)
    txt.setExtrudeDepth(0.5)
    if text.halignment == 0:
        align = Text3d.LEFT
    elif text.halignment == 1:
        align = Text3d.MIDDLE
    elif text.halignment == 2:
        align = Text3d.RIGHT
    elif text.halignment == 3:
        align = Text3d.FLUSH
    else:
        align = Text3d.MIDDLE
    txt.setAlignment(align)
    txt.setText(text.value)
    
    # Now Create an object
    ob = Object.New('Text', 'text')  # link mesh to an object
    ob.link(txt)
    
    transform(text.extrusion, ob)
    
    # move the object center to the text location
    ob.loc = tuple(text.loc)
    # scale it to the text size
    ob.SizeX = text.height*text.width_factor
    ob.SizeY = text.height
    ob.SizeZ = text.height
    # and rotate it around z
    ob.RotZ = radians(text.rotation)
    
    return ob
drawTexts.draw = drawText    

drawMtexts = Drawer()
def drawMtext(text, curves=False):
    """Do all the specific things needed to import mtexts into Blender."""
    # Generate the geometery
    txt = Text3d.New("mtext")
    txt.setSize(1)
    # Blender doesn't give access to its text object width currently
    # only to the text3d's curve width...
    #txt.setWidth(text.width/10)
    txt.setLineSeparation(text.line_space)
    txt.setExtrudeDepth(0.5)
    txt.setText(text.value)
    
    # Now Create an object
    ob = Object.New('Text', 'mtext')  # link mesh to an object
    ob.link(txt)
    
    transform(text.extrusion, ob)
    
    # move the object center to the text location
    ob.loc = tuple(text.loc)
    # scale it to the text size
    ob.SizeX = text.height*text.width_factor
    ob.SizeY = text.height
    ob.SizeZ = text.height
    # and rotate it around z
    ob.RotZ = radians(text.rotation)
    
    return ob
drawMtexts.draw = drawMtext



drawCircles = Drawer()
def drawCircle(circle, curves=False):
    """Do all the specific things needed to import circles into Blender."""
    # Generate the geometery
    # Now Create an object
    if curves:
        ob = drawCurveCircle(circle)
    else:
        center = circle.loc
        radius = circle.radius
        
        circ = 2 * pi * radius
        if circ < 65: # if circumfrance is too small
            verts = 32 # set a fixed number of 32 verts
        else:
            verts = circ/.5 # figure out how many verts we need
            if verts > 100: # Blender only accepts values
                verts = 100 # [3:100]
            
        c = Mesh.Primitives.Circle(int(verts), radius*2)
    
        ob = Object.New('Mesh', 'circle')
        ob.link(c)                  # link curve data with this object
        
    ob.loc = tuple(center)
    transform(circle.extrusion, ob)
        
    return ob
drawCircles.draw = drawCircle

drawArcs = Drawer()
def drawArc(arc, curves=False):
    """Do all the specific things needed to import arcs into Blender."""
    # Generate the geometery
    # Now Create an object
    if curves:
        ob = drawCurveArc(arc)
    else:
        center = arc.loc
        radius = arc.radius
        start = arc.start_angle
        end = arc.end_angle
        verts, edges = drawArc(None, radius, start, end)
        
        a = Mesh.New('arc')
        
        a.verts.extend(verts)          # add vertices to mesh
        a.edges.extend(edges)           # add edges to the mesh
        
        ob = Object.New('Mesh', 'arc')
        ob.link(a)                  # link curve data with this object
        ob.loc = tuple(center)
        ob.RotX = radians(180)
    
    transform(arc.extrusion, ob)
    ob.size = (1,1,1)
    
    return ob
drawArcs.draw = drawArc


def drawArc(center, radius, start, end, step=0.5):
    """Draw a mesh arc with the given parameters."""
    # center is currently set by object
    
    # if start > end:
        # start = start - 360
    # if end > 360:
        # end = end%360
    startmatrix = Mathutils.RotationMatrix(start, 3, "Z")
    startpoint = startmatrix * Mathutils.Vector((radius, 0, 0))
    endmatrix = Mathutils.RotationMatrix(end, 3, "Z")
    endpoint = endmatrix * Mathutils.Vector((radius, 0, 0))
    points = [startpoint]
    
    if end < start:
        end +=360
    
    delta = end - start
    length = radians(delta) * radius
    if radius < step*10: # if circumfrance is too small
        pieces = int(delta/10) # set a fixed step of 10 degrees
    else:
        pieces = int(length/step) # figure out how many pieces we need for our arc
    if pieces == 0: # stupid way to avoid a div by zero error
        pieces = 1  # what would be a smarter way to fix this?
    step = delta/pieces # set step so pieces * step = degrees in arc
    
    stepmatrix = Mathutils.RotationMatrix(step, 3, "Z")
    point = Mathutils.Vector(startpoint)
    for i in xrange(int(pieces)):
        point = stepmatrix * point
        points.append(point)
    points.append(endpoint)
    
    if center:
        points = [[point[0]+center[0], point[1]+center[1], point[2]+center[2]] for point in points]
    edges = [[num, num+1] for num in xrange(len(points)-1)]
    
    return points, edges
drawEllipses = Drawer()
def drawEllipse(ellipse, curves=False):
    """Do all the specific things needed to import ellipses into Blender."""
    # Generate the geometery
    # Now Create an object
    if curves:
        ob = drawCurveArc(ellipse)
    else:
        major = Mathutils.Vector(ellipse.major)
        delta = Mathutils.AngleBetweenVecs(major, WORLDX)
        center = ellipse.loc
        radius = major.length
        start = degrees(ellipse.start_angle)
        end = degrees(ellipse.end_angle)
        verts, edges = drawArc(None, radius, start, end)
        
        e = Mesh.New('ellipse')
        
        e.verts.extend(verts)          # add vertices to mesh
        e.edges.extend(edges)           # add edges to the mesh
        
    
        ob = Object.New('Mesh', 'arc')
        ob.link(e)                  # link curve data with this object
        ob.loc = tuple(center)
        ob.SizeY = ellipse.ratio
        #ob.RotZ = radians(delta)
        ob.RotX = radians(180)

    
    transform(ellipse.extrusion, ob)
    ob.RotZ = radians(delta)
        
    return ob
drawEllipses.draw = drawEllipse
drawBlocks = Drawer(True)
def drawBlock(insert, handle, settings):
    """recursivly draw block objects.
    
    Blocks are made of three objects:
        the block_record in the tables section
        the block in the blocks section
        the insert object in the entities section
    
    block_records give the insert units, blocks provide the objects drawn in the
    block, and the insert object gives the location/scale/rotation of the block 
    instances.  To draw a block you must first get a group with all the
    blocks entities drawn in it, then scale the entities to match the world
    units, then dupligroup that data to an object matching each insert object."""
    if settings.blocks:
        # get our block group
        block = settings.blocks(insert.block)
    
        # Now Create an object
        ob = Object.New('Mesh', insert.block)
        ob.link(handle) # Give the object a handle
        ob.DupGroup = block
        ob.enableDupGroup = True
    else:    
        ob = Object.New('Mesh')
        
    ob.loc = tuple(insert.loc)
    transform(insert.extrusion, ob)
    ob.RotZ += radians(insert.rotation)
    ob.size = tuple(insert.scale)
            
    return ob
drawBlocks.draw = drawBlock
        

drawFaces = Drawer()
def drawFace(face, curves=False):
    """Do all the specific things needed to import 3d faces into Blender."""
    # Generate the geometery
    points = face.points
    if len(face.points) > 3:
        faces = [[0, 1, 2, 3]]
    else:
        faces = [[0, 1, 2]]
    
    me = Mesh.New('line')          # create a new mesh
    
    me.verts.extend(points)          # add vertices to mesh
    me.faces.extend(faces)           # add faces to the mesh
    
    # Now Create an object
    ob = Object.New('Mesh', '3dface')  # link mesh to an object
    ob.link(me)
    
    return ob
drawFaces.draw = drawFace
# Here are some alternate drawing functions for creating curve geometery.

def drawCurveCircle(circle):
    """Given a dxf circle object return a blender circle object using curves."""
    c = Curve.New('circle')             # create new  curve data
    
    center = circle.loc
    radius = circle.radius
    
    p1 = (0, -radius, 0)
    p2 = (radius, 0, 0)
    p3 = (0, radius, 0)
    p4 = (-radius, 0, 0)
    
    p1 = BezTriple.New(p1)
    p2 = BezTriple.New(p2)
    p3 = BezTriple.New(p3)
    p4 = BezTriple.New(p4)
    
    curve = c.appendNurb(p1)
    curve.append(p2)
    curve.append(p3)
    curve.append(p4)
    for point in curve:
        point.handleTypes = [AUTO, AUTO]
    curve.flagU = 1 # Set curve cyclic
    c.update()
    
    ob = Object.New('Curve', 'circle')    # make curve object
    return ob
    
def drawCurveArc(arc):
    """Given a dxf circle object return a blender circle object using curves."""
    if start > end:
        start = start - 360
    startmatrix = Mathutils.RotationMatrix(start, 3, "Z")
    startpoint = startmatrix * Mathutils.Vector((radius, 0, 0))
    endmatrix = Mathutils.RotationMatrix(end, 3, "Z")
    endpoint = endmatrix * Mathutils.Vector((radius, 0, 0))
    # Note: handles must be tangent to arc and of correct length...

    a = Curve.New('arc')             # create new  curve data
    
    center = circle.loc
    radius = circle.radius
    
    p1 = (0, -radius, 0)
    p2 = (radius, 0, 0)
    p3 = (0, radius, 0)
    p4 = (-radius, 0, 0)
    
    p1 = BezTriple.New(p1)
    p2 = BezTriple.New(p2)
    p3 = BezTriple.New(p3)
    p4 = BezTriple.New(p4)
    
    curve = a.appendNurb(p1)
    curve.append(p2)
    curve.append(p3)
    curve.append(p4)
    for point in curve:
        point.handleTypes = [AUTO, AUTO]
    curve.flagU = 1 # Set curve cyclic
    a.update()
    
    ob = Object.New('Curve', 'arc')    # make curve object
    return ob


type_map = {
    'line':drawLines,
    'lwpolyline':drawLWpolylines,
    'polyline':drawPolylines,
    'text':drawTexts,
    'mtext':drawMtexts,
    'circle':drawCircles,
    'arc':drawArcs,
    'ellipse':drawEllipses,
    'insert':drawBlocks,
    '3dface':drawFaces
}


if __name__ == "__main__":
    Window.FileSelector(main, 'Import a DXF file', '*.dxf')


"""
# For testing compatibility
if 1:
	# DEBUG ONLY
	TIME= Blender.sys.time()
	import os
	print 'Searching for files'
	os.system('find /metavr/ -iname "*.dxf" > /tmp/tempdxf_list')
	print '...Done'
	file= open('/tmp/tempdxf_list', 'r')
	lines= file.readlines()
	file.close()

	def between(v,a,b):
		if v <= max(a,b) and v >= min(a,b):
			return True		
		return False
		
	for i, _file in enumerate(lines):
		if between(i, 50,60):
			_file= _file[:-1]
			print 'Importing', _file, '\nNUMBER', i, 'of', len(lines)
			_file_name= _file.split('/')[-1].split('\\')[-1]
			newScn= Scene.New(_file_name)
			newScn.makeCurrent()
			main(_file)

	print 'TOTAL TIME: %.6f' % (Blender.sys.time() - TIME)
"""