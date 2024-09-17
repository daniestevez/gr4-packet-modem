#include <pybind11/complex.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/PluginLoader.hpp>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

PYBIND11_MODULE(gr4_packet_modem_python, m)
{
    py::module_::import("pmtv");

    m.doc() = R"pbdoc(
        gr4-packet-modem Python bindings proof of concept
        -------------------------------------------------

        .. currentmodule:: gr4-packet-modem

        .. autosummary::
           :toctree: _generate

    )pbdoc";

    py::class_<gr::PluginLoader>(m, "PluginLoader")
        .def(py::init<gr::BlockRegistry&, std::span<const std::filesystem::path>>());

    // m.def("globalPluginLoader", &gr::globalPluginLoader);

    py::class_<gr::Graph>(m, "Graph")
        .def(py::init<gr::property_map>(), py::arg("settings") = gr::property_map())
        .def("emplaceBlock",
             static_cast<gr::BlockModel& (gr::Graph::*)(std::string_view,
                                                        std::string_view,
                                                        gr::property_map,
                                                        gr::PluginLoader&)>(
                 &gr::Graph::emplaceBlock),
             py::arg("type"),
             py::arg("parameters"),
             py::arg("initialSettings"),
             py::arg("loader") /* = gr::globalPluginLoader()*/);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
