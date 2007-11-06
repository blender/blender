# $Id$
#
# --------------------------------------------------------------------------
# BPyBlender.py version 0.3 Mar 20, 2005
# --------------------------------------------------------------------------
# helper functions to be used by other scripts
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

# Basic set of modules Blender should have in all supported platforms.
# The second and third lines are the contents of the Python23.zip file
# included with Windows Blender binaries along with zlib.pyd.
# Other platforms are assumed to have Python installed.
basic_modules = [
'Blender',
'chunk','colorsys','copy','copy_reg','gzip','os','random','repr','stat',
'string','StringIO','types','UserDict','webbrowser', 'zlib', 'math',
'BPyBlender', 'BPyRegistry'
]
