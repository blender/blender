/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Generic CLI "--command" declarations.
 *
 * Duplicate Commands
 * ==================
 *
 * When two or more commands share the same identifier, a warning is printed and both are disabled.
 *
 * This is done because command-line actions may be destructive so the down-side of running the
 * wrong command could be severe. The reason this is not considered an error is we can't prevent
 * it so easily, unlike operator ID's which may be longer, commands are typically short terms
 * which wont necessarily include an add-ons identifier as a prefix for e.g.
 * Further, an error would break loading add-ons who's primary is *not*
 * necessarily to provide command-line access.
 * An alternative solution could be to generate unique names (number them for example)
 * but this isn't reliable as it would depend on it the order add-ons are loaded which
 * isn't under user control.
 */

#include <iostream>

#include "BLI_vector.hh"

#include "BKE_blender_cli_command.hh" /* own include */

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Internal API
 * \{ */

using CommandHandlerPtr = std::unique_ptr<CommandHandler>;

/**
 * All registered command handlers.
 * \note the order doesn't matter as duplicates are detected and prevented from running.
 */
blender::Vector<CommandHandlerPtr> g_command_handlers;

static CommandHandler *blender_cli_command_lookup(const std::string &id)
{
  for (CommandHandlerPtr &cmd_iter : g_command_handlers) {
    if (id == cmd_iter->id) {
      return cmd_iter.get();
    }
  }
  return nullptr;
}

static int blender_cli_command_index(const CommandHandler *cmd)
{
  int index = 0;
  for (CommandHandlerPtr &cmd_iter : g_command_handlers) {
    if (cmd_iter.get() == cmd) {
      return index;
    }
    index++;
  }
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void BKE_blender_cli_command_register(std::unique_ptr<CommandHandler> cmd)
{
  bool is_duplicate = false;
  if (CommandHandler *cmd_exists = blender_cli_command_lookup(cmd->id)) {
    std::cerr << "warning: registered duplicate command \"" << cmd->id
              << "\", this will be inaccessible" << std::endl;
    cmd_exists->is_duplicate = true;
    is_duplicate = true;
  }
  cmd->is_duplicate = is_duplicate;
  g_command_handlers.append(std::move(cmd));
}

bool BKE_blender_cli_command_unregister(CommandHandler *cmd)
{
  const int cmd_index = blender_cli_command_index(cmd);
  if (cmd_index == -1) {
    std::cerr << "failed to unregister command handler" << std::endl;
    return false;
  }

  /* Update duplicates after removal. */
  if (cmd->is_duplicate) {
    CommandHandler *cmd_other = nullptr;
    for (CommandHandlerPtr &cmd_iter : g_command_handlers) {
      /* Skip self. */
      if (cmd == cmd_iter.get()) {
        continue;
      }
      if (cmd_iter->is_duplicate && (cmd_iter->id == cmd->id)) {
        if (cmd_other) {
          /* Two or more found, clear and break. */
          cmd_other = nullptr;
          break;
        }
        cmd_other = cmd_iter.get();
      }
    }
    if (cmd_other) {
      cmd_other->is_duplicate = false;
    }
  }

  g_command_handlers.remove_and_reorder(cmd_index);

  return true;
}

int BKE_blender_cli_command_exec(bContext *C, const char *id, const int argc, const char **argv)
{
  CommandHandler *cmd = blender_cli_command_lookup(id);
  if (cmd == nullptr) {
    std::cerr << "Unrecognized command: \"" << id << "\"" << std::endl;
    return EXIT_FAILURE;
  }
  if (cmd->is_duplicate) {
    std::cerr << "Command: \"" << id
              << "\" was registered multiple times, must be resolved, aborting!" << std::endl;
    return EXIT_FAILURE;
  }

  return cmd->exec(C, argc, argv);
}

void BKE_blender_cli_command_print_help()
{
  /* As this is isn't ordered sorting in-place is acceptable,
   * sort alphabetically for display purposes only. */
  std::sort(g_command_handlers.begin(),
            g_command_handlers.end(),
            [](const CommandHandlerPtr &a, const CommandHandlerPtr &b) { return a->id < b->id; });

  for (int pass = 0; pass < 2; pass++) {
    std::cout << ((pass == 0) ? "Blender Command Listing:" :
                                "Duplicate Command Listing (ignored):")
              << std::endl;

    const bool is_duplicate = pass > 0;
    bool found = false;
    bool has_duplicate = false;
    for (CommandHandlerPtr &cmd_iter : g_command_handlers) {
      if (cmd_iter->is_duplicate) {
        has_duplicate = true;
      }
      if (cmd_iter->is_duplicate != is_duplicate) {
        continue;
      }

      std::cout << "\t" << cmd_iter->id << std::endl;
      found = true;
    }

    if (!found) {
      std::cout << "\tNone found" << std::endl;
    }
    /* Don't print that no duplicates are found as it's not helpful. */
    if (pass == 0 && !has_duplicate) {
      break;
    }
  }
}

void BKE_blender_cli_command_free_all()
{
  g_command_handlers.clear();
}

/** \} */
