
from TextTools.TextTools import *

#####################################################
# FOLLOWING IS THE BOOTSTRAP PARSER, HAND-CODED!

parsernamelist = [
'declarationset', # 0
'declaration', # 1
'implicit_group', # 2 --> no longer used
'added_token', # 3
'seq_added_token', #4
'fo_added_token', #5
'or_added_token', #6
'and_added_token', #7
'element_token', #8
'group', #9
'negpos_indicator', #10
'occurence_indicator', #11
'unreportedname', #12
'name', #13
'<ts>', # 14
'literal', #15
'range', # 16
'CHARBRACE', #17
'CHARDASH', # 18
'CHARRANGE', # 19
'CHARNOBRACE', # 20
'ESCAPEDCHAR', # 21
'SPECIALESCAPEDCHAR', # 22
'OCTALESCAPEDCHAR' # 23
]

parsertuplelist = range( 24 )



parsertuplelist[0] = ( # declarationset
	('declaration', TableInList,(parsertuplelist, 1)), # must be at least one declaration
	('declaration', TableInList,(parsertuplelist, 1),1,0)
)
parsertuplelist[1] = ( # declaration
	(None, TableInList,(parsertuplelist, 14)), # ts
	(None, SubTable, (
			('unreportedname', TableInList,(parsertuplelist, 12),1,2),
			('name', TableInList,(parsertuplelist, 13)), # name
		)
	),
	(None, TableInList,(parsertuplelist, 14)), # ts
	(None, Word, ':='), 
	(None, TableInList,(parsertuplelist, 14)), # ts
	('element_token', TableInList,(parsertuplelist, 8)),
	(None, SubTable, ( # added_token
		('seq_added_token', TableInList, (parsertuplelist,4), 1, 5 ),
		('fo_added_token', TableInList, (parsertuplelist,5), 1, 4 ),
		('or_added_token', TableInList, (parsertuplelist,6), 1, 3 ),
		('and_added_token', TableInList, (parsertuplelist,7), 1, 2 ),
		(None, Fail, Here),
		('seq_added_token', TableInList, (parsertuplelist,4), 1, 0 ),
		('fo_added_token', TableInList, (parsertuplelist,5), 1, -1 ),
		('or_added_token', TableInList, (parsertuplelist,6), 1, -2 ),
		('and_added_token', TableInList, (parsertuplelist,7), 1, -3 ),
	),1,1),
	(None, TableInList,(parsertuplelist, 14)), # ts
)
parsertuplelist[3] = ( # added_token
	('seq_added_token', TableInList, (parsertuplelist,4), 1, 5 ),
	('fo_added_token', TableInList, (parsertuplelist,5), 1, 4 ),
	('or_added_token', TableInList, (parsertuplelist,6), 1, 3 ),
	('and_added_token', TableInList, (parsertuplelist,7), 1, 2 ),
	(None, Fail, Here),
	('seq_added_token', TableInList, (parsertuplelist,4), 1, 0 ),
	('fo_added_token', TableInList, (parsertuplelist,5), 1, -1 ),
	('or_added_token', TableInList, (parsertuplelist,6), 1, -2 ),
	('and_added_token', TableInList, (parsertuplelist,7), 1, -3 ),
)
parsertuplelist[4] = ( # seq_added_token
	(None, TableInList,(parsertuplelist, 14)), # ts
	(None, Is, ','),
	(None, TableInList,(parsertuplelist, 14)), # ts
	('element_token', TableInList,(parsertuplelist, 8)),
	(None, TableInList,(parsertuplelist, 14),4,1), # ts
	(None, Is, ',',3,1),
	(None, TableInList,(parsertuplelist, 14),2,1), # ts
	('element_token', TableInList,(parsertuplelist, 8),1,-3),
)
parsertuplelist[5] = ( # fo_added_token
	(None, TableInList,(parsertuplelist, 14)), # ts
	(None, Is, '/'),
	(None, TableInList,(parsertuplelist, 14)), # ts
	('element_token', TableInList,(parsertuplelist, 8)),
	(None, TableInList,(parsertuplelist, 14),4,1), # ts
	(None, Is, '/',3,1),
	(None, TableInList,(parsertuplelist, 14),2,1), # ts
	('element_token', TableInList,(parsertuplelist, 8),1,-3),
)
parsertuplelist[6] = ( # or_added_token
	(None, TableInList,(parsertuplelist, 14)), # ts
	(None, Is, '|'),
	(None, TableInList,(parsertuplelist, 14)), # ts
	('element_token', TableInList,(parsertuplelist, 8)),
	(None, TableInList,(parsertuplelist, 14),4,1), # ts
	(None, Is, '|',3,1),
	(None, TableInList,(parsertuplelist, 14),2,1), # ts
	('element_token', TableInList,(parsertuplelist, 8),1,-3),
)
parsertuplelist[7] = ( # and_added_token
	(None, TableInList,(parsertuplelist, 14)), # ts
	(None, Is, '&'),
	(None, TableInList,(parsertuplelist, 14)), # ts
	('element_token', TableInList,(parsertuplelist, 8)),
	(None, TableInList,(parsertuplelist, 14),4,1), # ts
	(None, Is, '&',3,1),
	(None, TableInList,(parsertuplelist, 14),2,1), # ts
	('element_token', TableInList,(parsertuplelist, 8),1,-3),
)
parsertuplelist[8] = ( # element_token
	('negpos_indicator', TableInList,(parsertuplelist, 10),1,1),
	(None, TableInList,(parsertuplelist, 14),1,1), # ts, very inefficient :(
	('literal', TableInList, (parsertuplelist,15),1, 4 ),
	('range', TableInList, (parsertuplelist,16),1, 3  ),
	('group', TableInList, (parsertuplelist,9),1, 2  ),
	('name', TableInList, (parsertuplelist,13) ),
	(None, TableInList,(parsertuplelist, 14),1,1), # ts, very inefficient :(
	('occurence_indicator', TableInList,(parsertuplelist, 11), 1,1),
)
parsertuplelist[9] = ( # group
	(None, Is, '('),
	(None, TableInList,(parsertuplelist, 14),1,1), # ts
	('element_token', TableInList, (parsertuplelist,8) ),
	(None, SubTable, ( # added_token
		('seq_added_token', TableInList, (parsertuplelist,4), 1, 5 ),
		('fo_added_token', TableInList, (parsertuplelist,5), 1, 4 ),
		('or_added_token', TableInList, (parsertuplelist,6), 1, 3 ),
		('and_added_token', TableInList, (parsertuplelist,7), 1, 2 ),
		(None, Fail, Here),
		('seq_added_token', TableInList, (parsertuplelist,4), 1, 0 ),
		('fo_added_token', TableInList, (parsertuplelist,5), 1, -1 ),
		('or_added_token', TableInList, (parsertuplelist,6), 1, -2 ),
		('and_added_token', TableInList, (parsertuplelist,7), 1, -3 ),
	),1,1),
	(None, TableInList,(parsertuplelist, 14),1,1), # ts
	(None, Is, ')'),
)
parsertuplelist[10] = ( # negpos_indicator
	(None, Is, "+",1,2),
	(None, Is, "-"),
)
parsertuplelist[11]  = ( #occurence_indicator
	(None, Is, "+",1,3),
	(None, Is, "*",1,2),
	(None, Is, '?'),
)
parsertuplelist[12] = ( #unreportedname
	(None, Is, '<'),
	('name', TableInList, (parsertuplelist, 13)), # inefficiency in final system :(
	(None, Is, '>'),
)
parsertuplelist[13] = ( # name
	(None, IsIn, alpha+'_'),
	(None, AllIn, alphanumeric+'_',1,1)
)

parsertuplelist[14] = ( # ts (whitespace)
		(None, AllIn, ' \011\012\013\014\015',1,1),
		(None, SubTable, (
				(None, Is, '#' ),
				(None, AllNotIn, '\n',1,1 ) # problem if there's a comment at the end of the file :(
			)
			,1,-1 ),
		)
# this isn't actually used in the bootstrap parser...
_specialescapedchar = parsertuplelist[22] = ( # SPECIALESCAPEDCHAR
	('SPECIALESCAPEDCHAR', IsIn, '\\abfnrtv'),
)
_octalescapechar = parsertuplelist[23] = ( # OCTALESCAPEDCHAR
	(None, IsIn, '01234567'),
	(None, IsIn, '01234567',2),
	(None, IsIn, '01234567',1),
)
_escapedchar = parsertuplelist[21] = ( # escapedcharacter
	(None, Is, '\\' ),
	('SPECIALESCAPEDCHAR', IsIn, '\\abfnrtv',1,4),
	('OCTALESCAPEDCHAR', SubTable, _octalescapechar)
)
	
_charnobrace = parsertuplelist[20] = ( # charnobrace
	('ESCAPEDCHAR', Table, _escapedchar, 1,2),
	('CHAR', IsNot, ']'),
)
_rangedef = parsertuplelist[19] = ( # charrange
	('CHARNOBRACE', Table, _charnobrace ),
	(None, Is, '-'),
	('CHARNOBRACE', Table, _charnobrace ),
)
	
	
parsertuplelist[16] = ( #range
	(None, Is, '['),
	('CHARBRACE', Is, ']',1,1),
	('CHARDASH', Is, '-',1,1),
	('CHARRANGE', Table, _rangedef, 1,0),
	(None, SubTable, _charnobrace, 1,-1),
	(None, Is, ']')
)

_sqstr = (
		(None, Is, "'" ),
#		(None, Is, "'",1, 5 ), # immediate close
		(None, AllNotIn, "\\'",1,1 ), # all not an escape or end
		(None, Is, "\\", 2, 1),   # is an escaped char
		(None, Skip, 1, 1, -2),   # consume the escaped char and loop back
		(None, Is, "'" ) # in case there was no matching ', which would also cause a fail for allnotin
	)
_dblstr = (
		(None, Is, '"' ),
#		(None, Is, '"',1, 5 ), # immediate close
		(None, AllNotIn, '\\"' ,1,1), # not an escaped or end
		(None, Is, "\\", 2, 1),   # is an escaped char
		(None, Skip, 1, 1, -2),   # consume the escaped char and loop back
		(None, Is, '"' ) # in case there was no matching ", which would also cause a fail for allnotin
	)


	      
# literal             :=  ("'",(CHARNOSNGLQUOTE/ESCAPEDCHAR)*,"'")  /  ('"',(CHARNODBLQUOTE/ESCAPEDCHAR)*,'"')

parsertuplelist[15] = ( # literal
	(None, Is, "'", 4, 1 ),
	('CHARNOSNGLQUOTE', AllNotIn, "\\'",1,1 ), # all not an escape or end
	('ESCAPEDCHAR', Table, _escapedchar, 1, -1),
	(None, Is, "'", 1,5 ),
	(None, Is, '"' ),
	('CHARNODBLQUOTE', AllNotIn, '\\"',1,1 ), # all not an escape or end
	('ESCAPEDCHAR', Table, _escapedchar, 1, -1),
	(None, Is, '"'),
)

declaration = r'''declarationset      :=  declaration+
declaration         :=  ts , (unreportedname/name) ,ts,':=',ts, element_token, ( seq_added_token / fo_added_token / or_added_token / and_added_token )*, ts
seq_added_token     :=  (ts,',',ts, element_token)+
fo_added_token      :=  (ts,'/',ts, element_token)+
or_added_token      :=  (ts,'|',ts, element_token)+ # not currently supported
and_added_token     :=  (ts,'&',ts, element_token)+ # not currently supported
element_token       :=  negpos_indicator?, ts, (literal/range/group/name),ts, occurence_indicator?
group               :=  '(',ts, element_token, ( seq_added_token / fo_added_token / or_added_token / and_added_token )*, ts, ')'

negpos_indicator    :=  '+'/'-'
occurence_indicator :=  '+'/'*'/'?'
unreportedname      :=  '<', name, '>'
name                :=  [a-zA-Z_],[a-zA-Z0-9_]*
<ts>                :=  ( [ \011-\015]+ / ('#',-'\n'+,'\n')+ )*
literal             :=  ("'",(CHARNOSNGLQUOTE/ESCAPEDCHAR)*,"'")  /  ('"',(CHARNODBLQUOTE/ESCAPEDCHAR)*,'"')


range               :=  '[',CHARBRACE?,CHARDASH?, (CHARRANGE/CHARNOBRACE)*, CHARDASH?,']'
CHARBRACE           :=  ']'
CHARDASH            :=  '-'
CHARRANGE           :=  CHARNOBRACE, '-', CHARNOBRACE
CHARNOBRACE         :=  ESCAPEDCHAR/CHAR
CHAR                :=  -[]]
ESCAPEDCHAR         :=  '\\',( SPECIALESCAPEDCHAR / OCTALESCAPEDCHAR )
SPECIALESCAPEDCHAR  :=  [\\abfnrtv]
OCTALESCAPEDCHAR    :=  [0-7],[0-7]?,[0-7]?
CHARNODBLQUOTE      :=  -[\\"]+
CHARNOSNGLQUOTE     :=  -[\\']+
'''

def parse( instr = declaration, parserelement = 'declarationset' ):
	tbl = (
		(parserelement, Table, parsertuplelist[parsernamelist.index( parserelement )] ),
	)
	return tag( instr,  tbl)

if __name__ == '__main__':
	import sys, pprint
	pprint.pprint( apply( parse, tuple( sys.argv[1:] ) ) )
	

