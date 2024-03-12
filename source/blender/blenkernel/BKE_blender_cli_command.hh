/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Blender CLI Generic `--command` Support.
 *
 * \note all registered commands must print help to the STDOUT & exit with a zero exit-code
 * when `--help` is passed in as the first argument to a command.
 */

#include "BLI_utility_mixins.hh"

#include <memory>
#include <string>

/**
 * Each instance of this class can run the command with an argument list.
 * The arguments begin at the first argument after the command identifier.
 */
class CommandHandler : blender::NonCopyable, blender::NonMovable {
 public:
  CommandHandler(const std::string &id) : id(id) {}
  virtual ~CommandHandler() = default;

  /** Matched against `--command {id}`. */
  const std::string id;

  /**
   * The main execution function.
   * The return value is used as the commands exit-code.
   */
  virtual int exec(struct bContext *C, int argc, const char **argv) = 0;

  /** True when one or more registered commands share an ID. */
  bool is_duplicate = false;
};
/**
 * \param cmd: The memory for a command type (ownership is transferred).
 */
void BKE_blender_cli_command_register(std::unique_ptr<CommandHandler> cmd);

/**
 * Unregister a previously registered command.
 */
bool BKE_blender_cli_command_unregister(CommandHandler *cmd);

/**
 * Run the command by `id`, passing in the argument list & context.
 * The argument list must begin after the command identifier.
 */
int BKE_blender_cli_command_exec(struct bContext *C,
                                 const char *id,
                                 const int argc,
                                 const char **argv);

/**
 * Print all known commands (used for passing `--command help` in the command-line).
 */
void BKE_blender_cli_command_print_help();
/**
 * Frees all commands (using their #CommandFreeFn call-backs).
 */
void BKE_blender_cli_command_free_all();
