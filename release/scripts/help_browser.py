#!BPY

"""
Name: 'Scripts Help Browser'
Blender: 234
Group: 'Help'
Tooltip: 'Show help information about a chosen installed script.'
"""

__author__ = "Willian P. Germano"
__version__ = "0.3 01/21/09"
__email__ = ('scripts', 'Author, wgermano:ig*com*br')
__url__ = ('blender', 'blenderartists.org')

__bpydoc__ ="""\
This script shows help information for scripts registered in the menus.

Usage:

- Start Screen:

To read any script's "user manual" select a script from one of the
available category menus.  If the script has help information in the format
expected by this Help Browser, it will be displayed in the Script Help
Screen.  Otherwise you'll be offered the possibility of loading the chosen
script's source file in Blender's Text Editor.  The programmer(s) may have
written useful comments there for users.

Hotkeys:<br>
   ESC or Q: [Q]uit

- Script Help Screen:

This screen shows the user manual page for the chosen script. If the text
doesn't fit completely on the screen, you can scroll it up or down with
arrow keys or a mouse wheel.  There may be link and email buttons that if
clicked should open your default web browser and email client programs for
further information or support.

Hotkeys:<br>
   ESC: back to Start Screen<br>
   Q:   [Q]uit<br>
   S:   view script's [S]ource code in Text Editor<br>
   UP, DOWN Arrows and mouse wheel: scroll text up / down
"""

# $Id$
#
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004: Willian P. Germano, wgermano _at_ ig.com.br
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
# Thanks: Brendon Murphy (suggestion) and Kevin Morgan (implementation)
# for the "run" button; Jean-Michel Soler for pointing a parsing error
# with multilines using triple single quotes.

import Blender
from Blender import sys as bsys, Draw, Window, Registry

WEBBROWSER = True
try:
	import webbrowser
except:
	WEBBROWSER = False

DEFAULT_EMAILS = {
	'scripts': ['Bf-scripts-dev', 'bf-scripts-dev@blender.org']
}

DEFAULT_LINKS = {
	'blender': ["blender.org\'s Python forum", "http://www.blender.org/modules.php?op=modload&name=phpBB2&file=viewforum&f=9"]
}

PADDING = 15
COLUMNS = 1
TEXT_WRAP = 100
WIN_W = WIN_H = 200
SCROLL_DOWN = 0

def screen_was_resized():
	global WIN_W, WIN_H

	w, h = Window.GetAreaSize()
	if WIN_W != w or WIN_H != h:
		WIN_W = w
		WIN_H = h
		return True
	return False

def fit_on_screen():
	global TEXT_WRAP, PADDING, WIN_W, WIN_H, COLUMNS

	COLUMNS = 1
	WIN_W, WIN_H = Window.GetAreaSize()
	TEXT_WRAP = int((WIN_W - PADDING) / 6)
	if TEXT_WRAP < 40:
		TEXT_WRAP = 40
	elif TEXT_WRAP > 100:
		if TEXT_WRAP > 110:
			COLUMNS = 2
			TEXT_WRAP /= 2
		else: TEXT_WRAP = 100

def cut_point(text, length):
	"Returns position of the last space found before 'length' chars"
	l = length
	c = text[l]
	while c != ' ':
		l -= 1
		if l == 0: return length # no space found
		c = text[l]
	return l

def text_wrap(text, length = None):
	global TEXT_WRAP

	wrapped = []
	lines = text.split('<br>')
	llen = len(lines)
	if llen > 1:
		if lines[-1] == '': llen -= 1
		for i in range(llen - 1):
			lines[i] = lines[i].rstrip() + '<br>'
		lines[llen-1] = lines[llen-1].rstrip()

	if not length: length = TEXT_WRAP

	for l in lines:
		while len(l) > length:
			cpt = cut_point(l, length)
			line, l = l[:cpt], l[cpt + 1:]
			wrapped.append(line)
		wrapped.append(l)
	return wrapped

def load_script_text(script):
	global PATHS, SCRIPT_INFO

	if script.userdir:
		path = PATHS['uscripts']
	else:
		path = PATHS['scripts']

	fname = bsys.join(path, script.fname)

	source = Blender.Text.Load(fname)
	if source:
		Draw.PupMenu("File loaded%%t|Please check the file \"%s\" in the Text Editor window" % source.name)


# for theme colors:
def float_colors(cols):
	return map(lambda x: x / 255.0, cols)

# globals

SCRIPT_INFO = None

PATHS = {
	'home': Blender.Get('homedir'),
	'scripts': Blender.Get('scriptsdir'),
	'uscripts': Blender.Get('uscriptsdir')
}

if not PATHS['home']:
	errmsg = """
Can't find Blender's home dir and so can't find the
Bpymenus file automatically stored inside it, which
is needed by this script.  Please run the
Help -> System -> System Information script to get
information about how to fix this.
"""
	raise SystemError, errmsg

BPYMENUS_FILE = bsys.join(PATHS['home'], 'Bpymenus')

f = file(BPYMENUS_FILE, 'r')
lines = f.readlines()
f.close()

AllGroups = []

class Script:

	def __init__(self, data):
		self.name = data[0]
		self.version = data[1]
		self.fname = data[2]
		self.userdir = data[3]
		self.tip = data[4]

# End of class Script


class Group:

	def __init__(self, name):
		self.name = name
		self.scripts = []

	def add_script(self, script):
		self.scripts.append(script)

	def get_name(self):
		return self.name

	def get_scripts(self):
		return self.scripts

# End of class Group


class BPy_Info:

	def __init__(self, script, dict):

		self.script = script

		self.d = dict

		self.header = []
		self.len_header = 0
		self.content = []
		self.len_content = 0
		self.spaces = 0
		self.fix_urls()
		self.make_header()
		self.wrap_lines()

	def make_header(self):

		sc = self.script
		d = self.d

		header = self.header

		title = "Script: %s" % sc.name
		version = "Version: %s for Blender %1.2f or newer" % (d['__version__'],
			sc.version / 100.0)

		if len(d['__author__']) == 1:
			asuffix = ':'
		else: asuffix = 's:'

		authors = "%s%s %s" % ("Author", asuffix, ", ".join(d['__author__']))

		header.append(title)
		header.append(version)
		header.append(authors)
		self.len_header = len(header)


	def fix_urls(self):

		emails = self.d['__email__']
		fixed = []
		for a in emails:
			if a in DEFAULT_EMAILS.keys():
				fixed.append(DEFAULT_EMAILS[a])
			else:
				a = a.replace('*','.').replace(':','@')
				ltmp = a.split(',')
				if len(ltmp) != 2:
					ltmp = [ltmp[0], ltmp[0]]
				fixed.append(ltmp)

		self.d['__email__'] = fixed

		links = self.d['__url__']
		fixed = []
		for a in links:
			if a in DEFAULT_LINKS.keys():
				fixed.append(DEFAULT_LINKS[a])
			else:
				ltmp = a.split(',')
				if len(ltmp) != 2:
					ltmp = [ltmp[0], ltmp[0]]
				fixed.append([ltmp[0].strip(), ltmp[1].strip()])

		self.d['__url__'] = fixed


	def wrap_lines(self, reset = 0):

		lines = self.d['__bpydoc__'].split('\n')
		self.content = []
		newlines = []
		newline = []

		if reset:
			self.len_content = 0
			self.spaces = 0

		for l in lines:
			if l == '' and newline:
				newlines.append(newline)
				newline = []
				newlines.append('')
			else: newline.append(l)
		if newline: newlines.append(newline)

		for lst in newlines:
			wrapped = text_wrap(" ".join(lst))
			for l in wrapped:
				self.content.append(l)
				if l: self.len_content += 1
				else: self.spaces += 1

		if not self.content[-1]:
			self.len_content -= 1


# End of class BPy_Info

def parse_pyobj_close(closetag, lines, i):
	i += 1
	l = lines[i]
	while l.find(closetag) < 0:
		i += 1
		l = "%s%s" % (l, lines[i])
	return [l, i]

def parse_pyobj(var, lines, i):
	"Bad code, was in a hurry for release"

	l = lines[i].replace(var, '').replace('=','',1).strip()

	i0 = i - 1

	if l[0] == '"':
		if l[1:3] == '""': # """
			if l.find('"""', 3) < 0: # multiline
				l2, i = parse_pyobj_close('"""', lines, i)
				if l[-1] == '\\': l = l[:-1]
				l = "%s%s" % (l, l2)
		elif l[-1] == '"' and l[-2] != '\\': # single line: "..."
			pass
		else:
			l = "ERROR"

	elif l[0] == "'":
		if l[1:3] == "''": # '''
			if l.find("'''", 3) < 0: # multiline
				l2, i = parse_pyobj_close("'''", lines, i)
				if l[-1] == '\\': l = l[:-1]
				l = "%s%s" % (l, l2)
		elif l[-1] == '\\':
			l2, i = parse_pyobj_close("'", lines, i)
			l = "%s%s" % (l, l2)
		elif l[-1] == "'" and l[-2] !=  '\\': # single line: '...'
			pass
		else:
			l = "ERROR"

	elif l[0] == '(':
		if l[-1] != ')':
			l2, i = parse_pyobj_close(')', lines, i)
			l = "%s%s" % (l, l2)

	elif l[0] == '[':
		if l[-1] != ']':
			l2, i = parse_pyobj_close(']', lines, i)
			l = "%s%s" % (l, l2)

	return [l, i - i0]

# helper functions:

def parse_help_info(script):

	global PATHS, SCRIPT_INFO

	if script.userdir:
		path = PATHS['uscripts']
	else:
		path = PATHS['scripts']

	fname = bsys.join(path, script.fname)

	if not bsys.exists(fname):
		Draw.PupMenu('IO Error: couldn\'t find script %s' % fname)
		return None

	f = file(fname, 'r')
	lines = f.readlines()
	f.close()

	# fix line endings:
	if lines[0].find('\r'):
		unixlines = []
		for l in lines:
			unixlines.append(l.replace('\r',''))
		lines = unixlines

	llen = len(lines)
	has_doc = 0

	doc_data = {
		'__author__': '',
		'__version__': '',
		'__url__': '',
		'__email__': '',
		'__bpydoc__': '',
		'__doc__': ''
	}

	i = 0
	while i < llen:
		l = lines[i]
		incr = 1
		for k in doc_data.keys():
			if l.find(k, 0, 20) == 0:
				value, incr = parse_pyobj(k, lines, i)
				exec("doc_data['%s'] = %s" % (k, value))
				has_doc = 1
				break
		i += incr

	# fix these to seqs, simplifies coding elsewhere
	for w in ['__author__', '__url__', '__email__']:
		val = doc_data[w]
		if val and type(val) == str:
			doc_data[w] = [doc_data[w]]

	if not doc_data['__bpydoc__']:
		if doc_data['__doc__']:
			doc_data['__bpydoc__'] = doc_data['__doc__']

	if has_doc: # any data, maybe should confirm at least doc/bpydoc
		info = BPy_Info(script, doc_data)
		SCRIPT_INFO = info
		return True

	else:
		return False


def parse_script_line(l):

	tip = 'No tooltip'
	try:
		pieces = l.split("'")
		name = pieces[1].replace('...','')
		data = pieces[2].strip().split()
		version = data[0]
		userdir = data[-1]
		fname = data[1]
		i = 1
		while not fname.endswith('.py'):
			i += 1
			fname = '%s %s' % (fname, data[i])
		if len(pieces) > 3: tip = pieces[3]
	except:
		return None

	return [name, int(version), fname, int(userdir), tip]


def parse_bpymenus(lines):

	global AllGroups

	llen = len(lines)

	for i in range(llen):
		l = lines[i].strip()
		if not l: continue
		if l[-1] == '{':
			group = Group(l[:-2])
			AllGroups.append(group)
			i += 1
			l = lines[i].strip()
			while l != '}':
				if l[0] != '|':
					data = parse_script_line(l)
					if data:
						script = Script(data)
						group.add_script(script)
				i += 1
				l = lines[i].strip()

#	AllGroups.reverse()


def create_group_menus():

	global AllGroups
	menus = []

	for group in AllGroups:

		name = group.get_name()
		menu = []
		scripts = group.get_scripts()
		for s in scripts: menu.append(s.name)
		menu = "|".join(menu)
		menu = "%s%%t|%s" % (name, menu)
		menus.append([name, menu])

	return menus


# Collecting data:
fit_on_screen()
parse_bpymenus(lines)
GROUP_MENUS = create_group_menus()


# GUI:

from Blender import BGL
from Blender.Window import Theme

# globals:

START_SCREEN  = 0
SCRIPT_SCREEN = 1

SCREEN = START_SCREEN

# gui buttons:
len_gmenus = len(GROUP_MENUS)

BUT_GMENU = range(len_gmenus)
for i in range(len_gmenus):
	BUT_GMENU[i] = Draw.Create(0)

# events:
BEVT_LINK  = None # range(len(SCRIPT_INFO.links))
BEVT_EMAIL = None # range(len(SCRIPT_INFO.emails))
BEVT_GMENU = range(100, len_gmenus + 100)
BEVT_VIEWSOURCE = 1
BEVT_EXIT = 2
BEVT_BACK = 3
BEVT_EXEC = 4	# Executes Script

# gui callbacks:

def gui(): # drawing the screen

	global SCREEN, START_SCREEN, SCRIPT_SCREEN
	global SCRIPT_INFO, AllGroups, GROUP_MENUS
	global BEVT_EMAIL, BEVT_LINK
	global BEVT_VIEWSOURCE, BEVT_EXIT, BEVT_BACK, BEVT_GMENU, BUT_GMENU, BEVT_EXEC
	global PADDING, WIN_W, WIN_H, SCROLL_DOWN, COLUMNS, FMODE

	theme = Theme.Get()[0]
	tui = theme.get('ui')
	ttxt = theme.get('text')

	COL_BG = float_colors(ttxt.back)
	COL_TXT = ttxt.text
	COL_TXTHI = ttxt.text_hi

	BGL.glClearColor(COL_BG[0],COL_BG[1],COL_BG[2],COL_BG[3])
	BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
	BGL.glColor3ub(COL_TXT[0],COL_TXT[1], COL_TXT[2])

	resize = screen_was_resized()
	if resize: fit_on_screen()

	if SCREEN == START_SCREEN:
		x = PADDING
		bw = 85
		bh = 25
		hincr = 50

		butcolumns = (WIN_W - 2*x)/ bw
		if butcolumns < 2: butcolumns = 2
		elif butcolumns > 7: butcolumns = 7

		len_gm = len(GROUP_MENUS)
		butlines = len_gm / butcolumns
		if len_gm % butcolumns: butlines += 1

		h = hincr * butlines + 20
		y = h + bh

		BGL.glColor3ub(COL_TXTHI[0],COL_TXTHI[1], COL_TXTHI[2])
		BGL.glRasterPos2i(x, y)
		Draw.Text('Scripts Help Browser')

		y -= bh

		BGL.glColor3ub(COL_TXT[0],COL_TXT[1], COL_TXT[2])

		i = 0
		j = 0
		for group_menu in GROUP_MENUS:
			BGL.glRasterPos2i(x, y)
			Draw.Text(group_menu[0]+':')
			BUT_GMENU[j] = Draw.Menu(group_menu[1], BEVT_GMENU[j],
				x, y-bh-5, bw, bh, 0,
				'Choose a script to read its help information')
			if i == butcolumns - 1:
				x = PADDING
				i = 0
				y -= hincr
			else:
				i += 1
				x += bw + 3
			j += 1

		x = PADDING
		y = 10
		BGL.glRasterPos2i(x, y)
		Draw.Text('Select script for its help.  Press Q or ESC to leave.')

	elif SCREEN == SCRIPT_SCREEN:
		if SCRIPT_INFO:

			if resize:
				SCRIPT_INFO.wrap_lines(1)
				SCROLL_DOWN = 0

			h = 18 * SCRIPT_INFO.len_content + 12 * SCRIPT_INFO.spaces
			x = PADDING
			y = WIN_H
			bw = 38
			bh = 16

			BGL.glColor3ub(COL_TXTHI[0],COL_TXTHI[1], COL_TXTHI[2])
			for line in SCRIPT_INFO.header:
				y -= 18
				BGL.glRasterPos2i(x, y)
				size = Draw.Text(line)

			for line in text_wrap('Tooltip: %s' % SCRIPT_INFO.script.tip):
				y -= 18
				BGL.glRasterPos2i(x, y)
				size = Draw.Text(line)

			i = 0
			y -= 28
			for data in SCRIPT_INFO.d['__url__']:
				Draw.PushButton('link %d' % (i + 1), BEVT_LINK[i],
					x + i*bw, y, bw, bh, data[0])
				i += 1
			y -= bh + 1

			i = 0
			for data in SCRIPT_INFO.d['__email__']:
				Draw.PushButton('email', BEVT_EMAIL[i], x + i*bw, y, bw, bh, data[0])
				i += 1
			y -= 18

			y0 = y
			BGL.glColor3ub(COL_TXT[0],COL_TXT[1], COL_TXT[2])
			for line in SCRIPT_INFO.content[SCROLL_DOWN:]:
				if line:
					line = line.replace('<br>', '')
					BGL.glRasterPos2i(x, y)
					Draw.Text(line)
					y -= 18
				else: y -= 12
				if y < PADDING + 20: # reached end, either stop or go to 2nd column
					if COLUMNS == 1: break
					elif x == PADDING: # make sure we're still in column 1
						x = 6*TEXT_WRAP + PADDING / 2
						y = y0

			x = PADDING
			Draw.PushButton('source', BEVT_VIEWSOURCE, x, 17, 45, bh,
				'View this script\'s source code in the Text Editor (hotkey: S)')
			Draw.PushButton('exit', BEVT_EXIT, x + 45, 17, 45, bh,
				'Exit from Scripts Help Browser (hotkey: Q)')
			if not FMODE: 
				Draw.PushButton('back', BEVT_BACK, x + 2*45, 17, 45, bh,
				'Back to scripts selection screen (hotkey: ESC)')
				Draw.PushButton('run script', BEVT_EXEC, x + 3*45, 17, 60, bh, 'Run this script')

			BGL.glColor3ub(COL_TXTHI[0],COL_TXTHI[1], COL_TXTHI[2])
			BGL.glRasterPos2i(x, 5)
			Draw.Text('use the arrow keys or the mouse wheel to scroll text', 'small')

def fit_scroll():
	global SCROLL_DOWN
	if not SCRIPT_INFO:
		SCROLL_DOWN = 0
		return
	max = SCRIPT_INFO.len_content + SCRIPT_INFO.spaces - 1
	if SCROLL_DOWN > max: SCROLL_DOWN = max
	if SCROLL_DOWN < 0: SCROLL_DOWN = 0


def event(evt, val): # input events

	global SCREEN, START_SCREEN, SCRIPT_SCREEN
	global SCROLL_DOWN, FMODE

	if not val: return

	if evt == Draw.ESCKEY:
		if SCREEN == START_SCREEN or FMODE: Draw.Exit()
		else:
			SCREEN = START_SCREEN
			SCROLL_DOWN = 0
			Draw.Redraw()
		return
	elif evt == Draw.QKEY:
		Draw.Exit()
		return
	elif evt in [Draw.DOWNARROWKEY, Draw.WHEELDOWNMOUSE] and SCREEN == SCRIPT_SCREEN:
		SCROLL_DOWN += 1
		fit_scroll()
		Draw.Redraw()
		return
	elif evt in [Draw.UPARROWKEY, Draw.WHEELUPMOUSE] and SCREEN == SCRIPT_SCREEN:
		SCROLL_DOWN -= 1
		fit_scroll()
		Draw.Redraw()
		return
	elif evt == Draw.SKEY:
		if SCREEN == SCRIPT_SCREEN and SCRIPT_INFO:
			load_script_text(SCRIPT_INFO.script)
			return

def button_event(evt): # gui button events

	global SCREEN, START_SCREEN, SCRIPT_SCREEN
	global BEVT_LINK, BEVT_EMAIL, BEVT_GMENU, BUT_GMENU, SCRIPT_INFO
	global SCROLL_DOWN, FMODE

	if evt >= 100: # group menus
		for i in range(len(BUT_GMENU)):
			if evt == BEVT_GMENU[i]:
				group = AllGroups[i]
				index = BUT_GMENU[i].val - 1
				if index < 0: return # user didn't pick a menu entry
				script = group.get_scripts()[BUT_GMENU[i].val - 1]
				if parse_help_info(script):
					SCREEN = SCRIPT_SCREEN
					BEVT_LINK = range(20, len(SCRIPT_INFO.d['__url__']) + 20)
					BEVT_EMAIL = range(50, len(SCRIPT_INFO.d['__email__']) + 50)
					Draw.Redraw()
				else:
					res = Draw.PupMenu("No help available%t|View Source|Cancel")
					if res == 1:
						load_script_text(script)
	elif evt >= 20:
		if not WEBBROWSER:
			Draw.PupMenu('Missing standard Python module%t|You need module "webbrowser" to access the web')
			return

		if evt >= 50: # script screen email buttons
			email = SCRIPT_INFO.d['__email__'][evt - 50][1]
			webbrowser.open("mailto:%s" % email)
		else: # >= 20: script screen link buttons
			link = SCRIPT_INFO.d['__url__'][evt - 20][1]
			webbrowser.open(link)
	elif evt == BEVT_VIEWSOURCE:
		if SCREEN == SCRIPT_SCREEN: load_script_text(SCRIPT_INFO.script)
	elif evt == BEVT_EXIT:
		Draw.Exit()
		return
	elif evt == BEVT_BACK:
		if SCREEN == SCRIPT_SCREEN and not FMODE:
			SCREEN = START_SCREEN
			SCRIPT_INFO = None
			SCROLL_DOWN = 0
			Draw.Redraw()
	elif evt == BEVT_EXEC: # Execute script
		exec_line = ''
		if SCRIPT_INFO.script.userdir:
			exec_line = bsys.join(Blender.Get('uscriptsdir'), SCRIPT_INFO.script.fname)
		else:
			exec_line = bsys.join(Blender.Get('scriptsdir'), SCRIPT_INFO.script.fname)

		Blender.Run(exec_line)

keepon = True
FMODE = False # called by Blender.ShowHelp(name) API function ?

KEYNAME = '__help_browser'
rd = Registry.GetKey(KEYNAME)
if rd:
	rdscript = rd['script']
	keepon = False
	Registry.RemoveKey(KEYNAME)
	for group in AllGroups:
		for script in group.get_scripts():
			if rdscript == script.fname:
				parseit = parse_help_info(script)
				if parseit == True:
					keepon = True
					SCREEN = SCRIPT_SCREEN
					BEVT_LINK = range(20, len(SCRIPT_INFO.d['__url__']) + 20)
					BEVT_EMAIL = range(50, len(SCRIPT_INFO.d['__email__']) + 50)
					FMODE = True
				elif parseit == False:
					Draw.PupMenu("ERROR: script doesn't have proper help data")
				break

if not keepon:
	Draw.PupMenu("ERROR: couldn't find script")
else:
	Draw.Register(gui, event, button_event)
