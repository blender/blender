/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#pragma once

namespace blender::gpu::shader::parser {

enum TokenType : char {
  Invalid = 0,
  /* Use ascii chars to store them in string, and for easy debugging / testing. */
  Word = 'w',
  NewLine = '\n',
  Space = ' ',
  Dot = '.',
  Hash = '#',
  Ampersand = '&',
  Number = '0',
  String = '_',
  ParOpen = '(',
  ParClose = ')',
  BracketOpen = '{',
  BracketClose = '}',
  SquareOpen = '[',
  SquareClose = ']',
  AngleOpen = '<',
  AngleClose = '>',
  Assign = '=',
  SemiColon = ';',
  Question = '?',
  Not = '!',
  Colon = ':',
  Comma = ',',
  Star = '*',
  Plus = '+',
  Minus = '-',
  Divide = '/',
  Tilde = '~',
  Caret = '^',
  Pipe = '|',
  Percent = '%',
  Backslash = '\\',
  /* Keywords */
  Break = 'b',
  Const = 'c',
  Constexpr = 'C',
  Decrement = 'D',
  Deref = 'D',
  Do = 'd',
  Equal = 'E',
  NotEqual = 'e',
  For = 'f',
  While = 'F',
  GEqual = 'G',
  Case = 'H',
  Switch = 'h',
  Else = 'I',
  If = 'i',
  LEqual = 'L',
  Enum = 'M',
  Static = 'm',
  Namespace = 'n',
  PreprocessorNewline = 'N',
  Continue = 'O',
  Increment = 'P',
  Return = 'r',
  Class = 'S',
  Struct = 's',
  Template = 't',
  This = 'T',
  Using = 'u',
  Private = 'v',
  Public = 'V',
  Inline = 'l',
  Union = 'o',
  LogicalAnd = 'a',
  LogicalOr = 'g',
  /* Aliases. */
  Multiply = Star,
  And = Ampersand,
  Or = Pipe,
  Xor = Caret,
  GThan = AngleClose,
  LThan = AngleOpen,
  BitwiseNot = Tilde,
  Modulo = Percent,
};

enum class ScopeType : char {
  Invalid = 0,
  /* Use ascii chars to store them in string, and for easy debugging / testing. */
  Global = 'G',
  Namespace = 'N',
  Struct = 'S',
  Function = 'F',
  LoopArgs = 'l',
  LoopBody = 'p',
  SwitchArg = 'w',
  SwitchBody = 'W',
  FunctionArgs = 'f',
  FunctionCall = 'c',
  Template = 'T',
  TemplateArg = 't',
  Subscript = 'A',
  Preprocessor = 'P',
  Assignment = 'a',
  Attributes = 'B',
  Attribute = 'b',
  /* Added scope inside function body. */
  Local = 'L',
  /* Added scope inside FunctionArgs. */
  FunctionArg = 'g',
  /* Added scope inside FunctionCall. */
  FunctionParam = 'm',
  /* Added scope inside LoopArgs. */
  LoopArg = 'r',
};

}  // namespace blender::gpu::shader::parser
