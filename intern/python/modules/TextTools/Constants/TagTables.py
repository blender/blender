""" Constants for writing tag tables

    The documentation in this file is obsoleted by the HTML docs in
    the Doc/ subdirectory of the package. Constants defined here must
    match those in mxTextTools/mxte.h.

    (c) Copyright Marc-Andre Lemburg; All Rights Reserved.
    See the documentation for further information on copyrights,
    or contact the author (mal@lemburg.com).
"""
#########################################################################
# This file contains the definitions and constants used by the tagging
# engine:
#
# 1. Matching Tables
# 2. Commands & Constants
# 3. Matching Functions
# 4. Callable tagobjects
# 5. Calling the engine & Taglists
#

#########################################################################
# 1. Matching Tables:
#
# these are tuples of tuples, each entry having the following meaning:
#
# syntax: (tag, cmd, chars|table|fct [,jne] [,je=1])
#          tag = object used to mark this section, if it matches
#          cmd = command (see below)
#          chars = match one or more of these characters
#          table = table to use for matching characters
#          fct = function to call (see below)
#          jne = if the current character doesn't match, jump this
#                many table entries relative to the current entry
#          je = if we have a match make a relative jump of this length
#
# * a table matches a string iff the end of the table is reached
#   (that is: an index is requested that is beyond the end-of-table)
# * a table is not matched if a tag is not matched and no jne is given;
#   if it is matched then processing simply moves on to the next entry
# * marking is done by adding the matching slice in the string
#   together with the marking object to the tag list; if the object is
#   None, then it will not be appended to the taglist list
# * if the flag CallTag is set in cmd, then instead of appending
#   matches to the taglist, the tagobj will be called (see below) 
#
# TIP: if you are getting an error 'call of a non-function' while
#      writing a table definition, you probably have a missing ','
#      somewhere in the tuple !
#
# For examples see the tag*.py - files that came with this engine.
#

#########################################################################
# 2. Commands & Constants
# 
#

#
# some useful constants for writing matching tables
#

To = None		# good for cmd=Jump
Here = None		# good for cmd=Fail and EOF
MatchOk = 20000		# somewhere beyond the end of the tag table...
MatchFail = -20000	# somewhere beyond the start of the tag table...
ToEOF = -1		# good for cmd=Move

ThisTable = 999		# to recursively match using the current table;
			# can be passed as argument to Table and SubTable
			# instead of a tuple

#
# commands and flags passed in cmd (see below)
#
# note: I might add some further commands to this list, if needed
#       (the numbers will then probably change, but not the
#        names)
#
# convention: a command "matches", if and only if it moves the
#       current position at least one character; a command "reads" 
#       characters the characters, if they match ok
#
# notations:
#
#  x    refers to the current position in the string
#  len  refers to the string length or what the function tag() is told to
#       believe it to be (i.e. the engine only looks at the slice text[x:len])
#  text refers to the text string
#  jne  is the optional relative jump distance in case the command
#       did not match, i.e. x before and after applying the command
#       are the same (if not given the current table is considered
#       not to match)
#  je   is the optional relative jump distance in case the command
#       did match (it defaults to +1)
#

# commands
Fail = 0           # this will always fail (position remains unchanged)
Jump = 0           # jump to jne (position remains unchanged)

# match & read chars
AllIn = 11         # all chars in match (at least one)
AllNotIn = 12      # all chars not in match (at least one)
Is = 13            # current char must be == match (matches one char)
IsIn = 14          # current char must be in match (matches one char)
IsNot = 15         # current char must be be != match (matches one char)
IsNotIn = 15       # current char must be not be in match (matches one char)

AllInSet = 31
IsInSet = 32

# match & read for whole words
Word = 21          # the next chars must be those in match
WordStart = 22	   # all chars up to the first occ. of match (at least one)
WordEnd = 23	   # same as WordStart, accept that the text pointer
                   # is moved behind the match
NoWord = WordStart # all chars up to the first occ. of match (at least one)


# match using search objects BMS or FS
sWordStart = 111   # all chars up to the first occ. of match (may be 0 chars)
sWordEnd = 112	   # same as WordStart, accept that the text pointer
                   # is moved behind the match
sFindWord = 113    # find match and process the found slice only (ignoring
		   # the chars that lead up to the match); positions
		   # the text pointer right after the match like WordEnd

# functions & tables
Call = 201         # call match(text,x,len) as function (see above)
CallArg = 202      # match has to be a 2-tuple (fct,arg), then
                   # fct(text,x,len,arg) is called; the return value is taken
		   # as new x; it is considered matching if the new x is
		   # different than the x before the call -- like always
		   # (note: arg has to be *one* object, e.g. a tuple)
Table = 203        # match using table (given in match)
SubTable = 207     # match using sub table (given in match); the sub table
		   # uses the same taglist as the calling table
TableInList = 204  # same as Table, but match is a tuple (list,index)
                   # and the table list[index] is used as matching
		   # table
SubTableInList = 208
                   # same as TableInList, but the sub table
		   # uses the same taglist as the calling table

# specials
EOF = 1            # current position must be EOF, e.g. >= len(string)
Skip = 2           # skip match (must be an integer) chars; note: this cmd
                   # always matches ok, so jne doesn't have any meaning in
		   # this context
Move = 3	   # move the current text position to match (if negative,
		   # the text length + 1 (!) is added, thus -1 moves to the
		   # EOF, -2 to the last char and so on); note: this cmd
                   # always matches ok, so jne doesn't have any meaning in
		   # this context

# loops
Loop = 205         # loop-construct
                   #               
                   # (tagobj,Loop,Count,jne,je) - sets/decrements the
		   # loop variable for current table according to the
		   # following rules:
		   # 1. the first time the engine passes this entry
		   #    sets the loop variable to Count and continues
		   #    without reading any character, but saving the
		   #    current position in text
		   # 2. the next time, it decrements the loop variable
		   #    and checks if it is < 0:
		   #    (a) if it is, then the tagobj is added to the
		   #        taglist with the slice (saved position, current
		   #        position) and processing continues at entry
		   #        current + jne
		   #    (b) else, processing continues at entry current + je
		   # Note: if you jump out of the loop while the loop
		   #       variable is still > 0, then you *must*
		   #       reset the loop mechanism with 
		   #       (None,LoopControl,Reset)
		   # Note: you can skip the remaining loops by calling
		   #       (None,LoopControl,Break) and jumping back
		   #       to the Loop-entry; this sets the loop
		   #       variable to 0
		   # Note: tables cannot have nested loops within their
		   #       context; you can have nested loops in nested
		   #       tables though (there is one loop var per
		   #       tag()-call which takes place every time
		   #       a table match is done)
		   #
LoopControl = 206  # controls the loop variable (always succeeds, i.e.
                   #                             jne has no meaning);
                   # match may be one of:
Break = 0          # * sets the loop variable to 0, thereby allowing
                   #   to skip the remaining loops
Reset = -1         # * resets the loop mechanism (see note above)
                   #
		   # See tagLoop.py for some examples.

##########################################################################
#
# Flags (to be '+'ed with the above command code)
#
CallTag = 256      # call tagobj(taglist,text,l,r,subtags)
		   # upon successfully matching the slice [l:r] in text
		   # * taglist is the current list tags found (may be None)
                   # * subtags is a sub-list, passed when a subtable was used
                   #   to do the matching -- it is None otherwise !)
#
# example entry with CallTag-flag set:
#
# (found_a_tag,CallTag+Table,tagtable)
#  -- if tagtable matches the current text position, 
#     found_a_tag(taglist,text,l,r,newtaglist) is called and
#     the match is *not* appended to the taglist by the tagging
#     engine (the function would have to do this, in case it is needed)

AppendToTagobj = 512  	# this appends the slice found to the tagobj, assuming
                      	# that it is a Python list:
		      	# does a tagobj.append((None,l,r,subtags)) call
# Alias for b/w comp.
AppendToTag = AppendToTagobj

AppendTagobj = 1024   	# don't append (tagobj,l,r,subtags) to the taglist,
			# but only tagobj itself; the information in l,r,subtags
		   	# is lost, yet this can be used to write tag tables
		   	# whose output can be used directly by tag.join()

AppendMatch = 2048	# append the match to the taglist instead of
			# the tag object; this produces non-standard
			# taglists !

#########################################################################
# 3. Matching Functions
#
# syntax:
#
# fct(s,x,len_s)
#          where s = string we are working on
#                x = current index in s where we wnat to match something
#                len_s = 'length' of s, this is how far the search may be
#                    conducted in s, not necessarily the true length of s
# 
# * the function has to return the index of the char right after
#   matched string, e.g.
#
#   'xyzabc' ---> 'xyz' matches ---> return x+3
#
# * if the string doesn't match simply return x; in other words:
#   the function has to return the matching slice's right index
# * you can use this to match e.g. 10 characters of a certain kind,
#   or any word out of a given list, etc.
# * note: you cannot give the function additional parameters from within
#   the matching table, so it has to know everything it needs to
#   know a priori; use dynamic programming !
#
# some examples (not needed, since all are implemented by commands)
#
#
#def matchword(x):
#    s = """
#def a(s,x,len_text):
#    y = x+%i
#    if s[x:y] == %s: return y
#    return x
#"""
#    exec s % (len(x),repr(x))
#    return a
#
#def rejectword(x):
#    s = """
#def a(s,x,len_text):
#    while x < len(s) and s[x:x+%i] != %s:
#	x = x + 1
#    return x
#"""
#    exec s % (len(x),repr(x))
#    return a
#
#def HTML_Comment(s,x,len_text):
#    while x < len_text and s[x:x+3] != '-->':
#	x = x + 1
#    return x
#
#

#########################################################################
# 4. Callable tagobjects
#
# a sample callable tagobj:
#
#
#def test(taglist,text,l,r,newtaglist):
#
#    print 'found',repr(text[l:r])[:40],(l,r)
#
#

#########################################################################
# 5. Calling the engine & Taglists
#
# The function
#      tag(text,table,start=0,len_text=len(text),taglistinit=[])
# found in mxTextTools:
#
# This function does all the matching according to the above rules.
# You give it a text string and a tag table and it will
# start processing the string starting from 'start' (which defaults to 0)
# and continue working until it reaches the 'EOF', i.e. len_text (which
# defaults to the text length). It thus tags the slice text[start:len_text].
#
# The function will create a list of found tags in the following
# format (which I call taglist):
#
#      (tagobj,l,r,subtaglist)
#
# where: tagobj = specified tag object taken from the table
#        [l:r] = slice that matched the tag in text
#        subtaglist = if matching was done using a subtable
#                     this is the taglist it produced; in all other
#                     cases this will be None
#
# * if you pass None as taglistinit, then no taglist will be created,
#   i.e. only CallTag commands will have any effect. (This saves
#   temporary memory for big files)
# * the function returns a tuple:
#      (success, taglist, nextindex)
#   where: success = 0/1
#          taglist = the produced list or None
#          nextindex = the index+1 of the last char that matched
#                    (in case of failure, this points to the beginning
#                     of the substring that caused the problem)
# 

### Module init.

def _module_init():

    global id2cmd

    import types
    id2cmd = {}
    IntType = types.IntType
    for cmd,value in globals().items():
	if type(value) == IntType:
	    if value == 0:
		id2cmd[0] = 'Fail/Jump'
	    else:
		id2cmd[value] = cmd

_module_init()
