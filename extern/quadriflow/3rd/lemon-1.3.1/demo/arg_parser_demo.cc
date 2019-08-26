/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2010
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

///\ingroup demos
///\file
///\brief Argument parser demo
///
/// This example shows how the argument parser can be used.
///
/// \include arg_parser_demo.cc

#include <lemon/arg_parser.h>

using namespace lemon;
int main(int argc, char **argv)
{
  // Initialize the argument parser
  ArgParser ap(argc, argv);
  int i;
  std::string s;
  double d = 1.0;
  bool b, nh;
  bool g1, g2, g3;

  // Add a mandatory integer option with storage reference
  ap.refOption("n", "An integer input.", i, true);
  // Add a double option with storage reference (the default value is 1.0)
  ap.refOption("val", "A double input.", d);
  // Add a double option without storage reference (the default value is 3.14)
  ap.doubleOption("val2", "A double input.", 3.14);
  // Set synonym for -val option
  ap.synonym("vals", "val");
  // Add a string option
  ap.refOption("name", "A string input.", s);
  // Add bool options
  ap.refOption("f", "A switch.", b)
    .refOption("nohelp", "", nh)
    .refOption("gra", "Choice A", g1)
    .refOption("grb", "Choice B", g2)
    .refOption("grc", "Choice C", g3);
  // Bundle -gr* options into a group
  ap.optionGroup("gr", "gra")
    .optionGroup("gr", "grb")
    .optionGroup("gr", "grc");
  // Set the group mandatory
  ap.mandatoryGroup("gr");
  // Set the options of the group exclusive (only one option can be given)
  ap.onlyOneGroup("gr");
  // Add non-parsed arguments (e.g. input files)
  ap.other("infile", "The input file.")
    .other("...");

  // Throw an exception when problems occurs. The default behavior is to
  // exit(1) on these cases, but this makes Valgrind falsely warn
  // about memory leaks.
  ap.throwOnProblems();

  // Perform the parsing process
  // (in case of any error it terminates the program)
  // The try {} construct is necessary only if the ap.trowOnProblems()
  // setting is in use.
  try {
    ap.parse();
  } catch (ArgParserException &) { return 1; }

  // Check each option if it has been given and print its value
  std::cout << "Parameters of '" << ap.commandName() << "':\n";

  std::cout << "  Value of -n: " << i << std::endl;
  if(ap.given("val")) std::cout << "  Value of -val: " << d << std::endl;
  if(ap.given("val2")) {
    d = ap["val2"];
    std::cout << "  Value of -val2: " << d << std::endl;
  }
  if(ap.given("name")) std::cout << "  Value of -name: " << s << std::endl;
  if(ap.given("f")) std::cout << "  -f is given\n";
  if(ap.given("nohelp")) std::cout << "  Value of -nohelp: " << nh << std::endl;
  if(ap.given("gra")) std::cout << "  -gra is given\n";
  if(ap.given("grb")) std::cout << "  -grb is given\n";
  if(ap.given("grc")) std::cout << "  -grc is given\n";

  switch(ap.files().size()) {
  case 0:
    std::cout << "  No file argument was given.\n";
    break;
  case 1:
    std::cout << "  1 file argument was given. It is:\n";
    break;
  default:
    std::cout << "  "
              << ap.files().size() << " file arguments were given. They are:\n";
  }
  for(unsigned int i=0;i<ap.files().size();++i)
    std::cout << "    '" << ap.files()[i] << "'\n";

  return 0;
}
