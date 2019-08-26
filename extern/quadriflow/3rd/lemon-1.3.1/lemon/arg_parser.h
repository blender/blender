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

#ifndef LEMON_ARG_PARSER_H
#define LEMON_ARG_PARSER_H

#include <vector>
#include <map>
#include <list>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <lemon/assert.h>

///\ingroup misc
///\file
///\brief A tool to parse command line arguments.

namespace lemon {

  ///Exception used by ArgParser

  ///Exception used by ArgParser.
  ///
  class ArgParserException : public Exception {
  public:
    /// Reasons for failure

    /// Reasons for failure.
    ///
    enum Reason {
      HELP,         ///< <tt>--help</tt> option was given.
      UNKNOWN_OPT,  ///< Unknown option was given.
      INVALID_OPT   ///< Invalid combination of options.
    };

  private:
    Reason _reason;

  public:
    ///Constructor
    ArgParserException(Reason r) throw() : _reason(r) {}
    ///Virtual destructor
    virtual ~ArgParserException() throw() {}
    ///A short description of the exception
    virtual const char* what() const throw() {
      switch(_reason)
        {
        case HELP:
          return "lemon::ArgParseException: ask for help";
          break;
        case UNKNOWN_OPT:
          return "lemon::ArgParseException: unknown option";
          break;
        case INVALID_OPT:
          return "lemon::ArgParseException: invalid combination of options";
          break;
        }
      return "";
    }
    ///Return the reason for the failure
    Reason reason() const {return _reason; }
  };


  ///Command line arguments parser

  ///\ingroup misc
  ///Command line arguments parser.
  ///
  ///For a complete example see the \ref arg_parser_demo.cc demo file.
  class ArgParser {

    static void _showHelp(void *p);
  protected:

    int _argc;
    const char * const *_argv;

    enum OptType { UNKNOWN=0, BOOL=1, STRING=2, DOUBLE=3, INTEGER=4, FUNC=5 };

    class ParData {
    public:
      union {
        bool *bool_p;
        int *int_p;
        double *double_p;
        std::string *string_p;
        struct {
          void (*p)(void *);
          void *data;
        } func_p;

      };
      std::string help;
      bool mandatory;
      OptType type;
      bool set;
      bool ingroup;
      bool has_syn;
      bool syn;
      bool self_delete;
      ParData() : mandatory(false), type(UNKNOWN), set(false), ingroup(false),
                  has_syn(false), syn(false), self_delete(false) {}
    };

    typedef std::map<std::string,ParData> Opts;
    Opts _opts;

    class GroupData
    {
    public:
      typedef std::list<std::string> Opts;
      Opts opts;
      bool only_one;
      bool mandatory;
      GroupData() :only_one(false), mandatory(false) {}
    };

    typedef std::map<std::string,GroupData> Groups;
    Groups _groups;

    struct OtherArg
    {
      std::string name;
      std::string help;
      OtherArg(std::string n, std::string h) :name(n), help(h) {}

    };

    std::vector<OtherArg> _others_help;
    std::vector<std::string> _file_args;
    std::string _command_name;


  private:
    //Bind a function to an option.

    //\param name The name of the option. The leading '-' must be omitted.
    //\param help A help string.
    //\retval func The function to be called when the option is given. It
    //  must be of type "void f(void *)"
    //\param data Data to be passed to \c func
    ArgParser &funcOption(const std::string &name,
                    const std::string &help,
                    void (*func)(void *),void *data);

    bool _exit_on_problems;

    void _terminate(ArgParserException::Reason reason) const;

  public:

    ///Constructor
    ArgParser(int argc, const char * const *argv);

    ~ArgParser();

    ///\name Options
    ///

    ///@{

    ///Add a new integer type option

    ///Add a new integer type option.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param value A default value for the option.
    ///\param obl Indicate if the option is mandatory.
    ArgParser &intOption(const std::string &name,
                    const std::string &help,
                    int value=0, bool obl=false);

    ///Add a new floating point type option

    ///Add a new floating point type option.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param value A default value for the option.
    ///\param obl Indicate if the option is mandatory.
    ArgParser &doubleOption(const std::string &name,
                      const std::string &help,
                      double value=0, bool obl=false);

    ///Add a new bool type option

    ///Add a new bool type option.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param value A default value for the option.
    ///\param obl Indicate if the option is mandatory.
    ///\note A mandatory bool obtion is of very little use.
    ArgParser &boolOption(const std::string &name,
                      const std::string &help,
                      bool value=false, bool obl=false);

    ///Add a new string type option

    ///Add a new string type option.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param value A default value for the option.
    ///\param obl Indicate if the option is mandatory.
    ArgParser &stringOption(const std::string &name,
                      const std::string &help,
                      std::string value="", bool obl=false);

    ///Give help string for non-parsed arguments.

    ///With this function you can give help string for non-parsed arguments.
    ///The parameter \c name will be printed in the short usage line, while
    ///\c help gives a more detailed description.
    ArgParser &other(const std::string &name,
                     const std::string &help="");

    ///@}

    ///\name Options with External Storage
    ///Using this functions, the value of the option will be directly written
    ///into a variable once the option appears in the command line.

    ///@{

    ///Add a new integer type option with a storage reference

    ///Add a new integer type option with a storage reference.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param obl Indicate if the option is mandatory.
    ///\retval ref The value of the argument will be written to this variable.
    ArgParser &refOption(const std::string &name,
                    const std::string &help,
                    int &ref, bool obl=false);

    ///Add a new floating type option with a storage reference

    ///Add a new floating type option with a storage reference.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param obl Indicate if the option is mandatory.
    ///\retval ref The value of the argument will be written to this variable.
    ArgParser &refOption(const std::string &name,
                      const std::string &help,
                      double &ref, bool obl=false);

    ///Add a new bool type option with a storage reference

    ///Add a new bool type option with a storage reference.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param obl Indicate if the option is mandatory.
    ///\retval ref The value of the argument will be written to this variable.
    ///\note A mandatory bool obtion is of very little use.
    ArgParser &refOption(const std::string &name,
                      const std::string &help,
                      bool &ref, bool obl=false);

    ///Add a new string type option with a storage reference

    ///Add a new string type option with a storage reference.
    ///\param name The name of the option. The leading '-' must be omitted.
    ///\param help A help string.
    ///\param obl Indicate if the option is mandatory.
    ///\retval ref The value of the argument will be written to this variable.
    ArgParser &refOption(const std::string &name,
                      const std::string &help,
                      std::string &ref, bool obl=false);

    ///@}

    ///\name Option Groups and Synonyms
    ///

    ///@{

    ///Bundle some options into a group

    /// You can group some option by calling this function repeatedly for each
    /// option to be grouped with the same groupname.
    ///\param group The group name.
    ///\param opt The option name.
    ArgParser &optionGroup(const std::string &group,
                           const std::string &opt);

    ///Make the members of a group exclusive

    ///If you call this function for a group, than at most one of them can be
    ///given at the same time.
    ArgParser &onlyOneGroup(const std::string &group);

    ///Make a group mandatory

    ///Using this function, at least one of the members of \c group
    ///must be given.
    ArgParser &mandatoryGroup(const std::string &group);

    ///Create synonym to an option

    ///With this function you can create a synonym \c syn of the
    ///option \c opt.
    ArgParser &synonym(const std::string &syn,
                           const std::string &opt);

    ///@}

  private:
    void show(std::ostream &os,Opts::const_iterator i) const;
    void show(std::ostream &os,Groups::const_iterator i) const;
    void showHelp(Opts::const_iterator i) const;
    void showHelp(std::vector<OtherArg>::const_iterator i) const;

    void unknownOpt(std::string arg) const;

    void requiresValue(std::string arg, OptType t) const;
    void checkMandatories() const;

    void shortHelp() const;
    void showHelp() const;
  public:

    ///Start the parsing process
    ArgParser &parse();

    /// Synonym for parse()
    ArgParser &run()
    {
      return parse();
    }

    ///Give back the command name (the 0th argument)
    const std::string &commandName() const { return _command_name; }

    ///Check if an opion has been given to the command.
    bool given(std::string op) const
    {
      Opts::const_iterator i = _opts.find(op);
      return i!=_opts.end()?i->second.set:false;
    }


    ///Magic type for operator[]

    ///This is the type of the return value of ArgParser::operator[]().
    ///It automatically converts to \c int, \c double, \c bool or
    ///\c std::string if the type of the option matches, which is checked
    ///with an \ref LEMON_ASSERT "assertion" (i.e. it performs runtime
    ///type checking).
    class RefType
    {
      const ArgParser &_parser;
      std::string _name;
    public:
      ///\e
      RefType(const ArgParser &p,const std::string &n) :_parser(p),_name(n) {}
      ///\e
      operator bool()
      {
        Opts::const_iterator i = _parser._opts.find(_name);
        LEMON_ASSERT(i!=_parser._opts.end(),
                     std::string()+"Unkown option: '"+_name+"'");
        LEMON_ASSERT(i->second.type==ArgParser::BOOL,
                     std::string()+"'"+_name+"' is a bool option");
        return *(i->second.bool_p);
      }
      ///\e
      operator std::string()
      {
        Opts::const_iterator i = _parser._opts.find(_name);
        LEMON_ASSERT(i!=_parser._opts.end(),
                     std::string()+"Unkown option: '"+_name+"'");
        LEMON_ASSERT(i->second.type==ArgParser::STRING,
                     std::string()+"'"+_name+"' is a string option");
        return *(i->second.string_p);
      }
      ///\e
      operator double()
      {
        Opts::const_iterator i = _parser._opts.find(_name);
        LEMON_ASSERT(i!=_parser._opts.end(),
                     std::string()+"Unkown option: '"+_name+"'");
        LEMON_ASSERT(i->second.type==ArgParser::DOUBLE ||
                     i->second.type==ArgParser::INTEGER,
                     std::string()+"'"+_name+"' is a floating point option");
        return i->second.type==ArgParser::DOUBLE ?
          *(i->second.double_p) : *(i->second.int_p);
      }
      ///\e
      operator int()
      {
        Opts::const_iterator i = _parser._opts.find(_name);
        LEMON_ASSERT(i!=_parser._opts.end(),
                     std::string()+"Unkown option: '"+_name+"'");
        LEMON_ASSERT(i->second.type==ArgParser::INTEGER,
                     std::string()+"'"+_name+"' is an integer option");
        return *(i->second.int_p);
      }

    };

    ///Give back the value of an option

    ///Give back the value of an option.
    ///\sa RefType
    RefType operator[](const std::string &n) const
    {
      return RefType(*this, n);
    }

    ///Give back the non-option type arguments.

    ///Give back a reference to a vector consisting of the program arguments
    ///not starting with a '-' character.
    const std::vector<std::string> &files() const { return _file_args; }

    ///Throw instead of exit in case of problems
    void throwOnProblems()
    {
      _exit_on_problems=false;
    }
  };
}

#endif // LEMON_ARG_PARSER_H
