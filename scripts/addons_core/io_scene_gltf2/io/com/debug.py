# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

#
# Imports
#

import time
import logging
import logging.handlers

#
# Globals
#

g_profile_started = False
g_profile_start = 0.0
g_profile_end = 0.0
g_profile_delta = 0.0


def get_timestamp():
    current_time = time.gmtime()
    return time.strftime("%H:%M:%S", current_time)


def profile_start():
    """Start profiling by storing the current time."""
    global g_profile_start
    global g_profile_started

    if g_profile_started:
        print('ERROR', 'Profiling already started')
        return

    g_profile_started = True

    g_profile_start = time.time()


def profile_end(label=None):
    """Stop profiling and printing out the delta time since profile start."""
    global g_profile_end
    global g_profile_delta
    global g_profile_started

    if not g_profile_started:
        print('ERROR', 'Profiling not started')
        return

    g_profile_started = False

    g_profile_end = time.time()
    g_profile_delta = g_profile_end - g_profile_start

    output = 'Delta time: ' + str(g_profile_delta)

    if label is not None:
        output = output + ' (' + label + ')'

    print('PROFILE', output)


class Log:
    def __init__(self, loglevel):
        self.logger = logging.getLogger('glTFImporter')

        # For console display
        self.console_handler = logging.StreamHandler()
        formatter = logging.Formatter('%(asctime)s | %(levelname)s: %(message)s', "%H:%M:%S")
        self.console_handler.setFormatter(formatter)

        # For popup display
        self.popup_handler = logging.handlers.MemoryHandler(1024 * 10)

        self.logger.addHandler(self.console_handler)
        # self.logger.addHandler(self.popup_handler) => Make sure to not attach the popup handler to the logger

        self.logger.setLevel(int(loglevel))

    def error(self, message, popup=False):
        self.logger.error(message)
        if popup:
            self.popup_handler.buffer.append(('ERROR', message))

    def warning(self, message, popup=False):
        self.logger.warning(message)
        if popup:
            self.popup_handler.buffer.append(('WARNING', message))

    def info(self, message, popup=False):
        self.logger.info(message)
        if popup:
            self.popup_handler.buffer.append(('INFO', message))

    def debug(self, message, popup=False):
        self.logger.debug(message)
        if popup:
            self.popup_handler.buffer.append(('DEBUG', message))

    def critical(self, message, popup=False):
        self.logger.critical(message)
        if popup:
            # There is no Critical level in Blender, so we use error
            self.popup_handler.buffer.append(('ERROR', message))

    def profile(self, message, popup=False):  # There is no profile level in logging, so we use info
        self.logger.info(message)
        if popup:
            self.popup_handler.buffer.append(('PROFILE', message))

    def messages(self):
        return self.popup_handler.buffer

    def flush(self):
        self.logger.removeHandler(self.console_handler)
        self.popup_handler.flush()
        self.logger.removeHandler(self.popup_handler)
