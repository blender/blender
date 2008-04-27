#!BPY
"""
Name: 'Manual'
Blender: 234
Group: 'Help'
Tooltip: 'The Blender reference manual'
"""

__author__ = "Matt Ebb"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.0"
__bpydoc__ = """\
This script opens the user's default web browser at www.blender3d.org's
"Blender Manual" page.
"""

# --------------------------------------------------------------------------
# Manual Help Menu Item
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

import Blender, webbrowser
version = str(Blender.Get('version'))
webbrowser.open('http://www.blender3d.org/Help/?pg=Manual&ver=' + version)
