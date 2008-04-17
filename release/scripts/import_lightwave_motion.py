#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Lightwave Motion (.mot)...'
Blender: 245
Group: 'Import'
Tip: 'Import Loc Rot Size chanels from a Lightwave .mot file'
"""

__author__ = "Daniel Salazar (ZanQdo)"
__url__ = ("blender", "elysiun",
"e-mail: zanqdo@gmail.com")
__version__ = "16/04/08"

__bpydoc__ = """\
This script loads Lightwave motion files (.mot)
into the selected objects

Usage:
Run the script with one or more objects selected (any kind)
Be sure to set the framerate correctly

"""

# $Id: export_lightwave_motion.py 9924 2007-01-27 02:15:14Z campbellbarton $
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003, 2004: A Vanpoucke
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

import math as M
import Blender as B
import bpy


def FuncionPrincipal (Dir):
	B.Window.WaitCursor(1)
	ObjSelect = B.Object.GetSelected()
	
	if not ObjSelect:
		B.Draw.PupMenu('Select one or more objects, aborting.')
		return
	
	
	SC = B.Scene.GetCurrent()
	SCR = SC.getRenderingContext()
	FrameRate = float(SCR.framesPerSec())
	
	
	# Creating new IPO
	
	IPO = B.Ipo.New('Object', 'LW_Motion')
	
	
	# Creating Curves in the IPO
	
	LocX   = IPO.addCurve("LocX")
	LocX.setInterpolation("Bezier")
	
	LocY   = IPO.addCurve("LocY")
	LocX.setInterpolation("Bezier")
	
	LocZ   = IPO.addCurve("LocZ")
	LocX.setInterpolation("Bezier")
	
	RotX   = IPO.addCurve("RotX")
	LocX.setInterpolation("Bezier")
	
	RotY   = IPO.addCurve("RotY")
	LocX.setInterpolation("Bezier")
	
	RotZ   = IPO.addCurve("RotZ")
	LocX.setInterpolation("Bezier")
	
	ScaleX = IPO.addCurve("ScaleX")
	LocX.setInterpolation("Bezier")
	
	ScaleY = IPO.addCurve("ScaleY")
	LocX.setInterpolation("Bezier")
	
	ScaleZ = IPO.addCurve("ScaleZ")
	LocX.setInterpolation("Bezier")
	
	
	# Opening the mot file
	
	File = open (Dir, 'rU')
	
	
	# Init flags
	
	CurChannel = -1
	ScaleFlag = 0
	
	# Main file reading cycle
	
	for Line in File:
		
		'''
		# Number of channels in the file
		
		if "NumChannels" in Line:
			Line = Line.split (' ')
			NumChannels = int(Line[1])
		'''
		
		# Current Channel Flag
		
		if "Channel 0" in Line:
			CurChannel = 0
			
		elif "Channel 1" in Line:
			CurChannel = 1
			
		elif "Channel 2" in Line:
			CurChannel = 2
			
		elif "Channel 3" in Line:
			CurChannel = 3
			
		elif "Channel 4" in Line:
			CurChannel = 4
			
		elif "Channel 5" in Line:
			CurChannel = 5
			
		elif "Channel 6" in Line:
			CurChannel = 6
			
		elif "Channel 7" in Line:
			CurChannel = 7
			
		elif "Channel 8" in Line:
			CurChannel = 8
		
		
		# Getting the data and writing to IPOs
		
		if CurChannel == 0:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_0 = float (Line [3])
				TimeCh_0  = float (Line [4]) * FrameRate
				LocX.addBezier ((TimeCh_0, ValCh_0))
				
		if CurChannel == 1:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_1 = float (Line [3])
				TimeCh_1  = float (Line [4]) * FrameRate
				LocZ.addBezier ((TimeCh_1, ValCh_1))
				
		if CurChannel == 2:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_2 = float (Line [3])
				TimeCh_2  = float (Line [4]) * FrameRate
				LocY.addBezier ((TimeCh_2, ValCh_2))
				
		if CurChannel == 3:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_3 = M.degrees ( - float (Line [3]) ) / 10
				TimeCh_3  = float (Line [4]) * FrameRate
				RotZ.addBezier ((TimeCh_3, ValCh_3))
				
		if CurChannel == 4:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_4 = M.degrees ( - float (Line [3]) ) / 10
				TimeCh_4  = float (Line [4]) * FrameRate
				RotX.addBezier ((TimeCh_4, ValCh_4))
				
		if CurChannel == 5:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_5 = M.degrees ( - float (Line [3]) ) / 10
				TimeCh_5  = float (Line [4]) * FrameRate
				RotY.addBezier ((TimeCh_5, ValCh_5))
				
		if CurChannel == 6:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_6 = float (Line [3])
				TimeCh_6  = float (Line [4]) * FrameRate
				ScaleX.addBezier ((TimeCh_6, ValCh_6))
		elif ScaleFlag < 3:
			ScaleFlag += 1
			ScaleX.addBezier ((0, 1))
				
		if CurChannel == 7:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_7 = float (Line [3])
				TimeCh_7 = float (Line [4]) * FrameRate
				ScaleZ.addBezier ((TimeCh_7, ValCh_7))
		elif ScaleFlag < 3:
			ScaleFlag += 1
			ScaleZ.addBezier ((0, 1))
				
		if CurChannel == 8:
			if "Key" in Line:
				Line = Line.split (' ')
				ValCh_8 = float (Line [3])
				TimeCh_8  = float (Line [4]) * FrameRate
				ScaleY.addBezier ((TimeCh_8, ValCh_8))
		elif ScaleFlag < 3:
			ScaleFlag += 1
			ScaleY.addBezier ((0, 1))
			
			
	# Link the IPO to all selected objects
	
	for ob in ObjSelect:
		ob.setIpo(IPO)
	
	File.close()
	
	print '\nDone, the following motion file has been loaded:\n\n%s' % Dir
	B.Window.WaitCursor(0)

def main():
	B.Window.FileSelector(FuncionPrincipal, "Load IPO from .mot File", B.sys.makename(ext='.mot'))

if __name__=='__main__':
	main()

