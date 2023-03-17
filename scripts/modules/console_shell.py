# SPDX-License-Identifier: GPL-2.0-or-later
import os
import bpy

language_id = "shell"


def add_scrollback(text, text_type):
    for l in text.split("\n"):
        bpy.ops.console.scrollback_append(text=l.replace("\t", "    "), type=text_type)


def shell_run(text):
    import subprocess

    val, output = subprocess.getstatusoutput(text)

    if not val:
        style = "OUTPUT"
    else:
        style = "ERROR"

    add_scrollback(output, style)


PROMPT = "$ "


def execute(context, _is_interactive):
    sc = context.space_data

    try:
        line = sc.history[-1].body
    except:
        return {"CANCELLED"}

    bpy.ops.console.scrollback_append(text=sc.prompt + line, type="INPUT")

    shell_run(line)

    # insert a new blank line
    bpy.ops.console.history_append(text="", current_character=0, remove_duplicates=True)

    sc.prompt = os.getcwd() + PROMPT
    return {"FINISHED"}


def autocomplete(_context):
    return {"CANCELLED"}


def banner(context):
    sc = context.space_data

    shell_run("bash --version")
    sc.prompt = os.getcwd() + PROMPT

    return {"FINISHED"}
