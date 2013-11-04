# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Global settings used by all scripts in this dir.
# XXX Before any use of the tools in this dir, please make a copy of this file
#     named "setting.py"
# XXX This is a template, most values should be OK, but some you’ll have to
#     edit (most probably, BLENDER_EXEC and SOURCE_DIR).


import json
import os
import sys

import bpy

###############################################################################
# MISC
###############################################################################

# The languages defined in Blender.
LANGUAGES_CATEGORIES = (
    # Min completeness level, UI english label.
    ( 0.95, "Complete"),
    ( 0.33, "In Progress"),
    ( -1.0, "Starting"),
)
LANGUAGES = (
    # ID, UI english label, ISO code.
    ( 0, "Default (Default)", "DEFAULT"),
    ( 1, "English (English)", "en_US"),
    ( 2, "Japanese (日本語)", "ja_JP"),
    ( 3, "Dutch (Nederlandse taal)", "nl_NL"),
    ( 4, "Italian (Italiano)", "it_IT"),
    ( 5, "German (Deutsch)", "de_DE"),
    ( 6, "Finnish (Suomi)", "fi_FI"),
    ( 7, "Swedish (Svenska)", "sv_SE"),
    ( 8, "French (Français)", "fr_FR"),
    ( 9, "Spanish (Español)", "es"),
    (10, "Catalan (Català)", "ca_AD"),
    (11, "Czech (Český)", "cs_CZ"),
    (12, "Portuguese (Português)", "pt_PT"),
    (13, "Simplified Chinese (简体中文)", "zh_CN"),
    (14, "Traditional Chinese (繁體中文)", "zh_TW"),
    (15, "Russian (Русский)", "ru_RU"),
    (16, "Croatian (Hrvatski)", "hr_HR"),
    (17, "Serbian (Српски)", "sr_RS"),
    (18, "Ukrainian (Український)", "uk_UA"),
    (19, "Polish (Polski)", "pl_PL"),
    (20, "Romanian (Român)", "ro_RO"),
    # Using the utf8 flipped form of Arabic (العربية).
    (21, "Arabic (ﺔﻴﺑﺮﻌﻟﺍ)", "ar_EG"),
    (22, "Bulgarian (Български)", "bg_BG"),
    (23, "Greek (Ελληνικά)", "el_GR"),
    (24, "Korean (한국 언어)", "ko_KR"),
    (25, "Nepali (नेपाली)", "ne_NP"),
    # Using the utf8 flipped form of Persian (فارسی).
    (26, "Persian (ﯽﺳﺭﺎﻓ)", "fa_IR"),
    (27, "Indonesian (Bahasa indonesia)", "id_ID"),
    (28, "Serbian Latin (Srpski latinica)", "sr_RS@latin"),
    (29, "Kyrgyz (Кыргыз тили)", "ky_KG"),
    (30, "Turkish (Türkçe)", "tr_TR"),
    (31, "Hungarian (Magyar)", "hu_HU"),
    (32, "Brazilian Portuguese (Português do Brasil)", "pt_BR"),
    # Using the utf8 flipped form of Hebrew (עִבְרִית)).
    (33, "Hebrew (תירִבְעִ)", "he_IL"),
    (34, "Estonian (Eestlane)", "et_EE"),
    (35, "Esperanto (Esperanto)", "eo"),
    (36, "Spanish from Spain (Español de España)", "es_ES"),
    (37, "Amharic (አማርኛ)", "am_ET"),
    (38, "Uzbek (Oʻzbek)", "uz_UZ"),
    (39, "Uzbek Cyrillic (Ўзбек)", "uz_UZ@cyrillic"),
    (40, "Hindi (मानक हिन्दी)", "hi_IN"),
)

# Default context, in py!
DEFAULT_CONTEXT = bpy.app.translations.contexts.default

# Name of language file used by Blender to generate translations' menu.
LANGUAGES_FILE = "languages"

# The min level of completeness for a po file to be imported from /branches into /trunk, as a percentage.
IMPORT_MIN_LEVEL = 0.0

# Languages in /branches we do not want to import in /trunk currently...
IMPORT_LANGUAGES_SKIP = {
    'am_ET', 'bg_BG', 'fi_FI', 'el_GR', 'et_EE', 'ne_NP', 'pl_PL', 'ro_RO', 'uz_UZ', 'uz_UZ@cyrillic',
}

# Languages that need RTL pre-processing.
IMPORT_LANGUAGES_RTL = {
    'ar_EG', 'fa_IR', 'he_IL',
}

# The comment prefix used in generated messages.txt file.
MSG_COMMENT_PREFIX = "#~ "

# The comment prefix used in generated messages.txt file.
MSG_CONTEXT_PREFIX = "MSGCTXT:"

# The default comment prefix used in po's.
PO_COMMENT_PREFIX= "# "

# The comment prefix used to mark sources of msgids, in po's.
PO_COMMENT_PREFIX_SOURCE = "#: "

# The comment prefix used to mark sources of msgids, in po's.
PO_COMMENT_PREFIX_SOURCE_CUSTOM = "#. :src: "

# The general "generated" comment prefix, in po's.
PO_COMMENT_PREFIX_GENERATED = "#. "

# The comment prefix used to comment entries in po's.
PO_COMMENT_PREFIX_MSG= "#~ "

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
    "# Copyright (C) {year} The Blender Foundation.\n"
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
PYGETTEXT_ALLOWED_EXTS = {".c", ".cpp", ".cxx", ".hpp", ".hxx", ".h"}

# Max number of contexts into a BLF_I18N_MSGID_MULTI_CTXT macro...
PYGETTEXT_MAX_MULTI_CTXT = 16

# Where to search contexts definitions, relative to SOURCE_DIR (defined below).
PYGETTEXT_CONTEXTS_DEFSRC = os.path.join("source", "blender", "blenfont", "BLF_translation.h")

# Regex to extract contexts defined in BLF_translation.h
# XXX Not full-proof, but should be enough here!
PYGETTEXT_CONTEXTS = "#define\\s+(BLF_I18NCONTEXT_[A-Z_0-9]+)\\s+\"([^\"]*)\""

# Keywords' regex.
# XXX Most unfortunately, we can't use named backreferences inside character sets,
#     which makes the regexes even more twisty... :/
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
        ")+.)"  # Don't forget the last char!
    "(?P={_}2)"  # And closing quote.
)
str_clean_re = _str_base.format(_="g", capt="P<clean>")
_inbetween_str_re = (
    # XXX Strings may have comments between their pieces too, not only spaces!
    r"(?:\s*(?:"
        # A C comment
        r"/\*.*(?!\*/).\*/|"
        # Or a C++ one!
        r"//[^\n]*\n"
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
_ctxt_re_gen = lambda uid : r"(?P<ctxt_raw{uid}>(?:".format(uid=uid) + \
                            _str_whole_re.format(_="_ctxt{uid}".format(uid=uid)) + \
                            r")|(?:[A-Z_0-9]+))"
_ctxt_re = _ctxt_re_gen("")
_msg_re = r"(?P<msg_raw>" + _str_whole_re.format(_="_msg") + r")"
PYGETTEXT_KEYWORDS = (() +
    tuple((r"{}\(\s*" + _msg_re + r"\s*\)").format(it)
          for it in ("IFACE_", "TIP_", "DATA_", "N_")) +

    tuple((r"{}\(\s*" + _ctxt_re + r"\s*,\s*" + _msg_re + r"\s*\)").format(it)
          for it in ("CTX_IFACE_", "CTX_TIP_", "CTX_DATA_", "CTX_N_")) +

    tuple(("{}\\((?:[^\"',]+,){{1,2}}\\s*" + _msg_re + r"\s*(?:\)|,)").format(it)
          for it in ("BKE_report", "BKE_reportf", "BKE_reports_prepend", "BKE_reports_prependf",
                     "CTX_wm_operator_poll_msg_set")) +

    tuple(("{}\\((?:[^\"',]+,){{3}}\\s*" + _msg_re + r"\s*\)").format(it)
          for it in ("BMO_error_raise",)) +

    tuple(("{}\\((?:[^\"',]+,)\\s*" + _msg_re + r"\s*(?:\)|,)").format(it)
          for it in ("modifier_setError",)) +

    tuple((r"{}\(\s*" + _msg_re + r"\s*,\s*(?:" +
           r"\s*,\s*)?(?:".join(_ctxt_re_gen(i) for i in range(PYGETTEXT_MAX_MULTI_CTXT)) + r")?\s*\)").format(it)
          for it in ("BLF_I18N_MSGID_MULTI_CTXT",))
)

# Check printf mismatches between msgid and msgstr.
CHECK_PRINTF_FORMAT = (
    r"(?!<%)(?:%%)*%"          # Begining, with handling for crazy things like '%%%%%s'
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
    "ascii",
    "author",                        # Addons' field. :/
    "bItasc",
    "description",                   # Addons' field. :/
    "dx",
    "fBM",
    "flac",
    "fps: %.2f",
    "fps: %i",
    "fStop",
    "gimbal",
    "global",
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
    "p0",
    "px",
    "re",
    "res",
    "rv",
    "sin(x) / x",
    "sqrt(x*x+y*y+z*z)",
    "sRGB",
    "utf-8",
    "var",
    "vBVH",
    "view",
    "wav",
    "y",
    # Sub-strings.
    "available with",
    "can't save image while rendering",
    "expected a timeline/animation area to be active",
    "expected a view3d region",
    "expected a view3d region & editcurve",
    "expected a view3d region & editmesh",
    "image file not found",
    "image path can't be written to",
    "in memory to enable editing!",
    "unable to load movie clip",
    "unable to load text",
    "unable to open the file",
    "unknown error reading file",
    "unknown error stating file",
    "unknown error writing file",
    "unsupported font format",
    "unsupported format",
    "unsupported image format",
    "unsupported movie clip format",
    "verts only",
    "virtual parents",
}
WARN_MSGID_NOT_CAPITALIZED_ALLOWED |= set(lng[2] for lng in LANGUAGES)

WARN_MSGID_END_POINT_ALLOWED = {
    "Numpad .",
    "Circle|Alt .",
    "Temp. Diff.",
    "Float Neg. Exp.",
    "    RNA Path: bpy.types.",
    "Max Ext.",
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

# The FriBidi C compiled library (.so under Linux, .dll under windows...).
# You’ll likely have to edit it in your user_settings.py if you’re under Windows., e.g. using the included one:
#     FRIBIDI_LIB = os.path.join(TOOLS_DIR, "libfribidi.dll")
FRIBIDI_LIB = "libfribidi.so.0"

# The name of the (currently empty) file that must be present in a po's directory to enable rtl-preprocess.
RTL_PREPROCESS_FILE = "is_rtl"

# The Blender source root path.
# This is just an example, you’ll have to override it in your user_settings.py!
SOURCE_DIR = os.path.abspath(os.path.join("blender"))

# The bf-translation repository (you'll have to override this in your user_settings.py).
I18N_DIR = os.path.abspath(os.path.join("i18n"))

# The /branches path (relative to I18N_DIR).
REL_BRANCHES_DIR = os.path.join("branches")

# The /trunk path (relative to I18N_DIR).
REL_TRUNK_DIR = os.path.join("trunk")

# The /trunk/po path (relative to I18N_DIR).
REL_TRUNK_PO_DIR = os.path.join(REL_TRUNK_DIR, "po")

# The /trunk/mo path (relative to I18N_DIR).
REL_TRUNK_MO_DIR = os.path.join(REL_TRUNK_DIR, "locale")

# The Blender source path to check for i18n macros (relative to SOURCE_DIR).
REL_POTFILES_SOURCE_DIR = os.path.join("source")

# The template messages file (relative to I18N_DIR).
REL_FILE_NAME_POT = os.path.join(REL_BRANCHES_DIR, DOMAIN + ".pot")

# Mo root datapath.
REL_MO_PATH_ROOT = os.path.join(REL_TRUNK_DIR, "locale")

# Mo path generator for a given language.
REL_MO_PATH_TEMPLATE = os.path.join(REL_MO_PATH_ROOT, "{}", "LC_MESSAGES")

# Mo path generator for a given language (relative to any "locale" dir).
MO_PATH_ROOT_RELATIVE = os.path.join("locale")
MO_PATH_TEMPLATE_RELATIVE = os.path.join(MO_PATH_ROOT_RELATIVE, "{}", "LC_MESSAGES")

# Mo file name.
MO_FILE_NAME = DOMAIN + ".mo"

# Where to search for py files that may contain ui strings (relative to one of the 'resource_path' of Blender).
CUSTOM_PY_UI_FILES = [
    os.path.join("scripts", "startup", "bl_ui"),
    os.path.join("scripts", "modules", "rna_prop_ui.py"),
]

# An optional text file listing files to force include/exclude from py_xgettext process.
SRC_POTFILES = ""

# A cache storing validated msgids, to avoid re-spellchecking them.
SPELL_CACHE = os.path.join("/tmp", ".spell_cache")

# Threshold defining whether a new msgid is similar enough with an old one to reuse its translation...
SIMILAR_MSGID_THRESHOLD = 0.75

# Additional import paths to add to sys.path (';' separated)...
INTERN_PY_SYS_PATHS = ""

# Custom override settings must be one dir above i18n tools itself!
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

def _gen_get_set_paths(ref, name):
    def _get(self):
        return [_do_get(getattr(self, ref), p) for p in getattr(self, name)]
    def _set(self, value):
        setattr(self, name, [_do_set(getattr(self, ref), p) for p in value])
    return _get, _set

class I18nSettings:
    """
    Class allowing persistence of our settings!
    Saved in JSon format, so settings should be JSon'able objects!
    """
    _settings = None

    def __new__(cls, *args, **kwargs):
        # Addon preferences are singleton by definition, so is this class!
        if not I18nSettings._settings:
            cls._settings = super(I18nSettings, cls).__new__(cls)
            cls._settings.__dict__ = {uid: data for uid, data in globals().items() if not uid.startswith("_")}
        return I18nSettings._settings

    def from_json(self, string):
        data = dict(json.loads(string))
        # Special case... :/
        if "INTERN_PY_SYS_PATHS" in data:
            self.PY_SYS_PATHS = data["INTERN_PY_SYS_PATHS"]
        self.__dict__.update(data)

    def to_json(self):
        # Only save the diff from default i18n_settings!
        glob = globals()
        export_dict = {uid: val for uid, val in self.__dict__.items() if glob.get(uid) != val}
        return json.dumps(export_dict)

    def load(self, fname, reset=False):
        if reset:
            self.__dict__ = {uid: data for uid, data in globals().items() if not uid.startswith("_")}
        if isinstance(fname, str):
            if not os.path.isfile(fname):
                return
            with open(fname) as f:
                self.from_json(f.read())
        # Else assume fname is already a file(like) object!
        else:
            self.from_json(fname.read())

    def save(self, fname):
        if isinstance(fname, str):
            with open(fname, 'w') as f:
                f.write(self.to_json())
        # Else assume fname is already a file(like) object!
        else:
            fname.write(self.to_json())

    BRANCHES_DIR = property(*(_gen_get_set_path("I18N_DIR", "REL_BRANCHES_DIR")))
    TRUNK_DIR = property(*(_gen_get_set_path("I18N_DIR", "REL_TRUNK_DIR")))
    TRUNK_PO_DIR = property(*(_gen_get_set_path("I18N_DIR", "REL_TRUNK_PO_DIR")))
    TRUNK_MO_DIR = property(*(_gen_get_set_path("I18N_DIR", "REL_TRUNK_MO_DIR")))
    POTFILES_SOURCE_DIR = property(*(_gen_get_set_path("SOURCE_DIR", "REL_POTFILES_SOURCE_DIR")))
    FILE_NAME_POT = property(*(_gen_get_set_path("I18N_DIR", "REL_FILE_NAME_POT")))
    MO_PATH_ROOT = property(*(_gen_get_set_path("I18N_DIR", "REL_MO_PATH_ROOT")))
    MO_PATH_TEMPLATE = property(*(_gen_get_set_path("I18N_DIR", "REL_MO_PATH_TEMPLATE")))

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
