# Apache License, Version 2.0

# Simple utility that prints all WITH_* options in a CMakeLists.txt
# Called by 'make help_features'

import re
import sys

cmakelists_file = sys.argv[-1]

def main():
    options = []
    for l in open(cmakelists_file, 'r').readlines():
        if not l.lstrip().startswith('#'):
            l_option = re.sub(r'.*\boption\s*\(\s*(WITH_[a-zA-Z0-9_]+)\s+\"(.*)\"\s*.*', r'\g<1> - \g<2>', l)
            if l_option != l:
                l_option = l_option.strip()
                if l_option.startswith('WITH_'):
                    options.append(l_option)

    print('\n'.join(options))


if __name__ == "__main__":
    main()
