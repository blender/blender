#ifndef BLENDERSTYLEMODULE_H
#define BLENDERSTYLEMODULE_H

#include "../stroke/StyleModule.h"
#include "../system/PythonInterpreter.h"

extern "C" {
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_text.h"
}

class BlenderStyleModule : public StyleModule
{
public:	

	BlenderStyleModule(struct Text *text, const string &name,
		Interpreter *inter) : StyleModule(name, inter) {
		_text = text;
	}

	virtual ~BlenderStyleModule() {
		unlink_text(G.main, _text);
		free_libblock(&G.main->text, _text);
	}

protected:

	virtual int interpret() {
		PythonInterpreter* py_inter = dynamic_cast<PythonInterpreter*>(_inter);
		assert(py_inter != 0);
		return py_inter->interpretText(_text, getFileName());
	}

private:
	struct Text *_text;
};

#endif // BLENDERSTYLEMODULE_H
