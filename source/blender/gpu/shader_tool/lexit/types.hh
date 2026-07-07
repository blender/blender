/* SPDX-FileCopyrightText: 2026 Clement Foucault
 *
 * SPDX-License-Identifier: MIT */

#pragma once

#include <cstdint>

namespace lexit {

/**
 * Class for each characters inside the ASCII table.
 *
 * The tokenizer identifies runs of characters with similar classes.
 * A character is grouped with its predecessor if it shares a class.
 * The Separator class is the exception which never group chars together.
 *
 * Note: The values were chosen to allow fast comparison, masking, and cast to printable TokenType.
 */
enum class CharClass : uint8_t {
  /* Will decay into single char token. */
  None = 0,
  /* Will decay into single char of the token. */
  Separator = (1 << 1),
  /* Will decay into the first char of the token. */
  MultiTok = (1 << 2),
  WhiteSpace = (1 << 3),
  /* Will decay into Word. Can start an identifier. */
  Alpha = 'A', /* 0b01000001 */
  /* Will decay into Number. Can continue an identifier. */
  Numeric = '1', /* 0b00110001 */

  /* These classes will merge characters together. */
  CanMerge = Alpha | Numeric | MultiTok | WhiteSpace,
  /* Classes above this value will cast to TokenType instead of using the character. */
  ClassToTypeThreshold = Numeric - 1,
};

/* Make sure to declare this enum as being a char.
 * This is allow casting to string possible. */
enum TokenType : uint8_t {
  Invalid = 0,
  Word = TokenType(CharClass::Alpha),
  Number = TokenType(CharClass::Numeric),
  /* Use printable ascii chars to store them in string, and for easy debugging / testing. */
  NewLine = '\n',
  Space = ' ',
  Dot = '.',
  Hash = '#',
  Ampersand = '&',
  DoubleQuote = '"',
  SingleQuote = '\'',
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
  /* Mark end of stream. */
  EndOfFile = '\0',

  /* --- Keywords --- */

  LogicalAnd = 'a',
  DoubleHash = 'A',
  Break = 'b',
  // Unused = 'B',
  Const = 'c',
  Constexpr = 'C',
  Do = 'd',
  Decrement = 'D',
  NotEqual = 'e',
  Equal = 'E',
  For = 'f',
  While = 'F',
  LogicalOr = 'g',
  GEqual = 'G',
  Switch = 'h',
  Case = 'H',
  If = 'i',
  Else = 'I',
  // Unused = 'j',
  // Unused = 'J',
  // Unused = 'k',
  // Unused = 'K',
  Inline = 'l',
  LEqual = 'L',
  Static = 'm',
  Enum = 'M',
  Namespace = 'n',
  PreprocessorNewline = 'N', /* TODO(fclem): Remove. */
  Union = 'o',
  Continue = 'O',
  // Unused = 'p',
  Increment = 'P',
  // Unused = 'q',
  // Unused = 'Q',
  Return = 'r',
  // Unused = 'R',
  Struct = 's',
  Class = 'S',
  Template = 't',
  This = 'T',
  Using = 'u',
  // Unused = 'U',
  Private = 'v',
  Public = 'V',
  // Word = 'w',
  // Unused = 'W',
  // Unused = 'x',
  // Unused = 'X',
  // Unused = 'y',
  // Unused = 'Y',
  // Unused = 'z',
  // Unused = 'Z',
  // Number = '0',
  // Unused = '1',
  // Unused = '2',
  // Unused = '3',
  // Unused = '4',
  // Unused = '5',
  // Unused = '6',
  // Unused = '7',
  // Unused = '8',
  // Unused = '9',

  /* Aliases. */
  Multiply = Star,
  And = Ampersand,
  Or = Pipe,
  Xor = Caret,
  GThan = AngleClose,
  LThan = AngleOpen,
  BitwiseNot = Tilde,
  Modulo = Percent,

  String = DoubleQuote,
};

}  // namespace lexit
