#ifndef  ITERATOR_H
#define ITERATOR_H

#include <iostream>
#include <string>
using namespace std;


class Iterator
{
public:

  virtual ~Iterator() {}

  virtual string getExactTypeName() const {
    return "Iterator";
  }

  virtual void increment() {
	cerr << "Warning: increment() not implemented" << endl;
  }

  virtual void decrement() {
	cerr << "Warning: decrement() not implemented" << endl;
  }

  virtual bool isBegin() const {
	cerr << "Warning: isBegin() not implemented" << endl;
	return false;
  }	

  virtual bool isEnd() const {
	cerr << "Warning:  isEnd() not implemented" << endl;
	return false;
  }

};

#endif // ITERATOR_H