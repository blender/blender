#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script is used to help cleaning RNA api.

Typical line in the input file (elements in [] are optional).

[comment *] ToolSettings.snap_align_rotation -> use_snap_align_rotation:    boolean    [Align description]

Geterate output format from blender run this:
 ./blender.bin --background --python ./scripts/modules/_rna_info.py 2> source/blender/makesrna/rna_cleanup/out.txt
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
    print("".join((
        "USAGE:\n",
        "{:s} input-file (.txt|.py) order-priority ({:s}).\n".format(font_bold(scriptname), sort_choices_string),
        "{:s} -h for help\n".format(font_bold(scriptname)),
    )))
    exit()


def help():
    """
    Prints script' help.
    """
    print("".join((
        'HELP:\n'
        'Run this script to re-format the edits you make in the input file.\n'
        'Do quick modification to important fields like \'to\' and don\'t care about ',
        'fields like \'changed\' or \'description\' and save.\n'
        'The script outputs 3 files:\n'
        '   1) *_clean.txt: is formatted same as the .txt input, can be edited by user.\n'
        '   2) *_clean.py: is formatted same as the .py input, can be edited by user.\n'
        '   3) rna_api.py is not formatted for readability and go under complete check. Can be used for rna cleanup.\n',
    )))
    usage()


def check_commandline():
    """
    Takes parameters from the commandline.
    """
    import sys
    # Usage
    if len(sys.argv) == 1 or len(sys.argv) > 3:
        usage()
    if sys.argv[1] == '-h':
        help()
    elif not sys.argv[1].endswith((".txt", ".py")):
        print('\nBad input file extension... exiting.')
        usage()
    else:
        inputfile = sys.argv[1]
    if len(sys.argv) == 2:
        sort_priority = default_sort_choice
        print('\nSecond parameter missing: choosing to order by %s.' % font_bold(sort_priority))
    elif len(sys.argv) == 3:
        sort_priority = sys.argv[2]
        if sort_priority not in sort_choices:
            print('\nWrong sort_priority... exiting.')
            usage()
    return (inputfile, sort_priority)


def check_prefix(prop, btype):
    # reminder: props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
    if btype == "boolean":
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
    else:
        return ""


def check_if_changed(a, b):
    if a != b:
        return 'changed'
    else:
        return 'same'


def get_props_from_txt(input_filename):
    """
    If the file is *.txt, the script assumes it is formatted as outlined in this script doc-string.
    """

    file = open(input_filename, 'r')
    file_lines = file.readlines()
    file.close()

    props_list = []
    props_length_max = [0, 0, 0, 0, 0, 0, 0, 0]

    done_text = "+"
    done = 0
    tot = 0

    for iii, line in enumerate(file_lines):

        # debug
        # print(line)
        line_strip = line.strip()
        # empty line or comment
        if not line_strip:
            continue

        if line_strip == "EOF":
            break

        if line.startswith("#"):
            line = line[1:]

        # class
        bclass, tail = [x.strip() for x in line.split('.', 1)]

        # comment
        if '*' in bclass:
            comment, bclass = [x.strip() for x in bclass.split('*', 1)]
        else:
            comment = ''

        # skipping the header if we have one.
        # the header is assumed to be "NOTE * CLASS.FROM -> TO:   TYPE  DESCRIPTION"
        if comment == 'NOTE' and bclass == 'CLASS':
            continue

        # from
        bfrom, tail = [x.strip() for x in tail.split('->', 1)]

        # to
        bto, tail = [x.strip() for x in tail.split(':', 1)]

        # type, description
        try:
            btype, description = tail.split(None, 1)
            # make life easy and strip quotes
            description = description.replace("'", "").replace('"', "").replace("\\", "").strip()
        except ValueError:
            btype, description = [tail, 'NO DESCRIPTION']

        # keyword-check
        kwcheck = check_prefix(bto, btype)

        # changed
        changed = check_if_changed(bfrom, bto)

        # lists formatting
        props = [comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
        props_list.append(props)
        props_length_max = list(map(max, zip(props_length_max, list(map(len, props)))))

        if done_text in comment:
            done += 1
        tot += 1

    print("Total done %.2f" % (done / tot * 100.0))

    return (props_list, props_length_max)


def get_props_from_py(input_filename):
    """
    If the file is *.py, the script assumes it contains a python list (as "rna_api=[...]")
    This means that this script executes the text in the py file with an exec(text).
    """
    # adds the list "rna_api" to this function's scope
    rna_api = __import__(input_filename[:-3]).rna_api

    props_length_max = [0 for i in rna_api[0]]  # this way if the vector will take more elements we are safe
    for index, props in enumerate(rna_api):
        comment, changed, bclass, bfrom, bto, kwcheck, btype, description = props
        kwcheck = check_prefix(bto, btype)   # keyword-check
        changed = check_if_changed(bfrom, bto)  # changed?
        description = repr(description)
        description = description.replace("'", "").replace('"', "").replace("\\", "").strip()
        rna_api[index] = [comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
        props_length = list(map(len, props))  # lengths
        props_length_max = list(map(max, zip(props_length_max, props_length)))    # max lengths
    return (rna_api, props_length_max)


def get_props(input_filename):
    if input_filename.endswith(".txt"):
        props_list, props_length_max = get_props_from_txt(input_filename)
    elif input_filename.endswith(".py"):
        props_list, props_length_max = get_props_from_py(input_filename)
    return (props_list, props_length_max)


def sort(props_list, sort_priority):
    """
    reminder
    props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
    """

    # order based on the i-th element in lists
    if sort_priority == "class.to":
        props_list = sorted(props_list, key=lambda p: (p[2], p[4]))
    else:
        i = sort_choices.index(sort_priority)
        if i == 0:
            props_list = sorted(props_list, key=lambda p: p[i], reverse=True)
        else:
            props_list = sorted(props_list, key=lambda p: p[i])

    print('\nSorted by %s.' % font_bold(sort_priority))
    return props_list


def file_basename(input_filename):
    # If needed will use `os.path`.
    if input_filename.endswith(".txt"):
        if input_filename.endswith("_work.txt"):
            base_filename = input_filename.replace("_work.txt", "")
        else:
            base_filename = input_filename.replace(".txt", "")
    elif input_filename.endswith(".py"):
        if input_filename.endswith("_work.py"):
            base_filename = input_filename.replace("_work.py", "")
        else:
            base_filename = input_filename.replace(".py", "")

    return base_filename


def write_files(basename, props_list, props_length_max):
    """
    Writes in 3 files:
      * output_filename_work.txt: formatted as txt input file (can be edited)
      * output_filename_work.py:  formatted for readability (can be edited)
      * rna_api.py: unformatted, just as final output
    """

    f_rna = open("rna_api.py", 'w')
    f_txt = open(basename + '_work.txt', 'w')
    f_py = open(basename + '_work.py', 'w')

    # reminder: props=[comment, changed, bclass, bfrom, bto, kwcheck, btype, description]
    # [comment *] ToolSettings.snap_align_rotation -> use_snap_align_rotation:    boolean    [Align description]
    rna = py = txt = ''
    props_list = [['NOTE', 'CHANGED', 'CLASS', 'FROM', 'TO', 'KEYWORD-CHECK', 'TYPE', 'DESCRIPTION']] + props_list
    for props in props_list:
        # txt

        # quick way we can tell if it changed
        if props[3] == props[4]:
            txt += "#"
        else:
            txt += " "

        if props[0] != '':
            txt += '%s * ' % props[0]   # comment
        txt += '%s.%s -> %s:   %s  "%s"\n' % tuple(props[2:5] + props[6:])   # skipping keyword-check
        # rna_api
        if props[0] == 'NOTE':
            indent = '#   '
        else:
            indent = '    '
        # Description is already string formatted.
        rna += indent + '("%s", "%s", "%s", "%s", "%s"),\n' % tuple(props[2:5] + props[6:])
        # py
        blanks = [' ' * (x[0] - x[1]) for x in zip(props_length_max, list(map(len, props)))]
        props = [('"%s"%s' if props[-1] != x[0] else "%s%s") % (x[0], x[1]) for x in zip(props, blanks)]
        py += indent + '(%s, %s, %s, %s, %s, %s, %s, "%s"),\n' % tuple(props)

    f_txt.write(txt)
    f_py.write("rna_api = [\n%s]\n" % py)
    f_rna.write("rna_api = [\n%s]\n" % rna)

    # write useful py script, won't hurt
    f_py.write("\n'''\n")
    f_py.write("for p_note, p_changed, p_class, p_from, p_to, p_check, p_type, p_desc in rna_api:\n")
    f_py.write("    print(p_to)\n")
    f_py.write("\n'''\n")

    f_txt.close()
    f_py.close()
    f_rna.close()

    print('\nSaved %s, %s and %s.\n' % (font_bold(f_txt.name), font_bold(f_py.name), font_bold(f_rna.name)))


def main():

    global sort_choices, default_sort_choice
    global kw_prefixes, kw

    sort_choices = ['note', 'changed', 'class', 'from', 'to', 'kw', 'class.to']
    default_sort_choice = sort_choices[-1]
    kw_prefixes = ['active', 'apply', 'bl', 'exclude', 'has', 'invert', 'is', 'lock',
                   'pressed', 'show', 'show_only', 'use', 'use_only', 'layers', 'states', 'select']
    kw = ['active', 'hide', 'invert', 'select', 'layers', 'mute', 'states', 'use', 'lock']

    input_filename, sort_priority = check_commandline()
    props_list, props_length_max = get_props(input_filename)
    props_list = sort(props_list, sort_priority)

    output_basename = file_basename(input_filename)
    write_files(output_basename, props_list, props_length_max)


if __name__ == '__main__':
    import sys
    if sys.version_info.major < 3:
        print("Incorrect Python version, use Python 3 or newer!")
    else:
        main()
