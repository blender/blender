

def execute(bcon):
	'''
	This function has been taken from a BGE console autocomp I wrote a while ago
	the dictionaty bcon is not needed but it means I can copy and paste from the old func
	which works ok for now.
	
	'bcon' dictionary keys, set by the caller
	* 'cursor' - index of the editing character (int)
	* 'edit_text' - text string for editing (string)
	* 'scrollback' - text to add to the scrollback, options are added here. (text)
	* 'namespace' - namespace, (dictionary)
	
	'''
	
	
	def is_delimiter(ch):
		'''
		For skipping words
		'''
		if ch == '_':
			return False
		if ch.isalnum():
			return False
		
		return True
	
	def is_delimiter_autocomp(ch):
		'''
		When autocompleteing will earch back and 
		'''
		if ch in '._[] "\'':
			return False
		if ch.isalnum():
			return False
		
		return True

	
	def do_autocomp(autocomp_prefix, autocomp_members):
		'''
		return text to insert and a list of options
		'''
		autocomp_members = [v for v in autocomp_members if v.startswith(autocomp_prefix)]
		
		print("AUTO: '%s'" % autocomp_prefix)
		print("MEMBERS: '%s'" % str(autocomp_members))
		
		if not autocomp_prefix:
			return '', autocomp_members
		elif len(autocomp_members) > 1:
			# find a common string between all members after the prefix 
			# 'ge' [getA, getB, getC] --> 'get'
			
			# get the shortest member
			min_len = min([len(v) for v in autocomp_members])
			
			autocomp_prefix_ret = ''
			
			for i in range(len(autocomp_prefix), min_len):
				char_soup = set()
				for v in autocomp_members:
					char_soup.add(v[i])
				
				if len(char_soup) > 1:
					break
				else:
					autocomp_prefix_ret += char_soup.pop()
			
			return autocomp_prefix_ret, autocomp_members
		elif len(autocomp_members) == 1:
			if autocomp_prefix == autocomp_members[0]:
				# the variable matched the prefix exactly
				# add a '.' so you can quickly continue.
				# Could try add [] or other possible extensions rather then '.' too if we had the variable.
				return '.', []
			else:
				# finish off the of the word word
				return autocomp_members[0][len(autocomp_prefix):], []
		else:
			return '', []
	

	def BCon_PrevChar(bcon):
		cursor = bcon['cursor']-1
		if cursor<0:
			return None
			
		try:
			return bcon['edit_text'][cursor]
		except:
			return None
		
		
	def BCon_NextChar(bcon):
		try:
			return bcon['edit_text'][bcon['cursor']]
		except:
			return None
	
	def BCon_cursorLeft(bcon):
		bcon['cursor'] -= 1
		if bcon['cursor'] < 0:
			bcon['cursor'] = 0

	def BCon_cursorRight(bcon):
			bcon['cursor'] += 1
			if bcon['cursor'] > len(bcon['edit_text']):
				bcon['cursor'] = len(bcon['edit_text'])
	
	def BCon_AddScrollback(bcon, text):
		
		bcon['scrollback'] = bcon['scrollback'] + text
		
	
	def BCon_cursorInsertChar(bcon, ch):
		if bcon['cursor']==0:
			bcon['edit_text'] = ch + bcon['edit_text']
		elif bcon['cursor']==len(bcon['edit_text']):
			bcon['edit_text'] = bcon['edit_text'] + ch
		else:
			bcon['edit_text'] = bcon['edit_text'][:bcon['cursor']] + ch + bcon['edit_text'][bcon['cursor']:]
			
		bcon['cursor'] 
		if bcon['cursor'] > len(bcon['edit_text']):
			bcon['cursor'] = len(bcon['edit_text'])
		BCon_cursorRight(bcon)
	
	
	TEMP_NAME = '___tempname___'
	
	cursor_orig = bcon['cursor']
	
	ch = BCon_PrevChar(bcon)
	while ch != None and (not is_delimiter(ch)):
		ch = BCon_PrevChar(bcon)
		BCon_cursorLeft(bcon)
	
	if ch != None:
		BCon_cursorRight(bcon)
	
	#print (cursor_orig, bcon['cursor'])
	
	cursor_base = bcon['cursor']
	
	autocomp_prefix = bcon['edit_text'][cursor_base:cursor_orig]
	
	print("PREFIX:'%s'" % autocomp_prefix)
	
	# Get the previous word
	if BCon_PrevChar(bcon)=='.':
		BCon_cursorLeft(bcon)
		ch = BCon_PrevChar(bcon)
		while ch != None and is_delimiter_autocomp(ch)==False:
			ch = BCon_PrevChar(bcon)
			BCon_cursorLeft(bcon)
		
		cursor_new = bcon['cursor']
		
		if ch != None:
			cursor_new+=1
		
		pytxt = bcon['edit_text'][cursor_new:cursor_base-1].strip()
		print("AUTOCOMP EVAL: '%s'" % pytxt)
		#try:
		if pytxt:
			bcon['console'].runsource(TEMP_NAME + '=' + pytxt, '<input>', 'single')
			# print val
		else: ##except:
			val = None
		
		try:
			val = bcon['namespace'][TEMP_NAME]
			del bcon['namespace'][TEMP_NAME]
		except:
			val = None
		
		if val:
			autocomp_members = dir(val)
			
			autocomp_prefix_ret, autocomp_members = do_autocomp(autocomp_prefix, autocomp_members)
			
			bcon['cursor'] = cursor_orig
			for v in autocomp_prefix_ret:
				BCon_cursorInsertChar(bcon, v)
			cursor_orig = bcon['cursor']
			
			if autocomp_members:
				BCon_AddScrollback(bcon, ', '.join(autocomp_members))
		
		del val
		
	else:
		# Autocomp global namespace
		autocomp_members = bcon['namespace'].keys()
		
		if autocomp_prefix:
			autocomp_members = [v for v in autocomp_members if v.startswith(autocomp_prefix)]
		
		autocomp_prefix_ret, autocomp_members = do_autocomp(autocomp_prefix, autocomp_members)
		
		bcon['cursor'] = cursor_orig
		for v in autocomp_prefix_ret:
			BCon_cursorInsertChar(bcon, v)
		cursor_orig = bcon['cursor']
		
		if autocomp_members:
			BCon_AddScrollback(bcon, ', '.join(autocomp_members))
	
	bcon['cursor'] = cursor_orig