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

# Usage,
# run this script from blenders root path once you have compiled blender
# ./blender.bin -b -P source/blender/python/epy_doc_gen.py
# 
# This will generate rna.py, generate html docs  by running...
# epydoc source/blender/python/doc/rna.py -o source/blender/python/doc/html -v --no-sourcecode --name="RNA API" --url="http://brechtvanlommelfanclub.com" --graph=classtree
# 
# if you dont have graphvis installed ommit the --graph arg.



def rna2epy(target_path):
	
	def range_str(val):
		if val < -10000000:	return '-inf'
		if val >  10000000:	return 'inf'
		if type(val)==float:
			return '%g'  % val
		else:
			return str(val)	
	
	def write_struct(rna_struct):
		identifier = rna_struct.identifier
		
		rna_base = rna_struct.base
		
		if rna_base:
			out.write('class %s(%s):\n' % (identifier, rna_base.identifier))
		else:
			out.write('class %s:\n' % identifier)
		
		out.write('\t"""\n')
		
		title = 'The %s Object' % rna_struct.name
		
		out.write('\t%s\n' %  title)
		out.write('\t%s\n' %  ('=' * len(title)))
		out.write('\t\t%s\n' %  rna_struct.description)
		
		for rna_prop_identifier, rna_prop in rna_struct.properties.items():
			
			if rna_prop_identifier=='RNA':
				continue
			
			if rna_prop_identifier=='rna_type':
				continue
			
			rna_desc = rna_prop.description
			if not rna_desc: rna_desc = rna_prop.name
			if not rna_desc: rna_desc = 'Note - No documentation for this property!'
			
			rna_prop_type = rna_prop.type.lower()
			
			if rna_prop_type=='collection':	collection_str = 'Collection of '
			else:							collection_str = ''
			
			try:		rna_prop_ptr = rna_prop.fixed_type
			except:	rna_prop_ptr = None
			
			try:		length = rna_prop.array_length
			except:	length = 0
			
			if length > 0:	array_str = ' array of %d items' % length
			else:		array_str = ''
			
			if rna_prop.readonly:	readonly_str = ' (readonly)'
			else:				readonly_str = ''
			
			if rna_prop_ptr: # Use the pointer type
				out.write('\t@ivar %s: %s\n' %  (rna_prop_identifier, rna_desc))
				out.write('\t@type %s: %sL{%s}%s%s\n' %  (rna_prop_identifier, collection_str, rna_prop_ptr.identifier, array_str, readonly_str))
			else:
				if rna_prop_type == 'enum':
					out.write('\t@ivar %s: %s in (%s)\n' %  (rna_prop_identifier, rna_desc, ', '.join(rna_prop.items.keys())))
					out.write('\t@type %s: %s%s%s\n' %  (rna_prop_identifier, rna_prop_type,  array_str, readonly_str))
				elif rna_prop_type == 'int' or rna_prop_type == 'float':
					out.write('\t@ivar %s: %s\n' %  (rna_prop_identifier, rna_desc))
					out.write('\t@type %s: %s%s%s in [%s, %s]\n' %  (rna_prop_identifier, rna_prop_type, array_str, readonly_str, range_str(rna_prop.hard_min), range_str(rna_prop.hard_max) ))
				elif rna_prop_type == 'string':
					out.write('\t@ivar %s: %s (maximum length of %s)\n' %  (rna_prop_identifier, rna_desc, rna_prop.max_length))
					out.write('\t@type %s: %s%s%s\n' %  (rna_prop_identifier, rna_prop_type, array_str, readonly_str))
				else:
					out.write('\t@ivar %s: %s\n' %  (rna_prop_identifier, rna_desc))
					out.write('\t@type %s: %s%s%s\n' %  (rna_prop_identifier, rna_prop_type, array_str, readonly_str))
				
			
		out.write('\t"""\n\n')
	
	
	out = open(target_path, 'w')

	def base_id(rna_struct):
		try:		return rna_struct.base.identifier
		except:	return None

	structs = [(base_id(rna_struct), rna_struct.identifier, rna_struct) for rna_struct in bpydoc.structs.values()]
	structs.sort() # not needed but speeds up sort below, setting items without an inheritance first
	
	# Arrange so classes are always defined in the correct order
	deps_ok = False
	while deps_ok == False:
		deps_ok = True
		rna_done = set()
		
		for i, (rna_base, identifier, rna_struct) in enumerate(structs):
			
			rna_done.add(identifier)
			
			if rna_base and rna_base not in rna_done:
				deps_ok = False
				data = structs.pop(i)
				ok = False
				while i < len(structs):
					if structs[i][1]==rna_base:
						structs.insert(i+1, data) # insert after the item we depend on.
						ok = True
						break
					i+=1
					
				if not ok:
					print('Dependancy "%s"could not be found for "%s"' % (identifier, rna_base))
				
				break
	
	structs = [data[2] for data in structs]
	# Done ordering structs
	
	
	for rna_struct in structs:
		write_struct(rna_struct)
		
	out.write('\n')
	out.close()
	
	# # We could also just run....
	# os.system('epydoc source/blender/python/doc/rna.py -o ./source/blender/python/doc/html -v')

if __name__ == '__main__':
	rna2epy('source/blender/python/doc/rna.py')

