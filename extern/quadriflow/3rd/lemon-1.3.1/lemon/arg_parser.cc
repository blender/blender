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

#include <lemon/arg_parser.h>

namespace lemon {

  void ArgParser::_terminate(ArgParserException::Reason reason) const
  {
    if(_exit_on_problems)
      exit(1);
    else throw(ArgParserException(reason));
  }


  void ArgParser::_showHelp(void *p)
  {
    (static_cast<ArgParser*>(p))->showHelp();
    (static_cast<ArgParser*>(p))->_terminate(ArgParserException::HELP);
  }

  ArgParser::ArgParser(int argc, const char * const *argv)
    :_argc(argc), _argv(argv), _command_name(argv[0]),
    _exit_on_problems(true) {
    funcOption("-help","Print a short help message",_showHelp,this);
    synonym("help","-help");
    synonym("h","-help");
  }

  ArgParser::~ArgParser()
  {
    for(Opts::iterator i=_opts.begin();i!=_opts.end();++i)
      if(i->second.self_delete)
        switch(i->second.type) {
        case BOOL:
          delete i->second.bool_p;
          break;
        case STRING:
          delete i->second.string_p;
          break;
        case DOUBLE:
          delete i->second.double_p;
          break;
        case INTEGER:
          delete i->second.int_p;
          break;
        case UNKNOWN:
          break;
        case FUNC:
          break;
        }
  }


  ArgParser &ArgParser::intOption(const std::string &name,
                               const std::string &help,
                               int value, bool obl)
  {
    ParData p;
    p.int_p=new int(value);
    p.self_delete=true;
    p.help=help;
    p.type=INTEGER;
    p.mandatory=obl;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::doubleOption(const std::string &name,
                               const std::string &help,
                               double value, bool obl)
  {
    ParData p;
    p.double_p=new double(value);
    p.self_delete=true;
    p.help=help;
    p.type=DOUBLE;
    p.mandatory=obl;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::boolOption(const std::string &name,
                               const std::string &help,
                               bool value, bool obl)
  {
    ParData p;
    p.bool_p=new bool(value);
    p.self_delete=true;
    p.help=help;
    p.type=BOOL;
    p.mandatory=obl;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::stringOption(const std::string &name,
                               const std::string &help,
                               std::string value, bool obl)
  {
    ParData p;
    p.string_p=new std::string(value);
    p.self_delete=true;
    p.help=help;
    p.type=STRING;
    p.mandatory=obl;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::refOption(const std::string &name,
                               const std::string &help,
                               int &ref, bool obl)
  {
    ParData p;
    p.int_p=&ref;
    p.self_delete=false;
    p.help=help;
    p.type=INTEGER;
    p.mandatory=obl;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::refOption(const std::string &name,
                                  const std::string &help,
                                  double &ref, bool obl)
  {
    ParData p;
    p.double_p=&ref;
    p.self_delete=false;
    p.help=help;
    p.type=DOUBLE;
    p.mandatory=obl;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::refOption(const std::string &name,
                                  const std::string &help,
                                  bool &ref, bool obl)
  {
    ParData p;
    p.bool_p=&ref;
    p.self_delete=false;
    p.help=help;
    p.type=BOOL;
    p.mandatory=obl;
    _opts[name]=p;

    ref = false;

    return *this;
  }

  ArgParser &ArgParser::refOption(const std::string &name,
                               const std::string &help,
                               std::string &ref, bool obl)
  {
    ParData p;
    p.string_p=&ref;
    p.self_delete=false;
    p.help=help;
    p.type=STRING;
    p.mandatory=obl;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::funcOption(const std::string &name,
                               const std::string &help,
                               void (*func)(void *),void *data)
  {
    ParData p;
    p.func_p.p=func;
    p.func_p.data=data;
    p.self_delete=false;
    p.help=help;
    p.type=FUNC;
    p.mandatory=false;
    _opts[name]=p;
    return *this;
  }

  ArgParser &ArgParser::optionGroup(const std::string &group,
                                    const std::string &opt)
  {
    Opts::iterator i = _opts.find(opt);
    LEMON_ASSERT(i!=_opts.end(), "Unknown option: '"+opt+"'");
    LEMON_ASSERT(!(i->second.ingroup),
                 "Option already in option group: '"+opt+"'");
    GroupData &g=_groups[group];
    g.opts.push_back(opt);
    i->second.ingroup=true;
    return *this;
  }

  ArgParser &ArgParser::onlyOneGroup(const std::string &group)
  {
    GroupData &g=_groups[group];
    g.only_one=true;
    return *this;
  }

  ArgParser &ArgParser::synonym(const std::string &syn,
                                const std::string &opt)
  {
    Opts::iterator o = _opts.find(opt);
    Opts::iterator s = _opts.find(syn);
    LEMON_ASSERT(o!=_opts.end(), "Unknown option: '"+opt+"'");
    LEMON_ASSERT(s==_opts.end(), "Option already used: '"+syn+"'");
    ParData p;
    p.help=opt;
    p.mandatory=false;
    p.syn=true;
    _opts[syn]=p;
    o->second.has_syn=true;
    return *this;
  }

  ArgParser &ArgParser::mandatoryGroup(const std::string &group)
  {
    GroupData &g=_groups[group];
    g.mandatory=true;
    return *this;
  }

  ArgParser &ArgParser::other(const std::string &name,
                              const std::string &help)
  {
    _others_help.push_back(OtherArg(name,help));
    return *this;
  }

  void ArgParser::show(std::ostream &os,Opts::const_iterator i) const
  {
    os << "-" << i->first;
    if(i->second.has_syn)
      for(Opts::const_iterator j=_opts.begin();j!=_opts.end();++j)
        if(j->second.syn&&j->second.help==i->first)
          os << "|-" << j->first;
    switch(i->second.type) {
    case STRING:
      os << " str";
      break;
    case INTEGER:
      os << " int";
      break;
    case DOUBLE:
      os << " num";
      break;
    default:
      break;
    }
  }

  void ArgParser::show(std::ostream &os,Groups::const_iterator i) const
  {
    GroupData::Opts::const_iterator o=i->second.opts.begin();
    while(o!=i->second.opts.end()) {
      show(os,_opts.find(*o));
      ++o;
      if(o!=i->second.opts.end()) os<<'|';
    }
  }

  void ArgParser::showHelp(Opts::const_iterator i) const
  {
    if(i->second.help.size()==0||i->second.syn) return;
    std::cerr << "  ";
    show(std::cerr,i);
    std::cerr << std::endl;
    std::cerr << "     " << i->second.help << std::endl;
  }
  void ArgParser::showHelp(std::vector<ArgParser::OtherArg>::const_iterator i)
    const
  {
    if(i->help.size()==0) return;
    std::cerr << "  " << i->name << std::endl
              << "     " << i->help << std::endl;
  }

  void ArgParser::shortHelp() const
  {
    const unsigned int LINE_LEN=77;
    const std::string indent("    ");
    std::cerr << "Usage:\n  " << _command_name;
    int pos=_command_name.size()+2;
    for(Groups::const_iterator g=_groups.begin();g!=_groups.end();++g) {
      std::ostringstream cstr;
      cstr << ' ';
      if(!g->second.mandatory) cstr << '[';
      show(cstr,g);
      if(!g->second.mandatory) cstr << ']';
      if(pos+cstr.str().size()>LINE_LEN) {
        std::cerr << std::endl << indent;
        pos=indent.size();
      }
      std::cerr << cstr.str();
      pos+=cstr.str().size();
    }
    for(Opts::const_iterator i=_opts.begin();i!=_opts.end();++i)
      if(!i->second.ingroup&&!i->second.syn) {
        std::ostringstream cstr;
        cstr << ' ';
        if(!i->second.mandatory) cstr << '[';
        show(cstr,i);
        if(!i->second.mandatory) cstr << ']';
        if(pos+cstr.str().size()>LINE_LEN) {
          std::cerr << std::endl << indent;
          pos=indent.size();
        }
        std::cerr << cstr.str();
        pos+=cstr.str().size();
      }
    for(std::vector<OtherArg>::const_iterator i=_others_help.begin();
        i!=_others_help.end();++i)
      {
        std::ostringstream cstr;
        cstr << ' ' << i->name;

        if(pos+cstr.str().size()>LINE_LEN) {
          std::cerr << std::endl << indent;
          pos=indent.size();
        }
        std::cerr << cstr.str();
        pos+=cstr.str().size();
      }
    std::cerr << std::endl;
  }

  void ArgParser::showHelp() const
  {
    shortHelp();
    std::cerr << "Where:\n";
    for(std::vector<OtherArg>::const_iterator i=_others_help.begin();
        i!=_others_help.end();++i) showHelp(i);
    for(Opts::const_iterator i=_opts.begin();i!=_opts.end();++i) showHelp(i);
    _terminate(ArgParserException::HELP);
  }


  void ArgParser::unknownOpt(std::string arg) const
  {
    std::cerr << "\nUnknown option: " << arg << "\n";
    std::cerr << "\nType '" << _command_name <<
      " --help' to obtain a short summary on the usage.\n\n";
    _terminate(ArgParserException::UNKNOWN_OPT);
  }

  void ArgParser::requiresValue(std::string arg, OptType t) const
  {
    std::cerr << "Argument '" << arg << "' requires a";
    switch(t) {
    case STRING:
      std::cerr << " string";
      break;
    case INTEGER:
      std::cerr << "n integer";
      break;
    case DOUBLE:
      std::cerr << " floating point";
      break;
    default:
      break;
    }
    std::cerr << " value\n\n";
    showHelp();
  }


  void ArgParser::checkMandatories() const
  {
    bool ok=true;
    for(Opts::const_iterator i=_opts.begin();i!=_opts.end();++i)
      if(i->second.mandatory&&!i->second.set)
        {
          if(ok)
            std::cerr << _command_name
                      << ": The following mandatory arguments are missing.\n";
          ok=false;
          showHelp(i);
        }
    for(Groups::const_iterator i=_groups.begin();i!=_groups.end();++i)
      if(i->second.mandatory||i->second.only_one)
        {
          int set=0;
          for(GroupData::Opts::const_iterator o=i->second.opts.begin();
              o!=i->second.opts.end();++o)
            if(_opts.find(*o)->second.set) ++set;
          if(i->second.mandatory&&!set) {
            std::cerr << _command_name <<
              ": At least one of the following arguments is mandatory.\n";
            ok=false;
            for(GroupData::Opts::const_iterator o=i->second.opts.begin();
                o!=i->second.opts.end();++o)
              showHelp(_opts.find(*o));
          }
          if(i->second.only_one&&set>1) {
            std::cerr << _command_name <<
              ": At most one of the following arguments can be given.\n";
            ok=false;
            for(GroupData::Opts::const_iterator o=i->second.opts.begin();
                o!=i->second.opts.end();++o)
              showHelp(_opts.find(*o));
          }
        }
    if(!ok) {
      std::cerr << "\nType '" << _command_name <<
        " --help' to obtain a short summary on the usage.\n\n";
      _terminate(ArgParserException::INVALID_OPT);
    }
  }

  ArgParser &ArgParser::parse()
  {
    for(int ar=1; ar<_argc; ++ar) {
      std::string arg(_argv[ar]);
      if (arg[0] != '-' || arg.size() == 1) {
        _file_args.push_back(arg);
      }
      else {
        Opts::iterator i = _opts.find(arg.substr(1));
        if(i==_opts.end()) unknownOpt(arg);
        else {
          if(i->second.syn) i=_opts.find(i->second.help);
          ParData &p(i->second);
          if (p.type==BOOL) *p.bool_p=true;
          else if (p.type==FUNC) p.func_p.p(p.func_p.data);
          else if(++ar==_argc) requiresValue(arg, p.type);
          else {
            std::string val(_argv[ar]);
            std::istringstream vals(val);
            switch(p.type) {
            case STRING:
              *p.string_p=val;
              break;
            case INTEGER:
              vals >> *p.int_p;
              break;
            case DOUBLE:
              vals >> *p.double_p;
              break;
            default:
              break;
            }
            if(p.type!=STRING&&(!vals||!vals.eof()))
              requiresValue(arg, p.type);
          }
          p.set = true;
        }
      }
    }
    checkMandatories();

    return *this;
  }

}
