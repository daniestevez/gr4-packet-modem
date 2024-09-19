#include <pybind11/complex.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <gnuradio-4.0/BlockModel.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <cstdint>
#include <stdexcept>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

static void register_blocks()
{
    using namespace gr::packet_modem;

    gr::registerBlock<NullSource,
                      double,
                      float,
                      int64_t,
                      int32_t,
                      int16_t,
                      int8_t,
                      uint64_t,
                      uint32_t,
                      uint16_t,
                      uint8_t>(gr::globalBlockRegistry());
    gr::registerBlock<NullSink,
                      double,
                      float,
                      int64_t,
                      int32_t,
                      int16_t,
                      int8_t,
                      uint64_t,
                      uint32_t,
                      uint16_t,
                      uint8_t>(gr::globalBlockRegistry());
    gr::registerBlock<Head,
                      double,
                      float,
                      int64_t,
                      int32_t,
                      int16_t,
                      int8_t,
                      uint64_t,
                      uint32_t,
                      uint16_t,
                      uint8_t>(gr::globalBlockRegistry());
}

namespace py = pybind11;

template <typename Sched>
void bind_scheduler(py::module& m, const char* python_class_name)
{
    py::class_<Sched>(m, python_class_name)
        .def(py::init([](gr::property_map settings) {
                 gr::Graph fg{ settings };
                 return std::make_unique<Sched>(std::move(fg));
             }),
             py::arg("settings") = gr::property_map())
        .def("graph",
             static_cast<gr::Graph& (Sched::*)()>(&Sched::graph),
             py::return_value_policy::reference_internal)
        .def("runAndWait", [](Sched& sched) {
            const auto ret = sched.runAndWait();
            if (!ret.has_value()) {
                std::runtime_error(fmt::format("scheduler error: {}", ret.error()));
            }
        });
}

PYBIND11_MODULE(gr4_packet_modem_python, m)
{
    register_blocks();

    py::module_::import("pmtv");

    m.doc() = R"pbdoc(
        gr4-packet-modem Python bindings proof of concept
        -------------------------------------------------

        .. currentmodule:: gr4-packet-modem

        .. autosummary::
           :toctree: _generate

    )pbdoc";

    // PMT constructors that are not defined in pmtv

    m.def("pmt_from_uint64_t", [](uint64_t val) { return pmtv::pmt(val); });

    // BlockModel.hpp

    py::class_<gr::BlockModel>(m, "BlockModel")
        .def("name", &gr::BlockModel::name)
        .def("typeName", &gr::BlockModel::typeName)
        .def("setName", &gr::BlockModel::setName, py::arg("name"))
        // TODO: modifications from Python to the returned object do not
        // propagate the C++ BlockModel
        .def(
            "metaInformation",
            [](gr::BlockModel& model) { return model.metaInformation(); },
            py::return_value_policy::reference_internal)
        .def("uniqueName", &gr::BlockModel::uniqueName)
        .def(
            "settings",
            [](gr::BlockModel& model) -> gr::SettingsBase& { return model.settings(); },
            py::return_value_policy::reference_internal);

    // BlockRegistry.hpp

    py::class_<gr::BlockRegistry>(m, "BlockRegistry")
        .def(py::init())
        // providedBlocks() returns std::span<std::string>, but for Python it is
        // more idiomatic to convert this to std::vector<std::string>, thus
        // copying to transfer ownership. Therefore, the providedBlocks() Python
        // method ends up doing the same as the knownBlocks() method.
        .def("providedBlocks", &gr::BlockRegistry::knownBlocks)
        .def("knownBlocks", &gr::BlockRegistry::knownBlocks)
        .def("createBlock",
             &gr::BlockRegistry::createBlock,
             py::arg("name"),
             py::arg("type"),
             py::arg("params"))
        .def("isBlockKnown", &gr::BlockRegistry::isBlockKnown, py::arg("block"))
        .def("knownBlockParameterizations",
             &gr::BlockRegistry::knownBlockParameterizations,
             py::arg("block"));

    // gr::globalBlockRegistry returns a reference to a singleton object with a
    // static lifetime, so return_value_policy::reference is appropriate, as the
    // C++ object is never deallocated.
    m.def("globalBlockRegistry",
          &gr::globalBlockRegistry,
          py::return_value_policy::reference);

    // Graph.hpp

    // In C++, the user is supposed to instantiate a gr::Graph, emplace blocks
    // in the graph, make connections, and then move the graph into a
    // scheduler. Moving isn't captured well by Python semantics nor by
    // pybind11, so instead we do the following. The constructor for the
    // scheduler creates a new graph with the settings passed to the
    // constructor. The graph is immediately moved into the scheduler, which is
    // returned by the constructor. The scheduler has a method graph() that
    // gives access to the graph, so the user can then emplace blocks and make
    // connections. To avoid confusion, the constructor of gr::Graph is not
    // visible in Python. The only way to get a graph is from inside a
    // scheduler.

    py::class_<gr::Graph>(m, "Graph")
        .def(
            "emplaceBlock",
            [](gr::Graph& fg,
               std::string_view type,
               std::string_view parameters,
               gr::property_map initialSettings) -> gr::BlockModel& {
                return fg.emplaceBlock(type, parameters, initialSettings);
            },
            py::arg("type"),
            py::arg("parameters"),
            py::arg("initialSettings"),
            py::return_value_policy::reference_internal)
        // connection using port names as strings
        .def(
            "connect",
            [](gr::Graph& fg,
               gr::BlockModel& source,
               std::string source_port,
               gr::BlockModel& destination,
               std::string destination_port) {
                if (fg.connect(source, source_port, destination, destination_port) !=
                    gr::ConnectionResult::SUCCESS) {
                    throw std::runtime_error("could not make port connection");
                }
            },
            py::arg("source"),
            py::arg("source_port"),
            py::arg("destination"),
            py::arg("destination_port"))
        // connection using port indices as integers
        .def(
            "connect",
            [](gr::Graph& fg,
               gr::BlockModel& source,
               size_t source_port,
               gr::BlockModel& destination,
               size_t destination_port) {
                if (fg.connect(source, source_port, destination, destination_port) !=
                    gr::ConnectionResult::SUCCESS) {
                    throw std::runtime_error("could not make port connection");
                }
            },
            py::arg("source"),
            py::arg("source_port"),
            py::arg("destination"),
            py::arg("destination_port"));

    // Scheduler.hpp

    // bind all schedulers in gnuradio4/core
    bind_scheduler<gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded>>(
        m, "SchedulerSimpleSingleThreaded");
    bind_scheduler<gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::multiThreaded>>(
        m, "SchedulerSimpleMultiThreaded");
    bind_scheduler<
        gr::scheduler::BreadthFirst<gr::scheduler::ExecutionPolicy::singleThreaded>>(
        m, "SchedulerBreadthFirstSingleThreaded");
    bind_scheduler<
        gr::scheduler::BreadthFirst<gr::scheduler::ExecutionPolicy::multiThreaded>>(
        m, "SchedulerBreadthFirstMultiThreaded");

    // Settings.hpp

    py::class_<gr::SettingsCtx>(m, "SettingsCtx").def(py::init());

    py::class_<gr::SettingsBase>(m, "SettingsBase")
        .def("getStoredAll", &gr::SettingsBase::getStoredAll)
        .def("stagedParameters", &gr::SettingsBase::stagedParameters)
        .def("defaultParameters", &gr::SettingsBase::defaultParameters)
        .def("activeParameters", &gr::SettingsBase::defaultParameters);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
