#!BPY

"""
Name: 'BPy Doc Browser'
Blender: 232
Group: 'Misc'
Tip: 'Browse BPython (scripting API) modules doc strings.'
"""

__author__ = "Daniel Dunbar"
__url__ = ("blender", "elysiun")
__version__ = "1.0"
__bpydoc__ = """\
The "Doc Browser" lets users navigate the documentation strings of part of
the Blender Python API.

It doesn't give access yet to object method functions and variables, only to
module functions, but still it is a handy reference.  Specially for quick
access, for example to Blender.BGL: the module that wraps OpenGL calls.

Notes:<br>
    Everyone interested in the bpython api is also invited to read "The Blender
Python API Reference" doc, available online ("Python Scripting Reference"
entry in Blender's Help menu).
"""


# $Id$
#
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004:  Daniel Dunbar, ddunbar _at_ diads.com
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

#####
# Blender help browser
# By Daniel Dunbar, 
#
# This should function as a self-explanatory (interface, not code)
# (mostly) help browser. The code is wacky and nasty and or fun,
# but mainly bad, just cause it works doesn't mean its readable! 
# 
# TEEHEE!
#
# The row_draw function could easily be made into a more generic
# and usefull table drawing function... 
#

import Blender
from types import ListType, IntType, FloatType, StringType, ModuleType
from Blender.Draw import *
from Blender.BGL import *

# Simple version check, since I use the
# buffer calls... DONT use this code,
# assume everyone has 1.73+, and force
# them to upgrade if they dont
try:
	a= BufList
	version= 172
except:
	version= 173
	
# I could have used the split from the string module,
# but some people might not have it
def split(str, on):
	out= [""]
	for s in str:
		if s in on: out.append("")
		else: out[-1]= out[-1]+s

	if out[-1]=="": del(out[-1])

	return out

last_sort= 1; direction= 1
def sort_browselist(type):
	global browselist
	global last_sort, direction

	if (type==last_sort): direction= -direction
	else: direction= 1

	last_sort= type

	if (direction==1):
		def byname(x, y): return cmp(x[0],y[0]);
		def bytype(x, y): return cmp(x[1],y[1])
		def bydata(x, y): return cmp(x[2],y[2])
	else:
		def byname(x, y): return cmp(y[0],x[0]);
		def bytype(x, y): return cmp(y[1],x[1])
		def bydata(x, y): return cmp(y[2],x[2])

	if (type==1): browselist.sort(byname)
	elif (type==2): browselist.sort(bytype)
	elif (type==3): browselist.sort(bydata)

selected= -1
def view_doc(num):
	global selected, selected_page

	if (selected==num): selected= -1
	else: selected= num

	selected_page= 0

function_filter= 0
def toggle_function_filter():
	global function_filter

	function_filter= not function_filter
	make_browselist()

def view_page(dir):
	global selected_page

	selected_page= selected_page + dir

browse_scrollstart= 0
def browse_module(num):
	global browsing, selected, browse_scrollstart
	
	if (num>=0): newstr= browsing.val + "." + browselist[num][0]
	else:
		modules= split(browsing.val, ".")
		newstr= ""
		for m in modules[:-1]:
			newstr= newstr+m
	try:
		browsing= Create(newstr)
		make_browselist()
	except:
		browsing= Create('Blender')
		make_browselist()
		
	browse_scrollstart= 0
	scrolling= 0
	selected= -1

def make_browselist():
	global browselist

	browselist= []

	module= eval(browsing.val)
	items= dir(module)

	for item_name in items:
		if (item_name[:2]=='__'): continue

		data= [item_name, 'None', '', '']
		item= eval(item_name,module.__dict__)
		t= type(item)

		if (t==IntType): data[1]= 'Int'; data[2]= `item`
		elif (t==FloatType): data[1]= 'Float'; data[2]= `item`
		elif (t==StringType): data[1]= 'String'
		elif (t==ModuleType): data[1]= 'Module'
		elif (callable(item)):
			data[1]= 'Function'
			doc= item.__doc__
			if (doc): data[3]= doc

		if (function_filter and data[1]!='Function'): continue

		browselist.append(data)

browsing= Create('Blender')
make_browselist()

BROWSE_EVT= 1

SORT_BYNAME= 2
SORT_BYTYPE= 3
SORT_BYDATA= 4

DOC_PAGE_UP= 5
DOC_PAGE_DOWN= 6

BACK_MODULE= 7
CLOSE_VIEW= 8
FILTER_DISPLAY= 9

SCROLLBAR= 10

VIEW_DOC= 100
BROWSE_MODULE= 20000

scr= Create(0)
browse_scrollstart= 0

winrect= [0.0, 0.0, 0.0, 0.0]
def draw():
	global browsing, winrect, scr, browse_scrollstart

	# Blender doesn't give us direct access to
	# the window size yet, but it does set the
	# GL scissor box for it, so we can get the 
	# size from that.

	if (version<173):
		size= Buffer(GL_FLOAT, None, 4)
		glGetFloat(GL_SCISSOR_BOX, size)
		size= BufList(size)
	else:
		size= Buffer(GL_FLOAT, 4)
		glGetFloatv(GL_SCISSOR_BOX, size)
		size= size.list

	winrect= size[:]

	size[0]= size[1]= 0.0
	
	# Shrink the size to make a nice frame
	# (also a good technique so you can be sure you are clipping things properly)
	size[0], size[1]= int(size[0]+10), int(size[1]+10)
	size[2], size[3]= int(size[2]-12), int(size[3]-10)

	glClearColor(0.6, 0.5, 0.3, 0.0)
	glClear(GL_COLOR_BUFFER_BIT)

	# The frame
	glColor3f(0.4, 0.5, 0.2)
	glRectf(size[0], size[1], size[2], size[3])

	# Window header	
	glColor3f(0.2, 0.2, 0.4)
	glRectf(size[0], size[3]-25, size[2], size[3])

	glColor3f(0.6, 0.6, 0.6)
	glRasterPos2f(size[0]+15, size[3]-17)
	Text("Zr's Help Browser")

	Button("Filter", FILTER_DISPLAY, size[2]-400, size[3]-22, 45, 18)
	Button("Back", BACK_MODULE, size[2]-300, size[3]-22, 45, 18)
	browsing= String("Browse: ", BROWSE_EVT, size[2]-250, size[3]-22, 245, 18, browsing.val, 30)

	# The real table
	def row_draw(rect, data, cols, cell_colors, text_colors):
		if (len(data)!=len(cols)):
			print "Must have same length data and columns"
			return

		if (type(cell_colors)!=ListType): cell_colors= [cell_colors]
		if (type(text_colors)!=ListType): text_colors= [text_colors]

		sx= rect[0]
		for i in range(len(data)):
			d= data[i]
			c= cols[i]
	
			c, align= c[0], c[1]

			if (type(c)==FloatType): c= c*(rect[2]-rect[0])
			ex= sx + c

			color= cell_colors[i%len(cell_colors)]
			apply(glColor3f, color)
			glRectf(sx, rect[1], ex, rect[3])

			color= text_colors[i%len(text_colors)]
			apply(glColor3f, color)

			if (type(d)==StringType):
				str_width= len(d)*8
				if (align=='left'): glRasterPos2f(sx+3, rect[1]+5)
				elif (align=='center'): glRasterPos2f((sx+ex)/2 - str_width/2 +3, rect[1]+5)
				elif (align=='right'): glRasterPos2f(ex - str_width -3, rect[1]+5)

				Text(d)
			else:
				d(map(int,[sx, rect[1], ex, rect[3]]))

			sx= ex
	# Some colors
	black= (0.0, 0.0, 0.0)
	white= (1.0, 1.0, 1.0)
	red= (0.8, 0.1, 0.1)

	gray0= (0.17, 0.17, 0.17)
	gray1= (0.25, 0.25, 0.25)
	gray2= (0.33, 0.33, 0.33)
	gray3= (0.41, 0.41, 0.41)
	gray4= (0.49, 0.49, 0.49)
	gray5= (0.57, 0.57, 0.57)
	gray6= (0.65, 0.65, 0.65)

	cols= [[.3, 'left'], [.2, 'left'], [.4, 'right'], [.1, 'center']]

	header= [size[0]+20, size[3]-60, size[2]-40, size[3]-40]

	def sort_byname(co): Button("Name",SORT_BYNAME, co[0]+3, co[1], co[2]-co[0]-4, 19)
	def sort_bytype(co): Button("Type",SORT_BYTYPE, co[0]+3, co[1], co[2]-co[0]-4, 19)
	def sort_bydata(co): Button("Data",SORT_BYDATA, co[0]+3, co[1], co[2]-co[0]-4, 19)

	row_draw(header, [sort_byname, sort_bytype, sort_bydata,'Link'], cols, [gray0, gray1], gray6)

	if (selected!=-1):
		table= [size[0]+20, size[1]+220, size[2]-40, size[3]-60]
	else:
		table= [size[0]+20, size[1]+20, size[2]-40, size[3]-60]

	row_height= 25
	items= (table[3]-table[1])/row_height

	items= 10
	if (items>len(browselist)): items= len(browselist)

	end= len(browselist)-items
	if (end>0):
		scr= Scrollbar(SCROLLBAR, table[2]+5, table[1], 20, table[3]-table[1], scr.val, 0.0, end, 0, "Page Up/Down scrolls list.")

	row= table
	row[1]= row[3]-row_height
	start= browse_scrollstart
	if (start+items>len(browselist)): items= len(browselist)-start
	for i in range(items):
		i= start+i
		data= browselist[i][:]

		if (i%2): colors= [gray1, gray2]
		else: colors= [gray2, gray3]

		# Strange pythonic code
		def view_doc(co,num=i):
			Button("Doc",VIEW_DOC+num, co[0]+3, co[1]+2, co[2]-co[0]-4, 19)

		def browse_module(co,num=i):
			Button("Browse",BROWSE_MODULE+num, co[0]+3, co[1]+2, co[2]-co[0]-4, 19)

		if (data[1]=='Function'):
			if data[3]:
				data[3]= view_doc
				tcolor= black
			else:
				tcolor= red
				data[2]= 'NO DOC STRING'
				data[3]= ''
		else:
			if (data[1]=='Module'): data[3]= browse_module
			else: data[3]= ''

			tcolor= black

		row_draw(row, data, cols, colors, tcolor)

		row[1]= row[1]-row_height
		row[3]= row[3]-row_height

	if (selected!=-1):
		table= [size[0]+20, size[1]+20, size[2]-40, size[1]+180]

		apply(glColor3f, gray5)
		glRectf(table[0], table[3], table[2], table[3]+20)
		apply(glColor3f, gray2)
		glRectf(table[0], table[1], table[2], table[3])

		apply(glColor3f, black)
		glRasterPos2f(table[0]+3, table[3]+5)
		Text("Function: " + browsing.val + "." + browselist[selected][0])

		Button("Close", CLOSE_VIEW, table[2]-50, table[3], 45, 18)

		row_height= 20
		view_lines= int((table[3]-table[1])/row_height)-1

		lines= split(browselist[selected][3], "\n")
		doc_lines= len(lines)

		sindex= view_lines*selected_page
		eindex= view_lines*(selected_page+1)
		if (sindex>0):
			sindex= sindex-1
			eindex= eindex-1

		lines= lines[sindex:eindex]

		y= table[3]-20
		for line in lines:
			glRasterPos2f(table[0]+3, y)
			Text(line)

			y= y-20

		if (sindex): Button("Page up", DOC_PAGE_UP, table[2]-100, table[3]-20, 90, 18)
		if (eindex<doc_lines): Button("Page down", DOC_PAGE_DOWN, table[2]-100, table[1]+5, 90, 18)

lmouse= [0, 0]
def event(evt, val):
	global browse_scrollstart

	if (evt==QKEY or evt==ESCKEY): Exit()
	elif (evt in [PAGEUPKEY, PAGEDOWNKEY] and val): 
		if (evt==PAGEUPKEY): browse_scrollstart= browse_scrollstart-5
		else: browse_scrollstart= browse_scrollstart+5
		
		if (browse_scrollstart<0): browse_scrollstart= 0
		elif (browse_scrollstart>=len(browselist)): browse_scrollstart= len(browselist)-1

		Redraw()

def bevent(evt):
	if (evt==BROWSE_EVT): make_browselist()

	elif (evt==SORT_BYNAME): sort_browselist(1)
	elif (evt==SORT_BYTYPE): sort_browselist(2)
	elif (evt==SORT_BYDATA): sort_browselist(3)
	
	elif (evt==DOC_PAGE_UP): view_page(-1)
	elif (evt==DOC_PAGE_DOWN): view_page(1)

	elif (evt==BACK_MODULE): browse_module(-1)
	elif (evt==CLOSE_VIEW): view_doc(-1)
	elif (evt==FILTER_DISPLAY): toggle_function_filter()

	elif (evt==SCROLLBAR):
		global browse_scrollstart
		browse_scrollstart= int(scr.val)

	elif (evt>=BROWSE_MODULE): browse_module(evt-BROWSE_MODULE)
	elif (evt>=VIEW_DOC): view_doc(evt-VIEW_DOC)	

	Redraw()

Register(draw, event, bevent)
