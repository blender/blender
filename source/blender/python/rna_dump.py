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
 # Contributor(s): Campbell Barton
 #
 # #**** END GPL LICENSE BLOCK #****


PRINT_DATA = True
VERBOSE = False
VERBOSE_TYPE = False
SKIP_RECURSIVE = False


def seek(r, txt):
	print(txt)
	newtxt = ''
	
	if len(txt) > 200:
		print ("Somthing is wrong")
		print (txt)
		return
	
	type_r = type(r)
	
	# print(type_r)
	# print(dir(r))
	
	# basic types
	if type_r in (float, int, bool, type(None)):
		if PRINT_DATA:
			print(txt + ' -> ' + str(r))
		return
	
	if type_r == str:
		if PRINT_DATA:
			print(txt + ' -> "' + str(r) + '"')
		return
	
	try:	keys = r.keys()
	except: keys = None
	
	if keys != None:
		if PRINT_DATA:
			print(txt + '.keys() - ' + str(r.keys()))
	
	try:	__members__ = dir(r)
	except: __members__ = []
	
	for item in __members__:
		if item.startswith('__'):
			continue
			
		if PRINT_DATA:	newtxt = txt + '.' + item
		
		if item == 'rna_type' and VERBOSE_TYPE==False: # just avoid because it spits out loads of data
			continue
		
		if SKIP_RECURSIVE:
			if item in txt:
				if PRINT_DATA:
					print(newtxt + ' - (skipping to avoid recursive search)')
				continue
		
		try:	value = getattr(r, item)
		except:	value = None
		
		seek( value, newtxt)
	
	
	if keys:
		for k in keys:
			if PRINT_DATA:	newtxt = txt + '["' + k + '"]'
			seek(r.__getitem__(k), newtxt)
	
	else:
		try:	length = len( r )
		except:	length = 0
		
		if VERBOSE==False and length >= 4:
			for i in (0, length-1):
				if i>0:
					if PRINT_DATA:
						print((' '*len(txt)) + ' ... skipping '+str(length-2)+' items ...')
				
				if PRINT_DATA:	newtxt = txt + '[' + str(i) + ']'
				seek(r[i], newtxt)
		else:
			for i in range(length):
				if PRINT_DATA:	newtxt = txt + '[' + str(i) + ']'
				seek(r[i], newtxt)

#print (dir(bpy))
seek(bpy, 'bpy')


#print dir(bpy)
#import sys
#sys.exit()
