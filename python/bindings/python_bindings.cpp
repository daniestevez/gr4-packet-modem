#include <pybind11/complex.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockModel.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Message.hpp>
#include <gnuradio-4.0/Port.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/Settings.hpp>
#include <gnuradio-4.0/Tag.hpp>
#include <stdexcept>

void register_blocks();

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

template <typename BlockT>
static void bind_block_crtp(py::class_<BlockT>& block)
{
    block.def_readonly("msgIn", &BlockT::msgIn);
}

template <typename PortT>
static void bind_send_message(py::module& m)
{
    m.def(
        "sendMessage",
        [](gr::message::Command cmd,
           PortT& port,
           std::string_view serviceName,
           std::string_view endpoint,
           gr::property_map userMessage,
           std::string_view clientRequestID) {
            switch (cmd) {
            case gr::message::Command::Invalid:
                gr::sendMessage<gr::message::Command::Invalid>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Get:
                gr::sendMessage<gr::message::Command::Get>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Set:
                gr::sendMessage<gr::message::Command::Set>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Partial:
                gr::sendMessage<gr::message::Command::Partial>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Final:
                gr::sendMessage<gr::message::Command::Final>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Ready:
                gr::sendMessage<gr::message::Command::Ready>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Disconnect:
                gr::sendMessage<gr::message::Command::Disconnect>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Subscribe:
                gr::sendMessage<gr::message::Command::Subscribe>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Unsubscribe:
                gr::sendMessage<gr::message::Command::Unsubscribe>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Notify:
                gr::sendMessage<gr::message::Command::Notify>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            case gr::message::Command::Heartbeat:
                gr::sendMessage<gr::message::Command::Heartbeat>(
                    port, serviceName, endpoint, userMessage, clientRequestID);
                break;
            default:
                std::terminate();
            }
        },
        py::arg("cmd"),
        py::arg("port"),
        py::arg("serviceName"),
        py::arg("endpoint"),
        py::arg("userMessage"),
        py::arg("clientRequestID") = "");
}

template <typename PortT>
static auto bind_port(py::module& m, const char* python_class_name)
{
    auto port =
        py::class_<PortT>(m, python_class_name)
            .def(py::init())
            // TODO: gr::MsgPortOut::initBuffer doesn't compile because _ioHandler
            // doesn't have a try_publish method in that case.
            //
            // .def("initBuffer", &PortT::initBuffer, py::arg("nSamples") = 0)
            .def("isConnected", &PortT::isConnected)
            .def("type", &PortT::type)
            .def("direction", &PortT::direction)
            .def("domain", &PortT::domain)
            .def("isSynchronous", &PortT::isSynchronous)
            .def("isOptional", &PortT::isOptional)
            .def("available", &PortT::available)
            .def("min_buffer_size", &PortT::min_buffer_size)
            .def("max_buffer_size", &PortT::max_buffer_size)
            .def(
                "resizeBuffer",
                [](PortT& port, std::size_t min_size) {
                    if (port.resizeBuffer(min_size) != gr::ConnectionResult::SUCCESS) {
                        throw std::runtime_error("resizeBuffer failed");
                    }
                },
                py::arg("min_size"))
            .def("disconnect",
                 [](PortT& port) {
                     if (port.disconnect() != gr::ConnectionResult::SUCCESS) {
                         throw std::runtime_error("disconnect failed");
                     }
                 })
            .def("getMergedTag", &PortT::getMergedTag);

    if constexpr (PortT::kIsOutput) {
        port.def(
                "publishTag",
                [](PortT& port,
                   const gr::property_map& tag_data,
                   gr::Tag::signed_index_type tagOffset) {
                    port.publishTag(tag_data, tagOffset);
                },
                py::arg("tag_data"),
                py::arg("tagOffset") = -1)
            .def("publishPendingTags", &PortT::publishPendingTags);
    }

    return port;
}

template <typename PortSource, typename PortDest>
static void bind_port_connect(py::class_<PortSource>& port_source)
requires(PortSource::kIsOutput && PortDest::kIsInput)
{
    port_source.def(
        "connect",
        [](PortSource& src, PortDest& dest) {
            if (src.connect(dest) != gr::ConnectionResult::SUCCESS) {
                throw std::runtime_error("Port connect failed");
            }
        },
        py::arg("other"));
}

template <typename Sched>
void bind_scheduler(py::module& m, const char* python_class_name)
{
    auto sched =
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
                // Release the Python GIL, because runAndWait() potentially
                // takes a long to run, and Python code might want to run other
                // threads while this happens. However this can be potentially
                // problematic if the flowgraph execution tries to call Python
                // code or to access Python state in any way while the GIL is
                // not held.
                py::gil_scoped_release release;

                const auto ret = sched.runAndWait();
                if (!ret.has_value()) {
                    std::runtime_error(fmt::format("scheduler error: {}", ret.error()));
                }
            });
    bind_block_crtp(sched);
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

    // Block.hpp

    m.attr("kHeartbeat") = gr::block::property::kHeartbeat;
    m.attr("kEcho") = gr::block::property::kEcho;
    m.attr("kLifeCycleState") = gr::block::property::kLifeCycleState;
    m.attr("kSetting") = gr::block::property::kSetting;
    m.attr("kStagedSetting") = gr::block::property::kStagedSetting;
    m.attr("kStoreDefaults") = gr::block::property::kStoreDefaults;
    m.attr("kResetDefaults") = gr::block::property::kResetDefaults;

    // BlockModel.hpp

    py::class_<gr::BlockModel>(m, "BlockModel")
        .def("name", &gr::BlockModel::name)
        .def("typeName", &gr::BlockModel::typeName)
        .def("setName", &gr::BlockModel::setName, py::arg("name"))
        // TODO: modifications from Python to the returned object do not
        // propagate to the C++ BlockModel
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

    // Message.hpp

    py::enum_<gr::message::Command>(m, "Command")
        .value("Invalid", gr::message::Command::Invalid)
        .value("Get", gr::message::Command::Get)
        .value("Set", gr::message::Command::Set)
        .value("Partial", gr::message::Command::Partial)
        .value("Final", gr::message::Command::Final)
        .value("Ready", gr::message::Command::Ready)
        .value("Disconnect", gr::message::Command::Disconnect)
        .value("Subscribe", gr::message::Command::Subscribe)
        .value("Unsubscribe", gr::message::Command::Unsubscribe)
        .value("Notify", gr::message::Command::Notify)
        .value("Heartbeat", gr::message::Command::Heartbeat)
        .export_values();

    py::class_<gr::Message>(m, "Message")
        .def(py::init())
        .def_readwrite("protocol", &gr::Message::protocol)
        .def_readwrite("cmd", &gr::Message::cmd)
        .def_readwrite("serviceName", &gr::Message::serviceName)
        .def_readwrite("clientRequestID", &gr::Message::clientRequestID)
        .def_readwrite("endpoint", &gr::Message::endpoint)
        // TODO: bind std::expected as something that Python can access
        .def_readwrite("data", &gr::Message::data)
        .def_readwrite("rbac", &gr::Message::rbac);

    bind_send_message<gr::MsgPortOut>(m);
    bind_send_message<gr::MsgPortOutNamed<"__Builtin">>(m);

    // Port.hpp

    py::enum_<gr::PortDirection>(m, "PortDirection")
        .value("INPUT", gr::PortDirection::INPUT)
        .value("OUTPUT", gr::PortDirection::OUTPUT)
        .value("ANY", gr::PortDirection::ANY)
        .export_values();

    py::enum_<gr::PortType>(m, "PortType")
        .value("STREAM", gr::PortType::STREAM)
        .value("MESSAGE", gr::PortType::MESSAGE)
        .value("ANY", gr::PortType::ANY)
        .export_values();

    // Only bind message ports, since stream ports depend on a type parameter T.
    // Also bind named message ports with the name __Builtin, since they are used
    // for Block::msgIn and Block::msgOut.
    bind_port<gr::MsgPortIn>(m, "MsgPortIn");
    bind_port<gr::MsgPortInNamed<"__Builtin">>(m, "MsgPortInBuiltin");
    auto msg_port_out = bind_port<gr::MsgPortOut>(m, "MsgPortOut");
    auto msg_port_out_builtin =
        bind_port<gr::MsgPortOutNamed<"__Builtin">>(m, "MsgPortOutBuiltin");
    bind_port_connect<gr::MsgPortOut, gr::MsgPortIn>(msg_port_out);
    bind_port_connect<gr::MsgPortOut, gr::MsgPortInNamed<"__Builtin">>(msg_port_out);
    bind_port_connect<gr::MsgPortOutNamed<"__Builtin">, gr::MsgPortIn>(
        msg_port_out_builtin);
    bind_port_connect<gr::MsgPortOutNamed<"__Builtin">, gr::MsgPortInNamed<"__Builtin">>(
        msg_port_out_builtin);

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
        .def("changed", &gr::SettingsBase::changed)
        .def("setChanged", &gr::SettingsBase::setChanged, py::arg("b"))
        .def("setInitBlockParameters",
             &gr::SettingsBase::setInitBlockParameters,
             py::arg("parameters"))
        .def("init", &gr::SettingsBase::init)
        .def("set",
             &gr::SettingsBase::set,
             py::arg("parameters"),
             py::arg("ctx") = gr::SettingsCtx())
        .def("setStaged", &gr::SettingsBase::setStaged, py::arg("parameters"))
        .def("storeDefaults", &gr::SettingsBase::storeDefaults)
        .def("resetDefaults", &gr::SettingsBase::resetDefaults)
        .def("activateContext",
             &gr::SettingsBase::activateContext,
             py::arg("ctx") = gr::SettingsCtx())
        .def("autoUpdate", &gr::SettingsBase::autoUpdate, py::arg("tag"))
        .def(
            "get",
            [](const gr::SettingsBase& settings,
               const std::vector<std::string>& parameter_keys) {
                return settings.get(parameter_keys);
            },
            py::arg("parameter_keys") = std::vector<std::string>())
        .def(
            "get",
            [](const gr::SettingsBase& settings, const std::string& parameter_key) {
                return settings.get(parameter_key);
            },
            py::arg("parameter_key"))
        .def(
            "getStored",
            [](const gr::SettingsBase& settings,
               const std::vector<std::string>& parameterKeys,
               gr::SettingsCtx ctx) { return settings.getStored(parameterKeys, ctx); },
            py::arg("parameterKeys") = std::vector<std::string>(),
            py::arg("ctx") = gr::SettingsCtx())
        .def(
            "getStored",
            [](const gr::SettingsBase& settings,
               const std::string& parameter_key,
               gr::SettingsCtx ctx) { return settings.getStored(parameter_key, ctx); },
            py::arg("parameter_key"),
            py::arg("ctx") = gr::SettingsCtx())
        .def("getNStoredParameters", &gr::SettingsBase::getNStoredParameters)
        .def("getNAutoUpdateParameters", &gr::SettingsBase::getNAutoUpdateParameters)
        .def("getStoredAll", &gr::SettingsBase::getStoredAll)
        .def("stagedParameters", &gr::SettingsBase::stagedParameters)
        .def("defaultParameters", &gr::SettingsBase::defaultParameters)
        .def("activeParameters", &gr::SettingsBase::defaultParameters)
        .def("applyStagedParameters", &gr::SettingsBase::applyStagedParameters)
        .def("updateActiveParameters", &gr::SettingsBase::updateActiveParameters);

    py::class_<gr::ApplyStagedParametersResult>(m, "ApplyStagedParametersResult")
        .def(py::init())
        .def_readwrite("forwardParameters",
                       &gr::ApplyStagedParametersResult::forwardParameters)
        .def_readwrite("appliedParameters",
                       &gr::ApplyStagedParametersResult::appliedParameters);

    // Tag.hpp

    py::class_<gr::Tag>(m, "Tag")
        .def(py::init())
        .def_readwrite("index", &gr::Tag::index)
        // TODO: this copies the map from/to Python, so things such as
        //   tag.map['key'] = pmtv.pmt(val)
        // do not modify the map in the C++ Tag in-place
        .def_readwrite("map", &gr::Tag::map)
        .def("reset", &gr::Tag::reset)
        .def(
            "at",
            [](const gr::Tag& tag, const std::string& key) -> pmtv::pmt {
                return tag.at(key);
            },
            py::arg("key"))
        .def(
            "get",
            [](const gr::Tag& tag, const std::string& key) -> std::optional<pmtv::pmt> {
                const auto ret = tag.get(key);
                if (ret.has_value()) {
                    return *ret;
                }
                return std::nullopt;
            },
            py::arg("key"))
        .def(
            "insert_or_assign",
            [](gr::Tag& tag, const std::string& key, const pmtv::pmt& value) {
                tag.insert_or_assign(key, value);
            },
            py::arg("key"),
            py::arg("value"));

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
