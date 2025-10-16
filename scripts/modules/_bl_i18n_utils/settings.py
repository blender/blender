# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Global settings used by all scripts in this directory.
# XXX Before any use of the tools in this directory, please make a copy of this file
#     named "setting.py"
# XXX This is a template, most values should be OK, but some you’ll have to
#     edit (most probably, BLENDER_EXEC and SOURCE_DIR).


import json
import os
import re
import sys
import types

# Only do soft-dependency on `bpy` module, not real strong need for it currently.
try:
    import bpy
except ModuleNotFoundError:
    bpy = None

###############################################################################
# MISC
###############################################################################

# The languages defined in Blender.
LANGUAGES = (
    # ID, UI English label, ISO code.
    (0, "Automatic", "DEFAULT"),
    (1, "English (US)", "en_US"),
    (2, "Japanese - 日本語", "ja_JP"),
    (3, "Dutch - Nederlands", "nl_NL"),
    (4, "Italian - Italiano", "it_IT"),
    (5, "German - Deutsch", "de_DE"),
    (6, "Finnish - Suomi", "fi_FI"),
    (7, "Swedish - Svenska", "sv_SE"),
    (8, "French - Français", "fr_FR"),
    (9, "Spanish - Español", "es"),
    (10, "Catalan - Català", "ca_AD"),
    (11, "Czech - Čeština", "cs_CZ"),
    (12, "Portuguese (Portugal) - Português europeu", "pt_PT"),
    (13, "Chinese (Simplified) - 简体中文", "zh_HANS"),
    (14, "Chinese (Traditional) - 繁體中文", "zh_HANT"),
    (15, "Russian - Русский", "ru_RU"),
    (16, "Croatian - Hrvatski", "hr_HR"),
    (17, "Serbian (Cyrillic) - Српски", "sr_RS"),
    (18, "Ukrainian - Українська", "uk_UA"),
    (19, "Polish - Polski", "pl_PL"),
    (20, "Romanian - Român", "ro_RO"),
    # Using the utf8 flipped form of Arabic (العربية).
    (21, "Arabic - ﺔﻴﺑﺮﻌﻟﺍ", "ar_EG"),
    (22, "Bulgarian - Български", "bg_BG"),
    (23, "Greek - Ελληνικά", "el_GR"),
    (24, "Korean - 한국어", "ko_KR"),
    (25, "Nepali - नेपाली", "ne_NP"),
    # Using the utf8 flipped form of Persian (فارسی).
    (26, "Persian - ﯽﺳﺭﺎﻓ", "fa_IR"),
    (27, "Indonesian - Bahasa indonesia", "id_ID"),
    (28, "Serbian (Latin) - Srpski latinica", "sr_RS@latin"),
    (29, "Kyrgyz - Кыргыз тили", "ky_KG"),
    (30, "Turkish - Türkçe", "tr_TR"),
    (31, "Hungarian - Magyar", "hu_HU"),
    (32, "Portuguese (Brazil) - Português brasileiro", "pt_BR"),
    # Using the utf8 flipped form of Hebrew (עִבְרִית)).
    (33, "Hebrew - תירִבְעִ", "he_IL"),
    (34, "Estonian - Eesti keel", "et_EE"),
    (35, "Esperanto - Esperanto", "eo"),
    # 36 is free, used to be 'Spanish from Spain' (`es_ES`).
    (37, "Amharic - አማርኛ", "am_ET"),
    (38, "Uzbek (Latin) - Oʻzbek", "uz_UZ@latin"),
    (39, "Uzbek (Cyrillic) - Ўзбек", "uz_UZ@cyrillic"),
    (40, "Hindi - हिन्दी", "hi_IN"),
    (41, "Vietnamese - Tiếng Việt", "vi_VN"),
    (42, "Basque - Euskara", "eu_EU"),
    (43, "Hausa - Hausa", "ha"),
    (44, "Kazakh - Қазақша", "kk_KZ"),
    (45, "Abkhaz - Аԥсуа бызшәа", "ab"),
    (46, "Thai - ภาษาไทย", "th_TH"),
    (47, "Slovak - Slovenčina", "sk_SK"),
    (48, "Georgian - ქართული", "ka"),
    (49, "Tamil - தமிழ்", "ta"),
    (50, "Khmer - ខ្មែរ", "km"),
    (51, "Swahili - Kiswahili", "sw"),
    (52, "Belarusian - беларуску", "be"),
    (53, "Danish - Dansk", "da"),
    (54, "Slovenian - Slovenščina", "sl"),
    # Using the utf8 flipped form of Urdu (اُردُو).
    (55, "Urdu - وُدرُا", "ur"),
    (56, "Lithuanian - Lietuviškai", "lt"),
    (57, "English (UK)", "en_GB"),
    (58, "Malayalam - മലയാളം", "ml"),
)

# Default context, in py (keep in sync with `BLT_translation.hh`)!
if bpy is not None:
    assert bpy.app.translations.contexts.default == "*"
DEFAULT_CONTEXT = "*"

# Name of language file used by Blender to generate translations' menu.
LANGUAGES_FILE = "languages"

# The minimum level of completeness for a `.po` file to be imported from
# the working repository to the Blender one, as a percentage.
IMPORT_MIN_LEVEL = 0.0

# Languages in the working repository that should not be imported in the Blender one currently...
IMPORT_LANGUAGES_SKIP = {
    'am_ET', 'et_EE', 'uz_UZ@latin', 'uz_UZ@cyrillic', 'kk_KZ',
}

# Languages that need RTL pre-processing.
IMPORT_LANGUAGES_RTL = {
    'ar_EG', 'fa_IR', 'he_IL', 'ur',
}

# The comment prefix used in generated `messages.txt` file.
MSG_COMMENT_PREFIX = "#~ "

# The comment prefix used in generated `messages.txt` file.
MSG_CONTEXT_PREFIX = "MSGCTXT:"

# The default comment prefix used in po's.
PO_COMMENT_PREFIX = "# "

# The comment prefix used to mark sources of msgids, in po's.
PO_COMMENT_PREFIX_SOURCE = "#: "

# The comment prefix used to mark sources of msgids, in po's.
PO_COMMENT_PREFIX_SOURCE_CUSTOM = "#. :src: "

# The general "generated" comment prefix, in po's.
PO_COMMENT_PREFIX_GENERATED = "#. "

# The comment prefix used to comment entries in po's.
PO_COMMENT_PREFIX_MSG = "#~ "

# The comment prefix used to mark fuzzy msgids, in po's.
PO_COMMENT_FUZZY = "#, fuzzy"

# The prefix used to define context, in po's.
PO_MSGCTXT = "msgctxt "

# The prefix used to define msgid, in po's.
PO_MSGID = "msgid "

# The prefix used to define msgstr, in po's.
PO_MSGSTR = "msgstr "

# The 'header' key of po files.
PO_HEADER_KEY = (DEFAULT_CONTEXT, "")

PO_HEADER_MSGSTR = (
    "Project-Id-Version: {blender_ver} ({blender_hash})\\n\n"
    "Report-Msgid-Bugs-To: \\n\n"
    "POT-Creation-Date: {time}\\n\n"
    "PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\n"
    "Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\n"
    "Language-Team: LANGUAGE <LL@li.org>\\n\n"
    "Language: {uid}\\n\n"
    "MIME-Version: 1.0\\n\n"
    "Content-Type: text/plain; charset=UTF-8\\n\n"
    "Content-Transfer-Encoding: 8bit\n"
)
PO_HEADER_COMMENT_COPYRIGHT = (
    "# Blender's translation file (po format).\n"
    "# Copyright (C) {year} The Blender Authors.\n"
    "# This file is distributed under the same license as the Blender package.\n"
    "#\n"
)
PO_HEADER_COMMENT = (
    "# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.\n"
    "#"
)

TEMPLATE_ISO_ID = "__TEMPLATE__"

# Num buttons report their label with a trailing ': '...
NUM_BUTTON_SUFFIX = ": "

# Undocumented operator placeholder string.
UNDOC_OPS_STR = "(undocumented operator)"

# The gettext domain.
DOMAIN = "blender"

# Our own "gettext" stuff.
# File type (ext) to parse.
PYGETTEXT_ALLOWED_EXTS = {".c", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx", ".h"}

# Max number of contexts into a BLT_I18N_MSGID_MULTI_CTXT macro...
PYGETTEXT_MAX_MULTI_CTXT = 16

# Where to search contexts definitions, relative to SOURCE_DIR (defined below).
PYGETTEXT_CONTEXTS_DEFSRC = os.path.join("source", "blender", "blentranslation", "BLT_translation.h")

# Regex to extract contexts defined in BLT_translation.h
# XXX Not full-proof, but should be enough here!
PYGETTEXT_CONTEXTS = "#define\\s+(BLT_I18NCONTEXT_[A-Z_0-9]+)\\s+\"([^\"]*)\""

# autopep8: off

# Keywords' regex.
# XXX Most unfortunately, we can't use named back-references inside character sets,
#     which makes the REGEXES even more twisty... :/
_str_base = (
    # Match void string
    "(?P<{_}1>[\"'])(?P={_}1)"  # Get opening quote (' or "), and closing immediately.
    "|"
    # Or match non-void string
    "(?P<{_}2>[\"'])"  # Get opening quote (' or ").
        "(?{capt}(?:"
            # This one is for crazy things like "hi \\\\\" folks!"...
            r"(?:(?!<\\)(?:\\\\)*\\(?=(?P={_}2)))|"
            # The most common case.
            ".(?!(?P={_}2))"
        ")*.)"  # Don't forget the last char!
    "(?P={_}2)"  # And closing quote.
)
str_clean_re = _str_base.format(_="g", capt="P<clean>")
_inbetween_str_re = (
    # XXX Strings may have comments between their pieces too, not only spaces!
    r"(?:\s*(?:"
        # A C comment
        r"/\*.*(?!\*/).\*/|"
        # Or a C++ one!
        r"//[^\n]*\n|"
        # Or some #defined value (like `BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE`)
        # NOTE: This should be avoided at all cost, as it will simply make translation lookup fail.
        r"[ a-zA-Z0-9_]*"
    # And we are done!
    r")?)*"
)
# Here we have to consider two different cases (empty string and other).
_str_whole_re = (
    _str_base.format(_="{_}1_", capt=":") +
    # Optional loop start, this handles "split" strings...
    "(?:(?<=[\"'])" + _inbetween_str_re + "(?=[\"'])(?:"
        + _str_base.format(_="{_}2_", capt=":") +
    # End of loop.
    "))*"
)
_ctxt_re_gen = lambda uid: (
    r"(?P<ctxt_raw{uid}>(?:".format(uid=uid) +
    _str_whole_re.format(_="_ctxt{uid}".format(uid=uid)) +
    r")|(?:[A-Z_0-9]+))"
)
_ctxt_re = _ctxt_re_gen("")
_msg_re = r"(?P<msg_raw>" + _str_whole_re.format(_="_msg") + r")"

class PyGettextKeyword:
    def __init__(self, re_expr, context_override=None):
        self.re_expr = re_expr
        self.context_override = context_override

        self.re = re.compile(re_expr)
        self.search = self.re.search

PYGETTEXT_KEYWORDS = (() +
    tuple(PyGettextKeyword((r"{}\(\s*" + _msg_re + r"\s*\)").format(it))
         for it in ("IFACE_", "TIP_", "RPT_", "DATA_", "N_")) +

    tuple(PyGettextKeyword((r"{}\(\s*" + _ctxt_re + r"\s*,\s*" + _msg_re + r"\s*\)").format(it))
          for it in ("CTX_IFACE_", "CTX_TIP_", "CTX_RPT_", "CTX_DATA_", "CTX_N_")) +

    tuple(PyGettextKeyword(("{}\\((?:[^\"',]+,){{1,2}}\\s*" + _msg_re + r"\s*(?:\)|,)").format(it))
          for it in ("BKE_report", "BKE_reportf", "BKE_reports_prepend", "BKE_reports_prependf",
                     "CTX_wm_operator_poll_msg_set", "WM_global_report", "WM_global_reportf",
                     "UI_but_disable")) +

    # ED_undo_push() is used in Undo History menu and has the "Operator" context, same as operators.
    tuple(PyGettextKeyword(("{}\\((?:[^\"',]+,)\\s*" + _msg_re + r"\s*(?:\))").format(it),
                           context_override='BLT_I18NCONTEXT_OPERATOR_DEFAULT')
          for it in ("ED_undo_push", "ED_undo_grouped_push")) +

    # bmesh operator errors
    tuple(PyGettextKeyword(("{}\\((?:[^\"',]+,){{3}}\\s*" + _msg_re + r"\s*\)").format(it))
          for it in ("BMO_error_raise",)) +

    # Modifier errors
    tuple(PyGettextKeyword(("{}\\((?:[^\"',]+,){{2}}\\s*" + _msg_re + r"\s*(?:\)|,)").format(it))
          for it in ("BKE_modifier_set_error",)) +

    # Window manager job names.
    tuple(PyGettextKeyword(("{}\\((?:[^\"',]+,){{3}}\\s*" + _msg_re + r"\s*,").format(it))
          for it in ("WM_jobs_get",)) +

    # Compositor and EEVEE messages.
    # Ends either with `)` (function call close), or `,` when there are extra formatting parameters.
    tuple(PyGettextKeyword((r"{}\(\s*" + _msg_re + r"\s*(?:\)|,)").format(it))
          for it in ("set_info_message", "info_append_i18n")) +

    # This one is a tad more risky, but in practice would not expect a name/uid string parameter
    # (the second one in those functions) to ever have a comma in it, so think this is fine.
    tuple(PyGettextKeyword(("{}\\((?:[^,]+,){{2}}\\s*" + _msg_re + r"\s*(?:\)|,)").format(it))
          for it in ("modifier_subpanel_register", "gpencil_modifier_subpanel_register")) +

    # Node socket declarations: context-less names.
    tuple(PyGettextKeyword((r"\.{}(?:<decl::.*?>\(|[^,]+,)\s*" + _msg_re + r"(?:,[^),]+)*\s*\)"
           r"(?![^;]*\.translation_context\()").format(it))
          for it in ("add_input", "add_output")) +

    # Node socket declarations: names with contexts
    tuple(PyGettextKeyword((r"\.{}(?:<decl::.*?>\(|[^,]+,)\s*" + _msg_re +
           r"[^;]*\.translation_context\(\s*" + _ctxt_re + r"\s*\)").format(it))
          for it in ("add_input", "add_output")) +

    # Node socket declarations: description and error messages
    tuple(PyGettextKeyword((r"\.{}\(\s*" + _msg_re + r"\s*\)").format(it))
          for it in ("description", "error_message_add")) +

    # Node socket panels and labels from declarations: context-less names
    tuple(PyGettextKeyword((r"\.{}\(\s*" + _msg_re +
           r"\s*\)(?![^;]*\.translation_context\()[^;]*;").format(it))
          for it in ("short_label", "add_panel",)) +

    # Node socket panels and labels from declarations: names with contexts
    tuple(PyGettextKeyword((r"\.{}\(\s*" + _msg_re + r"[^;]*\.translation_context\(\s*" +
           _ctxt_re + r"\s*\)").format(it))
          for it in ("short_label", "add_panel",)) +

    # Dynamic node socket labels
    tuple(PyGettextKeyword((r"{}\(\s*[^,]+,\s*" + _msg_re + r"\s*\)").format(it))
          for it in ("node_sock_label",)) +

    # Geometry Nodes field inputs
    (PyGettextKeyword(r"FieldInput\(CPPType::get<.*?>\(\),\s*" + _msg_re + r"\s*\)"),) +

    # bUnitDef unit names
    (PyGettextKeyword(r"/\*name_display\*/\s*" + _msg_re + r"\s*,"),) +

    tuple(PyGettextKeyword(
        (r"{}\(\s*"
         + _msg_re
         + r"\s*,\s*(?:"
         + r"\s*,\s*)?(?:".join(_ctxt_re_gen(i) for i in range(PYGETTEXT_MAX_MULTI_CTXT))
         + r")?\s*,?\s*\)"
         ).format(it)
    ) for it in ("BLT_I18N_MSGID_MULTI_CTXT",))
)

# autopep8: on


# Check printf mismatches between msgid and msgstr.
CHECK_PRINTF_FORMAT = (
    r"(?!<%)(?:%%)*%"          # Beginning, with handling for crazy things like '%%%%%s'
    r"[-+#0]?"                 # Flags (note: do not add the ' ' (space) flag here, generates too much false positives!)
    r"(?:\*|[0-9]+)?"          # Width
    r"(?:\.(?:\*|[0-9]+))?"    # Precision
    r"(?:[hljztL]|hh|ll)?"     # Length
    r"[tldiuoxXfFeEgGaAcspn]"  # Specifiers (note we have Blender-specific %t and %l ones too)
)

# Should po parser warn when finding a first letter not capitalized?
WARN_MSGID_NOT_CAPITALIZED = True

# Strings that should not raise above warning!
WARN_MSGID_NOT_CAPITALIZED_ALLOWED = {
    "",                              # Simplifies things... :p
    "ac3",
    "along X",
    "along Y",
    "along Z",
    "along %s X",
    "along %s Y",
    "along %s Z",
    "along local Z",
    "arccos(A)",
    "arcsin(A)",
    "arctan(A)",
    "ascii",
    "author",                        # Addons' field. :/
    "bItasc",
    "blender.org",
    "bytes",
    "color_index is invalid",
    "cos(A)",
    "cosh(A)",
    "dB",                            # dB audio power unit.
    "dbl-",                          # Compacted for 'double', for keymap items.
    "description",                   # Addons' field. :/
    "dx",
    "fBM",
    "flac",
    "fps: %.2f",
    "fps: %i",
    "gimbal",
    "global",
    "glTF 2.0 (.glb/.gltf)",
    "glTF Binary (.glb)",
    "glTF Embedded (.gltf)",
    "glTF Material Output",
    "glTF Original PBR data",
    "glTF Separate (.gltf + .bin + textures)",
    "gltfpack",
    "glTFpack file path",
    "invoke() needs to be called before execute()",
    "iScale",
    "iso-8859-15",
    "iTaSC",
    "iTaSC parameters",
    "kb",
    "local",
    "location",                      # Addons' field. :/
    "locking %s X",
    "locking %s Y",
    "locking %s Z",
    "mkv",
    "mm",
    "mp2",
    "mp3",
    "normal",
    "ogg",
    "oneAPI",
    "p0",
    "parent_index should not be less than -1: %d",
    "parent_index (%d) should be less than the number of bone collections (%d)",
    "px",
    "re",
    "res",
    "rv",
    "seconds",
    "sin(A)",
    "sin(x) / x",
    "sinh(A)",
    "sqrt(x*x+y*y+z*z)",
    "sRGB",
    "sRGB display space",
    "sRGB display space with Filmic view transform",
    "sRGB IEC 61966-2-1 compound (piece-wise) encoding",
    "tan(A)",
    "tanh(A)",
    "utf-8",
    "uv_on_emitter() requires a modifier from an evaluated object",
    "var",
    "vBVH",
    "view",
    "wav",
    "wmOwnerID '%s' not in workspace '%s'",
    "y",
    "y = (Ax + B)",
    # ID plural names, defined in IDTypeInfo.
    "armatures",
    "brushes",
    "cache_files",
    "cameras",
    "collections",
    "curves",
    "fonts",
    "grease_pencils",
    "hair_curves",
    "ipos",
    "lattices",
    "libraries",
    "lightprobes",
    "lights",
    "linestyles",
    "link_placeholders",
    "masks",
    "metaballs",
    "materials",
    "meshes",
    "movieclips",
    "node_groups",
    "objects",
    "paint_curves",
    "palettes",
    "particles",
    "pointclouds",
    "screens",
    "shape_keys",
    "sounds",
    "speakers",
    "texts",
    "textures",
    "volumes",
    "window_managers",
    "workspaces",
    "worlds",
    # Sub-strings.
    "all",
    "all and invert unselected",
    "and AMD driver version %s or newer",
    "and AMD Radeon Pro %s driver or newer",
    "and NVIDIA driver version %s or newer",
    "and Windows driver version %s or newer",
    "available with",
    "brown fox",
    "can't save image while rendering",
    "category",
    "constructive modifier",
    "cursor",
    "custom",
    "custom matrix",
    "custom orientation",
    "drag-",
    "edge data",
    "exp(A)",
    "expected a timeline/animation area to be active",
    "expected a view3d region",
    "expected a view3d region & editcurve",
    "expected a view3d region & editmesh",
    "face data",
    "gimbal",
    "global",
    "glTF Settings",
    "image file not found",
    "image format is read-only",
    "image path can't be written to",
    "in %i days",
    "in %i hours",
    "in %i minutes",
    "in memory to enable editing!",
    "in the asset shelf.",
    "insufficient content",
    "into",
    "jumps over",
    "left",
    "local",
    "matrices", "no matrices",
    "multi-res modifier",
    "name",
    "non-triangle face",
    "normal",
    "on {:%Y-%m-%d}",
    "or AMD with macOS %s or newer",
    "parent",
    "performance impact!",
    "positions", "no positions",
    "read",
    "remove",
    "right",
    "selected",
    "selected and lock unselected",
    "selected and unlock unselected",
    "screen",
    "the lazy dog",
    "this legacy pose library to pose assets",
    "to the top level of the tree",
    "unable to load movie clip",
    "unable to load text",
    "unable to open the file",
    "unknown error reading file",
    "unknown error statting file",
    "unknown error writing file",
    "unselected",
    "unsupported font format",
    "unsupported format",
    "unsupported image format",
    "unsupported movie clip format",
    "untitled",
    "vertex data",
    "verts only",
    "view",
    "virtual parents",
    "which was replaced by the Asset Browser",
    "within seconds",
    "write",
}
WARN_MSGID_NOT_CAPITALIZED_ALLOWED |= set(lng[2] for lng in LANGUAGES)

WARN_MSGID_END_POINT_ALLOWED = {
    "Cannot figure out which object this bone belongs to.",
    "Circle|Alt .",
    "Float Neg. Exp.",
    "Max Ext.",
    "Newer graphics drivers may be available to improve Blender support.",
    "Not assigned to any bone collection.",
    "Numpad .",
    "Pad.",
    "Please file a bug report.",
    "    RNA Path: bpy.types.",
    "Temp. Diff.",
    "Temperature Diff.",
    "The program will now close.",
    "Your graphics card or driver has limited support. It may work, but with issues.",
    "Your graphics card or driver is not supported.",
    "Invalid surface UVs on %d curves.",
    "The pose library moved.",
    "in the asset shelf.",
    "Remove, local files not found.",
    "Remove all files in \"{}\".",
    "Remove, keeping local files.",
}

PARSER_CACHE_HASH = 'sha1'

PARSER_TEMPLATE_ID = "__POT__"
PARSER_PY_ID = "__PY__"

PARSER_PY_MARKER_BEGIN = "\n# ##### BEGIN AUTOGENERATED I18N SECTION #####\n"
PARSER_PY_MARKER_END = "\n# ##### END AUTOGENERATED I18N SECTION #####\n"

PARSER_MAX_FILE_SIZE = 2 ** 24  # in bytes, i.e. 16 Mb.

###############################################################################
# PATHS
###############################################################################

# The Python3 executable.You’ll likely have to edit it in your user_settings.py
# if you’re under Windows.
PYTHON3_EXEC = "python3"

# The Blender executable!
# This is just an example, you’ll have to edit it in your user_settings.py!
BLENDER_EXEC = os.path.abspath(os.path.join("foo", "bar", "blender"))
# check for blender.bin
if not os.path.exists(BLENDER_EXEC):
    if os.path.exists(BLENDER_EXEC + ".bin"):
        BLENDER_EXEC = BLENDER_EXEC + ".bin"

# The gettext msgfmt "compiler". You’ll likely have to edit it in your user_settings.py if you’re under Windows.
GETTEXT_MSGFMT_EXECUTABLE = "msgfmt"

# The FriBidi C compiled library (.so under Linux, `.dll` under windows...).
# You’ll likely have to edit it in your `user_settings.py` if you’re under Windows., e.g. using the included one:
# `FRIBIDI_LIB = os.path.join(TOOLS_DIR, "libfribidi.dll")`
FRIBIDI_LIB = "libfribidi.so.0"

# The name of the (currently empty) file that must be present in a po's directory to enable RTL-preprocess.
RTL_PREPROCESS_FILE = "is_rtl"

# The Blender source root path.
# This is just an example, you’ll have to override it in your user_settings.py!
SOURCE_DIR = os.path.abspath(os.path.join("blender"))

# The bf-translation repository (you'll have to override this in your user_settings.py).
I18N_DIR = os.path.abspath(os.path.join("i18n"))

# The 'work' path to PO files (relative to I18N_DIR).
REL_WORK_DIR = os.path.join("")


# The path to the Blender translation directory (relative to SOURCE_DIR).
REL_BLENDER_I18N_DIR = os.path.join("locale")

# The /po path of the Blender translation directory (relative to REL_BLENDER_I18N_DIR).
REL_BLENDER_I18N_PO_DIR = os.path.join("po")


# The Blender source path to check for i18n macros (relative to SOURCE_DIR).
REL_POTFILES_SOURCE_DIR = os.path.join("source")

# Where to search for preset names (relative to SOURCE_DIR).
REL_PRESETS_DIR = os.path.join("scripts", "presets")

# Where to search for templates (relative to SOURCE_DIR).
REL_TEMPLATES_DIR = os.path.join("scripts", "startup", "bl_app_templates_system")

# Name of the built-in asset catalog file.
ASSET_CATALOG_FILE = "blender_assets.cats.txt"

# The template messages file (relative to I18N_DIR).
REL_FILE_NAME_POT = os.path.join(REL_WORK_DIR, DOMAIN + ".pot")


# Mo path generator for a given language (relative to any "locale" directory).
MO_PATH_ROOT_RELATIVE = os.path.join("locale")
MO_PATH_TEMPLATE_RELATIVE = os.path.join(MO_PATH_ROOT_RELATIVE, "{}", "LC_MESSAGES")

# Mo file name.
MO_FILE_NAME = DOMAIN + ".mo"

# Where to search for py files that may contain ui strings (relative to one of the 'resource_path' of Blender).
CUSTOM_PY_UI_FILES = [
    os.path.join("scripts", "startup", "bl_ui"),
    os.path.join("scripts", "startup", "bl_operators"),
    os.path.join("scripts", "modules", "rna_prop_ui.py"),
    os.path.join("scripts", "modules", "rna_keymap_ui.py"),
    os.path.join("scripts", "modules", "_bpy_types.py"),
    os.path.join("scripts", "presets", "keyconfig"),
]

# An optional text file listing files to force include/exclude from py_xgettext process.
SRC_POTFILES = ""

# A cache storing validated msgids, to avoid re-spellchecking them.
SPELL_CACHE = os.path.join("/tmp", ".spell_cache")

# Threshold defining whether a new msgid is similar enough with an old one to reuse its translation...
SIMILAR_MSGID_THRESHOLD = 0.75

# Additional import paths to add to `sys.path` (';' separated)...
INTERN_PY_SYS_PATHS = ""

# Custom override settings must be one directory above i18n tools itself!
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
try:
    from bl_i18n_settings_override import *
except ImportError:  # If no i18n_override_settings available, it’s no error!
    pass

# Override with custom user settings, if available.
try:
    from settings_user import *
except ImportError:  # If no user_settings available, it’s no error!
    pass


for p in set(INTERN_PY_SYS_PATHS.split(";")):
    if p:
        sys.path.append(p)


# The settings class itself!
def _do_get(ref, path):
    return os.path.normpath(os.path.join(ref, path))


def _do_set(ref, path):
    path = os.path.normpath(path)
    # If given path is absolute, make it relative to current ref one (else we consider it is already the case!)
    if os.path.isabs(path):
        # can't always find the relative path (between drive letters on windows)
        try:
            return os.path.relpath(path, ref)
        except ValueError:
            pass
    return path


def _gen_get_set_path(ref, name):
    def _get(self):
        return _do_get(getattr(self, ref), getattr(self, name))

    def _set(self, value):
        setattr(self, name, _do_set(getattr(self, ref), value))
    return _get, _set


def _check_valid_data(uid, val):
    return not uid.startswith("_") and type(val) not in tuple(types.__dict__.values()) + (type,)


class I18nSettings:
    """
    Class allowing persistence of our settings!
    Saved in JSON format, so settings should be JSON'able objects!
    """
    _settings = None

    def __new__(cls, *args, **kwargs):
        # Addon preferences are singleton by definition, so is this class!
        if not I18nSettings._settings:
            cls._settings = super(I18nSettings, cls).__new__(cls)
            cls._settings.__dict__ = {uid: val for uid, val in globals().items() if _check_valid_data(uid, val)}
        return I18nSettings._settings

    def __getstate__(self):
        return self.to_dict()

    def __setstate__(self, mapping):
        return self.from_dict(mapping)

    def from_dict(self, mapping):
        # Special case... :/
        if "INTERN_PY_SYS_PATHS" in mapping:
            self.PY_SYS_PATHS = mapping["INTERN_PY_SYS_PATHS"]
        self.__dict__.update(mapping)

    def to_dict(self):
        glob = globals()
        return {uid: val for uid, val in self.__dict__.items() if _check_valid_data(uid, val) and uid in glob}

    def from_json(self, string):
        self.from_dict(dict(json.loads(string)))

    def to_json(self):
        # Only save the diff from default i18n_settings!
        glob = globals()
        export_dict = {
            uid: val for uid, val in self.__dict__.items()
            if _check_valid_data(uid, val) and glob.get(uid) != val
        }
        return json.dumps(export_dict)

    def load(self, fname, reset=False):
        reset = reset or fname is None
        if reset:
            self.__dict__ = {uid: data for uid, data in globals().items() if not uid.startswith("_")}
        if fname is None:
            return
        if isinstance(fname, str):
            if not os.path.isfile(fname):
                # Assume it is already real JSON string.
                self.from_json(fname)
                return
            with open(fname, encoding="utf8") as f:
                self.from_json(f.read())
        # Else assume fname is already a file(like) object!
        else:
            self.from_json(fname.read())

    def save(self, fname):
        if isinstance(fname, str):
            with open(fname, 'w', encoding="utf8") as f:
                f.write(self.to_json())
        # Else assume fname is already a file(like) object!
        else:
            fname.write(self.to_json())

    WORK_DIR = property(*(_gen_get_set_path("I18N_DIR", "REL_WORK_DIR")))
    BLENDER_I18N_ROOT = property(*(_gen_get_set_path("SOURCE_DIR", "REL_BLENDER_I18N_DIR")))
    BLENDER_I18N_PO_DIR = property(*(_gen_get_set_path("BLENDER_I18N_ROOT", "REL_BLENDER_I18N_PO_DIR")))
    POTFILES_SOURCE_DIR = property(*(_gen_get_set_path("SOURCE_DIR", "REL_POTFILES_SOURCE_DIR")))
    PRESETS_DIR = property(*(_gen_get_set_path("SOURCE_DIR", "REL_PRESETS_DIR")))
    TEMPLATES_DIR = property(*(_gen_get_set_path("SOURCE_DIR", "REL_TEMPLATES_DIR")))
    FILE_NAME_POT = property(*(_gen_get_set_path("I18N_DIR", "REL_FILE_NAME_POT")))

    def _get_py_sys_paths(self):
        return self.INTERN_PY_SYS_PATHS

    def _set_py_sys_paths(self, val):
        old_paths = set(self.INTERN_PY_SYS_PATHS.split(";")) - {""}
        new_paths = set(val.split(";")) - {""}
        for p in old_paths - new_paths:
            if p in sys.path:
                sys.path.remove(p)
        for p in new_paths - old_paths:
            sys.path.append(p)
        self.INTERN_PY_SYS_PATHS = val
    PY_SYS_PATHS = property(_get_py_sys_paths, _set_py_sys_paths)
