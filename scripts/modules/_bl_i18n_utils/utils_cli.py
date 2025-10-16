# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Some useful operations from utils' I18nMessages class exposed as a CLI.

import os

if __package__ is None:
    import settings as settings_i18n
    import utils as utils_i18n
    import utils_languages_menu
else:
    from . import settings as settings_i18n
    from . import utils as utils_i18n
    from . import utils_languages_menu


def update_po(args, settings):
    pot = utils_i18n.I18nMessages(uid=None, kind='PO', src=args.template, settings=settings)
    if os.path.isfile(args.dst):
        uid = os.path.splitext(os.path.basename(args.dst))[0]
        po = utils_i18n.I18nMessages(uid=uid, kind='PO', src=args.dst, settings=settings)
        po.update(pot)
    else:
        po = pot
    po.write(kind="PO", dest=args.dst)


def cleanup_po(args, settings):
    uid = os.path.splitext(os.path.basename(args.src))[0]
    if not args.dst:
        args.dst = args.src
    po = utils_i18n.I18nMessages(uid=uid, kind='PO', src=args.src, settings=settings)
    po.check(fix=True)
    po.clean_commented()
    po.write(kind="PO", dest=args.dst)


def strip_po(args, settings):
    uid = os.path.splitext(os.path.basename(args.src))[0]
    if not args.dst:
        args.dst = args.src
    po = utils_i18n.I18nMessages(uid=uid, kind='PO', src=args.src, settings=settings)
    po.clean_commented()
    po.write(kind="PO_COMPACT", dest=args.dst)


def rtl_process_po(args, settings):
    uid = os.path.splitext(os.path.basename(args.src))[0]
    if not args.dst:
        args.dst = args.src
    po = utils_i18n.I18nMessages(uid=uid, kind='PO', src=args.src, settings=settings)
    po.rtl_process()
    po.write(kind="PO", dest=args.dst)


def language_menu(args, settings):
    # 'DEFAULT' and en_US are always valid, fully-translated "languages"!
    stats = {"DEFAULT": 1.0, "en_US": 1.0}

    po_to_uid = {
        os.path.basename(po_path_work): uid
        for can_use, uid, _num_id, _name, _isocode, po_path_work
        in utils_i18n.list_po_dir(settings.WORK_DIR, settings)
        if can_use
    }
    for po_dir in os.listdir(settings.WORK_DIR):
        po_dir = os.path.join(settings.WORK_DIR, po_dir)
        if not os.path.isdir(po_dir):
            continue
        for po_path in os.listdir(po_dir):
            uid = po_to_uid.get(po_path, None)
            # print("Checking {:s}, found uid {:s}".format(po_path, uid))
            po_path = os.path.join(po_dir, po_path)
            if uid is not None:
                po = utils_i18n.I18nMessages(uid=uid, kind='PO', src=po_path, settings=settings)
                stats[uid] = po.nbr_trans_msgs / po.nbr_msgs if po.nbr_msgs > 0 else 0
    utils_languages_menu.gen_menu_file(stats, settings)


def main():
    import sys
    import argparse

    parser = argparse.ArgumentParser(description="Tool to perform common actions over PO/MO files.")
    parser.add_argument(
        '-s', '--settings', default=None,
        help="Override (some) default settings. Either a JSON file name, or a JSON string.",
    )
    sub_parsers = parser.add_subparsers()

    sub_parser = sub_parsers.add_parser('update_po', help="Update a PO file from a given POT template file")
    sub_parser.add_argument(
        '--template', metavar='template.pot', required=True,
        help="The source pot file to use as template for the update.",
    )
    sub_parser.add_argument('--dst', metavar='dst.po', required=True, help="The destination po to update.")
    sub_parser.set_defaults(func=update_po)

    sub_parser = sub_parsers.add_parser(
        'cleanup_po',
        help="Cleanup a PO file (check for and fix some common errors, remove commented messages).",
    )
    sub_parser.add_argument('--src', metavar='src.po', required=True, help="The source po file to clean up.")
    sub_parser.add_argument('--dst', metavar='dst.po', help="The destination po to write to.")
    sub_parser.set_defaults(func=cleanup_po)

    sub_parser = sub_parsers.add_parser(
        'strip_po',
        help="Reduce all non-essential data from given PO file (reduce its size).",
    )
    sub_parser.add_argument('--src', metavar='src.po', required=True, help="The source po file to strip.")
    sub_parser.add_argument('--dst', metavar='dst.po', help="The destination po to write to.")
    sub_parser.set_defaults(func=strip_po)

    sub_parser = sub_parsers.add_parser(
        'rtl_process_po',
        help="Pre-process PO files for RTL languages.",
    )
    sub_parser.add_argument('--src', metavar='src.po', required=True, help="The source po file to process.")
    sub_parser.add_argument('--dst', metavar='dst.po', help="The destination po to write to.")
    sub_parser.set_defaults(func=rtl_process_po)

    sub_parser = sub_parsers.add_parser(
        'language_menu',
        help="Generate the text file used by Blender to create its language menu.",
    )
    sub_parser.set_defaults(func=language_menu)

    args = parser.parse_args(sys.argv[1:])

    settings = settings_i18n.I18nSettings()
    settings.load(args.settings)

    if getattr(args, "template", None) is not None:
        settings.FILE_NAME_POT = args.template

    args.func(args=args, settings=settings)


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    main()
