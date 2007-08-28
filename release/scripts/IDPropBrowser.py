#!BPY

"""
Name: 'ID Property Browser'
Blender: 242
Group: 'Help'
Tooltip: 'Browse ID properties'
"""

__author__ = "Joe Eagar"
__version__ = "0.3.108"
__email__ = "joeedh@gmail.com"
__bpydoc__ = """\

Allows browsing, creating and editing of ID Properties
for various ID block types such as mesh, scene, object,
etc.
"""

# --------------------------------------------------------------------------
# ID Property Browser.
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


from Blender import *
from Blender.BGL import *
from Blender.Types import IDGroupType, IDArrayType
import Blender

def IsInRectWH(mx, my, x, y, wid, hgt):
	if mx >= x and mx <= x + wid:
		if my >= y and my <= y + hgt:
			return 1
	return 0

Button_Back = 1
Button_New = 2
Button_MatMenu = 3
Button_TypeMenu = 4

ButStart = 55

IDP_String = 0
IDP_Int = 1
IDP_Float = 2
IDP_Array = 5
IDP_Group = 6

ButDelStart = 255
#max limit for string input button
strmax = 100

State_Normal = 0
State_InArray = 1

#IDTypeModules entries are of form [module, active_object_index, module_name]
IDTypeModules = [[Scene, 0, "Scenes"], [Object, 0, "Objects"], [Mesh, 0, "Meshes"]]
IDTypeModules += [[Material, 0, "Materials"], [Texture, 0, "Textures"]]
IDTypeModules += [[Image, 0, "Images"]]

class IDArrayBrowser:
	array = 0
	parentbrowser = 0
	buts = 0
	
	def __init__(self):
		self.buts = []
	
	def Draw(self):
		pb = self.parentbrowser
		x = pb.x
		y = pb.y
		width = pb.width
		height = pb.height
		pad = pb.pad
		itemhgt = pb.itemhgt
		cellwid = 65
		y = y + height - itemhgt - pad
		
		Draw.PushButton("Back", Button_Back, x, y, 40, 20)
		y -= itemhgt + pad
		
		self.buts = []
		Draw.BeginAlign()
		for i in xrange(len(self.array)):
			st = ""
			if type(self.array[0]) == float:
				st = "%.5f" % self.array[i]
			else: st = str(self.array[i])
			
			b = Draw.String("", ButStart+i, x, y, cellwid, itemhgt, st, 30)
			self.buts.append(b)
			x += cellwid + pad
			if x + cellwid + pad > width:
				x = 0
				y -= itemhgt + pad
		Draw.EndAlign()
	def Button(self, bval):
		if bval == Button_Back:
			self.parentbrowser.state = State_Normal
			self.parentbrowser.array = 0
			self.buts = []
			Draw.Draw()
			self.array = 0
		elif bval >= ButStart:
			i = bval - ButStart
			st = self.buts[i].val
			n = 0
			if type(self.array[0]) == float:
				try:
					n = int(st)
				except:
					return
			elif type(self.array[0]) == int:
				try:
					n = float(st)
				except:
					return
			
			self.array[i] = n
			Draw.Draw()
			
	def Evt(self, evt, val):
		if evt == Draw.ESCKEY:
			Draw.Exit()
	
class IDPropertyBrowser:
	width = 0
	height = 0
	x = 0
	y = 0
	scrollx = 0
	scrolly = 0
	itemhgt = 22
	pad = 2
	
	group = 0
	parents = 0 #list stack of parent groups
	active_item = -1
	mousecursor = 0
	_i = 0
	buts = []
	
	state = 0
	array = 0
	prop = 0
	
	IDList = 0
	idindex = 0
	idblock = 0
	
	type = 0 # attach buildin type() method to class
	         # since oddly it's not available to button
	         # callbacks! EEK! :(
	
	def __init__(self, idgroup, mat, x, y, wid, hgt):
		self.group = idgroup
		self.prop = idgroup
		self.x = x
		self.y = y
		self.width = wid
		self.height = hgt
		self.mousecursor = [0, 0]
		self.parents = []
		self.idblock = mat
		self.type = type
		
	def DrawBox(self, glmode, x, y, width, height):
		glBegin(glmode)
		glVertex2f(x, y)
		glVertex2f(x+width, y)
		glVertex2f(x+width, y+height)
		glVertex2f(x, y+height)
		glEnd()
			
	def Draw(self):
		global IDTypeModules
		
		#first draw outlining box :)
		glColor3f(0, 0, 0)
		self.DrawBox(GL_LINE_LOOP, self.x, self.y, self.width, self.height)
				
		itemhgt = self.itemhgt
		pad = self.pad
		x = self.x
		y = self.y + self.height - itemhgt - pad
		
		if self.state == State_InArray:
			self.array.Draw()
			return
		
		plist = []
		self.buts = []
		for p in self.group.iteritems():
			plist.append(p)
		
		#-------do top buttons----------#
		Draw.BeginAlign()
		Draw.PushButton("New", Button_New, x, y, 40, 20)
		x += 40 + pad
		#do the menu button for all materials
		st = ""
		
		blocks =  IDTypeModules[self.IDList][0].Get()
		i = 1
		mi = 0
		for m in blocks:
			if m.name == self.idblock.name:
				mi = i
			st += m.name + " %x" + str(i) + "|"
			i += 1
		
		self.menubut = Draw.Menu(st, Button_MatMenu, x, y, 100, 20, mi)
		
		x += 100 + pad
		
		st = ""
		i = 0
		for e in IDTypeModules:
			st += e[2] + " %x" + str(i+1) + "|"
			i += 1
		
		cur = self.IDList + 1
		self.idmenu = Draw.Menu(st, Button_TypeMenu, x, y, 100, 20, cur)
		x = self.x
		y -= self.itemhgt + self.pad
		Draw.EndAlign()
		
		
		#-----------do property items---------#
		i = 0
		while y > self.y - 20 - pad and i < len(plist):
			k = plist[i][0]
			p = plist[i][1]
			if i == self.active_item:
				glColor3f(0.5, 0.4, 0.3)
				self.DrawBox(GL_POLYGON, x+pad, y, self.width-pad*2, itemhgt)
				
			glColor3f(0, 0, 0)	
			self.DrawBox(GL_LINE_LOOP, x+pad, y, self.width-pad*2, itemhgt)
			
			glRasterPos2f(x+pad*2, y+5)
			Draw.Text(str(k)) #str(self.mousecursor) + " " + str(self.active_item)) #p.name)
			tlen = Draw.GetStringWidth(str(k))
			
			type_p = type(p)
			if type_p == str:
				b = Draw.String("", ButStart+i, x+pad*5+tlen, y, 200, itemhgt, p, strmax)
				self.buts.append(b)
			elif type_p in [int, float]:
				#only do precision to 5 points on floats
				st = ""
				if type_p == float:
					st = "%.5f" % p
				else: st = str(p)
				b = Draw.String("", ButStart+i, x+pad*5+tlen, y, 75, itemhgt, st, strmax)
				self.buts.append(b)
			else:
				glRasterPos2f(x+pad*2  +tlen+10, y+5)
				if type_p == Types.IDArrayType:
					Draw.Text('(array, click to edit)')
				elif type_p == Types.IDGroupType:	
					Draw.Text('(group, click to edit)')
					
				
				self.buts.append(None)
				
			Draw.PushButton("Del", ButDelStart+i, x+self.width-35, y, 30, 20)
			
			i += 1
			y -= self.itemhgt + self.pad
		
		if len(self.parents) != 0:
			Draw.PushButton("Back", Button_Back, x, y, 40, 20)
			x = x + 40 + pad
			
	def SetActive(self):
		m = self.mousecursor
		itemhgt = self.itemhgt
		pad = self.pad
		
		x = self.x + pad
		y = self.y + self.height - itemhgt - pad - itemhgt
		
		plist = []
		for p in self.group.iteritems():
			plist.append(p)
		
		self.active_item = -1
		i = 0
		while y > self.y and i < len(plist):
			p = plist[i]
			if IsInRectWH(m[0], m[1], x, y, self.width-pad, itemhgt):
				self.active_item = i
				
			i += 1
			y -= self.itemhgt + self.pad
		
	def EventIn(self, evt, val):
		if self.state == State_InArray:
			self.array.Evt(evt, val)
		
		if evt == Draw.ESCKEY:
			Draw.Exit()
		if evt == Draw.MOUSEX or evt == Draw.MOUSEY:
			size = Buffer(GL_FLOAT, 4)
			glGetFloatv(GL_SCISSOR_BOX, size)
			if evt == Draw.MOUSEX:
				self.mousecursor[0] = val - size[0]
			else:
				self.mousecursor[1] = val - size[1]
			del size
			
			self.SetActive()
			self._i += 1
			if self._i == 5:
				Draw.Draw()
				self._i = 0

		
		if evt == Draw.LEFTMOUSE and val == 1:
			plist = list(self.group.iteritems())
			a = self.active_item
			if a >= 0 and a < len(plist):
				p = plist[a]
			
				basictypes = [IDGroupType, float, str, int]
				if type(p[1]) == IDGroupType:
					self.parents.append(self.group)
					self.group = p[1]
					self.active_item = -1
					Draw.Draw()
				elif type(p[1]) == IDArrayType:
					self.array = IDArrayBrowser()
					self.array.array = p[1]
					self.array.parentbrowser = self
					self.state = State_InArray
					Draw.Draw()
					
		if evt == Draw.TKEY and val == 1:
			try:
				self.prop['float'] = 0.0
				self.prop['int'] = 1
				self.prop['string'] = "hi!"
				self.prop['float array'] = [0, 0, 1.0, 0]
				self.prop['int array'] = [0, 0, 0, 0]
				self.prop.data['a subgroup'] = {"int": 0, "float": 0.0, "anothergroup": {"a": 0.0, "intarr": [0, 0, 0, 0]}}
				Draw.Draw()
			except:
				Draw.PupMenu("Can only do T once per block, the test names are already taken!")
				
						
	def Button(self, bval):
		global IDTypeModules
		if self.state == State_InArray:
			self.array.Button(bval)
			return
		
		if bval == Button_MatMenu:
			global IDTypeModules
			
			val = self.idindex = self.menubut.val - 1
			i = self.IDList
			block = IDTypeModules[i][0].Get()[val]
			self.idblock = block
			self.prop = block.properties
			self.group = self.prop
			self.active_item = -1
			self.parents = []
			Draw.Draw()
		
		if bval == Button_TypeMenu:			
			i = IDTypeModules[self.idmenu.val-1]
			if len(i[0].Get()) == 0:
				Draw.PupMenu("Error%t|There are no " + i[2] + "!")
				return
			
			IDTypeModules[self.IDList][1] = self.idindex
			self.IDList = self.idmenu.val-1
			val = self.idindex = IDTypeModules[self.IDList][1]
			i = self.IDList
			block = IDTypeModules[i][0].Get()[val]
			self.idblock = block
			self.prop = block.properties
			self.group = self.prop
			self.active_item = -1
			self.parents = []
			Draw.Draw()
			
		if bval >= ButDelStart:
			plist = [p for p in self.group]
			prop = plist[bval - ButDelStart]
			del self.group[prop]
			Draw.Draw()
			
		elif bval >= ButStart:
			plist = list(self.group.iteritems())
			
			prop = plist[bval - ButStart]
			print prop
			
			if self.type(prop[1]) == str:
				self.group[prop[0]] = self.buts[bval - ButStart].val
			elif self.type(prop[1]) == int:
				i = self.buts[bval - ButStart].val
				try:
					i = int(i)
					self.group[prop[0]] = i
				except:
					Draw.Draw()
					return
				Draw.Draw()
			elif self.type(prop[1]) == float:
				f = self.buts[bval - ButStart].val
				try:
					f = float(f)
					self.group[prop[0]] = f
				except:
					Draw.Draw()
					return
				Draw.Draw()
				
		elif bval == Button_Back:
			self.group = self.parents[len(self.parents)-1]
			self.parents.pop(len(self.parents)-1)
			Draw.Draw()
		
		elif bval == Button_New:
			name = Draw.Create("untitled")
			stype = Draw.Create(0)
			gtype = Draw.Create(0)
			ftype = Draw.Create(0)
			itype = Draw.Create(0)
			atype = Draw.Create(0)

			block = []
			block.append(("Name: ", name, 0, 30, "Click to type in the name of the new ID property"))
			block.append("Type")
			block.append(("String", stype))
			block.append(("Subgroup", gtype))
			block.append(("Float", ftype))
			block.append(("Int", itype))
			block.append(("Array", atype))
			
			retval = Blender.Draw.PupBlock("New IDProperty", block)
			if retval == 0: return
			
			name = name.val
			i = 1
			stop = 0
			while stop == 0:
				stop = 1
				for p in self.group:
					if p == name:
						d = name.rfind(".")
						if d != -1:
							name = name[:d]
						name = name + "." + str(i).zfill(3)
						i += 1
						stop = 0
				
			type = "String"
			if stype.val: 
				self.group[name] = ""
			elif gtype.val: 
				self.group[name] = {}
			elif ftype.val: 
				self.group[name] = 0.0
			elif itype.val: 
				self.group[name] = 0 #newProperty("Int", name, 0)
			elif atype.val: 
				arrfloat = Draw.Create(1)
				arrint = Draw.Create(0)
				arrlen = Draw.Create(3)
				block = []
				block.append("Type")
				block.append(("Float", arrfloat, "Make a float array"))
				block.append(("Int", arrint, "Make an integer array"))
				block.append(("Len", arrlen, 2, 200))
				
				if Blender.Draw.PupBlock("Array Properties", block):
					if arrfloat.val:
						tmpl = 0.0
					elif arrint.val:
						tmpl = 0
					else:
						return
					
					self.group[name] = [tmpl] * arrlen.val

				
	def Go(self):
		Draw.Register(self.Draw, self.EventIn, self.Button)

scenes = Scene.Get()

size = Window.GetAreaSize()
browser = IDPropertyBrowser(scenes[0].properties, scenes[0], 2, 2, size[0], size[1])
browser.Go()

#a = prop.newProperty("String", "hwello!", "bleh")
#b = prop.newProperty("Group", "subgroup")

#for p in prop:
	#print p.name
