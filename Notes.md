## Things that might have to be implemented

- get_all_moves
- basically, student_agent files are broken, they must be fixed and corrected
- need to build a compile.sh file for the final compliation strategy of the files


## General Pointers

- student_agent.py ~~ student_agent_cpp.py + student_agent.cpp (make edits together)
- we need to use c++, the overhead of conversion is not much really, since the board size never exceeds 12 x 13 (finite and easily done on python)

## Pointers for the wrapping of C++ code by the python code

- that change in the style of input was because of the fact that c++ natively did not have a piece as an input, but was a map, simple?
- and pybind11 has created equivalent c++ classes into a python module and we can use the c++ code by making function calls as if we were natively using a python code 
- pybind11 converts the c++ code
- use the following to access some variables in the c++ code
```
PYBIND11_MODULE(student_agent_module, m) {
    // ... Move class binding

    py::class_<StudentAgent>(m, "StudentAgent")
        .def(py::init<std::string>())
        .def("choose", &StudentAgent::choose)
        .def_readonly_static("agent_count", &StudentAgent::agent_count); // Expose as read-only
}
```

## Implemenations Strategy

- write a Naive minimax code first
- improve this to alpha beta pruning
- write a database