/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
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

///\ingroup lemon_io
///\file
///\brief \ref lgf-format "LEMON Graph Format" reader.


#ifndef LEMON_LGF_READER_H
#define LEMON_LGF_READER_H

#include <iostream>
#include <fstream>
#include <sstream>

#include <set>
#include <map>

#include <lemon/core.h>

#include <lemon/lgf_writer.h>

#include <lemon/concept_check.h>
#include <lemon/concepts/maps.h>

namespace lemon {

  namespace _reader_bits {

    template <typename Value>
    struct DefaultConverter {
      Value operator()(const std::string& str) {
        std::istringstream is(str);
        Value value;
        if (!(is >> value)) {
          throw FormatError("Cannot read token");
        }

        char c;
        if (is >> std::ws >> c) {
          throw FormatError("Remaining characters in token");
        }
        return value;
      }
    };

    template <>
    struct DefaultConverter<std::string> {
      std::string operator()(const std::string& str) {
        return str;
      }
    };

    template <typename _Item>
    class MapStorageBase {
    public:
      typedef _Item Item;

    public:
      MapStorageBase() {}
      virtual ~MapStorageBase() {}

      virtual void set(const Item& item, const std::string& value) = 0;

    };

    template <typename _Item, typename _Map,
              typename _Converter = DefaultConverter<typename _Map::Value> >
    class MapStorage : public MapStorageBase<_Item> {
    public:
      typedef _Map Map;
      typedef _Converter Converter;
      typedef _Item Item;

    private:
      Map& _map;
      Converter _converter;

    public:
      MapStorage(Map& map, const Converter& converter = Converter())
        : _map(map), _converter(converter) {}
      virtual ~MapStorage() {}

      virtual void set(const Item& item ,const std::string& value) {
        _map.set(item, _converter(value));
      }
    };

    template <typename _GR, bool _dir, typename _Map,
              typename _Converter = DefaultConverter<typename _Map::Value> >
    class GraphArcMapStorage : public MapStorageBase<typename _GR::Edge> {
    public:
      typedef _Map Map;
      typedef _Converter Converter;
      typedef _GR GR;
      typedef typename GR::Edge Item;
      static const bool dir = _dir;

    private:
      const GR& _graph;
      Map& _map;
      Converter _converter;

    public:
      GraphArcMapStorage(const GR& graph, Map& map,
                         const Converter& converter = Converter())
        : _graph(graph), _map(map), _converter(converter) {}
      virtual ~GraphArcMapStorage() {}

      virtual void set(const Item& item ,const std::string& value) {
        _map.set(_graph.direct(item, dir), _converter(value));
      }
    };

    class ValueStorageBase {
    public:
      ValueStorageBase() {}
      virtual ~ValueStorageBase() {}

      virtual void set(const std::string&) = 0;
    };

    template <typename _Value, typename _Converter = DefaultConverter<_Value> >
    class ValueStorage : public ValueStorageBase {
    public:
      typedef _Value Value;
      typedef _Converter Converter;

    private:
      Value& _value;
      Converter _converter;

    public:
      ValueStorage(Value& value, const Converter& converter = Converter())
        : _value(value), _converter(converter) {}

      virtual void set(const std::string& value) {
        _value = _converter(value);
      }
    };

    template <typename Value,
              typename Map = std::map<std::string, Value> >
    struct MapLookUpConverter {
      const Map& _map;

      MapLookUpConverter(const Map& map)
        : _map(map) {}

      Value operator()(const std::string& str) {
        typename Map::const_iterator it = _map.find(str);
        if (it == _map.end()) {
          std::ostringstream msg;
          msg << "Item not found: " << str;
          throw FormatError(msg.str());
        }
        return it->second;
      }
    };

    template <typename Value,
              typename Map1 = std::map<std::string, Value>,
              typename Map2 = std::map<std::string, Value> >
    struct DoubleMapLookUpConverter {
      const Map1& _map1;
      const Map2& _map2;

      DoubleMapLookUpConverter(const Map1& map1, const Map2& map2)
        : _map1(map1), _map2(map2) {}

      Value operator()(const std::string& str) {
        typename Map1::const_iterator it1 = _map1.find(str);
        typename Map2::const_iterator it2 = _map2.find(str);
        if (it1 == _map1.end()) {
          if (it2 == _map2.end()) {
            std::ostringstream msg;
            msg << "Item not found: " << str;
            throw FormatError(msg.str());
          } else {
            return it2->second;
          }
        } else {
          if (it2 == _map2.end()) {
            return it1->second;
          } else {
            std::ostringstream msg;
            msg << "Item is ambigous: " << str;
            throw FormatError(msg.str());
          }
        }
      }
    };

    template <typename GR>
    struct GraphArcLookUpConverter {
      const GR& _graph;
      const std::map<std::string, typename GR::Edge>& _map;

      GraphArcLookUpConverter(const GR& graph,
                              const std::map<std::string,
                                             typename GR::Edge>& map)
        : _graph(graph), _map(map) {}

      typename GR::Arc operator()(const std::string& str) {
        if (str.empty() || (str[0] != '+' && str[0] != '-')) {
          throw FormatError("Item must start with '+' or '-'");
        }
        typename std::map<std::string, typename GR::Edge>
          ::const_iterator it = _map.find(str.substr(1));
        if (it == _map.end()) {
          throw FormatError("Item not found");
        }
        return _graph.direct(it->second, str[0] == '+');
      }
    };

    inline bool isWhiteSpace(char c) {
      return c == ' ' || c == '\t' || c == '\v' ||
        c == '\n' || c == '\r' || c == '\f';
    }

    inline bool isOct(char c) {
      return '0' <= c && c <='7';
    }

    inline int valueOct(char c) {
      LEMON_ASSERT(isOct(c), "The character is not octal.");
      return c - '0';
    }

    inline bool isHex(char c) {
      return ('0' <= c && c <= '9') ||
        ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z');
    }

    inline int valueHex(char c) {
      LEMON_ASSERT(isHex(c), "The character is not hexadecimal.");
      if ('0' <= c && c <= '9') return c - '0';
      if ('a' <= c && c <= 'z') return c - 'a' + 10;
      return c - 'A' + 10;
    }

    inline bool isIdentifierFirstChar(char c) {
      return ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') || c == '_';
    }

    inline bool isIdentifierChar(char c) {
      return isIdentifierFirstChar(c) ||
        ('0' <= c && c <= '9');
    }

    inline char readEscape(std::istream& is) {
      char c;
      if (!is.get(c))
        throw FormatError("Escape format error");

      switch (c) {
      case '\\':
        return '\\';
      case '\"':
        return '\"';
      case '\'':
        return '\'';
      case '\?':
        return '\?';
      case 'a':
        return '\a';
      case 'b':
        return '\b';
      case 'f':
        return '\f';
      case 'n':
        return '\n';
      case 'r':
        return '\r';
      case 't':
        return '\t';
      case 'v':
        return '\v';
      case 'x':
        {
          int code;
          if (!is.get(c) || !isHex(c))
            throw FormatError("Escape format error");
          else if (code = valueHex(c), !is.get(c) || !isHex(c)) is.putback(c);
          else code = code * 16 + valueHex(c);
          return code;
        }
      default:
        {
          int code;
          if (!isOct(c))
            throw FormatError("Escape format error");
          else if (code = valueOct(c), !is.get(c) || !isOct(c))
            is.putback(c);
          else if (code = code * 8 + valueOct(c), !is.get(c) || !isOct(c))
            is.putback(c);
          else code = code * 8 + valueOct(c);
          return code;
        }
      }
    }

    inline std::istream& readToken(std::istream& is, std::string& str) {
      std::ostringstream os;

      char c;
      is >> std::ws;

      if (!is.get(c))
        return is;

      if (c == '\"') {
        while (is.get(c) && c != '\"') {
          if (c == '\\')
            c = readEscape(is);
          os << c;
        }
        if (!is)
          throw FormatError("Quoted format error");
      } else {
        is.putback(c);
        while (is.get(c) && !isWhiteSpace(c)) {
          if (c == '\\')
            c = readEscape(is);
          os << c;
        }
        if (!is) {
          is.clear();
        } else {
          is.putback(c);
        }
      }
      str = os.str();
      return is;
    }

    class Section {
    public:
      virtual ~Section() {}
      virtual void process(std::istream& is, int& line_num) = 0;
    };

    template <typename Functor>
    class LineSection : public Section {
    private:

      Functor _functor;

    public:

      LineSection(const Functor& functor) : _functor(functor) {}
      virtual ~LineSection() {}

      virtual void process(std::istream& is, int& line_num) {
        char c;
        std::string line;
        while (is.get(c) && c != '@') {
          if (c == '\n') {
            ++line_num;
          } else if (c == '#') {
            getline(is, line);
            ++line_num;
          } else if (!isWhiteSpace(c)) {
            is.putback(c);
            getline(is, line);
            _functor(line);
            ++line_num;
          }
        }
        if (is) is.putback(c);
        else if (is.eof()) is.clear();
      }
    };

    template <typename Functor>
    class StreamSection : public Section {
    private:

      Functor _functor;

    public:

      StreamSection(const Functor& functor) : _functor(functor) {}
      virtual ~StreamSection() {}

      virtual void process(std::istream& is, int& line_num) {
        _functor(is, line_num);
        char c;
        std::string line;
        while (is.get(c) && c != '@') {
          if (c == '\n') {
            ++line_num;
          } else if (!isWhiteSpace(c)) {
            getline(is, line);
            ++line_num;
          }
        }
        if (is) is.putback(c);
        else if (is.eof()) is.clear();
      }
    };

  }

  template <typename DGR>
  class DigraphReader;

  template <typename TDGR>
  DigraphReader<TDGR> digraphReader(TDGR& digraph, std::istream& is = std::cin);
  template <typename TDGR>
  DigraphReader<TDGR> digraphReader(TDGR& digraph, const std::string& fn);
  template <typename TDGR>
  DigraphReader<TDGR> digraphReader(TDGR& digraph, const char *fn);

  /// \ingroup lemon_io
  ///
  /// \brief \ref lgf-format "LGF" reader for directed graphs
  ///
  /// This utility reads an \ref lgf-format "LGF" file.
  ///
  /// The reading method does a batch processing. The user creates a
  /// reader object, then various reading rules can be added to the
  /// reader, and eventually the reading is executed with the \c run()
  /// member function. A map reading rule can be added to the reader
  /// with the \c nodeMap() or \c arcMap() members. An optional
  /// converter parameter can also be added as a standard functor
  /// converting from \c std::string to the value type of the map. If it
  /// is set, it will determine how the tokens in the file should be
  /// converted to the value type of the map. If the functor is not set,
  /// then a default conversion will be used. One map can be read into
  /// multiple map objects at the same time. The \c attribute(), \c
  /// node() and \c arc() functions are used to add attribute reading
  /// rules.
  ///
  ///\code
  /// DigraphReader<DGR>(digraph, std::cin).
  ///   nodeMap("coordinates", coord_map).
  ///   arcMap("capacity", cap_map).
  ///   node("source", src).
  ///   node("target", trg).
  ///   attribute("caption", caption).
  ///   run();
  ///\endcode
  ///
  /// By default, the reader uses the first section in the file of the
  /// proper type. If a section has an optional name, then it can be
  /// selected for reading by giving an optional name parameter to the
  /// \c nodes(), \c arcs() or \c attributes() functions.
  ///
  /// The \c useNodes() and \c useArcs() functions are used to tell the reader
  /// that the nodes or arcs should not be constructed (added to the
  /// graph) during the reading, but instead the label map of the items
  /// are given as a parameter of these functions. An
  /// application of these functions is multipass reading, which is
  /// important if two \c \@arcs sections must be read from the
  /// file. In this case the first phase would read the node set and one
  /// of the arc sets, while the second phase would read the second arc
  /// set into an \e ArcSet class (\c SmartArcSet or \c ListArcSet).
  /// The previously read label node map should be passed to the \c
  /// useNodes() functions. Another application of multipass reading when
  /// paths are given as a node map or an arc map.
  /// It is impossible to read this in
  /// a single pass, because the arcs are not constructed when the node
  /// maps are read.
  template <typename DGR>
  class DigraphReader {
  public:

    typedef DGR Digraph;

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(DGR);

    std::istream* _is;
    bool local_is;
    std::string _filename;

    DGR& _digraph;

    std::string _nodes_caption;
    std::string _arcs_caption;
    std::string _attributes_caption;

    typedef std::map<std::string, Node> NodeIndex;
    NodeIndex _node_index;
    typedef std::map<std::string, Arc> ArcIndex;
    ArcIndex _arc_index;

    typedef std::vector<std::pair<std::string,
      _reader_bits::MapStorageBase<Node>*> > NodeMaps;
    NodeMaps _node_maps;

    typedef std::vector<std::pair<std::string,
      _reader_bits::MapStorageBase<Arc>*> >ArcMaps;
    ArcMaps _arc_maps;

    typedef std::multimap<std::string, _reader_bits::ValueStorageBase*>
      Attributes;
    Attributes _attributes;

    bool _use_nodes;
    bool _use_arcs;

    bool _skip_nodes;
    bool _skip_arcs;

    int line_num;
    std::istringstream line;

  public:

    /// \brief Constructor
    ///
    /// Construct a directed graph reader, which reads from the given
    /// input stream.
    DigraphReader(DGR& digraph, std::istream& is = std::cin)
      : _is(&is), local_is(false), _digraph(digraph),
        _use_nodes(false), _use_arcs(false),
        _skip_nodes(false), _skip_arcs(false) {}

    /// \brief Constructor
    ///
    /// Construct a directed graph reader, which reads from the given
    /// file.
    DigraphReader(DGR& digraph, const std::string& fn)
      : _is(new std::ifstream(fn.c_str())), local_is(true),
        _filename(fn), _digraph(digraph),
        _use_nodes(false), _use_arcs(false),
        _skip_nodes(false), _skip_arcs(false) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Constructor
    ///
    /// Construct a directed graph reader, which reads from the given
    /// file.
    DigraphReader(DGR& digraph, const char* fn)
      : _is(new std::ifstream(fn)), local_is(true),
        _filename(fn), _digraph(digraph),
        _use_nodes(false), _use_arcs(false),
        _skip_nodes(false), _skip_arcs(false) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Destructor
    ~DigraphReader() {
      for (typename NodeMaps::iterator it = _node_maps.begin();
           it != _node_maps.end(); ++it) {
        delete it->second;
      }

      for (typename ArcMaps::iterator it = _arc_maps.begin();
           it != _arc_maps.end(); ++it) {
        delete it->second;
      }

      for (typename Attributes::iterator it = _attributes.begin();
           it != _attributes.end(); ++it) {
        delete it->second;
      }

      if (local_is) {
        delete _is;
      }

    }

  private:

    template <typename TDGR>
    friend DigraphReader<TDGR> digraphReader(TDGR& digraph, std::istream& is);
    template <typename TDGR>
    friend DigraphReader<TDGR> digraphReader(TDGR& digraph,
                                             const std::string& fn);
    template <typename TDGR>
    friend DigraphReader<TDGR> digraphReader(TDGR& digraph, const char *fn);

    DigraphReader(DigraphReader& other)
      : _is(other._is), local_is(other.local_is), _digraph(other._digraph),
        _use_nodes(other._use_nodes), _use_arcs(other._use_arcs),
        _skip_nodes(other._skip_nodes), _skip_arcs(other._skip_arcs) {

      other._is = 0;
      other.local_is = false;

      _node_index.swap(other._node_index);
      _arc_index.swap(other._arc_index);

      _node_maps.swap(other._node_maps);
      _arc_maps.swap(other._arc_maps);
      _attributes.swap(other._attributes);

      _nodes_caption = other._nodes_caption;
      _arcs_caption = other._arcs_caption;
      _attributes_caption = other._attributes_caption;

    }

    DigraphReader& operator=(const DigraphReader&);

  public:

    /// \name Reading Rules
    /// @{

    /// \brief Node map reading rule
    ///
    /// Add a node map reading rule to the reader.
    template <typename Map>
    DigraphReader& nodeMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Node, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Node>* storage =
        new _reader_bits::MapStorage<Node, Map>(map);
      _node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Node map reading rule
    ///
    /// Add a node map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    DigraphReader& nodeMap(const std::string& caption, Map& map,
                           const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Node, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Node>* storage =
        new _reader_bits::MapStorage<Node, Map, Converter>(map, converter);
      _node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Arc map reading rule
    ///
    /// Add an arc map reading rule to the reader.
    template <typename Map>
    DigraphReader& arcMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Arc, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Arc>* storage =
        new _reader_bits::MapStorage<Arc, Map>(map);
      _arc_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Arc map reading rule
    ///
    /// Add an arc map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    DigraphReader& arcMap(const std::string& caption, Map& map,
                          const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Arc, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Arc>* storage =
        new _reader_bits::MapStorage<Arc, Map, Converter>(map, converter);
      _arc_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Attribute reading rule
    ///
    /// Add an attribute reading rule to the reader.
    template <typename Value>
    DigraphReader& attribute(const std::string& caption, Value& value) {
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Value>(value);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Attribute reading rule
    ///
    /// Add an attribute reading rule with specialized converter to the
    /// reader.
    template <typename Value, typename Converter>
    DigraphReader& attribute(const std::string& caption, Value& value,
                             const Converter& converter = Converter()) {
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Value, Converter>(value, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Node reading rule
    ///
    /// Add a node reading rule to reader.
    DigraphReader& node(const std::string& caption, Node& node) {
      typedef _reader_bits::MapLookUpConverter<Node> Converter;
      Converter converter(_node_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Node, Converter>(node, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Arc reading rule
    ///
    /// Add an arc reading rule to reader.
    DigraphReader& arc(const std::string& caption, Arc& arc) {
      typedef _reader_bits::MapLookUpConverter<Arc> Converter;
      Converter converter(_arc_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Arc, Converter>(arc, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// @}

    /// \name Select Section by Name
    /// @{

    /// \brief Set \c \@nodes section to be read
    ///
    /// Set \c \@nodes section to be read
    DigraphReader& nodes(const std::string& caption) {
      _nodes_caption = caption;
      return *this;
    }

    /// \brief Set \c \@arcs section to be read
    ///
    /// Set \c \@arcs section to be read
    DigraphReader& arcs(const std::string& caption) {
      _arcs_caption = caption;
      return *this;
    }

    /// \brief Set \c \@attributes section to be read
    ///
    /// Set \c \@attributes section to be read
    DigraphReader& attributes(const std::string& caption) {
      _attributes_caption = caption;
      return *this;
    }

    /// @}

    /// \name Using Previously Constructed Node or Arc Set
    /// @{

    /// \brief Use previously constructed node set
    ///
    /// Use previously constructed node set, and specify the node
    /// label map.
    template <typename Map>
    DigraphReader& useNodes(const Map& map) {
      checkConcept<concepts::ReadMap<Node, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_nodes, "Multiple usage of useNodes() member");
      _use_nodes = true;
      _writer_bits::DefaultConverter<typename Map::Value> converter;
      for (NodeIt n(_digraph); n != INVALID; ++n) {
        _node_index.insert(std::make_pair(converter(map[n]), n));
      }
      return *this;
    }

    /// \brief Use previously constructed node set
    ///
    /// Use previously constructed node set, and specify the node
    /// label map and a functor which converts the label map values to
    /// \c std::string.
    template <typename Map, typename Converter>
    DigraphReader& useNodes(const Map& map,
                            const Converter& converter = Converter()) {
      checkConcept<concepts::ReadMap<Node, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_nodes, "Multiple usage of useNodes() member");
      _use_nodes = true;
      for (NodeIt n(_digraph); n != INVALID; ++n) {
        _node_index.insert(std::make_pair(converter(map[n]), n));
      }
      return *this;
    }

    /// \brief Use previously constructed arc set
    ///
    /// Use previously constructed arc set, and specify the arc
    /// label map.
    template <typename Map>
    DigraphReader& useArcs(const Map& map) {
      checkConcept<concepts::ReadMap<Arc, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_arcs, "Multiple usage of useArcs() member");
      _use_arcs = true;
      _writer_bits::DefaultConverter<typename Map::Value> converter;
      for (ArcIt a(_digraph); a != INVALID; ++a) {
        _arc_index.insert(std::make_pair(converter(map[a]), a));
      }
      return *this;
    }

    /// \brief Use previously constructed arc set
    ///
    /// Use previously constructed arc set, and specify the arc
    /// label map and a functor which converts the label map values to
    /// \c std::string.
    template <typename Map, typename Converter>
    DigraphReader& useArcs(const Map& map,
                           const Converter& converter = Converter()) {
      checkConcept<concepts::ReadMap<Arc, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_arcs, "Multiple usage of useArcs() member");
      _use_arcs = true;
      for (ArcIt a(_digraph); a != INVALID; ++a) {
        _arc_index.insert(std::make_pair(converter(map[a]), a));
      }
      return *this;
    }

    /// \brief Skips the reading of node section
    ///
    /// Omit the reading of the node section. This implies that each node
    /// map reading rule will be abandoned, and the nodes of the graph
    /// will not be constructed, which usually cause that the arc set
    /// could not be read due to lack of node name resolving.
    /// Therefore \c skipArcs() function should also be used, or
    /// \c useNodes() should be used to specify the label of the nodes.
    DigraphReader& skipNodes() {
      LEMON_ASSERT(!_skip_nodes, "Skip nodes already set");
      _skip_nodes = true;
      return *this;
    }

    /// \brief Skips the reading of arc section
    ///
    /// Omit the reading of the arc section. This implies that each arc
    /// map reading rule will be abandoned, and the arcs of the graph
    /// will not be constructed.
    DigraphReader& skipArcs() {
      LEMON_ASSERT(!_skip_arcs, "Skip arcs already set");
      _skip_arcs = true;
      return *this;
    }

    /// @}

  private:

    bool readLine() {
      std::string str;
      while(++line_num, std::getline(*_is, str)) {
        line.clear(); line.str(str);
        char c;
        if (line >> std::ws >> c && c != '#') {
          line.putback(c);
          return true;
        }
      }
      return false;
    }

    bool readSuccess() {
      return static_cast<bool>(*_is);
    }

    void skipSection() {
      char c;
      while (readSuccess() && line >> c && c != '@') {
        readLine();
      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readNodes() {

      std::vector<int> map_index(_node_maps.size());
      int map_num, label_index;

      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        if (!_node_maps.empty())
          throw FormatError("Cannot find map names");
        return;
      }
      line.putback(c);

      {
        std::map<std::string, int> maps;

        std::string map;
        int index = 0;
        while (_reader_bits::readToken(line, map)) {
          if (maps.find(map) != maps.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of node map: " << map;
            throw FormatError(msg.str());
          }
          maps.insert(std::make_pair(map, index));
          ++index;
        }

        for (int i = 0; i < static_cast<int>(_node_maps.size()); ++i) {
          std::map<std::string, int>::iterator jt =
            maps.find(_node_maps[i].first);
          if (jt == maps.end()) {
            std::ostringstream msg;
            msg << "Map not found: " << _node_maps[i].first;
            throw FormatError(msg.str());
          }
          map_index[i] = jt->second;
        }

        {
          std::map<std::string, int>::iterator jt = maps.find("label");
          if (jt != maps.end()) {
            label_index = jt->second;
          } else {
            label_index = -1;
          }
        }
        map_num = maps.size();
      }

      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::vector<std::string> tokens(map_num);
        for (int i = 0; i < map_num; ++i) {
          if (!_reader_bits::readToken(line, tokens[i])) {
            std::ostringstream msg;
            msg << "Column not found (" << i + 1 << ")";
            throw FormatError(msg.str());
          }
        }
        if (line >> std::ws >> c)
          throw FormatError("Extra character at the end of line");

        Node n;
        if (!_use_nodes) {
          n = _digraph.addNode();
          if (label_index != -1)
            _node_index.insert(std::make_pair(tokens[label_index], n));
        } else {
          if (label_index == -1)
            throw FormatError("Label map not found");
          typename std::map<std::string, Node>::iterator it =
            _node_index.find(tokens[label_index]);
          if (it == _node_index.end()) {
            std::ostringstream msg;
            msg << "Node with label not found: " << tokens[label_index];
            throw FormatError(msg.str());
          }
          n = it->second;
        }

        for (int i = 0; i < static_cast<int>(_node_maps.size()); ++i) {
          _node_maps[i].second->set(n, tokens[map_index[i]]);
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readArcs() {

      std::vector<int> map_index(_arc_maps.size());
      int map_num, label_index;

      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        if (!_arc_maps.empty())
          throw FormatError("Cannot find map names");
        return;
      }
      line.putback(c);

      {
        std::map<std::string, int> maps;

        std::string map;
        int index = 0;
        while (_reader_bits::readToken(line, map)) {
          if(map == "-") {
              if(index!=0)
                throw FormatError("'-' is not allowed as a map name");
              else if (line >> std::ws >> c)
                throw FormatError("Extra character at the end of line");
              else break;
            }
          if (maps.find(map) != maps.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of arc map: " << map;
            throw FormatError(msg.str());
          }
          maps.insert(std::make_pair(map, index));
          ++index;
        }

        for (int i = 0; i < static_cast<int>(_arc_maps.size()); ++i) {
          std::map<std::string, int>::iterator jt =
            maps.find(_arc_maps[i].first);
          if (jt == maps.end()) {
            std::ostringstream msg;
            msg << "Map not found: " << _arc_maps[i].first;
            throw FormatError(msg.str());
          }
          map_index[i] = jt->second;
        }

        {
          std::map<std::string, int>::iterator jt = maps.find("label");
          if (jt != maps.end()) {
            label_index = jt->second;
          } else {
            label_index = -1;
          }
        }
        map_num = maps.size();
      }

      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::string source_token;
        std::string target_token;

        if (!_reader_bits::readToken(line, source_token))
          throw FormatError("Source not found");

        if (!_reader_bits::readToken(line, target_token))
          throw FormatError("Target not found");

        std::vector<std::string> tokens(map_num);
        for (int i = 0; i < map_num; ++i) {
          if (!_reader_bits::readToken(line, tokens[i])) {
            std::ostringstream msg;
            msg << "Column not found (" << i + 1 << ")";
            throw FormatError(msg.str());
          }
        }
        if (line >> std::ws >> c)
          throw FormatError("Extra character at the end of line");

        Arc a;
        if (!_use_arcs) {

          typename NodeIndex::iterator it;

          it = _node_index.find(source_token);
          if (it == _node_index.end()) {
            std::ostringstream msg;
            msg << "Item not found: " << source_token;
            throw FormatError(msg.str());
          }
          Node source = it->second;

          it = _node_index.find(target_token);
          if (it == _node_index.end()) {
            std::ostringstream msg;
            msg << "Item not found: " << target_token;
            throw FormatError(msg.str());
          }
          Node target = it->second;

          a = _digraph.addArc(source, target);
          if (label_index != -1)
            _arc_index.insert(std::make_pair(tokens[label_index], a));
        } else {
          if (label_index == -1)
            throw FormatError("Label map not found");
          typename std::map<std::string, Arc>::iterator it =
            _arc_index.find(tokens[label_index]);
          if (it == _arc_index.end()) {
            std::ostringstream msg;
            msg << "Arc with label not found: " << tokens[label_index];
            throw FormatError(msg.str());
          }
          a = it->second;
        }

        for (int i = 0; i < static_cast<int>(_arc_maps.size()); ++i) {
          _arc_maps[i].second->set(a, tokens[map_index[i]]);
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readAttributes() {

      std::set<std::string> read_attr;

      char c;
      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::string attr, token;
        if (!_reader_bits::readToken(line, attr))
          throw FormatError("Attribute name not found");
        if (!_reader_bits::readToken(line, token))
          throw FormatError("Attribute value not found");
        if (line >> c)
          throw FormatError("Extra character at the end of line");

        {
          std::set<std::string>::iterator it = read_attr.find(attr);
          if (it != read_attr.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of attribute: " << attr;
            throw FormatError(msg.str());
          }
          read_attr.insert(attr);
        }

        {
          typename Attributes::iterator it = _attributes.lower_bound(attr);
          while (it != _attributes.end() && it->first == attr) {
            it->second->set(token);
            ++it;
          }
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
      for (typename Attributes::iterator it = _attributes.begin();
           it != _attributes.end(); ++it) {
        if (read_attr.find(it->first) == read_attr.end()) {
          std::ostringstream msg;
          msg << "Attribute not found: " << it->first;
          throw FormatError(msg.str());
        }
      }
    }

  public:

    /// \name Execution of the Reader
    /// @{

    /// \brief Start the batch processing
    ///
    /// This function starts the batch processing
    void run() {
      LEMON_ASSERT(_is != 0, "This reader assigned to an other reader");

      bool nodes_done = _skip_nodes;
      bool arcs_done = _skip_arcs;
      bool attributes_done = false;

      line_num = 0;
      readLine();
      skipSection();

      while (readSuccess()) {
        try {
          char c;
          std::string section, caption;
          line >> c;
          _reader_bits::readToken(line, section);
          _reader_bits::readToken(line, caption);

          if (line >> c)
            throw FormatError("Extra character at the end of line");

          if (section == "nodes" && !nodes_done) {
            if (_nodes_caption.empty() || _nodes_caption == caption) {
              readNodes();
              nodes_done = true;
            }
          } else if ((section == "arcs" || section == "edges") &&
                     !arcs_done) {
            if (_arcs_caption.empty() || _arcs_caption == caption) {
              readArcs();
              arcs_done = true;
            }
          } else if (section == "attributes" && !attributes_done) {
            if (_attributes_caption.empty() || _attributes_caption == caption) {
              readAttributes();
              attributes_done = true;
            }
          } else {
            readLine();
            skipSection();
          }
        } catch (FormatError& error) {
          error.line(line_num);
          error.file(_filename);
          throw;
        }
      }

      if (!nodes_done) {
        throw FormatError("Section @nodes not found");
      }

      if (!arcs_done) {
        throw FormatError("Section @arcs not found");
      }

      if (!attributes_done && !_attributes.empty()) {
        throw FormatError("Section @attributes not found");
      }

    }

    /// @}

  };

  /// \ingroup lemon_io
  ///
  /// \brief Return a \ref lemon::DigraphReader "DigraphReader" class
  ///
  /// This function just returns a \ref lemon::DigraphReader
  /// "DigraphReader" class.
  ///
  /// With this function a digraph can be read from an
  /// \ref lgf-format "LGF" file or input stream with several maps and
  /// attributes. For example, there is network flow problem on a
  /// digraph, i.e. a digraph with a \e capacity map on the arcs and
  /// \e source and \e target nodes. This digraph can be read with the
  /// following code:
  ///
  ///\code
  ///ListDigraph digraph;
  ///ListDigraph::ArcMap<int> cm(digraph);
  ///ListDigraph::Node src, trg;
  ///digraphReader(digraph, std::cin).
  ///  arcMap("capacity", cap).
  ///  node("source", src).
  ///  node("target", trg).
  ///  run();
  ///\endcode
  ///
  /// For a complete documentation, please see the
  /// \ref lemon::DigraphReader "DigraphReader"
  /// class documentation.
  /// \warning Don't forget to put the \ref lemon::DigraphReader::run() "run()"
  /// to the end of the parameter list.
  /// \relates DigraphReader
  /// \sa digraphReader(TDGR& digraph, const std::string& fn)
  /// \sa digraphReader(TDGR& digraph, const char* fn)
  template <typename TDGR>
  DigraphReader<TDGR> digraphReader(TDGR& digraph, std::istream& is) {
    DigraphReader<TDGR> tmp(digraph, is);
    return tmp;
  }

  /// \brief Return a \ref DigraphReader class
  ///
  /// This function just returns a \ref DigraphReader class.
  /// \relates DigraphReader
  /// \sa digraphReader(TDGR& digraph, std::istream& is)
  template <typename TDGR>
  DigraphReader<TDGR> digraphReader(TDGR& digraph, const std::string& fn) {
    DigraphReader<TDGR> tmp(digraph, fn);
    return tmp;
  }

  /// \brief Return a \ref DigraphReader class
  ///
  /// This function just returns a \ref DigraphReader class.
  /// \relates DigraphReader
  /// \sa digraphReader(TDGR& digraph, std::istream& is)
  template <typename TDGR>
  DigraphReader<TDGR> digraphReader(TDGR& digraph, const char* fn) {
    DigraphReader<TDGR> tmp(digraph, fn);
    return tmp;
  }

  template <typename GR>
  class GraphReader;

  template <typename TGR>
  GraphReader<TGR> graphReader(TGR& graph, std::istream& is = std::cin);
  template <typename TGR>
  GraphReader<TGR> graphReader(TGR& graph, const std::string& fn);
  template <typename TGR>
  GraphReader<TGR> graphReader(TGR& graph, const char *fn);

  /// \ingroup lemon_io
  ///
  /// \brief \ref lgf-format "LGF" reader for undirected graphs
  ///
  /// This utility reads an \ref lgf-format "LGF" file.
  ///
  /// It can be used almost the same way as \c DigraphReader.
  /// The only difference is that this class can handle edges and
  /// edge maps as well as arcs and arc maps.
  ///
  /// The columns in the \c \@edges (or \c \@arcs) section are the
  /// edge maps. However, if there are two maps with the same name
  /// prefixed with \c '+' and \c '-', then these can be read into an
  /// arc map.  Similarly, an attribute can be read into an arc, if
  /// it's value is an edge label prefixed with \c '+' or \c '-'.
  template <typename GR>
  class GraphReader {
  public:

    typedef GR Graph;

  private:

    TEMPLATE_GRAPH_TYPEDEFS(GR);

    std::istream* _is;
    bool local_is;
    std::string _filename;

    GR& _graph;

    std::string _nodes_caption;
    std::string _edges_caption;
    std::string _attributes_caption;

    typedef std::map<std::string, Node> NodeIndex;
    NodeIndex _node_index;
    typedef std::map<std::string, Edge> EdgeIndex;
    EdgeIndex _edge_index;

    typedef std::vector<std::pair<std::string,
      _reader_bits::MapStorageBase<Node>*> > NodeMaps;
    NodeMaps _node_maps;

    typedef std::vector<std::pair<std::string,
      _reader_bits::MapStorageBase<Edge>*> > EdgeMaps;
    EdgeMaps _edge_maps;

    typedef std::multimap<std::string, _reader_bits::ValueStorageBase*>
      Attributes;
    Attributes _attributes;

    bool _use_nodes;
    bool _use_edges;

    bool _skip_nodes;
    bool _skip_edges;

    int line_num;
    std::istringstream line;

  public:

    /// \brief Constructor
    ///
    /// Construct an undirected graph reader, which reads from the given
    /// input stream.
    GraphReader(GR& graph, std::istream& is = std::cin)
      : _is(&is), local_is(false), _graph(graph),
        _use_nodes(false), _use_edges(false),
        _skip_nodes(false), _skip_edges(false) {}

    /// \brief Constructor
    ///
    /// Construct an undirected graph reader, which reads from the given
    /// file.
    GraphReader(GR& graph, const std::string& fn)
      : _is(new std::ifstream(fn.c_str())), local_is(true),
        _filename(fn), _graph(graph),
        _use_nodes(false), _use_edges(false),
        _skip_nodes(false), _skip_edges(false) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Constructor
    ///
    /// Construct an undirected graph reader, which reads from the given
    /// file.
    GraphReader(GR& graph, const char* fn)
      : _is(new std::ifstream(fn)), local_is(true),
        _filename(fn), _graph(graph),
        _use_nodes(false), _use_edges(false),
        _skip_nodes(false), _skip_edges(false) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Destructor
    ~GraphReader() {
      for (typename NodeMaps::iterator it = _node_maps.begin();
           it != _node_maps.end(); ++it) {
        delete it->second;
      }

      for (typename EdgeMaps::iterator it = _edge_maps.begin();
           it != _edge_maps.end(); ++it) {
        delete it->second;
      }

      for (typename Attributes::iterator it = _attributes.begin();
           it != _attributes.end(); ++it) {
        delete it->second;
      }

      if (local_is) {
        delete _is;
      }

    }

  private:
    template <typename TGR>
    friend GraphReader<TGR> graphReader(TGR& graph, std::istream& is);
    template <typename TGR>
    friend GraphReader<TGR> graphReader(TGR& graph, const std::string& fn);
    template <typename TGR>
    friend GraphReader<TGR> graphReader(TGR& graph, const char *fn);

    GraphReader(GraphReader& other)
      : _is(other._is), local_is(other.local_is), _graph(other._graph),
        _use_nodes(other._use_nodes), _use_edges(other._use_edges),
        _skip_nodes(other._skip_nodes), _skip_edges(other._skip_edges) {

      other._is = 0;
      other.local_is = false;

      _node_index.swap(other._node_index);
      _edge_index.swap(other._edge_index);

      _node_maps.swap(other._node_maps);
      _edge_maps.swap(other._edge_maps);
      _attributes.swap(other._attributes);

      _nodes_caption = other._nodes_caption;
      _edges_caption = other._edges_caption;
      _attributes_caption = other._attributes_caption;

    }

    GraphReader& operator=(const GraphReader&);

  public:

    /// \name Reading Rules
    /// @{

    /// \brief Node map reading rule
    ///
    /// Add a node map reading rule to the reader.
    template <typename Map>
    GraphReader& nodeMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Node, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Node>* storage =
        new _reader_bits::MapStorage<Node, Map>(map);
      _node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Node map reading rule
    ///
    /// Add a node map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    GraphReader& nodeMap(const std::string& caption, Map& map,
                           const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Node, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Node>* storage =
        new _reader_bits::MapStorage<Node, Map, Converter>(map, converter);
      _node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Edge map reading rule
    ///
    /// Add an edge map reading rule to the reader.
    template <typename Map>
    GraphReader& edgeMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Edge, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* storage =
        new _reader_bits::MapStorage<Edge, Map>(map);
      _edge_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Edge map reading rule
    ///
    /// Add an edge map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    GraphReader& edgeMap(const std::string& caption, Map& map,
                          const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Edge, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* storage =
        new _reader_bits::MapStorage<Edge, Map, Converter>(map, converter);
      _edge_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Arc map reading rule
    ///
    /// Add an arc map reading rule to the reader.
    template <typename Map>
    GraphReader& arcMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Arc, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* forward_storage =
        new _reader_bits::GraphArcMapStorage<Graph, true, Map>(_graph, map);
      _edge_maps.push_back(std::make_pair('+' + caption, forward_storage));
      _reader_bits::MapStorageBase<Edge>* backward_storage =
        new _reader_bits::GraphArcMapStorage<GR, false, Map>(_graph, map);
      _edge_maps.push_back(std::make_pair('-' + caption, backward_storage));
      return *this;
    }

    /// \brief Arc map reading rule
    ///
    /// Add an arc map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    GraphReader& arcMap(const std::string& caption, Map& map,
                          const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Arc, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* forward_storage =
        new _reader_bits::GraphArcMapStorage<GR, true, Map, Converter>
        (_graph, map, converter);
      _edge_maps.push_back(std::make_pair('+' + caption, forward_storage));
      _reader_bits::MapStorageBase<Edge>* backward_storage =
        new _reader_bits::GraphArcMapStorage<GR, false, Map, Converter>
        (_graph, map, converter);
      _edge_maps.push_back(std::make_pair('-' + caption, backward_storage));
      return *this;
    }

    /// \brief Attribute reading rule
    ///
    /// Add an attribute reading rule to the reader.
    template <typename Value>
    GraphReader& attribute(const std::string& caption, Value& value) {
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Value>(value);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Attribute reading rule
    ///
    /// Add an attribute reading rule with specialized converter to the
    /// reader.
    template <typename Value, typename Converter>
    GraphReader& attribute(const std::string& caption, Value& value,
                             const Converter& converter = Converter()) {
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Value, Converter>(value, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Node reading rule
    ///
    /// Add a node reading rule to reader.
    GraphReader& node(const std::string& caption, Node& node) {
      typedef _reader_bits::MapLookUpConverter<Node> Converter;
      Converter converter(_node_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Node, Converter>(node, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Edge reading rule
    ///
    /// Add an edge reading rule to reader.
    GraphReader& edge(const std::string& caption, Edge& edge) {
      typedef _reader_bits::MapLookUpConverter<Edge> Converter;
      Converter converter(_edge_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Edge, Converter>(edge, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Arc reading rule
    ///
    /// Add an arc reading rule to reader.
    GraphReader& arc(const std::string& caption, Arc& arc) {
      typedef _reader_bits::GraphArcLookUpConverter<GR> Converter;
      Converter converter(_graph, _edge_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Arc, Converter>(arc, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// @}

    /// \name Select Section by Name
    /// @{

    /// \brief Set \c \@nodes section to be read
    ///
    /// Set \c \@nodes section to be read.
    GraphReader& nodes(const std::string& caption) {
      _nodes_caption = caption;
      return *this;
    }

    /// \brief Set \c \@edges section to be read
    ///
    /// Set \c \@edges section to be read.
    GraphReader& edges(const std::string& caption) {
      _edges_caption = caption;
      return *this;
    }

    /// \brief Set \c \@attributes section to be read
    ///
    /// Set \c \@attributes section to be read.
    GraphReader& attributes(const std::string& caption) {
      _attributes_caption = caption;
      return *this;
    }

    /// @}

    /// \name Using Previously Constructed Node or Edge Set
    /// @{

    /// \brief Use previously constructed node set
    ///
    /// Use previously constructed node set, and specify the node
    /// label map.
    template <typename Map>
    GraphReader& useNodes(const Map& map) {
      checkConcept<concepts::ReadMap<Node, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_nodes, "Multiple usage of useNodes() member");
      _use_nodes = true;
      _writer_bits::DefaultConverter<typename Map::Value> converter;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        _node_index.insert(std::make_pair(converter(map[n]), n));
      }
      return *this;
    }

    /// \brief Use previously constructed node set
    ///
    /// Use previously constructed node set, and specify the node
    /// label map and a functor which converts the label map values to
    /// \c std::string.
    template <typename Map, typename Converter>
    GraphReader& useNodes(const Map& map,
                            const Converter& converter = Converter()) {
      checkConcept<concepts::ReadMap<Node, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_nodes, "Multiple usage of useNodes() member");
      _use_nodes = true;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        _node_index.insert(std::make_pair(converter(map[n]), n));
      }
      return *this;
    }

    /// \brief Use previously constructed edge set
    ///
    /// Use previously constructed edge set, and specify the edge
    /// label map.
    template <typename Map>
    GraphReader& useEdges(const Map& map) {
      checkConcept<concepts::ReadMap<Edge, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_edges, "Multiple usage of useEdges() member");
      _use_edges = true;
      _writer_bits::DefaultConverter<typename Map::Value> converter;
      for (EdgeIt a(_graph); a != INVALID; ++a) {
        _edge_index.insert(std::make_pair(converter(map[a]), a));
      }
      return *this;
    }

    /// \brief Use previously constructed edge set
    ///
    /// Use previously constructed edge set, and specify the edge
    /// label map and a functor which converts the label map values to
    /// \c std::string.
    template <typename Map, typename Converter>
    GraphReader& useEdges(const Map& map,
                            const Converter& converter = Converter()) {
      checkConcept<concepts::ReadMap<Edge, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_edges, "Multiple usage of useEdges() member");
      _use_edges = true;
      for (EdgeIt a(_graph); a != INVALID; ++a) {
        _edge_index.insert(std::make_pair(converter(map[a]), a));
      }
      return *this;
    }

    /// \brief Skip the reading of node section
    ///
    /// Omit the reading of the node section. This implies that each node
    /// map reading rule will be abandoned, and the nodes of the graph
    /// will not be constructed, which usually cause that the edge set
    /// could not be read due to lack of node name
    /// could not be read due to lack of node name resolving.
    /// Therefore \c skipEdges() function should also be used, or
    /// \c useNodes() should be used to specify the label of the nodes.
    GraphReader& skipNodes() {
      LEMON_ASSERT(!_skip_nodes, "Skip nodes already set");
      _skip_nodes = true;
      return *this;
    }

    /// \brief Skip the reading of edge section
    ///
    /// Omit the reading of the edge section. This implies that each edge
    /// map reading rule will be abandoned, and the edges of the graph
    /// will not be constructed.
    GraphReader& skipEdges() {
      LEMON_ASSERT(!_skip_edges, "Skip edges already set");
      _skip_edges = true;
      return *this;
    }

    /// @}

  private:

    bool readLine() {
      std::string str;
      while(++line_num, std::getline(*_is, str)) {
        line.clear(); line.str(str);
        char c;
        if (line >> std::ws >> c && c != '#') {
          line.putback(c);
          return true;
        }
      }
      return false;
    }

    bool readSuccess() {
      return static_cast<bool>(*_is);
    }

    void skipSection() {
      char c;
      while (readSuccess() && line >> c && c != '@') {
        readLine();
      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readNodes() {

      std::vector<int> map_index(_node_maps.size());
      int map_num, label_index;

      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        if (!_node_maps.empty())
          throw FormatError("Cannot find map names");
        return;
      }
      line.putback(c);

      {
        std::map<std::string, int> maps;

        std::string map;
        int index = 0;
        while (_reader_bits::readToken(line, map)) {
          if (maps.find(map) != maps.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of node map: " << map;
            throw FormatError(msg.str());
          }
          maps.insert(std::make_pair(map, index));
          ++index;
        }

        for (int i = 0; i < static_cast<int>(_node_maps.size()); ++i) {
          std::map<std::string, int>::iterator jt =
            maps.find(_node_maps[i].first);
          if (jt == maps.end()) {
            std::ostringstream msg;
            msg << "Map not found: " << _node_maps[i].first;
            throw FormatError(msg.str());
          }
          map_index[i] = jt->second;
        }

        {
          std::map<std::string, int>::iterator jt = maps.find("label");
          if (jt != maps.end()) {
            label_index = jt->second;
          } else {
            label_index = -1;
          }
        }
        map_num = maps.size();
      }

      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::vector<std::string> tokens(map_num);
        for (int i = 0; i < map_num; ++i) {
          if (!_reader_bits::readToken(line, tokens[i])) {
            std::ostringstream msg;
            msg << "Column not found (" << i + 1 << ")";
            throw FormatError(msg.str());
          }
        }
        if (line >> std::ws >> c)
          throw FormatError("Extra character at the end of line");

        Node n;
        if (!_use_nodes) {
          n = _graph.addNode();
          if (label_index != -1)
            _node_index.insert(std::make_pair(tokens[label_index], n));
        } else {
          if (label_index == -1)
            throw FormatError("Label map not found");
          typename std::map<std::string, Node>::iterator it =
            _node_index.find(tokens[label_index]);
          if (it == _node_index.end()) {
            std::ostringstream msg;
            msg << "Node with label not found: " << tokens[label_index];
            throw FormatError(msg.str());
          }
          n = it->second;
        }

        for (int i = 0; i < static_cast<int>(_node_maps.size()); ++i) {
          _node_maps[i].second->set(n, tokens[map_index[i]]);
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readEdges() {

      std::vector<int> map_index(_edge_maps.size());
      int map_num, label_index;

      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        if (!_edge_maps.empty())
          throw FormatError("Cannot find map names");
        return;
      }
      line.putback(c);

      {
        std::map<std::string, int> maps;

        std::string map;
        int index = 0;
        while (_reader_bits::readToken(line, map)) {
          if(map == "-") {
              if(index!=0)
                throw FormatError("'-' is not allowed as a map name");
              else if (line >> std::ws >> c)
                throw FormatError("Extra character at the end of line");
              else break;
            }
          if (maps.find(map) != maps.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of edge map: " << map;
            throw FormatError(msg.str());
          }
          maps.insert(std::make_pair(map, index));
          ++index;
        }

        for (int i = 0; i < static_cast<int>(_edge_maps.size()); ++i) {
          std::map<std::string, int>::iterator jt =
            maps.find(_edge_maps[i].first);
          if (jt == maps.end()) {
            std::ostringstream msg;
            msg << "Map not found: " << _edge_maps[i].first;
            throw FormatError(msg.str());
          }
          map_index[i] = jt->second;
        }

        {
          std::map<std::string, int>::iterator jt = maps.find("label");
          if (jt != maps.end()) {
            label_index = jt->second;
          } else {
            label_index = -1;
          }
        }
        map_num = maps.size();
      }

      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::string source_token;
        std::string target_token;

        if (!_reader_bits::readToken(line, source_token))
          throw FormatError("Node u not found");

        if (!_reader_bits::readToken(line, target_token))
          throw FormatError("Node v not found");

        std::vector<std::string> tokens(map_num);
        for (int i = 0; i < map_num; ++i) {
          if (!_reader_bits::readToken(line, tokens[i])) {
            std::ostringstream msg;
            msg << "Column not found (" << i + 1 << ")";
            throw FormatError(msg.str());
          }
        }
        if (line >> std::ws >> c)
          throw FormatError("Extra character at the end of line");

        Edge e;
        if (!_use_edges) {

          typename NodeIndex::iterator it;

          it = _node_index.find(source_token);
          if (it == _node_index.end()) {
            std::ostringstream msg;
            msg << "Item not found: " << source_token;
            throw FormatError(msg.str());
          }
          Node source = it->second;

          it = _node_index.find(target_token);
          if (it == _node_index.end()) {
            std::ostringstream msg;
            msg << "Item not found: " << target_token;
            throw FormatError(msg.str());
          }
          Node target = it->second;

          e = _graph.addEdge(source, target);
          if (label_index != -1)
            _edge_index.insert(std::make_pair(tokens[label_index], e));
        } else {
          if (label_index == -1)
            throw FormatError("Label map not found");
          typename std::map<std::string, Edge>::iterator it =
            _edge_index.find(tokens[label_index]);
          if (it == _edge_index.end()) {
            std::ostringstream msg;
            msg << "Edge with label not found: " << tokens[label_index];
            throw FormatError(msg.str());
          }
          e = it->second;
        }

        for (int i = 0; i < static_cast<int>(_edge_maps.size()); ++i) {
          _edge_maps[i].second->set(e, tokens[map_index[i]]);
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readAttributes() {

      std::set<std::string> read_attr;

      char c;
      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::string attr, token;
        if (!_reader_bits::readToken(line, attr))
          throw FormatError("Attribute name not found");
        if (!_reader_bits::readToken(line, token))
          throw FormatError("Attribute value not found");
        if (line >> c)
          throw FormatError("Extra character at the end of line");

        {
          std::set<std::string>::iterator it = read_attr.find(attr);
          if (it != read_attr.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of attribute: " << attr;
            throw FormatError(msg.str());
          }
          read_attr.insert(attr);
        }

        {
          typename Attributes::iterator it = _attributes.lower_bound(attr);
          while (it != _attributes.end() && it->first == attr) {
            it->second->set(token);
            ++it;
          }
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
      for (typename Attributes::iterator it = _attributes.begin();
           it != _attributes.end(); ++it) {
        if (read_attr.find(it->first) == read_attr.end()) {
          std::ostringstream msg;
          msg << "Attribute not found: " << it->first;
          throw FormatError(msg.str());
        }
      }
    }

  public:

    /// \name Execution of the Reader
    /// @{

    /// \brief Start the batch processing
    ///
    /// This function starts the batch processing
    void run() {

      LEMON_ASSERT(_is != 0, "This reader assigned to an other reader");

      bool nodes_done = _skip_nodes;
      bool edges_done = _skip_edges;
      bool attributes_done = false;

      line_num = 0;
      readLine();
      skipSection();

      while (readSuccess()) {
        try {
          char c;
          std::string section, caption;
          line >> c;
          _reader_bits::readToken(line, section);
          _reader_bits::readToken(line, caption);

          if (line >> c)
            throw FormatError("Extra character at the end of line");

          if (section == "nodes" && !nodes_done) {
            if (_nodes_caption.empty() || _nodes_caption == caption) {
              readNodes();
              nodes_done = true;
            }
          } else if ((section == "edges" || section == "arcs") &&
                     !edges_done) {
            if (_edges_caption.empty() || _edges_caption == caption) {
              readEdges();
              edges_done = true;
            }
          } else if (section == "attributes" && !attributes_done) {
            if (_attributes_caption.empty() || _attributes_caption == caption) {
              readAttributes();
              attributes_done = true;
            }
          } else {
            readLine();
            skipSection();
          }
        } catch (FormatError& error) {
          error.line(line_num);
          error.file(_filename);
          throw;
        }
      }

      if (!nodes_done) {
        throw FormatError("Section @nodes not found");
      }

      if (!edges_done) {
        throw FormatError("Section @edges not found");
      }

      if (!attributes_done && !_attributes.empty()) {
        throw FormatError("Section @attributes not found");
      }

    }

    /// @}

  };

  /// \ingroup lemon_io
  ///
  /// \brief Return a \ref lemon::GraphReader "GraphReader" class
  ///
  /// This function just returns a \ref lemon::GraphReader "GraphReader" class.
  ///
  /// With this function a graph can be read from an
  /// \ref lgf-format "LGF" file or input stream with several maps and
  /// attributes. For example, there is weighted matching problem on a
  /// graph, i.e. a graph with a \e weight map on the edges. This
  /// graph can be read with the following code:
  ///
  ///\code
  ///ListGraph graph;
  ///ListGraph::EdgeMap<int> weight(graph);
  ///graphReader(graph, std::cin).
  ///  edgeMap("weight", weight).
  ///  run();
  ///\endcode
  ///
  /// For a complete documentation, please see the
  /// \ref lemon::GraphReader "GraphReader"
  /// class documentation.
  /// \warning Don't forget to put the \ref lemon::GraphReader::run() "run()"
  /// to the end of the parameter list.
  /// \relates GraphReader
  /// \sa graphReader(TGR& graph, const std::string& fn)
  /// \sa graphReader(TGR& graph, const char* fn)
  template <typename TGR>
  GraphReader<TGR> graphReader(TGR& graph, std::istream& is) {
    GraphReader<TGR> tmp(graph, is);
    return tmp;
  }

  /// \brief Return a \ref GraphReader class
  ///
  /// This function just returns a \ref GraphReader class.
  /// \relates GraphReader
  /// \sa graphReader(TGR& graph, std::istream& is)
  template <typename TGR>
  GraphReader<TGR> graphReader(TGR& graph, const std::string& fn) {
    GraphReader<TGR> tmp(graph, fn);
    return tmp;
  }

  /// \brief Return a \ref GraphReader class
  ///
  /// This function just returns a \ref GraphReader class.
  /// \relates GraphReader
  /// \sa graphReader(TGR& graph, std::istream& is)
  template <typename TGR>
  GraphReader<TGR> graphReader(TGR& graph, const char* fn) {
    GraphReader<TGR> tmp(graph, fn);
    return tmp;
  }

  template <typename BGR>
  class BpGraphReader;

  template <typename TBGR>
  BpGraphReader<TBGR> bpGraphReader(TBGR& graph, std::istream& is = std::cin);
  template <typename TBGR>
  BpGraphReader<TBGR> bpGraphReader(TBGR& graph, const std::string& fn);
  template <typename TBGR>
  BpGraphReader<TBGR> bpGraphReader(TBGR& graph, const char *fn);

  /// \ingroup lemon_io
  ///
  /// \brief \ref lgf-format "LGF" reader for bipartite graphs
  ///
  /// This utility reads an \ref lgf-format "LGF" file.
  ///
  /// It can be used almost the same way as \c GraphReader, but it
  /// reads the red and blue nodes from separate sections, and these
  /// sections can contain different set of maps.
  ///
  /// The red and blue node maps are read from the corresponding
  /// sections. If a map is defined with the same name in both of
  /// these sections, then it can be read as a node map.
  template <typename BGR>
  class BpGraphReader {
  public:

    typedef BGR Graph;

  private:

    TEMPLATE_BPGRAPH_TYPEDEFS(BGR);

    std::istream* _is;
    bool local_is;
    std::string _filename;

    BGR& _graph;

    std::string _nodes_caption;
    std::string _edges_caption;
    std::string _attributes_caption;

    typedef std::map<std::string, RedNode> RedNodeIndex;
    RedNodeIndex _red_node_index;
    typedef std::map<std::string, BlueNode> BlueNodeIndex;
    BlueNodeIndex _blue_node_index;
    typedef std::map<std::string, Edge> EdgeIndex;
    EdgeIndex _edge_index;

    typedef std::vector<std::pair<std::string,
      _reader_bits::MapStorageBase<RedNode>*> > RedNodeMaps;
    RedNodeMaps _red_node_maps;
    typedef std::vector<std::pair<std::string,
      _reader_bits::MapStorageBase<BlueNode>*> > BlueNodeMaps;
    BlueNodeMaps _blue_node_maps;

    typedef std::vector<std::pair<std::string,
      _reader_bits::MapStorageBase<Edge>*> > EdgeMaps;
    EdgeMaps _edge_maps;

    typedef std::multimap<std::string, _reader_bits::ValueStorageBase*>
      Attributes;
    Attributes _attributes;

    bool _use_nodes;
    bool _use_edges;

    bool _skip_nodes;
    bool _skip_edges;

    int line_num;
    std::istringstream line;

  public:

    /// \brief Constructor
    ///
    /// Construct an undirected graph reader, which reads from the given
    /// input stream.
    BpGraphReader(BGR& graph, std::istream& is = std::cin)
      : _is(&is), local_is(false), _graph(graph),
        _use_nodes(false), _use_edges(false),
        _skip_nodes(false), _skip_edges(false) {}

    /// \brief Constructor
    ///
    /// Construct an undirected graph reader, which reads from the given
    /// file.
    BpGraphReader(BGR& graph, const std::string& fn)
      : _is(new std::ifstream(fn.c_str())), local_is(true),
        _filename(fn), _graph(graph),
        _use_nodes(false), _use_edges(false),
        _skip_nodes(false), _skip_edges(false) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Constructor
    ///
    /// Construct an undirected graph reader, which reads from the given
    /// file.
    BpGraphReader(BGR& graph, const char* fn)
      : _is(new std::ifstream(fn)), local_is(true),
        _filename(fn), _graph(graph),
        _use_nodes(false), _use_edges(false),
        _skip_nodes(false), _skip_edges(false) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Destructor
    ~BpGraphReader() {
      for (typename RedNodeMaps::iterator it = _red_node_maps.begin();
           it != _red_node_maps.end(); ++it) {
        delete it->second;
      }

      for (typename BlueNodeMaps::iterator it = _blue_node_maps.begin();
           it != _blue_node_maps.end(); ++it) {
        delete it->second;
      }

      for (typename EdgeMaps::iterator it = _edge_maps.begin();
           it != _edge_maps.end(); ++it) {
        delete it->second;
      }

      for (typename Attributes::iterator it = _attributes.begin();
           it != _attributes.end(); ++it) {
        delete it->second;
      }

      if (local_is) {
        delete _is;
      }

    }

  private:
    template <typename TBGR>
    friend BpGraphReader<TBGR> bpGraphReader(TBGR& graph, std::istream& is);
    template <typename TBGR>
    friend BpGraphReader<TBGR> bpGraphReader(TBGR& graph,
                                             const std::string& fn);
    template <typename TBGR>
    friend BpGraphReader<TBGR> bpGraphReader(TBGR& graph, const char *fn);

    BpGraphReader(BpGraphReader& other)
      : _is(other._is), local_is(other.local_is), _graph(other._graph),
        _use_nodes(other._use_nodes), _use_edges(other._use_edges),
        _skip_nodes(other._skip_nodes), _skip_edges(other._skip_edges) {

      other._is = 0;
      other.local_is = false;

      _red_node_index.swap(other._red_node_index);
      _blue_node_index.swap(other._blue_node_index);
      _edge_index.swap(other._edge_index);

      _red_node_maps.swap(other._red_node_maps);
      _blue_node_maps.swap(other._blue_node_maps);
      _edge_maps.swap(other._edge_maps);
      _attributes.swap(other._attributes);

      _nodes_caption = other._nodes_caption;
      _edges_caption = other._edges_caption;
      _attributes_caption = other._attributes_caption;

    }

    BpGraphReader& operator=(const BpGraphReader&);

  public:

    /// \name Reading Rules
    /// @{

    /// \brief Node map reading rule
    ///
    /// Add a node map reading rule to the reader.
    template <typename Map>
    BpGraphReader& nodeMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Node, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<RedNode>* red_storage =
        new _reader_bits::MapStorage<RedNode, Map>(map);
      _red_node_maps.push_back(std::make_pair(caption, red_storage));
      _reader_bits::MapStorageBase<BlueNode>* blue_storage =
        new _reader_bits::MapStorage<BlueNode, Map>(map);
      _blue_node_maps.push_back(std::make_pair(caption, blue_storage));
      return *this;
    }

    /// \brief Node map reading rule
    ///
    /// Add a node map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    BpGraphReader& nodeMap(const std::string& caption, Map& map,
                           const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Node, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<RedNode>* red_storage =
        new _reader_bits::MapStorage<RedNode, Map, Converter>(map, converter);
      _red_node_maps.push_back(std::make_pair(caption, red_storage));
      _reader_bits::MapStorageBase<BlueNode>* blue_storage =
        new _reader_bits::MapStorage<BlueNode, Map, Converter>(map, converter);
      _blue_node_maps.push_back(std::make_pair(caption, blue_storage));
      return *this;
    }

    /// Add a red node map reading rule to the reader.
    template <typename Map>
    BpGraphReader& redNodeMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<RedNode, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<RedNode>* storage =
        new _reader_bits::MapStorage<RedNode, Map>(map);
      _red_node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Red node map reading rule
    ///
    /// Add a red node map node reading rule with specialized converter to
    /// the reader.
    template <typename Map, typename Converter>
    BpGraphReader& redNodeMap(const std::string& caption, Map& map,
                              const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<RedNode, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<RedNode>* storage =
        new _reader_bits::MapStorage<RedNode, Map, Converter>(map, converter);
      _red_node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// Add a blue node map reading rule to the reader.
    template <typename Map>
    BpGraphReader& blueNodeMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<BlueNode, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<BlueNode>* storage =
        new _reader_bits::MapStorage<BlueNode, Map>(map);
      _blue_node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Blue node map reading rule
    ///
    /// Add a blue node map reading rule with specialized converter to
    /// the reader.
    template <typename Map, typename Converter>
    BpGraphReader& blueNodeMap(const std::string& caption, Map& map,
                               const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<BlueNode, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<BlueNode>* storage =
        new _reader_bits::MapStorage<BlueNode, Map, Converter>(map, converter);
      _blue_node_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Edge map reading rule
    ///
    /// Add an edge map reading rule to the reader.
    template <typename Map>
    BpGraphReader& edgeMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Edge, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* storage =
        new _reader_bits::MapStorage<Edge, Map>(map);
      _edge_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Edge map reading rule
    ///
    /// Add an edge map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    BpGraphReader& edgeMap(const std::string& caption, Map& map,
                          const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Edge, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* storage =
        new _reader_bits::MapStorage<Edge, Map, Converter>(map, converter);
      _edge_maps.push_back(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Arc map reading rule
    ///
    /// Add an arc map reading rule to the reader.
    template <typename Map>
    BpGraphReader& arcMap(const std::string& caption, Map& map) {
      checkConcept<concepts::WriteMap<Arc, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* forward_storage =
        new _reader_bits::GraphArcMapStorage<Graph, true, Map>(_graph, map);
      _edge_maps.push_back(std::make_pair('+' + caption, forward_storage));
      _reader_bits::MapStorageBase<Edge>* backward_storage =
        new _reader_bits::GraphArcMapStorage<BGR, false, Map>(_graph, map);
      _edge_maps.push_back(std::make_pair('-' + caption, backward_storage));
      return *this;
    }

    /// \brief Arc map reading rule
    ///
    /// Add an arc map reading rule with specialized converter to the
    /// reader.
    template <typename Map, typename Converter>
    BpGraphReader& arcMap(const std::string& caption, Map& map,
                          const Converter& converter = Converter()) {
      checkConcept<concepts::WriteMap<Arc, typename Map::Value>, Map>();
      _reader_bits::MapStorageBase<Edge>* forward_storage =
        new _reader_bits::GraphArcMapStorage<BGR, true, Map, Converter>
        (_graph, map, converter);
      _edge_maps.push_back(std::make_pair('+' + caption, forward_storage));
      _reader_bits::MapStorageBase<Edge>* backward_storage =
        new _reader_bits::GraphArcMapStorage<BGR, false, Map, Converter>
        (_graph, map, converter);
      _edge_maps.push_back(std::make_pair('-' + caption, backward_storage));
      return *this;
    }

    /// \brief Attribute reading rule
    ///
    /// Add an attribute reading rule to the reader.
    template <typename Value>
    BpGraphReader& attribute(const std::string& caption, Value& value) {
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Value>(value);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Attribute reading rule
    ///
    /// Add an attribute reading rule with specialized converter to the
    /// reader.
    template <typename Value, typename Converter>
    BpGraphReader& attribute(const std::string& caption, Value& value,
                             const Converter& converter = Converter()) {
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Value, Converter>(value, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Node reading rule
    ///
    /// Add a node reading rule to reader.
    BpGraphReader& node(const std::string& caption, Node& node) {
      typedef _reader_bits::DoubleMapLookUpConverter<
        Node, RedNodeIndex, BlueNodeIndex> Converter;
      Converter converter(_red_node_index, _blue_node_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Node, Converter>(node, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Red node reading rule
    ///
    /// Add a red node reading rule to reader.
    BpGraphReader& redNode(const std::string& caption, RedNode& node) {
      typedef _reader_bits::MapLookUpConverter<RedNode> Converter;
      Converter converter(_red_node_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<RedNode, Converter>(node, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Blue node reading rule
    ///
    /// Add a blue node reading rule to reader.
    BpGraphReader& blueNode(const std::string& caption, BlueNode& node) {
      typedef _reader_bits::MapLookUpConverter<BlueNode> Converter;
      Converter converter(_blue_node_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<BlueNode, Converter>(node, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Edge reading rule
    ///
    /// Add an edge reading rule to reader.
    BpGraphReader& edge(const std::string& caption, Edge& edge) {
      typedef _reader_bits::MapLookUpConverter<Edge> Converter;
      Converter converter(_edge_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Edge, Converter>(edge, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// \brief Arc reading rule
    ///
    /// Add an arc reading rule to reader.
    BpGraphReader& arc(const std::string& caption, Arc& arc) {
      typedef _reader_bits::GraphArcLookUpConverter<BGR> Converter;
      Converter converter(_graph, _edge_index);
      _reader_bits::ValueStorageBase* storage =
        new _reader_bits::ValueStorage<Arc, Converter>(arc, converter);
      _attributes.insert(std::make_pair(caption, storage));
      return *this;
    }

    /// @}

    /// \name Select Section by Name
    /// @{

    /// \brief Set \c \@nodes section to be read
    ///
    /// Set \c \@nodes section to be read.
    BpGraphReader& nodes(const std::string& caption) {
      _nodes_caption = caption;
      return *this;
    }

    /// \brief Set \c \@edges section to be read
    ///
    /// Set \c \@edges section to be read.
    BpGraphReader& edges(const std::string& caption) {
      _edges_caption = caption;
      return *this;
    }

    /// \brief Set \c \@attributes section to be read
    ///
    /// Set \c \@attributes section to be read.
    BpGraphReader& attributes(const std::string& caption) {
      _attributes_caption = caption;
      return *this;
    }

    /// @}

    /// \name Using Previously Constructed Node or Edge Set
    /// @{

    /// \brief Use previously constructed node set
    ///
    /// Use previously constructed node set, and specify the node
    /// label map.
    template <typename Map>
    BpGraphReader& useNodes(const Map& map) {
      checkConcept<concepts::ReadMap<Node, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_nodes, "Multiple usage of useNodes() member");
      _use_nodes = true;
      _writer_bits::DefaultConverter<typename Map::Value> converter;
      for (RedNodeIt n(_graph); n != INVALID; ++n) {
        _red_node_index.insert(std::make_pair(converter(map[n]), n));
      }
      for (BlueNodeIt n(_graph); n != INVALID; ++n) {
        _blue_node_index.insert(std::make_pair(converter(map[n]), n));
      }
      return *this;
    }

    /// \brief Use previously constructed node set
    ///
    /// Use previously constructed node set, and specify the node
    /// label map and a functor which converts the label map values to
    /// \c std::string.
    template <typename Map, typename Converter>
    BpGraphReader& useNodes(const Map& map,
                            const Converter& converter = Converter()) {
      checkConcept<concepts::ReadMap<Node, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_nodes, "Multiple usage of useNodes() member");
      _use_nodes = true;
      for (RedNodeIt n(_graph); n != INVALID; ++n) {
        _red_node_index.insert(std::make_pair(converter(map[n]), n));
      }
      for (BlueNodeIt n(_graph); n != INVALID; ++n) {
        _blue_node_index.insert(std::make_pair(converter(map[n]), n));
      }
      return *this;
    }

    /// \brief Use previously constructed edge set
    ///
    /// Use previously constructed edge set, and specify the edge
    /// label map.
    template <typename Map>
    BpGraphReader& useEdges(const Map& map) {
      checkConcept<concepts::ReadMap<Edge, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_edges, "Multiple usage of useEdges() member");
      _use_edges = true;
      _writer_bits::DefaultConverter<typename Map::Value> converter;
      for (EdgeIt a(_graph); a != INVALID; ++a) {
        _edge_index.insert(std::make_pair(converter(map[a]), a));
      }
      return *this;
    }

    /// \brief Use previously constructed edge set
    ///
    /// Use previously constructed edge set, and specify the edge
    /// label map and a functor which converts the label map values to
    /// \c std::string.
    template <typename Map, typename Converter>
    BpGraphReader& useEdges(const Map& map,
                            const Converter& converter = Converter()) {
      checkConcept<concepts::ReadMap<Edge, typename Map::Value>, Map>();
      LEMON_ASSERT(!_use_edges, "Multiple usage of useEdges() member");
      _use_edges = true;
      for (EdgeIt a(_graph); a != INVALID; ++a) {
        _edge_index.insert(std::make_pair(converter(map[a]), a));
      }
      return *this;
    }

    /// \brief Skip the reading of node section
    ///
    /// Omit the reading of the node section. This implies that each node
    /// map reading rule will be abandoned, and the nodes of the graph
    /// will not be constructed, which usually cause that the edge set
    /// could not be read due to lack of node name
    /// could not be read due to lack of node name resolving.
    /// Therefore \c skipEdges() function should also be used, or
    /// \c useNodes() should be used to specify the label of the nodes.
    BpGraphReader& skipNodes() {
      LEMON_ASSERT(!_skip_nodes, "Skip nodes already set");
      _skip_nodes = true;
      return *this;
    }

    /// \brief Skip the reading of edge section
    ///
    /// Omit the reading of the edge section. This implies that each edge
    /// map reading rule will be abandoned, and the edges of the graph
    /// will not be constructed.
    BpGraphReader& skipEdges() {
      LEMON_ASSERT(!_skip_edges, "Skip edges already set");
      _skip_edges = true;
      return *this;
    }

    /// @}

  private:

    bool readLine() {
      std::string str;
      while(++line_num, std::getline(*_is, str)) {
        line.clear(); line.str(str);
        char c;
        if (line >> std::ws >> c && c != '#') {
          line.putback(c);
          return true;
        }
      }
      return false;
    }

    bool readSuccess() {
      return static_cast<bool>(*_is);
    }

    void skipSection() {
      char c;
      while (readSuccess() && line >> c && c != '@') {
        readLine();
      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readRedNodes() {

      std::vector<int> map_index(_red_node_maps.size());
      int map_num, label_index;

      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        if (!_red_node_maps.empty())
          throw FormatError("Cannot find map names");
        return;
      }
      line.putback(c);

      {
        std::map<std::string, int> maps;

        std::string map;
        int index = 0;
        while (_reader_bits::readToken(line, map)) {
          if (maps.find(map) != maps.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of red node map: " << map;
            throw FormatError(msg.str());
          }
          maps.insert(std::make_pair(map, index));
          ++index;
        }

        for (int i = 0; i < static_cast<int>(_red_node_maps.size()); ++i) {
          std::map<std::string, int>::iterator jt =
            maps.find(_red_node_maps[i].first);
          if (jt == maps.end()) {
            std::ostringstream msg;
            msg << "Map not found: " << _red_node_maps[i].first;
            throw FormatError(msg.str());
          }
          map_index[i] = jt->second;
        }

        {
          std::map<std::string, int>::iterator jt = maps.find("label");
          if (jt != maps.end()) {
            label_index = jt->second;
          } else {
            label_index = -1;
          }
        }
        map_num = maps.size();
      }

      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::vector<std::string> tokens(map_num);
        for (int i = 0; i < map_num; ++i) {
          if (!_reader_bits::readToken(line, tokens[i])) {
            std::ostringstream msg;
            msg << "Column not found (" << i + 1 << ")";
            throw FormatError(msg.str());
          }
        }
        if (line >> std::ws >> c)
          throw FormatError("Extra character at the end of line");

        RedNode n;
        if (!_use_nodes) {
          n = _graph.addRedNode();
          if (label_index != -1)
            _red_node_index.insert(std::make_pair(tokens[label_index], n));
        } else {
          if (label_index == -1)
            throw FormatError("Label map not found");
          typename std::map<std::string, RedNode>::iterator it =
            _red_node_index.find(tokens[label_index]);
          if (it == _red_node_index.end()) {
            std::ostringstream msg;
            msg << "Node with label not found: " << tokens[label_index];
            throw FormatError(msg.str());
          }
          n = it->second;
        }

        for (int i = 0; i < static_cast<int>(_red_node_maps.size()); ++i) {
          _red_node_maps[i].second->set(n, tokens[map_index[i]]);
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readBlueNodes() {

      std::vector<int> map_index(_blue_node_maps.size());
      int map_num, label_index;

      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        if (!_blue_node_maps.empty())
          throw FormatError("Cannot find map names");
        return;
      }
      line.putback(c);

      {
        std::map<std::string, int> maps;

        std::string map;
        int index = 0;
        while (_reader_bits::readToken(line, map)) {
          if (maps.find(map) != maps.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of blue node map: " << map;
            throw FormatError(msg.str());
          }
          maps.insert(std::make_pair(map, index));
          ++index;
        }

        for (int i = 0; i < static_cast<int>(_blue_node_maps.size()); ++i) {
          std::map<std::string, int>::iterator jt =
            maps.find(_blue_node_maps[i].first);
          if (jt == maps.end()) {
            std::ostringstream msg;
            msg << "Map not found: " << _blue_node_maps[i].first;
            throw FormatError(msg.str());
          }
          map_index[i] = jt->second;
        }

        {
          std::map<std::string, int>::iterator jt = maps.find("label");
          if (jt != maps.end()) {
            label_index = jt->second;
          } else {
            label_index = -1;
          }
        }
        map_num = maps.size();
      }

      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::vector<std::string> tokens(map_num);
        for (int i = 0; i < map_num; ++i) {
          if (!_reader_bits::readToken(line, tokens[i])) {
            std::ostringstream msg;
            msg << "Column not found (" << i + 1 << ")";
            throw FormatError(msg.str());
          }
        }
        if (line >> std::ws >> c)
          throw FormatError("Extra character at the end of line");

        BlueNode n;
        if (!_use_nodes) {
          n = _graph.addBlueNode();
          if (label_index != -1)
            _blue_node_index.insert(std::make_pair(tokens[label_index], n));
        } else {
          if (label_index == -1)
            throw FormatError("Label map not found");
          typename std::map<std::string, BlueNode>::iterator it =
            _blue_node_index.find(tokens[label_index]);
          if (it == _blue_node_index.end()) {
            std::ostringstream msg;
            msg << "Node with label not found: " << tokens[label_index];
            throw FormatError(msg.str());
          }
          n = it->second;
        }

        for (int i = 0; i < static_cast<int>(_blue_node_maps.size()); ++i) {
          _blue_node_maps[i].second->set(n, tokens[map_index[i]]);
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readEdges() {

      std::vector<int> map_index(_edge_maps.size());
      int map_num, label_index;

      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        if (!_edge_maps.empty())
          throw FormatError("Cannot find map names");
        return;
      }
      line.putback(c);

      {
        std::map<std::string, int> maps;

        std::string map;
        int index = 0;
        while (_reader_bits::readToken(line, map)) {
          if (maps.find(map) != maps.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of edge map: " << map;
            throw FormatError(msg.str());
          }
          maps.insert(std::make_pair(map, index));
          ++index;
        }

        for (int i = 0; i < static_cast<int>(_edge_maps.size()); ++i) {
          std::map<std::string, int>::iterator jt =
            maps.find(_edge_maps[i].first);
          if (jt == maps.end()) {
            std::ostringstream msg;
            msg << "Map not found: " << _edge_maps[i].first;
            throw FormatError(msg.str());
          }
          map_index[i] = jt->second;
        }

        {
          std::map<std::string, int>::iterator jt = maps.find("label");
          if (jt != maps.end()) {
            label_index = jt->second;
          } else {
            label_index = -1;
          }
        }
        map_num = maps.size();
      }

      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::string source_token;
        std::string target_token;

        if (!_reader_bits::readToken(line, source_token))
          throw FormatError("Red node not found");

        if (!_reader_bits::readToken(line, target_token))
          throw FormatError("Blue node not found");

        std::vector<std::string> tokens(map_num);
        for (int i = 0; i < map_num; ++i) {
          if (!_reader_bits::readToken(line, tokens[i])) {
            std::ostringstream msg;
            msg << "Column not found (" << i + 1 << ")";
            throw FormatError(msg.str());
          }
        }
        if (line >> std::ws >> c)
          throw FormatError("Extra character at the end of line");

        Edge e;
        if (!_use_edges) {
          typename RedNodeIndex::iterator rit =
            _red_node_index.find(source_token);
          if (rit == _red_node_index.end()) {
            std::ostringstream msg;
            msg << "Item not found: " << source_token;
            throw FormatError(msg.str());
          }
          RedNode source = rit->second;
          typename BlueNodeIndex::iterator it =
            _blue_node_index.find(target_token);
          if (it == _blue_node_index.end()) {
            std::ostringstream msg;
            msg << "Item not found: " << target_token;
            throw FormatError(msg.str());
          }
          BlueNode target = it->second;

          // It is checked that source is red and
          // target is blue, so this should be safe:
          e = _graph.addEdge(source, target);
          if (label_index != -1)
            _edge_index.insert(std::make_pair(tokens[label_index], e));
        } else {
          if (label_index == -1)
            throw FormatError("Label map not found");
          typename std::map<std::string, Edge>::iterator it =
            _edge_index.find(tokens[label_index]);
          if (it == _edge_index.end()) {
            std::ostringstream msg;
            msg << "Edge with label not found: " << tokens[label_index];
            throw FormatError(msg.str());
          }
          e = it->second;
        }

        for (int i = 0; i < static_cast<int>(_edge_maps.size()); ++i) {
          _edge_maps[i].second->set(e, tokens[map_index[i]]);
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readAttributes() {

      std::set<std::string> read_attr;

      char c;
      while (readLine() && line >> c && c != '@') {
        line.putback(c);

        std::string attr, token;
        if (!_reader_bits::readToken(line, attr))
          throw FormatError("Attribute name not found");
        if (!_reader_bits::readToken(line, token))
          throw FormatError("Attribute value not found");
        if (line >> c)
          throw FormatError("Extra character at the end of line");

        {
          std::set<std::string>::iterator it = read_attr.find(attr);
          if (it != read_attr.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of attribute: " << attr;
            throw FormatError(msg.str());
          }
          read_attr.insert(attr);
        }

        {
          typename Attributes::iterator it = _attributes.lower_bound(attr);
          while (it != _attributes.end() && it->first == attr) {
            it->second->set(token);
            ++it;
          }
        }

      }
      if (readSuccess()) {
        line.putback(c);
      }
      for (typename Attributes::iterator it = _attributes.begin();
           it != _attributes.end(); ++it) {
        if (read_attr.find(it->first) == read_attr.end()) {
          std::ostringstream msg;
          msg << "Attribute not found: " << it->first;
          throw FormatError(msg.str());
        }
      }
    }

  public:

    /// \name Execution of the Reader
    /// @{

    /// \brief Start the batch processing
    ///
    /// This function starts the batch processing
    void run() {

      LEMON_ASSERT(_is != 0, "This reader assigned to an other reader");

      bool red_nodes_done = _skip_nodes;
      bool blue_nodes_done = _skip_nodes;
      bool edges_done = _skip_edges;
      bool attributes_done = false;

      line_num = 0;
      readLine();
      skipSection();

      while (readSuccess()) {
        try {
          char c;
          std::string section, caption;
          line >> c;
          _reader_bits::readToken(line, section);
          _reader_bits::readToken(line, caption);

          if (line >> c)
            throw FormatError("Extra character at the end of line");

          if (section == "red_nodes" && !red_nodes_done) {
            if (_nodes_caption.empty() || _nodes_caption == caption) {
              readRedNodes();
              red_nodes_done = true;
            }
          } else if (section == "blue_nodes" && !blue_nodes_done) {
            if (_nodes_caption.empty() || _nodes_caption == caption) {
              readBlueNodes();
              blue_nodes_done = true;
            }
          } else if ((section == "edges" || section == "arcs") &&
                     !edges_done) {
            if (_edges_caption.empty() || _edges_caption == caption) {
              readEdges();
              edges_done = true;
            }
          } else if (section == "attributes" && !attributes_done) {
            if (_attributes_caption.empty() || _attributes_caption == caption) {
              readAttributes();
              attributes_done = true;
            }
          } else {
            readLine();
            skipSection();
          }
        } catch (FormatError& error) {
          error.line(line_num);
          error.file(_filename);
          throw;
        }
      }

      if (!red_nodes_done) {
        throw FormatError("Section @red_nodes not found");
      }

      if (!blue_nodes_done) {
        throw FormatError("Section @blue_nodes not found");
      }

      if (!edges_done) {
        throw FormatError("Section @edges not found");
      }

      if (!attributes_done && !_attributes.empty()) {
        throw FormatError("Section @attributes not found");
      }

    }

    /// @}

  };

  /// \ingroup lemon_io
  ///
  /// \brief Return a \ref lemon::BpGraphReader "BpGraphReader" class
  ///
  /// This function just returns a \ref lemon::BpGraphReader
  /// "BpGraphReader" class.
  ///
  /// With this function a graph can be read from an
  /// \ref lgf-format "LGF" file or input stream with several maps and
  /// attributes. For example, there is bipartite weighted matching problem
  /// on a graph, i.e. a graph with a \e weight map on the edges. This
  /// graph can be read with the following code:
  ///
  ///\code
  ///ListBpGraph graph;
  ///ListBpGraph::EdgeMap<int> weight(graph);
  ///bpGraphReader(graph, std::cin).
  ///  edgeMap("weight", weight).
  ///  run();
  ///\endcode
  ///
  /// For a complete documentation, please see the
  /// \ref lemon::BpGraphReader "BpGraphReader"
  /// class documentation.
  /// \warning Don't forget to put the \ref lemon::BpGraphReader::run() "run()"
  /// to the end of the parameter list.
  /// \relates BpGraphReader
  /// \sa bpGraphReader(TBGR& graph, const std::string& fn)
  /// \sa bpGraphReader(TBGR& graph, const char* fn)
  template <typename TBGR>
  BpGraphReader<TBGR> bpGraphReader(TBGR& graph, std::istream& is) {
    BpGraphReader<TBGR> tmp(graph, is);
    return tmp;
  }

  /// \brief Return a \ref BpGraphReader class
  ///
  /// This function just returns a \ref BpGraphReader class.
  /// \relates BpGraphReader
  /// \sa bpGraphReader(TBGR& graph, std::istream& is)
  template <typename TBGR>
  BpGraphReader<TBGR> bpGraphReader(TBGR& graph, const std::string& fn) {
    BpGraphReader<TBGR> tmp(graph, fn);
    return tmp;
  }

  /// \brief Return a \ref BpGraphReader class
  ///
  /// This function just returns a \ref BpGraphReader class.
  /// \relates BpGraphReader
  /// \sa bpGraphReader(TBGR& graph, std::istream& is)
  template <typename TBGR>
  BpGraphReader<TBGR> bpGraphReader(TBGR& graph, const char* fn) {
    BpGraphReader<TBGR> tmp(graph, fn);
    return tmp;
  }

  class SectionReader;

  SectionReader sectionReader(std::istream& is);
  SectionReader sectionReader(const std::string& fn);
  SectionReader sectionReader(const char* fn);

  /// \ingroup lemon_io
  ///
  /// \brief Section reader class
  ///
  /// In the \ref lgf-format "LGF" file extra sections can be placed,
  /// which contain any data in arbitrary format. Such sections can be
  /// read with this class. A reading rule can be added to the class
  /// with two different functions. With the \c sectionLines() function a
  /// functor can process the section line-by-line, while with the \c
  /// sectionStream() member the section can be read from an input
  /// stream.
  class SectionReader {
  private:

    std::istream* _is;
    bool local_is;
    std::string _filename;

    typedef std::map<std::string, _reader_bits::Section*> Sections;
    Sections _sections;

    int line_num;
    std::istringstream line;

  public:

    /// \brief Constructor
    ///
    /// Construct a section reader, which reads from the given input
    /// stream.
    SectionReader(std::istream& is)
      : _is(&is), local_is(false) {}

    /// \brief Constructor
    ///
    /// Construct a section reader, which reads from the given file.
    SectionReader(const std::string& fn)
      : _is(new std::ifstream(fn.c_str())), local_is(true),
        _filename(fn) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Constructor
    ///
    /// Construct a section reader, which reads from the given file.
    SectionReader(const char* fn)
      : _is(new std::ifstream(fn)), local_is(true),
        _filename(fn) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Destructor
    ~SectionReader() {
      for (Sections::iterator it = _sections.begin();
           it != _sections.end(); ++it) {
        delete it->second;
      }

      if (local_is) {
        delete _is;
      }

    }

  private:

    friend SectionReader sectionReader(std::istream& is);
    friend SectionReader sectionReader(const std::string& fn);
    friend SectionReader sectionReader(const char* fn);

    SectionReader(SectionReader& other)
      : _is(other._is), local_is(other.local_is) {

      other._is = 0;
      other.local_is = false;

      _sections.swap(other._sections);
    }

    SectionReader& operator=(const SectionReader&);

  public:

    /// \name Section Readers
    /// @{

    /// \brief Add a section processor with line oriented reading
    ///
    /// The first parameter is the type descriptor of the section, the
    /// second is a functor, which takes just one \c std::string
    /// parameter. At the reading process, each line of the section
    /// will be given to the functor object. However, the empty lines
    /// and the comment lines are filtered out, and the leading
    /// whitespaces are trimmed from each processed string.
    ///
    /// For example, let's see a section, which contain several
    /// integers, which should be inserted into a vector.
    ///\code
    ///  @numbers
    ///  12 45 23
    ///  4
    ///  23 6
    ///\endcode
    ///
    /// The functor is implemented as a struct:
    ///\code
    ///  struct NumberSection {
    ///    std::vector<int>& _data;
    ///    NumberSection(std::vector<int>& data) : _data(data) {}
    ///    void operator()(const std::string& line) {
    ///      std::istringstream ls(line);
    ///      int value;
    ///      while (ls >> value) _data.push_back(value);
    ///    }
    ///  };
    ///
    ///  // ...
    ///
    ///  reader.sectionLines("numbers", NumberSection(vec));
    ///\endcode
    template <typename Functor>
    SectionReader& sectionLines(const std::string& type, Functor functor) {
      LEMON_ASSERT(!type.empty(), "Type is empty.");
      LEMON_ASSERT(_sections.find(type) == _sections.end(),
                   "Multiple reading of section.");
      _sections.insert(std::make_pair(type,
        new _reader_bits::LineSection<Functor>(functor)));
      return *this;
    }


    /// \brief Add a section processor with stream oriented reading
    ///
    /// The first parameter is the type of the section, the second is
    /// a functor, which takes an \c std::istream& and an \c int&
    /// parameter, the latter regard to the line number of stream. The
    /// functor can read the input while the section go on, and the
    /// line number should be modified accordingly.
    template <typename Functor>
    SectionReader& sectionStream(const std::string& type, Functor functor) {
      LEMON_ASSERT(!type.empty(), "Type is empty.");
      LEMON_ASSERT(_sections.find(type) == _sections.end(),
                   "Multiple reading of section.");
      _sections.insert(std::make_pair(type,
         new _reader_bits::StreamSection<Functor>(functor)));
      return *this;
    }

    /// @}

  private:

    bool readLine() {
      std::string str;
      while(++line_num, std::getline(*_is, str)) {
        line.clear(); line.str(str);
        char c;
        if (line >> std::ws >> c && c != '#') {
          line.putback(c);
          return true;
        }
      }
      return false;
    }

    bool readSuccess() {
      return static_cast<bool>(*_is);
    }

    void skipSection() {
      char c;
      while (readSuccess() && line >> c && c != '@') {
        readLine();
      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

  public:


    /// \name Execution of the Reader
    /// @{

    /// \brief Start the batch processing
    ///
    /// This function starts the batch processing.
    void run() {

      LEMON_ASSERT(_is != 0, "This reader assigned to an other reader");

      std::set<std::string> extra_sections;

      line_num = 0;
      readLine();
      skipSection();

      while (readSuccess()) {
        try {
          char c;
          std::string section, caption;
          line >> c;
          _reader_bits::readToken(line, section);
          _reader_bits::readToken(line, caption);

          if (line >> c)
            throw FormatError("Extra character at the end of line");

          if (extra_sections.find(section) != extra_sections.end()) {
            std::ostringstream msg;
            msg << "Multiple occurence of section: " << section;
            throw FormatError(msg.str());
          }
          Sections::iterator it = _sections.find(section);
          if (it != _sections.end()) {
            extra_sections.insert(section);
            it->second->process(*_is, line_num);
          }
          readLine();
          skipSection();
        } catch (FormatError& error) {
          error.line(line_num);
          error.file(_filename);
          throw;
        }
      }
      for (Sections::iterator it = _sections.begin();
           it != _sections.end(); ++it) {
        if (extra_sections.find(it->first) == extra_sections.end()) {
          std::ostringstream os;
          os << "Cannot find section: " << it->first;
          throw FormatError(os.str());
        }
      }
    }

    /// @}

  };

  /// \ingroup lemon_io
  ///
  /// \brief Return a \ref SectionReader class
  ///
  /// This function just returns a \ref SectionReader class.
  ///
  /// Please see SectionReader documentation about the custom section
  /// input.
  ///
  /// \relates SectionReader
  /// \sa sectionReader(const std::string& fn)
  /// \sa sectionReader(const char *fn)
  inline SectionReader sectionReader(std::istream& is) {
    SectionReader tmp(is);
    return tmp;
  }

  /// \brief Return a \ref SectionReader class
  ///
  /// This function just returns a \ref SectionReader class.
  /// \relates SectionReader
  /// \sa sectionReader(std::istream& is)
  inline SectionReader sectionReader(const std::string& fn) {
    SectionReader tmp(fn);
    return tmp;
  }

  /// \brief Return a \ref SectionReader class
  ///
  /// This function just returns a \ref SectionReader class.
  /// \relates SectionReader
  /// \sa sectionReader(std::istream& is)
  inline SectionReader sectionReader(const char* fn) {
    SectionReader tmp(fn);
    return tmp;
  }

  /// \ingroup lemon_io
  ///
  /// \brief Reader for the contents of the \ref lgf-format "LGF" file
  ///
  /// This class can be used to read the sections, the map names and
  /// the attributes from a file. Usually, the LEMON programs know
  /// that, which type of graph, which maps and which attributes
  /// should be read from a file, but in general tools (like glemon)
  /// the contents of an LGF file should be guessed somehow. This class
  /// reads the graph and stores the appropriate information for
  /// reading the graph.
  ///
  ///\code
  /// LgfContents contents("graph.lgf");
  /// contents.run();
  ///
  /// // Does it contain any node section and arc section?
  /// if (contents.nodeSectionNum() == 0 || contents.arcSectionNum()) {
  ///   std::cerr << "Failure, cannot find graph." << std::endl;
  ///   return -1;
  /// }
  /// std::cout << "The name of the default node section: "
  ///           << contents.nodeSection(0) << std::endl;
  /// std::cout << "The number of the arc maps: "
  ///           << contents.arcMaps(0).size() << std::endl;
  /// std::cout << "The name of second arc map: "
  ///           << contents.arcMaps(0)[1] << std::endl;
  ///\endcode
  class LgfContents {
  private:

    std::istream* _is;
    bool local_is;

    std::vector<std::string> _node_sections;
    std::vector<std::string> _edge_sections;
    std::vector<std::string> _attribute_sections;
    std::vector<std::string> _extra_sections;

    std::vector<bool> _arc_sections;

    std::vector<std::vector<std::string> > _node_maps;
    std::vector<std::vector<std::string> > _edge_maps;

    std::vector<std::vector<std::string> > _attributes;


    int line_num;
    std::istringstream line;

  public:

    /// \brief Constructor
    ///
    /// Construct an \e LGF contents reader, which reads from the given
    /// input stream.
    LgfContents(std::istream& is)
      : _is(&is), local_is(false) {}

    /// \brief Constructor
    ///
    /// Construct an \e LGF contents reader, which reads from the given
    /// file.
    LgfContents(const std::string& fn)
      : _is(new std::ifstream(fn.c_str())), local_is(true) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Constructor
    ///
    /// Construct an \e LGF contents reader, which reads from the given
    /// file.
    LgfContents(const char* fn)
      : _is(new std::ifstream(fn)), local_is(true) {
      if (!(*_is)) {
        delete _is;
        throw IoError("Cannot open file", fn);
      }
    }

    /// \brief Destructor
    ~LgfContents() {
      if (local_is) delete _is;
    }

  private:

    LgfContents(const LgfContents&);
    LgfContents& operator=(const LgfContents&);

  public:


    /// \name Node Sections
    /// @{

    /// \brief Gives back the number of node sections in the file.
    ///
    /// Gives back the number of node sections in the file.
    int nodeSectionNum() const {
      return _node_sections.size();
    }

    /// \brief Returns the node section name at the given position.
    ///
    /// Returns the node section name at the given position.
    const std::string& nodeSection(int i) const {
      return _node_sections[i];
    }

    /// \brief Gives back the node maps for the given section.
    ///
    /// Gives back the node maps for the given section.
    const std::vector<std::string>& nodeMapNames(int i) const {
      return _node_maps[i];
    }

    /// @}

    /// \name Arc/Edge Sections
    /// @{

    /// \brief Gives back the number of arc/edge sections in the file.
    ///
    /// Gives back the number of arc/edge sections in the file.
    /// \note It is synonym of \c edgeSectionNum().
    int arcSectionNum() const {
      return _edge_sections.size();
    }

    /// \brief Returns the arc/edge section name at the given position.
    ///
    /// Returns the arc/edge section name at the given position.
    /// \note It is synonym of \c edgeSection().
    const std::string& arcSection(int i) const {
      return _edge_sections[i];
    }

    /// \brief Gives back the arc/edge maps for the given section.
    ///
    /// Gives back the arc/edge maps for the given section.
    /// \note It is synonym of \c edgeMapNames().
    const std::vector<std::string>& arcMapNames(int i) const {
      return _edge_maps[i];
    }

    /// @}

    /// \name Synonyms
    /// @{

    /// \brief Gives back the number of arc/edge sections in the file.
    ///
    /// Gives back the number of arc/edge sections in the file.
    /// \note It is synonym of \c arcSectionNum().
    int edgeSectionNum() const {
      return _edge_sections.size();
    }

    /// \brief Returns the section name at the given position.
    ///
    /// Returns the section name at the given position.
    /// \note It is synonym of \c arcSection().
    const std::string& edgeSection(int i) const {
      return _edge_sections[i];
    }

    /// \brief Gives back the edge maps for the given section.
    ///
    /// Gives back the edge maps for the given section.
    /// \note It is synonym of \c arcMapNames().
    const std::vector<std::string>& edgeMapNames(int i) const {
      return _edge_maps[i];
    }

    /// @}

    /// \name Attribute Sections
    /// @{

    /// \brief Gives back the number of attribute sections in the file.
    ///
    /// Gives back the number of attribute sections in the file.
    int attributeSectionNum() const {
      return _attribute_sections.size();
    }

    /// \brief Returns the attribute section name at the given position.
    ///
    /// Returns the attribute section name at the given position.
    const std::string& attributeSectionNames(int i) const {
      return _attribute_sections[i];
    }

    /// \brief Gives back the attributes for the given section.
    ///
    /// Gives back the attributes for the given section.
    const std::vector<std::string>& attributes(int i) const {
      return _attributes[i];
    }

    /// @}

    /// \name Extra Sections
    /// @{

    /// \brief Gives back the number of extra sections in the file.
    ///
    /// Gives back the number of extra sections in the file.
    int extraSectionNum() const {
      return _extra_sections.size();
    }

    /// \brief Returns the extra section type at the given position.
    ///
    /// Returns the section type at the given position.
    const std::string& extraSection(int i) const {
      return _extra_sections[i];
    }

    /// @}

  private:

    bool readLine() {
      std::string str;
      while(++line_num, std::getline(*_is, str)) {
        line.clear(); line.str(str);
        char c;
        if (line >> std::ws >> c && c != '#') {
          line.putback(c);
          return true;
        }
      }
      return false;
    }

    bool readSuccess() {
      return static_cast<bool>(*_is);
    }

    void skipSection() {
      char c;
      while (readSuccess() && line >> c && c != '@') {
        readLine();
      }
      if (readSuccess()) {
        line.putback(c);
      }
    }

    void readMaps(std::vector<std::string>& maps) {
      char c;
      if (!readLine() || !(line >> c) || c == '@') {
        if (readSuccess() && line) line.putback(c);
        return;
      }
      line.putback(c);
      std::string map;
      while (_reader_bits::readToken(line, map)) {
        maps.push_back(map);
      }
    }

    void readAttributes(std::vector<std::string>& attrs) {
      readLine();
      char c;
      while (readSuccess() && line >> c && c != '@') {
        line.putback(c);
        std::string attr;
        _reader_bits::readToken(line, attr);
        attrs.push_back(attr);
        readLine();
      }
      line.putback(c);
    }

  public:

    /// \name Execution of the Contents Reader
    /// @{

    /// \brief Starts the reading
    ///
    /// This function starts the reading.
    void run() {

      readLine();
      skipSection();

      while (readSuccess()) {

        char c;
        line >> c;

        std::string section, caption;
        _reader_bits::readToken(line, section);
        _reader_bits::readToken(line, caption);

        if (section == "nodes") {
          _node_sections.push_back(caption);
          _node_maps.push_back(std::vector<std::string>());
          readMaps(_node_maps.back());
          readLine(); skipSection();
        } else if (section == "arcs" || section == "edges") {
          _edge_sections.push_back(caption);
          _arc_sections.push_back(section == "arcs");
          _edge_maps.push_back(std::vector<std::string>());
          readMaps(_edge_maps.back());
          readLine(); skipSection();
        } else if (section == "attributes") {
          _attribute_sections.push_back(caption);
          _attributes.push_back(std::vector<std::string>());
          readAttributes(_attributes.back());
        } else {
          _extra_sections.push_back(section);
          readLine(); skipSection();
        }
      }
    }

    /// @}

  };
}

#endif
