#!BPY

"""
Name: 'System Information...'
Blender: 236
Group: 'HelpSystem'
Tooltip: 'Information about your Blender environment, useful to diagnose problems.'
"""

__author__ = "Willian P. Germano"
__url__ = ("blender", "elysiun")
__version__ = "1.1"
__bpydoc__ = """\
This script creates a text in Blender's Text Editor with information
about your OS, video card, OpenGL driver, Blender and Python versions and
more.

If you are experiencing trouble running Blender or its scripts in general,
this information can be useful for online searches (like checking if there
are known issues related to your video card) or to get help from other users
or the program's developers.
"""

# $Id$
#
# --------------------------------------------------------------------------
# sysinfo.py version 1.1 Mar 20, 2005
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

import Blender
import Blender.sys as bsys
from Blender.BGL import *
import sys

Blender.Window.WaitCursor(1)
# has_textwrap = 1 # see commented code below
output_filename = "system-info.txt"
warnings = 0
notices = 0 # non critical warnings

def cutPoint(text, length):
	"Returns position of the last space found before 'length' chars"
	l = length
	c = text[l]
	while c != ' ':
		l -= 1
		if l == 0: return length # no space found
		c = text[l]
	return l

def textWrap(text, length = 70):
	lines = []
	while len(text) > 70:
		cpt = cutPoint(text, length)
		line, text = text[:cpt], text[cpt + 1:]
		lines.append(line)
	lines.append(text)
	return lines

## Better use our own text wrap functions here
#try:
#  import textwrap
#except:
#  has_textwrap = 0
#  msg = sys.exc_info()[1].__str__().split()[3]
#  Blender.Draw.PupMenu("Python error:|This script requires the %s module" %msg)

header = "=  Blender %s System Information	=" % Blender.Get("version")
lilies = len(header)*"="+"\n"
header = lilies + header + "\n" + lilies

output = Blender.Text.New(output_filename)

output.write(header + "\n\n")

output.write("Platform: %s\n========\n\n" % sys.platform)

output.write("Python:\n======\n\n")
output.write("- Version: %s\n\n" % sys.version)
output.write("- Path:\n\n")
for p in sys.path:
	output.write(p + '\n')

output.write("\n- Directories:")

if not Blender.Get('homedir'):
	outmsg = """

<WARNING> - Blender home dir not found!
  This should probably be "<path>/.blender/"
  where <path> is usually the user's home dir.

  Blender's home dir is where entries like:
    folders scripts/, plugins/ and locale/ and
    files .Blanguages and .bfont.ttf
  are located.

  It's also where Blender stores the Bpymenus file
  with information about registered scripts, so it
  only needs to scan scripts dir(s) when they are
  modified.
"""
	output.write(outmsg)
	if Blender.Get('scriptsdir').find('release') > 0:
		output.write("""
It seems this Blender binary is located at its cvs source tree:
that's ok, but the scripts registration data will be recreated
from dir(s) whenever you start the program, instead of only
when those dirs are modified.

Adding a .blender/ subdir to e. g. your home dir can prevent that.
""")

dirlist = [
	['homedir', 'Blender home dir', 1],
	['scriptsdir', 'Default dir for scripts', 1],
	['datadir', 'Default "bpydata/" data dir for scripts', 1],
	['uscriptsdir', 'User defined dir for scripts', 0],
	['udatadir', 'Data dir "bpydata/" inside user defined dir', 0]
]

for dir in dirlist:
	dirname, dirstr, is_critical = dir
	dirpath = Blender.Get(dirname)
	output.write("\n\n %s:\n" % dirstr)
	if not dirpath:
		if is_critical:
			warnings += 1
			output.write("  <WARNING> -- not found")
		else:
			notices += 1
			output.write("  <NOTICE> -- not found")
	else:
		output.write("  %s" % dirpath)

configdir = bsys.join(Blender.Get('datadir'), 'config')
output.write('\n\n Default config data "bpydata/config/" dir:')
if bsys.exists(configdir):
	output.write("  %s" % configdir)
else:
	warnings += 1
	output.write("""
  <WARNING> -- not found.
  config/ should be inside the default scripts *data dir*.
  It's used by Blender to store scripts configuration data.
""")

if Blender.Get('udatadir'):
	uconfigdir = bsys.join(Blender.Get('udatadir'), 'config')
	output.write("\n\n User defined config data dir:")
	if bsys.exists(configdir):
		output.write("  %s" % configdir)
	else:
		notices += 1
		output.write("""
  <NOTICE> -- not found.
  bpydata/config/ should be inside the user defined scripts dir.
  It's used by Blender to store scripts configuration data.
  (Since it is on the user defined dir, a new Blender installation
  won't overwrite the data.)
""")

missing_mods = [] # missing basic modules

try:
	from BPyBlender import basic_modules
	for m in basic_modules:
		try: exec ("import %s" % m)
		except: missing_mods.append(m)

	if missing_mods:
		outmsg = """

<WARNING>:

Some expected modules were not found.
Because of that some scripts bundled with Blender may not work.
Please read the FAQ in the Readme.html file shipped with Blender
for information about how to fix the problem.
Missing modules:"""
		output.write(outmsg)
		warnings += 1
		for m in missing_mods:
			output.write('-> ' + m + '\n')
	else:
		output.write("\n\n- Modules: all basic ones were found.\n")

except ImportError:
	output.write("\n\n<WARNING>:\n  Couldn't find BPyBlender.py in scripts/bpymodules/ dir.")
	output.write("\n  Basic modules availability won't be tested.\n")
	warnings += 1

output.write("\nOpenGL:\n======\n\n")
output.write("- Renderer:   %s\n" % glGetString(GL_RENDERER))
output.write("- Vendor:     %s\n" % glGetString(GL_VENDOR))
output.write("- Version:    %s\n\n" % glGetString(GL_VERSION))
output.write("- Extensions:\n\n")

glext = glGetString(GL_EXTENSIONS)
glext = textWrap(glext, 70)

for l in glext:
	output.write(l + "\n")

output.write("\n\n- Simplistic almost useless benchmark:\n\n")
t = Blender.sys.time()
nredraws = 10
for i in range(nredraws):
	Blender.Redraw(-1) # redraw all windows
result = Blender.sys.time() - t
output.write("Redrawing all areas %s times took %.4f seconds.\n" % (nredraws, result))

if warnings or notices:
	output.write("\n%s%s\n" % (warnings*"#", notices*"."))
	if warnings:
		output.write("\n(*) Found %d warning" % warnings)
		if (warnings > 1): output.write("s") # (blush)
		output.write(", documented in the text above.\n")
	if notices:
		output.write("\n(*) Found %d notice" % notices)
		if (notices > 1): output.write("s") # (blush)
		output.write(", documented in the text above.\n")

else: output.write("\n==\nNo problems were found (scroll up for details).")

Blender.Window.WaitCursor(0)
exitmsg = "Done!|Please check the text %s in the Text Editor window" % output.name
Blender.Draw.PupMenu(exitmsg)
