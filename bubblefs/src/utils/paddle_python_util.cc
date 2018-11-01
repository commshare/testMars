/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

// Paddle/paddle/utils/PythonUtil.cpp

#include "utils/paddle_python_util.h"
#include <signal.h>
#include <sstream>
#include <vector>

namespace bubblefs {
namespace mypaddle {
  
static std::recursive_mutex g_pyMutex;

PyGuard::PyGuard() : guard_(g_pyMutex) {}

static void printPyErrorStack(std::ostream& os,
                              bool withEndl = false,
                              bool withPyPath = true) {
  PyObject *ptype, *pvalue, *ptraceback;
  PyErr_Fetch(&ptype, &pvalue, &ptraceback);
  PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
  PyErr_Clear();
  if (withPyPath) {
    os << "Current PYTHONPATH: " << py::repr(PySys_GetObject(strdup("path")));
    if (withEndl) {
      os << std::endl;
    }
  }
  PyTracebackObject* obj = (PyTracebackObject*)ptraceback;

  os << "Python Error: " << PyString_AsString(PyObject_Str(ptype)) << " : "
     << (pvalue == NULL ? "" : PyString_AsString(PyObject_Str(pvalue)));
  if (withEndl) {
    os << std::endl;
  }
  os << "Python Callstack: ";
  if (withEndl) {
    os << std::endl;
  }
  while (obj != NULL) {
    int line = obj->tb_lineno;
    const char* filename =
        PyString_AsString(obj->tb_frame->f_code->co_filename);
    os << "            " << filename << " : " << line;
    if (withEndl) {
      os << std::endl;
    }
    obj = obj->tb_next;
  }

  Py_XDECREF(ptype);
  Py_XDECREF(pvalue);
  Py_XDECREF(ptraceback);
}
PyObjectPtr callPythonFuncRetPyObj(const std::string& moduleName,
                                   const std::string& funcName,
                                   const std::vector<std::string>& args) {
  PyGuard guard;
  PyObjectPtr pyModule = py::import(moduleName);
  PyObjectPtr pyFunc(PyObject_GetAttrString(pyModule.get(), funcName.c_str()));
  CHECK_PY(pyFunc) << "GetAttrString failed.";
  PyObjectPtr pyArgs(PyTuple_New(args.size()));
  for (size_t i = 0; i < args.size(); ++i) {
    PyObjectPtr pyArg(PyString_FromString(args[i].c_str()));
    CHECK_PY(pyArg) << "Import pyArg failed.";
    PyTuple_SetItem(pyArgs.get(), i, pyArg.release());  //  Maybe a problem
  }
  PyObjectPtr ret(PyObject_CallObject(pyFunc.get(), pyArgs.get()));
  CHECK_PY(ret) << "Call Object failed.";
  return ret;
}

std::string callPythonFunc(const std::string& moduleName,
                           const std::string& funcName,
                           const std::vector<std::string>& args) {
  PyObjectPtr obj = callPythonFuncRetPyObj(moduleName, funcName, args);
  return std::string(PyString_AsString(obj.get()), PyString_Size(obj.get()));
}

PyObjectPtr createPythonClass(
    const std::string& moduleName,
    const std::string& className,
    const std::vector<std::string>& args,
    const std::map<std::string, std::string>& kwargs) {
  PyGuard guard;
  PyObjectPtr pyModule = py::import(moduleName);
  LOG(INFO) << "createPythonClass moduleName.c_str:" << moduleName.c_str();
  CHECK_PY(pyModule) << "Import module " << moduleName << " failed.";
  PyObjectPtr pyDict(PyModule_GetDict(pyModule.get()));
  CHECK_PY(pyDict) << "Get Dict failed.";
  PyObjectPtr pyClass(PyDict_GetItemString(pyDict.get(), className.c_str()));
  LOG(INFO) << "createPythonClass className.c_str():" << className.c_str();
  CHECK_PY(pyClass) << "Import class " << className << " failed.";
  PyObjectPtr argsObjectList(PyTuple_New(args.size()));
  for (size_t i = 0; i < args.size(); ++i) {
    PyObjectPtr pyArg(Py_BuildValue("s#", args[i].c_str(), args[i].length()));
    PyTuple_SetItem(argsObjectList.get(), i, pyArg.release());
  }

  PyObjectPtr kwargsObjectList(PyDict_New());
  for (auto& x : kwargs) {
    PyObjectPtr pyArg(Py_BuildValue("s#", x.second.c_str(), x.second.length()));
    PyDict_SetItemString(
        kwargsObjectList.get(), x.first.c_str(), pyArg.release());
  }

  PyObjectPtr pyInstance(PyInstance_New(
      pyClass.get(), argsObjectList.release(), kwargsObjectList.release()));
  CHECK_PY(pyInstance) << "Create class " << className << " failed.";
  return pyInstance;
}

namespace py {
char* repr(PyObject* obj) { return PyString_AsString(PyObject_Repr(obj)); }

std::string getPyCallStack() {
  std::ostringstream os;
  printPyErrorStack(os, true);
  return os.str();
}

PyObjectPtr import(const std::string& moduleName) {
  auto module = PyImport_ImportModule(moduleName.c_str());
  CHECK_PY(module) << "Import " << moduleName << "Error";
  return PyObjectPtr(module);
}

}  // namespace py

extern "C" {
extern const char enable_virtualenv_py[];
}

void initPython(int argc, char** argv) {
  Py_SetProgramName(argv[0]);
  Py_Initialize();
  PySys_SetArgv(argc, argv);
  // python blocks SIGINT. Need to enable it.
  signal(SIGINT, SIG_DFL);

  // Manually activate virtualenv when user is using virtualenv
  PyRun_SimpleString(enable_virtualenv_py);
}

}  // namespace mypaddle
}  // namespace bubblefs