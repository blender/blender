/** \file itasc/kdl/utilities/error_stack.cpp
 *  \ingroup itasc
 */
/*****************************************************************************
 *  \author 
 *  	Erwin Aertbelien, Div. PMA, Dep. of Mech. Eng., K.U.Leuven
 *
 *  \version 
 *		ORO_Geometry V0.2
 *
 *	\par History
 *		- $log$
 *
 *	\par Release
 *		$Name:  $ 
 ****************************************************************************/


#include "error_stack.h"
#include <stack>
#include <vector>
#include <string>
#include <cstring>

namespace KDL {

// Trace of the call stack of the I/O routines to help user
// interprete error messages from I/O
typedef std::stack<std::string>  ErrorStack;

ErrorStack errorstack;
// should be in Thread Local Storage if this gets multithreaded one day...


void IOTrace(const std::string& description) {
    errorstack.push(description);   
}


void IOTracePop() {
    errorstack.pop();
}

void IOTraceOutput(std::ostream& os) {
    while (!errorstack.empty()) {
        os << errorstack.top().c_str() << std::endl;
        errorstack.pop();
    }
}


void IOTracePopStr(char* buffer,int size) {
    if (errorstack.empty()) {
        *buffer = 0;
        return;
    }
    strncpy(buffer,errorstack.top().c_str(),size);
    errorstack.pop();
}

}
