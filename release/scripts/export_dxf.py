#!BPY

"""
 Name: 'Autodesk DXF (.dxf)'
 Blender: 247
 Group: 'Export'
 Tooltip: 'Export geometry to DXF-r12 (Drawing eXchange Format).'
"""

__version__ = "v1.25beta - 2008.09.28"
__author__  = "Stani & migius(Remigiusz Fiedler)"
__license__ = "GPL"
__url__	 = "http://wiki.blender.org/index.php/Scripts/Manual/Export/autodesk_dxf"
__bpydoc__ ="""The script exports Blender geometry to DXF format r12 version.

Copyright %s
Version %s
License %s
Homepage %s

See the homepage for documentation.
url:
""" % (__author__,__version__,__license__,__url__)

"""
IDEAs:
 - correct normals for POLYLINE-POLYFACE objects via correct point-order
 - HPGL output for 2d and flattened3d content
		
TODO:
 - support hierarchies: groups, instances, parented structures
 - support 210-code (3d orientation vector)
 - presets for architectural scales

History
v1.25 - 2008.09.28 by migius
 - modif FACE class for r12
 - add mesh-polygon -> Bezier-curve converter (Yorik's code)
 - add support for curves ->POLYLINEs
 - add "3d-View to Flat" - geometry projection to XY-plane
v1.24 - 2008.09.27 by migius
 - add start UI with preferences
 - modif POLYLINE class for r12
 - changing output format from r9 to r12(AC1009)
v1.23 - 2008.09.26 by migius
 - add finish message-box
v1.22 - 2008.09.26 by migius
 - add support for curves ->LINEs
 - add support for mesh-edges ->LINEs
v1.21 - 2008.06.04 by migius
 - initial adaptation for Blender
v1.1 (20/6/2005) by www.stani.be/python/sdxf
 - Python library to generate dxf drawings
______________________________________________________________
"""
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


import Blender
from Blender import Mathutils, Window, Scene, sys, Draw
import BPyMessages

try:
	import copy
	#from struct import pack
except:
	copy = None

####1) Private (only for developpers)
_HEADER_POINTS=['insbase','extmin','extmax']

#---helper functions-----------------------------------
def _point(x,index=0):
	"""Convert tuple to a dxf point"""
	#print '_point=', x #-------------
	return '\n'.join(['%s\n%s'%((i+1)*10+index,x[i]) for i in range(len(x))])

def _points(plist):
	"""Convert a list of tuples to dxf points"""
	return [_point(plist[i],i)for i in range(len(plist))]

#---base classes----------------------------------------
class _Call:
	"""Makes a callable class."""
	def copy(self):
		"""Returns a copy."""
		return copy.deepcopy(self)

	def __call__(self,**attrs):
		"""Returns a copy with modified attributes."""
		copied=self.copy()
		for attr in attrs:setattr(copied,attr,attrs[attr])
		return copied

#-------------------------------------------------------
class _Entity(_Call):
	"""Base class for _common group codes for entities."""
	def __init__(self,color=None,extrusion=None,layer='0',
				 lineType=None,lineTypeScale=None,lineWeight=None,
				 thickness=None,parent=None):
		"""None values will be omitted."""
		self.color		  = color
		self.extrusion	  = extrusion
		self.layer		  = layer
		self.lineType	   = lineType
		self.lineTypeScale  = lineTypeScale
		self.lineWeight	 = lineWeight
		self.thickness	  = thickness
		self.parent		 = parent

	def _common(self):
		"""Return common group codes as a string."""
		if self.parent:parent=self.parent
		else:parent=self
		result='8\n%s'%parent.layer
		if parent.color!=None:		  result+='\n62\n%s'%parent.color
		if parent.extrusion!=None:	  result+='\n%s'%_point(parent.extrusion,200)
		if parent.lineType!=None:	   result+='\n6\n%s'%parent.lineType
		if parent.lineWeight!=None:	 result+='\n370\n%s'%parent.lineWeight
		if parent.lineTypeScale!=None:  result+='\n48\n%s'%parent.lineTypeScale
		if parent.thickness!=None:	  result+='\n39\n%s'%parent.thickness
		return result

#--------------------------
class _Entities:
	"""Base class to deal with composed objects."""
	def __dxf__(self):
		return []

	def __str__(self):
		return '\n'.join([str(x) for x in self.__dxf__()])

#--------------------------
class _Collection(_Call):
	"""Base class to expose entities methods to main object."""
	def __init__(self,entities=[]):
		self.entities=copy.copy(entities)
		#link entities methods to drawing
		for attr in dir(self.entities):
			if attr[0]!='_':
				attrObject=getattr(self.entities,attr)
				if callable(attrObject):
					setattr(self,attr,attrObject)

####2) Constants
#---color values
BYBLOCK=0
BYLAYER=256

#---block-type flags (bit coded values, may be combined):
ANONYMOUS			   =1  # This is an anonymous block generated by hatching, associative dimensioning, other internal operations, or an application
NON_CONSTANT_ATTRIBUTES =2  # This block has non-constant attribute definitions (this bit is not set if the block has any attribute definitions that are constant, or has no attribute definitions at all)
XREF					=4  # This block is an external reference (xref)
XREF_OVERLAY			=8  # This block is an xref overlay
EXTERNAL				=16 # This block is externally dependent
RESOLVED				=32 # This is a resolved external reference, or dependent of an external reference (ignored on input)
REFERENCED			  =64 # This definition is a referenced external reference (ignored on input)

#---mtext flags
#attachment point
TOP_LEFT		= 1
TOP_CENTER	  = 2
TOP_RIGHT	   = 3
MIDDLE_LEFT	 = 4
MIDDLE_CENTER   = 5
MIDDLE_RIGHT	= 6
BOTTOM_LEFT	 = 7
BOTTOM_CENTER   = 8
BOTTOM_RIGHT	= 9
#drawing direction
LEFT_RIGHT	  = 1
TOP_BOTTOM	  = 3
BY_STYLE		= 5 #the flow direction is inherited from the associated text style
#line spacing style (optional):
AT_LEAST		= 1 #taller characters will override
EXACT		   = 2 #taller characters will not override

#---polyline flags
CLOSED					  =1	  # This is a closed polyline (or a polygon mesh closed in the M direction)
CURVE_FIT				   =2	  # Curve-fit vertices have been added
SPLINE_FIT				  =4	  # Spline-fit vertices have been added
POLYLINE_3D				 =8	  # This is a 3D polyline
POLYGON_MESH				=16	 # This is a 3D polygon mesh
CLOSED_N					=32	 # The polygon mesh is closed in the N direction
POLYFACE_MESH			   =64	 # The polyline is a polyface mesh
CONTINOUS_LINETYPE_PATTERN  =128	# The linetype pattern is generated continuously around the vertices of this polyline

#---text flags
#horizontal
LEFT		= 0
CENTER	  = 1
RIGHT	   = 2
ALIGNED	 = 3 #if vertical alignment = 0
MIDDLE	  = 4 #if vertical alignment = 0
FIT		 = 5 #if vertical alignment = 0
#vertical
BASELINE	= 0
BOTTOM	  = 1
MIDDLE	  = 2
TOP		 = 3

####3) Classes
#---entitities -----------------------------------------------
#--------------------------
class Arc(_Entity):
	"""Arc, angles in degrees."""
	def __init__(self,center=(0,0,0),radius=1,
				 startAngle=0.0,endAngle=90,**common):
		"""Angles in degrees."""
		_Entity.__init__(self,**common)
		self.center=center
		self.radius=radius
		self.startAngle=startAngle
		self.endAngle=endAngle
	def __str__(self):
		return '0\nARC\n%s\n%s\n40\n%s\n50\n%s\n51\n%s'%\
			   (self._common(),_point(self.center),
				self.radius,self.startAngle,self.endAngle)

#-----------------------------------------------
class Circle(_Entity):
	"""Circle"""
	def __init__(self,center=(0,0,0),radius=1,**common):
		_Entity.__init__(self,**common)
		self.center=center
		self.radius=radius
	def __str__(self):
		return '0\nCIRCLE\n%s\n%s\n40\n%s'%\
			   (self._common(),_point(self.center),self.radius)

#-----------------------------------------------
class Face(_Entity):
	"""3dface"""
	def __init__(self,points,**common):
		_Entity.__init__(self,**common)
		if len(points)<4: #fix for r12 format
			points.append(points[-1])
		self.points=points
		
	def __str__(self):
		return '\n'.join(['0\n3DFACE',self._common()]+
						 _points(self.points)
						 )
#-----------------------------------------------
class Insert(_Entity):
	"""Block instance."""
	def __init__(self,name,point=(0,0,0),
				 xscale=None,yscale=None,zscale=None,
				 cols=None,colspacing=None,rows=None,rowspacing=None,
				 rotation=None,
				 **common):
		_Entity.__init__(self,**common)
		self.name=name
		self.point=point
		self.xscale=xscale
		self.yscale=yscale
		self.zscale=zscale
		self.cols=cols
		self.colspacing=colspacing
		self.rows=rows
		self.rowspacing=rowspacing
		self.rotation=rotation

	def __str__(self):
		result='0\nINSERT\n2\n%s\n%s\n%s'%\
				(self.name,self._common(),_point(self.point))
		if self.xscale!=None:result+='\n41\n%s'%self.xscale
		if self.yscale!=None:result+='\n42\n%s'%self.yscale
		if self.zscale!=None:result+='\n43\n%s'%self.zscale
		if self.rotation:result+='\n50\n%s'%self.rotation
		if self.cols!=None:result+='\n70\n%s'%self.cols
		if self.colspacing!=None:result+='\n44\n%s'%self.colspacing
		if self.rows!=None:result+='\n71\n%s'%self.rows
		if self.rowspacing!=None:result+='\n45\n%s'%self.rowspacing
		return result

#-----------------------------------------------
class Line(_Entity):
	"""Line"""
	def __init__(self,points,**common):
		_Entity.__init__(self,**common)
		self.points=points
	def __str__(self):
		return '\n'.join(['0\nLINE',self._common()]+
						 _points(self.points))

#-----------------------------------------------
class PolyLine(_Entity):
	#TODO: Finish polyline (now implemented as a series of lines)
	def __init__(self,points,org_point=[0,0,0],flag=0,width=None,**common):
		_Entity.__init__(self,**common)
		self.points=points
		self.org_point=org_point
		self.flag=flag
		self.width=width
	def __str__(self):
		result= '0\nPOLYLINE\n%s\n70\n%s' %(self._common(),self.flag)
		#print 'self._common()', self._common() #----------
		result+='\n66\n1'
		result+='\n%s' %_point(self.org_point)
		for point in self.points:
			result+='\n0\nVERTEX'
			result+='\n8\n%s' %self.layer
			result+='\n%s' %_point(point)
			if self.width:result+='\n40\n%s\n41\n%s' %(self.width,self.width)
		result+='\n0\nSEQEND'
		result+='\n8\n%s' %self.layer
		return result

#-----------------------------------------------
class Point(_Entity):
	"""Colored solid fill."""
	def __init__(self,points=None,**common):
		_Entity.__init__(self,**common)
		self.points=points

#-----------------------------------------------
class Solid(_Entity):
	"""Colored solid fill."""
	def __init__(self,points=None,**common):
		_Entity.__init__(self,**common)
		self.points=points
	def __str__(self):
		return '\n'.join(['0\nSOLID',self._common()]+
						 _points(self.points[:2]+[self.points[3],self.points[2]])
						 )

#-----------------------------------------------
class Text(_Entity):
	"""Single text line."""
	def __init__(self,text='',point=(0,0,0),alignment=None,
				 flag=None,height=1,justifyhor=None,justifyver=None,
				 rotation=None,obliqueAngle=None,style=None,xscale=None,**common):
		_Entity.__init__(self,**common)
		self.text=text
		self.point=point
		self.alignment=alignment
		self.flag=flag
		self.height=height
		self.justifyhor=justifyhor
		self.justifyver=justifyver
		self.rotation=rotation
		self.obliqueAngle=obliqueAngle
		self.style=style
		self.xscale=xscale
	def __str__(self):
		result= '0\nTEXT\n%s\n%s\n40\n%s\n1\n%s'%\
				(self._common(),_point(self.point),self.height,self.text)
		if self.rotation:result+='\n50\n%s'%self.rotation
		if self.xscale:result+='\n41\n%s'%self.xscale
		if self.obliqueAngle:result+='\n51\n%s'%self.obliqueAngle
		if self.style:result+='\n7\n%s'%self.style
		if self.flag:result+='\n71\n%s'%self.flag
		if self.justifyhor:result+='\n72\n%s'%self.justifyhor
		if self.alignment:result+='\n%s'%_point(self.alignment,1)
		if self.justifyver:result+='\n73\n%s'%self.justifyver
		return result

#-----------------------------------------------
class Mtext(Text):
	"""Surrogate for mtext, generates some Text instances."""
	def __init__(self,text='',point=(0,0,0),width=250,spacingFactor=1.5,down=0,spacingWidth=None,**options):
		Text.__init__(self,text=text,point=point,**options)
		if down:spacingFactor*=-1
		self.spacingFactor=spacingFactor
		self.spacingWidth=spacingWidth
		self.width=width
		self.down=down
	def __str__(self):
		texts=self.text.replace('\r\n','\n').split('\n')
		if not self.down:texts.reverse()
		result=''
		x=y=0
		if self.spacingWidth:spacingWidth=self.spacingWidth
		else:spacingWidth=self.height*self.spacingFactor
		for text in texts:
			while text:
				result+='\n%s'%Text(text[:self.width],
					point=(self.point[0]+x*spacingWidth,
						   self.point[1]+y*spacingWidth,
						   self.point[2]),
					alignment=self.alignment,flag=self.flag,height=self.height,
					justifyhor=self.justifyhor,justifyver=self.justifyver,
					rotation=self.rotation,obliqueAngle=self.obliqueAngle,
					style=self.style,xscale=self.xscale,parent=self
				)
				text=text[self.width:]
				if self.rotation:x+=1
				else:y+=1
		return result[1:]

#-----------------------------------------------
##class _Mtext(_Entity):
##	"""Mtext not functioning for minimal dxf."""
##	def __init__(self,text='',point=(0,0,0),attachment=1,
##				 charWidth=None,charHeight=1,direction=1,height=100,rotation=0,
##				 spacingStyle=None,spacingFactor=None,style=None,width=100,
##				 xdirection=None,**common):
##		_Entity.__init__(self,**common)
##		self.text=text
##		self.point=point
##		self.attachment=attachment
##		self.charWidth=charWidth
##		self.charHeight=charHeight
##		self.direction=direction
##		self.height=height
##		self.rotation=rotation
##		self.spacingStyle=spacingStyle
##		self.spacingFactor=spacingFactor
##		self.style=style
##		self.width=width
##		self.xdirection=xdirection
##	def __str__(self):
##		input=self.text
##		text=''
##		while len(input)>250:
##			text+='\n3\n%s'%input[:250]
##			input=input[250:]
##		text+='\n1\n%s'%input
##		result= '0\nMTEXT\n%s\n%s\n40\n%s\n41\n%s\n71\n%s\n72\n%s%s\n43\n%s\n50\n%s'%\
##				(self._common(),_point(self.point),self.charHeight,self.width,
##				 self.attachment,self.direction,text,
##				 self.height,
##				 self.rotation)
##		if self.style:result+='\n7\n%s'%self.style
##		if self.xdirection:result+='\n%s'%_point(self.xdirection,1)
##		if self.charWidth:result+='\n42\n%s'%self.charWidth
##		if self.spacingStyle:result+='\n73\n%s'%self.spacingStyle
##		if self.spacingFactor:result+='\n44\n%s'%self.spacingFactor
##		return result

#---tables ---------------------------------------------------
#-----------------------------------------------
class Block(_Collection):
	"""Use list methods to add entities, eg append."""
	def __init__(self,name,layer='0',flag=0,base=(0,0,0),entities=[]):
		self.entities=copy.copy(entities)
		_Collection.__init__(self,entities)
		self.layer=layer
		self.name=name
		self.flag=0
		self.base=base
	def __str__(self):
		e='\n'.join([str(x)for x in self.entities])
		return '0\nBLOCK\n8\n%s\n2\n%s\n70\n%s\n%s\n3\n%s\n%s\n0\nENDBLK'%\
			   (self.layer,self.name.upper(),self.flag,_point(self.base),self.name.upper(),e)

#-----------------------------------------------
class Layer(_Call):
	"""Layer"""
	def __init__(self,name='pydxf',color=7,lineType='continuous',flag=64):
		self.name=name
		self.color=color
		self.lineType=lineType
		self.flag=flag
	def __str__(self):
		return '0\nLAYER\n2\n%s\n70\n%s\n62\n%s\n6\n%s'%\
			   (self.name.upper(),self.flag,self.color,self.lineType)

#-----------------------------------------------
class LineType(_Call):
	"""Custom linetype"""
	def __init__(self,name='continuous',description='Solid line',elements=[],flag=64):
		# TODO: Implement lineType elements
		self.name=name
		self.description=description
		self.elements=copy.copy(elements)
		self.flag=flag
	def __str__(self):
		return '0\nLTYPE\n2\n%s\n70\n%s\n3\n%s\n72\n65\n73\n%s\n40\n0.0'%\
			(self.name.upper(),self.flag,self.description,len(self.elements))

#-----------------------------------------------
class Style(_Call):
	"""Text style"""
	def __init__(self,name='standard',flag=0,height=0,widthFactor=40,obliqueAngle=50,
				 mirror=0,lastHeight=1,font='arial.ttf',bigFont=''):
		self.name=name
		self.flag=flag
		self.height=height
		self.widthFactor=widthFactor
		self.obliqueAngle=obliqueAngle
		self.mirror=mirror
		self.lastHeight=lastHeight
		self.font=font
		self.bigFont=bigFont
	def __str__(self):
		return '0\nSTYLE\n2\n%s\n70\n%s\n40\n%s\n41\n%s\n50\n%s\n71\n%s\n42\n%s\n3\n%s\n4\n%s'%\
			   (self.name.upper(),self.flag,self.flag,self.widthFactor,
				self.obliqueAngle,self.mirror,self.lastHeight,
				self.font.upper(),self.bigFont.upper())

#-----------------------------------------------
class View(_Call):
	def __init__(self,name,flag=0,width=1,height=1,center=(0.5,0.5),
				 direction=(0,0,1),target=(0,0,0),lens=50,
				 frontClipping=0,backClipping=0,twist=0,mode=0):
		self.name=name
		self.flag=flag
		self.width=width
		self.height=height
		self.center=center
		self.direction=direction
		self.target=target
		self.lens=lens
		self.frontClipping=frontClipping
		self.backClipping=backClipping
		self.twist=twist
		self.mode=mode
	def __str__(self):
		return '0\nVIEW\n2\n%s\n70\n%s\n40\n%s\n%s\n41\n%s\n%s\n%s\n42\n%s\n43\n%s\n44\n%s\n50\n%s\n71\n%s'%\
			   (self.name,self.flag,self.height,_point(self.center),self.width,
				_point(self.direction,1),_point(self.target,2),self.lens,
				self.frontClipping,self.backClipping,self.twist,self.mode)

#-----------------------------------------------
def ViewByWindow(name,leftBottom=(0,0),rightTop=(1,1),**options):
	width=abs(rightTop[0]-leftBottom[0])
	height=abs(rightTop[1]-leftBottom[1])
	center=((rightTop[0]+leftBottom[0])*0.5,(rightTop[1]+leftBottom[1])*0.5)
	return View(name=name,width=width,height=height,center=center,**options)

#---drawing
#-----------------------------------------------
class Drawing(_Collection):
	"""Dxf drawing. Use append or any other list methods to add objects."""
	def __init__(self,insbase=(0.0,0.0,0.0),extmin=(0.0,0.0),extmax=(0.0,0.0),
				 layers=[Layer()],linetypes=[LineType()],styles=[Style()],blocks=[],
				 views=[],entities=None,fileName='test.dxf'):
		# TODO: replace list with None,arial
		if not entities:entities=[]
		_Collection.__init__(self,entities)
		self.insbase=insbase
		self.extmin=extmin
		self.extmax=extmax
		self.layers=copy.copy(layers)
		self.linetypes=copy.copy(linetypes)
		self.styles=copy.copy(styles)
		self.views=copy.copy(views)
		self.blocks=copy.copy(blocks)
		self.fileName=fileName
		#private
		#self.acadver='9\n$ACADVER\n1\nAC1006'
		self.acadver='9\n$ACADVER\n1\nAC1009'
		"""DXF AutoCAD-Release format code
		AC1021  2008, 2007 
		AC1018  2006, 2005, 2004 
		AC1015  2002, 2000i, 2000 
		AC1014  R14,14.01 
		AC1012  R13    
		AC1009  R12,11 
		AC1006  R10    
		AC1004  R9    
		AC1002  R2.6  
		AC1.50  R2.05 
		"""

	def _name(self,x):
		"""Helper function for self._point"""
		return '9\n$%s'%x.upper()

	def _point(self,name,x):
		"""Point setting from drawing like extmin,extmax,..."""
		return '%s\n%s'%(self._name(name),_point(x))

	def _section(self,name,x):
		"""Sections like tables,blocks,entities,..."""
		if x:xstr='\n'+'\n'.join(x)
		else:xstr=''
		return '0\nSECTION\n2\n%s%s\n0\nENDSEC'%(name.upper(),xstr)

	def _table(self,name,x):
		"""Tables like ltype,layer,style,..."""
		if x:xstr='\n'+'\n'.join(x)
		else:xstr=''
		return '0\nTABLE\n2\n%s\n70\n%s%s\n0\nENDTAB'%(name.upper(),len(x),xstr)

	def __str__(self):
		"""Returns drawing as dxf string."""
		header=[self.acadver]+[self._point(attr,getattr(self,attr)) for attr in _HEADER_POINTS]
		header=self._section('header',header)

		tables=[self._table('ltype',[str(x) for x in self.linetypes]),
				self._table('layer',[str(x) for x in self.layers]),
				self._table('style',[str(x) for x in self.styles]),
				self._table('view',[str(x) for x in self.views]),
		]
		tables=self._section('tables',tables)

		blocks=self._section('blocks',[str(x) for x in self.blocks])

		entities=self._section('entities',[str(x) for x in self.entities])

		all='\n'.join([header,tables,blocks,entities,'0\nEOF\n'])
		return all

	def saveas(self,fileName):
		self.fileName=fileName
		self.save()

	def save(self):
		test=open(self.fileName,'w')
		test.write(str(self))
		test.close()


#---extras
#-----------------------------------------------
class Rectangle(_Entity):
	"""Rectangle, creates lines."""
	def __init__(self,point=(0,0,0),width=1,height=1,solid=None,line=1,**common):
		_Entity.__init__(self,**common)
		self.point=point
		self.width=width
		self.height=height
		self.solid=solid
		self.line=line
	def __str__(self):
		result=''
		points=[self.point,(self.point[0]+self.width,self.point[1],self.point[2]),
			(self.point[0]+self.width,self.point[1]+self.height,self.point[2]),
			(self.point[0],self.point[1]+self.height,self.point[2]),self.point]
		if self.solid:
			result+='\n%s'%Solid(points=points[:-1],parent=self.solid)
		if self.line:
			for i in range(4):result+='\n%s'%\
				Line(points=[points[i],points[i+1]],parent=self)
		return result[1:]

#-----------------------------------------------
class LineList(_Entity):
	"""Like polyline, but built of individual lines."""
	def __init__(self,points=[],org_point=[0,0,0],closed=0,**common):
		_Entity.__init__(self,**common)
		self.closed=closed
		self.points=copy.copy(points)
	def __str__(self):
		if self.closed:points=self.points+[self.points[0]]
		else: points=self.points
		result=''
		for i in range(len(points)-1):
			result+='\n%s' %Line(points=[points[i],points[i+1]],parent=self)
		return result[1:]

#-----------------------------------------------------
def projected_co(vec, mw):
	# convert the world coordinates of v to screen coordinates
	#co = vec.co.copy().resize4D()
	co = vec.copy().resize4D()
	co[3] = 1.0
	sc = co * mw
	#print 'viewprojection=', sc #---------
	return [sc[0],sc[1],0.0]


#-----------------------------------------------------
def dxf_export_ui(filepath):
	print '\n\nDXF-Export %s' %__version__
	#filepath = 'blend_test.dxf'
	# Dont overwrite
	if not BPyMessages.Warning_SaveOver(filepath):
		print 'Aborted by user: nothing exported'
		return
	#test():return

	ONLYSELECTED = True
	POLYLINES = True
	ONLYFACES = False
	FLATTEN = 0 #dimmensions:1,2,3. Force Z dimmension value to 0.0, equal ground projection
	SCALE_FACTOR = 1.0 #optional, can be done in CAD too
	PREF_ONLYSELECTED= Draw.Create(ONLYSELECTED)
	PREF_POLYLINES= Draw.Create(POLYLINES)
	PREF_ONLYFACES= Draw.Create(ONLYFACES)
	PREF_FLATTEN= Draw.Create(FLATTEN)
	PREF_SCALE_FACTOR= Draw.Create(SCALE_FACTOR)
	PREF_HELP= Draw.Create(0)
	block = [\
	("only selected", PREF_ONLYSELECTED, "export only selected geometry"),\
	("global Scale:", PREF_SCALE_FACTOR, 0.001, 1000, "set global Scale factor for exporting geometry"),\
	("only faces", PREF_ONLYFACES, "from mesh-objects export only faces, not edges"),\
	("write POLYLINEs", PREF_POLYLINES, "export curves to POLYLINEs, otherwise to LINEs"),\
	("3D-View to Flat", PREF_FLATTEN, "flatten geometry according current 3d-View"),\
	(''),\
	("online Help", PREF_HELP, "calls DXF-Exporter Manual Page on Wiki.Blender.org"),\
	]
	
	if not Draw.PupBlock("DXF-Exporter %s" %__version__[:10], block):
		return

	if PREF_HELP.val!=0:
		try:
			import webbrowser
			webbrowser.open('http://wiki.blender.org/index.php?title=Scripts/Manual/Export/autodesk_dxf')
		except:
			Draw.PupMenu('DXF Exporter: %t|no connection to manual-page on Blender-Wiki!	try:|\
http://wiki.blender.org/index.php?title=Scripts/Manual/Export/autodesk_dxf')
		return

	ONLYSELECTED = PREF_ONLYSELECTED.val
	POLYLINES = PREF_POLYLINES.val
	ONLYFACES = PREF_ONLYFACES.val
	FLATTEN = PREF_FLATTEN.val
	SCALE_FACTOR = PREF_SCALE_FACTOR.val

	sce = Scene.GetCurrent()
	if ONLYSELECTED: sel_group = sce.objects.selected
	else: sel_group = sce.objects

	if sel_group:
		Window.WaitCursor(1)
		t = sys.time()

		#init Drawing ---------------------
		d=Drawing()
		#add Tables -----------------
		#d.blocks.append(b)					#table blocks
		d.styles.append(Style())			#table styles
		d.views.append(View('Normal'))		#table view
		d.views.append(ViewByWindow('Window',leftBottom=(1,0),rightTop=(2,1)))  #idem

		#add Entities --------------------
		something_ready = False
		mw = Window.GetViewMatrix()
		#mw = Window.GetPerspMatrix() #TODO: how get it working?
		for ob in sel_group:
			entities = []
			mx = ob.matrix
			if (ob.type == 'Mesh'):
				me = ob.getData(mesh=1)
				faces=[]
				edges=[]
				if (not FLATTEN) and len(me.faces)>0 and ONLYFACES:
					#export 3D as 3DFACEs
					for f in me.faces:
						#print 'face=', f #---------
						verts = f.verts
						points = [verts[i].co*mx for i in range(len(verts))]
						if SCALE_FACTOR!=1.0:
							points = [p*SCALE_FACTOR for p in points]
						#print 'points=', points #---------
						dxfFACE = Face(points)
						entities.append(dxfFACE)
				else:	#export 3D as LINEs
					for e in me.edges:
						#print 'edge=', e #---------
						points=[]
						points = [e.v1.co*mx, e.v2.co*mx]
						if SCALE_FACTOR!=1.0:
							points = [p*SCALE_FACTOR for p in points]
						if FLATTEN:
#							for p in points: p[FLATTEN-1]=0.0
							for i,v in enumerate(points):
								v = projected_co(v, mw)
								points[i]=v
							#print 'flatten points=', points #---------
						dxfLINE = Line(points)
						entities.append(dxfLINE)
			elif (ob.type == 'Curve'):
				curve = ob.getData()
				for cur in curve:
					#print 'deb: START cur=', cur #--------------
					if 1: #not cur.isNurb():
						#print 'deb: START points' #--------------
						points = []
						org_point = [0.0,0.0,0.0]
						for point in cur:
							#print 'point=', point #---------
							if cur.isNurb():
								vec = point[0:3]
							else:
								point = point.getTriple()
								#print 'point=', point #---------
								vec = point[1]
							#print 'vec=', vec #---------
							pkt = Mathutils.Vector(vec) * mx
							#print 'pkt=', pkt #---------
							pkt *= SCALE_FACTOR
							if FLATTEN:
								pkt = projected_co(pkt, mw)
							points.append(pkt)
						if cur.isCyclic(): closed = 1
						else: closed = 0
						if len(points)>1:
							#print 'deb: points', points #--------------
							if POLYLINES: dxfPLINE = PolyLine(points,org_point,closed)
							else: dxfPLINE = LineList(points,org_point,closed)
							entities.append(dxfPLINE)
			for e in entities:
				d.append(e)
				something_ready = True
		if something_ready:
			d.saveas(filepath)
			Window.WaitCursor(0)
			#Draw.PupMenu('DXF Exporter: job finished|search for blend_test.dxf in current project directory')
			print 'exported to %s' % filepath
			print 'finished in %.2f seconds' % (sys.time()-t)
		else:
			print "Abort: no supported object types selected, nothing exported!"
			Draw.PupMenu('DXF Exporter:        Abort!|Not-supported object types selected.')
	else:
		print "Abort: selection was empty, no object to export!"
		Draw.PupMenu('DXF Exporter:        Abort!|empty selection, no object to export!')
	# Timing the script is a good way to be aware on any speed hits when scripting


#-----------------------------------------------------
def test():
	#Blocks
	b=Block('test')
	b.append(Solid(points=[(0,0,0),(1,0,0),(1,1,0),(0,1,0)],color=1))
	b.append(Arc(center=(1,0,0),color=2))

	#Drawing
	d=Drawing()
	#tables
	d.blocks.append(b)					  #table blocks
	d.styles.append(Style())				#table styles
	d.views.append(View('Normal'))		  #table view
	d.views.append(ViewByWindow('Window',leftBottom=(1,0),rightTop=(2,1)))  #idem

	#entities
	d.append(Circle(center=(1,1,0),color=3))
	d.append(Face(points=[(0,0,0),(1,0,0),(1,1,0),(0,1,0)],color=4))
	d.append(Insert('test',point=(3,3,3),cols=5,colspacing=2))
	d.append(Line(points=[(0,0,0),(1,1,1)]))
	d.append(Mtext('Click on Ads\nmultiple lines with mtext',point=(1,1,1),color=5,rotation=90))
	d.append(Text('Please donate!',point=(3,0,1)))
	d.append(Rectangle(point=(2,2,2),width=4,height=3,color=6,solid=Solid(color=2)))
	d.append(Solid(points=[(4,4,0),(5,4,0),(7,8,0),(9,9,0)],color=3))
	d.append(PolyLine(points=[(1,1,1),(2,1,1),(2,2,1),(1,2,1)],closed=1,color=1))

	#d.saveas('c:\\test.dxf')
	d.saveas('test.dxf')


#-----------------------------------------------------
if __name__=='__main__':
	#main()
	if not copy:
		Draw.PupMenu('Error%t|This script requires a full python install')
	Window.FileSelector(dxf_export_ui, 'EXPORT DXF', sys.makename(ext='.dxf'))
	
	
	