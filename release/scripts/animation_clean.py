#!BPY

"""
Name: 'Clean Animation Curves'
Blender: 249
Group: 'Animation'
Tooltip: 'Remove unused keyframes for ipo curves'
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2008-2009: Blender Foundation
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
# --------------------------------------------------------------------------

import bpy
from Blender import IpoCurve, Draw, Window

def clean_ipos(ipos):
	eul = 0.001

	def isflat(vec):
		prev_y = vec[0][1]
		mid_y = vec[1][1]
		next_y = vec[2][1]
		
		# flat status for prev and next
		return abs(mid_y-prev_y) < eul, abs(mid_y-next_y) < eul
		
		
			
	X=0
	Y=1
	PREV=0
	MID=1
	NEXT=2

	LEFT = 0
	RIGHT = 1

	TOT = 0
	TOTBEZ = 0
	# for ipo in bpy.data.ipos:
	for ipo in ipos:
		if ipo.lib: 
			continue
		# print ipo
		for icu in ipo:
			interp = icu.interpolation
			extend = icu.extend 
			
			bezierPoints = icu.bezierPoints
			bezierVecs = [bez.vec for bez in bezierPoints]
			
			l = len(bezierPoints)
			
			TOTBEZ += l
			
			# our aim is to simplify this ipo as much as possible!
			if interp == IpoCurve.InterpTypes.BEZIER or interp == interp == IpoCurve.InterpTypes.LINEAR:
				#print "Not yet supported"
				
				if interp == IpoCurve.InterpTypes.BEZIER:
					flats = [isflat(bez) for bez in bezierVecs]
				else:
					# A bit of a waste but fake the locations for these so they will always be flats
					# IS better then too much duplicate code.
					flats = [(True, True)] * l
					for v in bezierVecs:
						v[PREV][Y] = v[NEXT][Y] = v[MID][Y]
					
				
				# remove middle points
				if l>2:
					done_nothing = False
					
					while not done_nothing and len(bezierVecs) > 2:
						done_nothing = True
						i = l-2
					
						while i > 0:
							#print i
							#print i, len(bezierVecs)
							if flats[i]==(True,True)  and  flats[i-1][RIGHT]  and  flats[i+1][LEFT]:
							
								if abs(bezierVecs[i][MID][Y] - bezierVecs[i-1][MID][Y]) < eul   and   abs(bezierVecs[i][MID][Y] - bezierVecs[i+1][MID][Y]) < eul:
									done_nothing = False
									
									del flats[i]
									del bezierVecs[i]
									icu.delBezier(i)
									TOT += 1
									l-=1
							i-=1
				
				# remove endpoints
				if extend == IpoCurve.ExtendTypes.CONST and len(bezierVecs) > 1:
					#print l, len(bezierVecs)
					# start
					
					while l > 2 and (flats[0][RIGHT]  and  flats[1][LEFT] and (abs(bezierVecs[0][MID][Y] - bezierVecs[1][MID][Y]) < eul)):
						print "\tremoving 1 point from start of the curve"
						del flats[0]
						del bezierVecs[0]
						icu.delBezier(0)
						TOT += 1
						l-=1
					
					
					# End 
					while l > 2 and flats[-2][RIGHT]  and  flats[-1][LEFT] and (abs(bezierVecs[-2][MID][Y] - bezierVecs[-1][MID][Y]) < eul):
						print "\tremoving 1 point from end of the curve", l
						del flats[l-1]
						del bezierVecs[l-1]
						icu.delBezier(l-1)
						TOT += 1
						l-=1
						
				
						
				if l==2:
					if isflat( bezierVecs[0] )[RIGHT] and isflat( bezierVecs[1] )[LEFT] and abs(bezierVecs[0][MID][Y] - bezierVecs[1][MID][Y]) < eul:
						# remove the second point
						print "\tremoving 1 point from 2 point bez curve"
						# remove the second point
						del flats[1]
						del bezierVecs[1]
						icu.delBezier(1)
						TOT+=1
						l-=1
						
				# Change to linear for faster evaluation
				'''
				if l==1:
					print 'Linear'
					icu.interpolation = IpoCurve.InterpTypes.LINEAR
				'''
				
		
			
			
			if interp== IpoCurve.InterpTypes.CONST:
				print "Not yet supported"
				
	print 'total', TOT, TOTBEZ
	return TOT, TOTBEZ

def main():
	ret = Draw.PupMenu('Clean Selected Objects Ipos%t|Object IPO%x1|Object Action%x2|%l|All IPOs (be careful!)%x3')
	
	sce = bpy.data.scenes.active
	ipos = []
	
	if ret == 3:
		ipos.extend(list(bpy.data.ipos))
	else:
		for ob in sce.objects.context:
			if ret == 1:
				ipo = ob.ipo
				if ipo:
					ipos.append(ipo)
			
			elif ret == 2:
				action = ob.action
				if action:
					ipos.extend([ipo for ipo in action.getAllChannelIpos().values() if ipo])
		
			
	
	if not ipos:
		Draw.PupMenu('Error%t|No ipos found')
	else:
		total_removed, total = clean_ipos(ipos)
		Draw.PupMenu('Done!%t|Removed ' + str(total_removed) + ' of ' + str(total) + ' points')
	
	Window.RedrawAll()
	

if __name__ == '__main__':
	main()
