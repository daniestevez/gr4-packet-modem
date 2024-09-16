#include <pybind11/pybind11.h>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

int add(int i, int j) { return i + j; }

namespace py = pybind11;

PYBIND11_MODULE(gr4_packet_modem_python, m)
{
    m.doc() = R"pbdoc(
        gr4-packet-modem Python bindings proof of concept
        -------------------------------------------------

        .. currentmodule:: gr4-packet-modem

        .. autosummary::
           :toctree: _generate

    )pbdoc";

    // Example function. To be removed
    m.def("add", &add, R"pbdoc(
        Add two numbers

        Some other explanation about the add function.
    )pbdoc");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
