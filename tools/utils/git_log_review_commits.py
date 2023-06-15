#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This is a tool for reviewing commit ranges, writing into accept/reject files.

Useful for reviewing revisions to backport to stable builds.

Example usage:

   ./git_log_review_commits.py --source=../../.. --range=HEAD~40..HEAD --filter=BUGFIX
"""


class _Getch:
    """
    Gets a single character from standard input.
    Does not echo to the screen.
    """

    def __init__(self):
        try:
            self.impl = _GetchWindows()
        except ImportError:
            self.impl = _GetchUnix()

    def __call__(self):
        return self.impl()


class _GetchUnix:

    def __init__(self):
        import tty
        import sys

    def __call__(self):
        import sys
        import tty
        import termios
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch


class _GetchWindows:

    def __init__(self):
        import msvcrt

    def __call__(self):
        import msvcrt
        return msvcrt.getch()


getch = _Getch()
# ------------------------------------------------------------------------------
# Pretty Printing

USE_COLOR = True

if USE_COLOR:
    color_codes = {
        'black': '\033[0;30m',
        'bright_gray': '\033[0;37m',
        'blue': '\033[0;34m',
        'white': '\033[1;37m',
        'green': '\033[0;32m',
        'bright_blue': '\033[1;34m',
        'cyan': '\033[0;36m',
        'bright_green': '\033[1;32m',
        'red': '\033[0;31m',
        'bright_cyan': '\033[1;36m',
        'purple': '\033[0;35m',
        'bright_red': '\033[1;31m',
        'yellow': '\033[0;33m',
        'bright_purple': '\033[1;35m',
        'dark_gray': '\033[1;30m',
        'bright_yellow': '\033[1;33m',
        'normal': '\033[0m',
    }

    def colorize(msg, color=None):
        return (color_codes[color] + msg + color_codes['normal'])
else:
    def colorize(msg, color=None):
        return msg
bugfix = ""
# avoid encoding issues
import os
import sys
import io

sys.stdin = os.fdopen(sys.stdin.fileno(), "rb")
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='surrogateescape', line_buffering=True)
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='surrogateescape', line_buffering=True)


def print_commit(c):
    print("------------------------------------------------------------------------------")
    print(colorize("{{GitCommit|%s}}" % c.sha1.decode(), color='green'), end=" ")
    # print("Author: %s" % colorize(c.author, color='bright_blue'))
    print(colorize(c.author, color='bright_blue'))
    print()
    print(colorize(c.body, color='normal'))
    print()
    print(colorize("Files: (%d)" % len(c.files_status), color='yellow'))
    for f in c.files_status:
        print(colorize("  %s %s" % (f[0].decode('ascii'), f[1].decode('ascii')), 'yellow'))
    print()


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    usage_text = "Review revisions."

    epilog = "This script is typically used to help write release notes"

    parser = argparse.ArgumentParser(description=usage_text, epilog=epilog)

    parser.add_argument(
        "--source", dest="source_dir",
        metavar='PATH', required=True,
        help="Path to git repository")
    parser.add_argument(
        "--range", dest="range_sha1",
                        metavar='SHA1_RANGE', required=True,
                        help="Range to use, eg: 169c95b8..HEAD")
    parser.add_argument(
        "--author", dest="author",
        metavar='AUTHOR', type=str, required=False,
        help=("Method to filter commits in ['BUGFIX', todo]"))
    parser.add_argument(
        "--filter", dest="filter_type",
        metavar='FILTER', type=str, required=False,
        help=("Method to filter commits in ['BUGFIX', todo]"))

    return parser


def main():
    ACCEPT_FILE = "review_accept.txt"
    REJECT_FILE = "review_reject.txt"

    # ----------
    # Parse Args

    args = argparse_create().parse_args()

    from git_log import GitCommitIter

    # --------------
    # Filter Commits

    def match(c):
        # filter_type
        if not args.filter_type:
            pass
        elif args.filter_type == 'BUGFIX':
            first_line = c.body.strip().split("\n")[0]
            assert len(first_line)
            if any(w for w in first_line.split() if w.lower().startswith(("fix", "bugfix", "bug-fix"))):
                pass
            else:
                return False
        elif args.filter_type == 'NOISE':
            first_line = c.body.strip().split("\n")[0]
            assert len(first_line)
            if any(w for w in first_line.split() if w.lower().startswith("cleanup")):
                pass
            else:
                return False
        else:
            raise Exception("Filter type %r isn't known" % args.filter_type)

        # author
        if not args.author:
            pass
        elif args.author != c.author:
            return False

        return True

    commits = [c for c in GitCommitIter(args.source_dir, args.range_sha1) if match(c)]

    # oldest first
    commits.reverse()

    tot_accept = 0
    tot_reject = 0

    def exit_message():
        print("  Written",
              colorize(ACCEPT_FILE, color='green'), "(%d)" % tot_accept,
              colorize(REJECT_FILE, color='red'), "(%d)" % tot_reject,
              )

    for i, c in enumerate(commits):
        if os.name == "posix":
            # Also clears scroll-back.
            os.system("tput reset")
        else:
            print('\x1b[2J')  # clear

        sha1 = c.sha1

        # diff may scroll off the screen, that's OK
        os.system("git --git-dir %s show %s --format=%%n" % (c._git_dir, sha1.decode('ascii')))
        print("")
        print_commit(c)
        sys.stdout.flush()
        # print(ch)
        while True:
            print("Space=" + colorize("Accept", 'green'),
                  "Enter=" + colorize("Skip", 'red'),
                  "Ctrl+C or Q=" + colorize("Quit", color='white'),
                  "[%d of %d]" % (i + 1, len(commits)),
                  "(+%d | -%d)" % (tot_accept, tot_reject),
                  )
            ch = getch()

            if ch == b'\x03' or ch == b'q':
                # Ctrl+C
                exit_message()
                print("Goodbye! (%s)" % c.sha1.decode())
                return

            elif ch == b' ':
                log_filepath = ACCEPT_FILE
                tot_accept += 1
                break
            elif ch == b'\r':
                log_filepath = REJECT_FILE
                tot_reject += 1
                break
            else:
                print("Unknown input %r" % ch)

        with open(log_filepath, 'ab') as f:
            f.write(sha1 + b'\n')

    exit_message()


if __name__ == "__main__":
    main()
