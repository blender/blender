import os
from sverchok.ui.development import get_version_string

# pylint: disable=c0304
# pylint: disable=c0326
# pylint: disable=w1401

def logo():
    l1 = " ______ _    _ _______  ______ _______ _     _  _____  _     _"
    l2 = "/______  \  /  |______ |_____/ |       |_____| |     | |____/"
    l3 = "______/   \/   |______ |    \_ |_____  |     | |_____| |    \_"
    l4 = "initialized."
    lines = [l1, l2, l3, l4]

    can_paint = os.name in {'posix'}

    with_color = "\033[1;31m{0}\033[0m" if can_paint else "{0}"
    for line in lines:
        print(with_color.format(line))

def show_welcome():
    print("** version: ", get_version_string()," **")
    print("** Have a nice day with sverchok  **\n")
    logo()
