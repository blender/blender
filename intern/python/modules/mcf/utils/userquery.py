import string

def userquery( prompt, choices, contextdata = '', defaultind=0 ):
	if contextdata:
		print 'Contextual Information:', contextdata
	for x in range( len( choices ) ):
		print '(%s)'%x, `choices[x]`
	choice = raw_input( prompt+( '(%s):'%defaultind ) )
	if not choice:
		return choices[ defaultind ]
	try:
		choice = string.atoi( choice )
		return choices[ choice]
	except IndexError :
		return choices[ defaultind ]
	except ValueError:
		return choice
