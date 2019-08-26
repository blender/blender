#!/bin/bash
#
# This file is a part of LEMON, a generic C++ optimization library.
#
# Copyright (C) 2003-2009
# Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
# (Egervary Research Group on Combinatorial Optimization, EGRES).
#
# Permission to use, modify and distribute this software is granted
# provided that this copyright notice appears in all copies. For
# precise terms see the accompanying LICENSE file.
#
# This software is provided "AS IS" with no warranty of any kind,
# express or implied, and with no claim as to its suitability for any
# purpose.

YEAR=`date +%Y`
HGROOT=`hg root`

function hg_year() {
    if [ -n "$(hg st $1)" ]; then
        echo $YEAR
    else
        hg log -l 1 --template='{date|isodate}\n' $1 |
        cut -d '-' -f 1
    fi
}

# file enumaration modes

function all_files() {
    hg status -a -m -c |
    cut -d ' ' -f 2 | grep -E '(\.(cc|h|dox)$|Makefile\.am$)' |
    while read file; do echo $HGROOT/$file; done
}

function modified_files() {
    hg status -a -m |
    cut -d ' ' -f 2 | grep -E  '(\.(cc|h|dox)$|Makefile\.am$)' |
    while read file; do echo $HGROOT/$file; done
}

function changed_files() {
    {
        if [ -n "$HG_PARENT1" ]
        then
            hg status --rev $HG_PARENT1:$HG_NODE -a -m
        fi
        if [ -n "$HG_PARENT2" ]
        then
            hg status --rev $HG_PARENT2:$HG_NODE -a -m
        fi
    } | cut -d ' ' -f 2 | grep -E '(\.(cc|h|dox)$|Makefile\.am$)' | 
    sort | uniq |
    while read file; do echo $HGROOT/$file; done
}

function given_files() {
    for file in $GIVEN_FILES
    do
	echo $file
    done
}

# actions

function update_action() {
    if ! diff -q $1 $2 >/dev/null
    then
	echo -n " [$3 updated]"
	rm $2
	mv $1 $2
	CHANGED=YES
    fi
}

function update_warning() {
    echo -n " [$2 warning]"
    WARNED=YES
}

function update_init() {
    echo Update source files...
    TOTAL_FILES=0
    CHANGED_FILES=0
    WARNED_FILES=0
}

function update_done() {
    echo $CHANGED_FILES out of $TOTAL_FILES files has been changed.
    echo $WARNED_FILES out of $TOTAL_FILES files triggered warnings.
}

function update_begin() {
    ((TOTAL_FILES++))
    CHANGED=NO
    WARNED=NO
}

function update_end() {
    if [ $CHANGED == YES ]
    then
	((++CHANGED_FILES))
    fi
    if [ $WARNED == YES ]
    then
	((++WARNED_FILES))
    fi
}

function check_action() {
    if [ "$3" == 'tabs' ]
    then
        if echo $2 | grep -q -v -E 'Makefile\.am$'
        then
            PATTERN=$(echo -e '\t')
        else
            PATTERN='        '
        fi
    elif [ "$3" == 'trailing spaces' ]
    then
        PATTERN='\ +$'
    else
        PATTERN='*'
    fi

    if ! diff -q $1 $2 >/dev/null
    then
        if [ "$PATTERN" == '*' ]
        then
            diff $1 $2 | grep '^[0-9]' | sed "s|^\(.*\)c.*$|$2:\1: check failed: $3|g" |
              sed "s/:\([0-9]*\),\([0-9]*\):\(.*\)$/:\1:\3 (until line \2)/g"
        else
            grep -n -E "$PATTERN" $2 | sed "s|^\([0-9]*\):.*$|$2:\1: check failed: $3|g"
        fi
        FAILED=YES
    fi
}

function check_warning() {
    if [ "$2" == 'long lines' ]
    then
        grep -n -E '.{81,}' $1 | sed "s|^\([0-9]*\):.*$|$1:\1: warning: $2|g"
    else
        echo "$1: warning: $2"
    fi
    WARNED=YES
}

function check_init() {
    echo Check source files...
    FAILED_FILES=0
    WARNED_FILES=0
    TOTAL_FILES=0
}

function check_done() {
    echo $FAILED_FILES out of $TOTAL_FILES files has been failed.
    echo $WARNED_FILES out of $TOTAL_FILES files triggered warnings.

    if [ $WARNED_FILES -gt 0 -o $FAILED_FILES -gt 0 ]
    then
	if [ "$WARNING" == 'INTERACTIVE' ]
	then
	    echo -n "Are the files with errors/warnings acceptable? (yes/no) "
	    while read answer
	    do
		if [ "$answer" == 'yes' ]
		then
		    return 0
		elif [ "$answer" == 'no' ]
		then
		    return 1
		fi
		echo -n "Are the files with errors/warnings acceptable? (yes/no) "
	    done
	elif [ "$WARNING" == 'WERROR' ]
	then
	    return 1
	fi
    fi
}

function check_begin() {
    ((TOTAL_FILES++))
    FAILED=NO
    WARNED=NO
}

function check_end() {
    if [ $FAILED == YES ]
    then
	((++FAILED_FILES))
    fi
    if [ $WARNED == YES ]
    then
	((++WARNED_FILES))
    fi
}



# checks

function header_check() {
    if echo $1 | grep -q -E 'Makefile\.am$'
    then
	return
    fi

    TMP_FILE=`mktemp`

    (echo "/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-"$(hg_year $1)"
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided \"AS IS\" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */
"
    awk 'BEGIN { pm=0; }
     pm==3 { print }
     /\/\* / && pm==0 { pm=1;}
     /[^:blank:]/ && (pm==0 || pm==2) { pm=3; print;}
     /\*\// && pm==1 { pm=2;}
    ' $1
    ) >$TMP_FILE

    "$ACTION"_action "$TMP_FILE" "$1" header
}

function tabs_check() {
    if echo $1 | grep -q -v -E 'Makefile\.am$'
    then
        OLD_PATTERN=$(echo -e '\t')
        NEW_PATTERN='        '
    else
        OLD_PATTERN='        '
        NEW_PATTERN=$(echo -e '\t')
    fi
    TMP_FILE=`mktemp`
    cat $1 | sed -e "s/$OLD_PATTERN/$NEW_PATTERN/g" >$TMP_FILE

    "$ACTION"_action "$TMP_FILE" "$1" 'tabs'
}

function spaces_check() {
    TMP_FILE=`mktemp`
    cat $1 | sed -e 's/ \+$//g' >$TMP_FILE

    "$ACTION"_action "$TMP_FILE" "$1" 'trailing spaces'
}

function long_lines_check() {
    if cat $1 | grep -q -E '.{81,}'
    then
	"$ACTION"_warning $1 'long lines'
    fi
}

# process the file

function process_file() {
    if [ "$ACTION" == 'update' ]
    then
        echo -n "    $ACTION $1..."
    else
        echo "	  $ACTION $1..."
    fi

    CHECKING="header tabs spaces long_lines"

    "$ACTION"_begin $1
    for check in $CHECKING
    do
	"$check"_check $1
    done
    "$ACTION"_end $1
    if [ "$ACTION" == 'update' ]
    then
        echo
    fi
}

function process_all {
    "$ACTION"_init
    while read file
    do
	process_file $file
    done < <($FILES)
    "$ACTION"_done
}

while [ $# -gt 0 ]
do
    
    if [ "$1" == '--help' ] || [ "$1" == '-h' ]
    then
	echo -n \
"Usage:
  $0 [OPTIONS] [files]
Options:
  --dry-run|-n
     Check the files, but do not modify them.
  --interactive|-i
     If --dry-run is specified and the checker emits warnings,
     then the user is asked if the warnings should be considered
     errors.
  --werror|-w
     Make all warnings into errors.
  --all|-a
     Check all source files in the repository.
  --modified|-m
     Check only the modified (and new) source files. This option is
     useful to check the modification before making a commit.
  --changed|-c
     Check only the changed source files compared to the parent(s) of
     the current hg node.  This option is useful as hg hook script.
     To automatically check all your changes before making a commit,
     add the following section to the appropriate .hg/hgrc file.

       [hooks]
       pretxncommit.checksources = scripts/unify-sources.sh -c -n -i

  --help|-h
     Print this help message.
  files
     The files to check/unify. If no file names are given, the modified
     source files will be checked/unified (just like using the
     --modified|-m option).
"
        exit 0
    elif [ "$1" == '--dry-run' ] || [ "$1" == '-n' ]
    then
	[ -n "$ACTION" ] && echo "Conflicting action options" >&2 && exit 1
	ACTION=check
    elif [ "$1" == "--all" ] || [ "$1" == '-a' ]
    then
	[ -n "$FILES" ] && echo "Conflicting target options" >&2 && exit 1
	FILES=all_files
    elif [ "$1" == "--changed" ] || [ "$1" == '-c' ]
    then
	[ -n "$FILES" ] && echo "Conflicting target options" >&2 && exit 1
	FILES=changed_files
    elif [ "$1" == "--modified" ] || [ "$1" == '-m' ]
    then
	[ -n "$FILES" ] && echo "Conflicting target options" >&2 && exit 1
	FILES=modified_files
    elif [ "$1" == "--interactive" ] || [ "$1" == "-i" ]
    then
	[ -n "$WARNING" ] && echo "Conflicting warning options" >&2 && exit 1
	WARNING='INTERACTIVE'
    elif [ "$1" == "--werror" ] || [ "$1" == "-w" ]
    then
	[ -n "$WARNING" ] && echo "Conflicting warning options" >&2 && exit 1
	WARNING='WERROR'
    elif [ $(echo x$1 | cut -c 2) == '-' ]
    then
	echo "Invalid option $1" >&2 && exit 1
    else
	[ -n "$FILES" ] && echo "Invalid option $1" >&2 && exit 1
	GIVEN_FILES=$@
	FILES=given_files
	break
    fi
    
    shift
done

if [ -z $FILES ]
then
    FILES=modified_files
fi

if [ -z $ACTION ]
then
    ACTION=update
fi

process_all
