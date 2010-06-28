#! /usr/bin/env python3

"""
This script is used to help cleaning RNA api.

Typical line in the input file (elements in [] are optional).

[comment *] ToolSettings.snap_align_rotation -> use_snap_align_rotation:    boolean    [Align rotation with the snapping target]
"""


def font_bold(mystring):
    """
    Formats the string as bold, to be used in printouts.
    """
    font_bold = "\033[1m"
    font_reset = "\033[0;0m"
    return font_bold + mystring + font_reset
    

def usage():
    """
    Prints script usage.
    """
    import sys
    scriptname = sys.argv[0]
    sort_choices_string = '|'.join(sort_choices)
    message = "\nUSAGE:"
    message += "\n%s input-file (.txt|.py) order-priority (%s).\n" % (font_bold(scriptname), sort_choices_string)
    message += "%s -h for help\n" % font_bold(scriptname)
    print(message)
    exit()


def help():
    """
    Prints script' help.
    """
    message = '\nHELP:'
    message += '\nRun this script to re-format the edits you make in the input file.\n'
    message += 'Do quick modification to important fields like \'to\' and don\'t care about fields like \'changed\' or \'description\' and save.\n'
    message += 'The script outputs 3 files:\n'
    message += '   1) *_clean.txt: is formatted same as the .txt input, can be edited by user.\n'
    message += '   2) *_clean.py: is formatted same as the .py input, can be edited by user.\n'
    message += '   3) rna_api.py is not formatted for readability and go under complete check. Can be used for rna cleanup.\n'
    print(message)
    usage()


def check_commandline():
    """
    Takes parameters from the commandline.
    """
    import sys
    # Usage
    if len(sys.argv)==1 or len(sys.argv)>3:
        usage()
    if sys.argv[1]!= '-h':
        input_filename = sys.argv[1]
    else:
        help()
    if not (input_filename[-4:] == '.txt' or input_filename[-3:] == '.py'):
        print ('\nBad input file extension... exiting.')
        usage()
    if len(sys.argv)==2:
        order_priority = default_sort_choice
        print ('\nSecond parameter missing: choosing to order by %s.' % font_bold(order_priority))
    elif len(sys.argv)==3:
        order_priority = sys.argv[2]
        if order_priority not in sort_choices:
            print('\nWrong order_priority... exiting.')
            usage()
    return (input_filename, order_priority)


def check_prefix(prop):
    # reminder: props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
    if '_' in prop:
        prefix = prop.split('_')[0]
        if prefix not in kw_prefixes:
            return 'BAD-PREFIX: ' + prefix
        else:
            return prefix + '_'
    elif prop in kw:
        return 'SPECIAL-KEYWORD: ' + prop
    else:
        return 'BAD-KEYWORD: ' + prop


def check_if_changed(a,b):
    if a != b: return 'changed'
    else: return 'same'


def get_props_from_txt(input_filename):
    """
    If the file is *.txt, the script assumes it is formatted as outlined in this script docstring
    """
    
    file=open(input_filename,'r')
    file_lines=file.readlines()
    file.close()

    props_list=[]
    props_length_max=[0,0,0,0,0,0,0,0]
    for line in file_lines:
        
        # debug
        #print(line)
        
        # empty line or comment
        if not line.strip() or line.startswith('#'):
            continue

        # class
        [bclass, tail] = [x.strip() for x in line.split('.', 1)]

        # comment
        if '*' in bclass:
            [comment, bclass] = [x.strip() for x in bclass.split('*', 1)]
        else:
            comment= ''

        # skipping the header if we have one.
        # the header is assumed to be "NOTE * CLASS.FROM -> TO:   TYPE  DESCRIPTION"
        if comment == 'NOTE' and bclass == 'CLASS':
            continue

        # from
        [bfrom, tail] = [x.strip() for x in tail.split('->', 1)]

        # to
        [bto, tail] = [x.strip() for x in tail.split(':', 1)]

        # type, description
        try:
            [btype, description] = tail.split(None, 1)
            if '"' in description:
                description.replace('"', "'")
        except ValueError:
            [btype, description] = [tail,'NO DESCRIPTION']

        # keyword-check
        kwcheck = check_prefix(bto)

        # changed
        changed = check_if_changed(bfrom, bto)
        
        # lists formatting
        props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
        props_list.append(props)
        props_length_max=list(map(max,zip(props_length_max,list(map(len,props)))))
        
    return (props_list,props_length_max)


def get_props_from_py(input_filename):
    """
    If the file is *.py, the script assumes it contains a python list (as "rna_api=[...]")
    This means that this script executes the text in the py file with an exec(text).
    """
    file=open(input_filename,'r')
    file_text=file.read()
    file.close()
    
    # adds the list "rna_api" to this function's scope
    rna_api = __import__(input_filename[:-3]).rna_api

    props_length_max = [0 for i in rna_api[0]] # this way if the vector will take more elements we are safe
    for props in rna_api:
        [comment, changed, bclass, bfrom, bto, kwcheck, btype, description] = props
        kwcheck = check_prefix(bto)   # keyword-check
        changed = check_if_changed(bfrom, bto)  # changed?
        props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
        props_length = list(map(len,props)) # lengths
        props_length_max = list(map(max,zip(props_length_max,props_length)))    # max lengths
    return (rna_api,props_length_max)


def read_file(input_filename):
    if input_filename[-4:] == '.txt':
        props_list,props_length_max = get_props_from_txt(input_filename)
    elif input_filename[-3:] == '.py':
        props_list,props_length_max = get_props_from_py(input_filename)
    return (props_list,props_length_max)

        
def sort(props_list, sort_priority):
    """
    reminder
    props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
    """

    # order based on the i-th element in lists
    i = sort_choices.index(sort_priority)
    if i == 0:
        props_list = sorted(props_list, key=lambda p: p[i], reverse=True)
    else:
        props_list = sorted(props_list, key=lambda p: p[i])
        
    print ('\nSorted by %s.' % font_bold(sort_priority))
    return props_list


def write_files(props_list, props_length_max):
    """
    Writes in 3 files:
      * output_filename_txt: formatted as txt input file
      * output_filename_py:  formatted for readability (could be worked on)
      * rna_api.py: unformatted, just as final output
    """

    # horrible :) will use os.path more properly, I'm being lazy
    if input_filename[-4:] == '.txt':
        if input_filename[-9:] == '_work.txt':
            base_filename = input_filename[:-9]
        else:
            base_filename = input_filename[:-4]
    elif input_filename[-3:] == '.py':
        if input_filename[-8:] == '_work.py':
            base_filename = input_filename[:-8]
        else:
            base_filename = input_filename[:-3]

    f_rna = open("rna_api.py",'w')
    f_txt = open(base_filename+'_work.txt','w')
    f_py = open(base_filename+'_work.py','w')

    # reminder: props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
    # [comment *] ToolSettings.snap_align_rotation -> use_snap_align_rotation:    boolean    [Align rotation with the snapping target]
    rna = '#    "NOTE", "CHANGED", "CLASS", "FROM", "TO", "KEYWORD-CHECK", "TYPE", "DESCRIPTION" \n'
    py = '#    "NOTE"                     , "CHANGED", "CLASS"                       , "FROM"                              , "TO"                                , "KEYWORD-CHECK"            , "TYPE"    , "DESCRIPTION" \n'
    txt = 'NOTE * CLASS.FROM -> TO:   TYPE  DESCRIPTION \n'
    for props in props_list:
        #txt
        txt +=  '%s * %s.%s -> %s:   %s  %s\n' % tuple(props[:1] + props[2:5] + props[6:]) # awful I'll do it smarter later
        # rna_api
        rna += '    ("%s", "%s", "%s", "%s", "%s", "%s", "%s", "%s"),\n' % tuple(props)    
        # py
        blanks = [' '* (x[0]-x[1]) for x in zip(props_length_max,list(map(len,props)))]
        props = ['"%s"%s'%(x[0],x[1]) for x in zip(props,blanks)]
        py += '    (%s, %s, %s, %s, %s, %s, %s, %s),\n' % tuple(props)
    f_txt.write(txt)
    f_py.write("rna_api = [\n%s    ]\n" % py)
    f_rna.write("rna_api = [\n%s    ]\n" % rna)

    print ('\nSaved %s, %s and %s.\n' % (font_bold(f_txt.name), font_bold(f_py.name), font_bold(f_rna.name) ) )


def main():

    global input_filename
    global sort_choices, default_sort_choice
    global kw_prefixes, kw

    sort_choices = ['note','changed','class','from','to','kw']
    default_sort_choice = sort_choices[0]
    kw_prefixes = ['invert','is','lock','show','showonly','use','useonly']
    kw = ['hidden','selected','layer','state']

    input_filename, sort_priority = check_commandline()
    props_list,props_length_max = read_file(input_filename)
    props_list = sort(props_list,sort_priority)
    write_files(props_list,props_length_max)


if __name__=='__main__':
    main()

