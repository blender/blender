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

#ifndef LEMON_BITS_ALTERATION_NOTIFIER_H
#define LEMON_BITS_ALTERATION_NOTIFIER_H

#include <vector>
#include <list>

#include <lemon/core.h>
#include <lemon/bits/lock.h>

//\ingroup graphbits
//\file
//\brief Observer notifier for graph alteration observers.

namespace lemon {

  // \ingroup graphbits
  //
  // \brief Notifier class to notify observes about alterations in
  // a container.
  //
  // The simple graphs can be refered as two containers: a node container
  // and an edge container. But they do not store values directly, they
  // are just key continars for more value containers, which are the
  // node and edge maps.
  //
  // The node and edge sets of the graphs can be changed as we add or erase
  // nodes and edges in the graph. LEMON would like to handle easily
  // that the node and edge maps should contain values for all nodes or
  // edges. If we want to check on every indicing if the map contains
  // the current indicing key that cause a drawback in the performance
  // in the library. We use another solution: we notify all maps about
  // an alteration in the graph, which cause only drawback on the
  // alteration of the graph.
  //
  // This class provides an interface to a node or edge container.
  // The first() and next() member functions make possible
  // to iterate on the keys of the container.
  // The id() function returns an integer id for each key.
  // The maxId() function gives back an upper bound of the ids.
  //
  // For the proper functonality of this class, we should notify it
  // about each alteration in the container. The alterations have four type:
  // add(), erase(), build() and clear(). The add() and
  // erase() signal that only one or few items added or erased to or
  // from the graph. If all items are erased from the graph or if a new graph
  // is built from an empty graph, then it can be signaled with the
  // clear() and build() members. Important rule that if we erase items
  // from graphs we should first signal the alteration and after that erase
  // them from the container, on the other way on item addition we should
  // first extend the container and just after that signal the alteration.
  //
  // The alteration can be observed with a class inherited from the
  // ObserverBase nested class. The signals can be handled with
  // overriding the virtual functions defined in the base class.  The
  // observer base can be attached to the notifier with the
  // attach() member and can be detached with detach() function. The
  // alteration handlers should not call any function which signals
  // an other alteration in the same notifier and should not
  // detach any observer from the notifier.
  //
  // Alteration observers try to be exception safe. If an add() or
  // a clear() function throws an exception then the remaining
  // observeres will not be notified and the fulfilled additions will
  // be rolled back by calling the erase() or clear() functions.
  // Hence erase() and clear() should not throw exception.
  // Actullay, they can throw only \ref ImmediateDetach exception,
  // which detach the observer from the notifier.
  //
  // There are some cases, when the alteration observing is not completly
  // reliable. If we want to carry out the node degree in the graph
  // as in the \ref InDegMap and we use the reverseArc(), then it cause
  // unreliable functionality. Because the alteration observing signals
  // only erasing and adding but not the reversing, it will stores bad
  // degrees. Apart form that the subgraph adaptors cannot even signal
  // the alterations because just a setting in the filter map can modify
  // the graph and this cannot be watched in any way.
  //
  // \param _Container The container which is observed.
  // \param _Item The item type which is obserbved.

  template <typename _Container, typename _Item>
  class AlterationNotifier {
  public:

    typedef True Notifier;

    typedef _Container Container;
    typedef _Item Item;

    // \brief Exception which can be called from clear() and
    // erase().
    //
    // From the clear() and erase() function only this
    // exception is allowed to throw. The exception immediatly
    // detaches the current observer from the notifier. Because the
    // clear() and erase() should not throw other exceptions
    // it can be used to invalidate the observer.
    struct ImmediateDetach {};

    // \brief ObserverBase is the base class for the observers.
    //
    // ObserverBase is the abstract base class for the observers.
    // It will be notified about an item was inserted into or
    // erased from the graph.
    //
    // The observer interface contains some pure virtual functions
    // to override. The add() and erase() functions are
    // to notify the oberver when one item is added or erased.
    //
    // The build() and clear() members are to notify the observer
    // about the container is built from an empty container or
    // is cleared to an empty container.
    class ObserverBase {
    protected:
      typedef AlterationNotifier Notifier;

      friend class AlterationNotifier;

      // \brief Default constructor.
      //
      // Default constructor for ObserverBase.
      ObserverBase() : _notifier(0) {}

      // \brief Constructor which attach the observer into notifier.
      //
      // Constructor which attach the observer into notifier.
      ObserverBase(AlterationNotifier& nf) {
        attach(nf);
      }

      // \brief Constructor which attach the obserever to the same notifier.
      //
      // Constructor which attach the obserever to the same notifier as
      // the other observer is attached to.
      ObserverBase(const ObserverBase& copy) {
        if (copy.attached()) {
          attach(*copy.notifier());
        }
      }

      // \brief Destructor
      virtual ~ObserverBase() {
        if (attached()) {
          detach();
        }
      }

      // \brief Attaches the observer into an AlterationNotifier.
      //
      // This member attaches the observer into an AlterationNotifier.
      void attach(AlterationNotifier& nf) {
        nf.attach(*this);
      }

      // \brief Detaches the observer into an AlterationNotifier.
      //
      // This member detaches the observer from an AlterationNotifier.
      void detach() {
        _notifier->detach(*this);
      }

      // \brief Gives back a pointer to the notifier which the map
      // attached into.
      //
      // This function gives back a pointer to the notifier which the map
      // attached into.
      Notifier* notifier() const { return const_cast<Notifier*>(_notifier); }

      // Gives back true when the observer is attached into a notifier.
      bool attached() const { return _notifier != 0; }

    private:

      ObserverBase& operator=(const ObserverBase& copy);

    protected:

      Notifier* _notifier;
      typename std::list<ObserverBase*>::iterator _index;

      // \brief The member function to notificate the observer about an
      // item is added to the container.
      //
      // The add() member function notificates the observer about an item
      // is added to the container. It have to be overrided in the
      // subclasses.
      virtual void add(const Item&) = 0;

      // \brief The member function to notificate the observer about
      // more item is added to the container.
      //
      // The add() member function notificates the observer about more item
      // is added to the container. It have to be overrided in the
      // subclasses.
      virtual void add(const std::vector<Item>& items) = 0;

      // \brief The member function to notificate the observer about an
      // item is erased from the container.
      //
      // The erase() member function notificates the observer about an
      // item is erased from the container. It have to be overrided in
      // the subclasses.
      virtual void erase(const Item&) = 0;

      // \brief The member function to notificate the observer about
      // more item is erased from the container.
      //
      // The erase() member function notificates the observer about more item
      // is erased from the container. It have to be overrided in the
      // subclasses.
      virtual void erase(const std::vector<Item>& items) = 0;

      // \brief The member function to notificate the observer about the
      // container is built.
      //
      // The build() member function notificates the observer about the
      // container is built from an empty container. It have to be
      // overrided in the subclasses.
      virtual void build() = 0;

      // \brief The member function to notificate the observer about all
      // items are erased from the container.
      //
      // The clear() member function notificates the observer about all
      // items are erased from the container. It have to be overrided in
      // the subclasses.
      virtual void clear() = 0;

    };

  protected:

    const Container* container;

    typedef std::list<ObserverBase*> Observers;
    Observers _observers;
    lemon::bits::Lock _lock;

  public:

    // \brief Default constructor.
    //
    // The default constructor of the AlterationNotifier.
    // It creates an empty notifier.
    AlterationNotifier()
      : container(0) {}

    // \brief Constructor.
    //
    // Constructor with the observed container parameter.
    AlterationNotifier(const Container& _container)
      : container(&_container) {}

    // \brief Copy Constructor of the AlterationNotifier.
    //
    // Copy constructor of the AlterationNotifier.
    // It creates only an empty notifier because the copiable
    // notifier's observers have to be registered still into that notifier.
    AlterationNotifier(const AlterationNotifier& _notifier)
      : container(_notifier.container) {}

    // \brief Destructor.
    //
    // Destructor of the AlterationNotifier.
    ~AlterationNotifier() {
      typename Observers::iterator it;
      for (it = _observers.begin(); it != _observers.end(); ++it) {
        (*it)->_notifier = 0;
      }
    }

    // \brief Sets the container.
    //
    // Sets the container.
    void setContainer(const Container& _container) {
      container = &_container;
    }

  protected:

    AlterationNotifier& operator=(const AlterationNotifier&);

  public:

    // \brief First item in the container.
    //
    // Returns the first item in the container. It is
    // for start the iteration on the container.
    void first(Item& item) const {
      container->first(item);
    }

    // \brief Next item in the container.
    //
    // Returns the next item in the container. It is
    // for iterate on the container.
    void next(Item& item) const {
      container->next(item);
    }

    // \brief Returns the id of the item.
    //
    // Returns the id of the item provided by the container.
    int id(const Item& item) const {
      return container->id(item);
    }

    // \brief Returns the maximum id of the container.
    //
    // Returns the maximum id of the container.
    int maxId() const {
      return container->maxId(Item());
    }

  protected:

    void attach(ObserverBase& observer) {
      _lock.lock();
      observer._index = _observers.insert(_observers.begin(), &observer);
      observer._notifier = this;
      _lock.unlock();
    }

    void detach(ObserverBase& observer) {
      _lock.lock();
      _observers.erase(observer._index);
      observer._index = _observers.end();
      observer._notifier = 0;
      _lock.unlock();
    }

  public:

    // \brief Notifies all the registed observers about an item added to
    // the container.
    //
    // It notifies all the registed observers about an item added to
    // the container.
    void add(const Item& item) {
      typename Observers::reverse_iterator it;
      try {
        for (it = _observers.rbegin(); it != _observers.rend(); ++it) {
          (*it)->add(item);
        }
      } catch (...) {
        typename Observers::iterator jt;
        for (jt = it.base(); jt != _observers.end(); ++jt) {
          (*jt)->erase(item);
        }
        throw;
      }
    }

    // \brief Notifies all the registed observers about more item added to
    // the container.
    //
    // It notifies all the registed observers about more item added to
    // the container.
    void add(const std::vector<Item>& items) {
      typename Observers::reverse_iterator it;
      try {
        for (it = _observers.rbegin(); it != _observers.rend(); ++it) {
          (*it)->add(items);
        }
      } catch (...) {
        typename Observers::iterator jt;
        for (jt = it.base(); jt != _observers.end(); ++jt) {
          (*jt)->erase(items);
        }
        throw;
      }
    }

    // \brief Notifies all the registed observers about an item erased from
    // the container.
    //
    // It notifies all the registed observers about an item erased from
    // the container.
    void erase(const Item& item) throw() {
      typename Observers::iterator it = _observers.begin();
      while (it != _observers.end()) {
        try {
          (*it)->erase(item);
          ++it;
        } catch (const ImmediateDetach&) {
          (*it)->_index = _observers.end();
          (*it)->_notifier = 0;
          it = _observers.erase(it);
        }
      }
    }

    // \brief Notifies all the registed observers about more item erased
    // from the container.
    //
    // It notifies all the registed observers about more item erased from
    // the container.
    void erase(const std::vector<Item>& items) {
      typename Observers::iterator it = _observers.begin();
      while (it != _observers.end()) {
        try {
          (*it)->erase(items);
          ++it;
        } catch (const ImmediateDetach&) {
          (*it)->_index = _observers.end();
          (*it)->_notifier = 0;
          it = _observers.erase(it);
        }
      }
    }

    // \brief Notifies all the registed observers about the container is
    // built.
    //
    // Notifies all the registed observers about the container is built
    // from an empty container.
    void build() {
      typename Observers::reverse_iterator it;
      try {
        for (it = _observers.rbegin(); it != _observers.rend(); ++it) {
          (*it)->build();
        }
      } catch (...) {
        typename Observers::iterator jt;
        for (jt = it.base(); jt != _observers.end(); ++jt) {
          (*jt)->clear();
        }
        throw;
      }
    }

    // \brief Notifies all the registed observers about all items are
    // erased.
    //
    // Notifies all the registed observers about all items are erased
    // from the container.
    void clear() {
      typename Observers::iterator it = _observers.begin();
      while (it != _observers.end()) {
        try {
          (*it)->clear();
          ++it;
        } catch (const ImmediateDetach&) {
          (*it)->_index = _observers.end();
          (*it)->_notifier = 0;
          it = _observers.erase(it);
        }
      }
    }
  };

}

#endif
