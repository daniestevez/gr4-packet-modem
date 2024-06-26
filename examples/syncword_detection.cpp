#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/add.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/noise_source.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>

int main(int argc, char** argv)
{
    using namespace boost::ut;
    using c64 = std::complex<float>;

    expect(fatal(eq(argc, 2)));
    const float freq_error = std::stof(argv[1]);

    gr::Graph fg;
    const std::vector<size_t> packet_lengths = { 10,  25,   100,  1500, 27,   38, 243,
                                                 514, 1500, 1500, 1024, 1024, 42, 34 };
    auto& vector_source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<gr::packet_modem::Pdu<uint8_t>>>(
            { { "repeat", true } });
    for (auto len : packet_lengths) {
        std::vector<uint8_t> v(len);
        std::iota(v.begin(), v.end(), 0);
        vector_source.data.emplace_back(std::move(v));
    }
    const bool stream_mode = false;
    const size_t samples_per_symbol = 4U;
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& pdu_to_stream = fg.emplaceBlock<gr::packet_modem::PduToTaggedStream<c64>>(
        { { "packet_len_tag_key", "" } });
    if (max_in_samples) {
        pdu_to_stream.in.max_samples = max_in_samples;
    }
    auto& rotator =
        fg.emplaceBlock<gr::packet_modem::Rotator<>>({ { "phase_incr", freq_error } });
    auto& noise_source = fg.emplaceBlock<gr::packet_modem::NoiseSource<c64>>(
        { { "noise_type", "gaussian" }, { "amplitude", 0.05f } });
    auto& add_noise = fg.emplaceBlock<gr::packet_modem::Add<c64>>();
    auto& head =
        fg.emplaceBlock<gr::packet_modem::Head<c64>>({ { "num_items", 1000000UZ } });
    const bool header_debug = false;
    const bool zmq_output = false;
    const bool log = true;
    auto packet_receiver = gr::packet_modem::PacketReceiver(
        fg, samples_per_symbol, "packet_len", header_debug, zmq_output, log);

    auto& file_sink = fg.emplaceBlock<gr::packet_modem::FileSink<uint8_t>>(
        { { "filename", "syncword_detection.u8" } });
    auto& vector_sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();

    expect(eq(gr::ConnectionResult::SUCCESS,
              vector_source.out.connect(*packet_transmitter_pdu.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              packet_transmitter_pdu.out_packet->connect(pdu_to_stream.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(pdu_to_stream).to<"in">(rotator)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(rotator).to<"in0">(add_noise)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(noise_source).to<"in1">(add_noise)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(add_noise).to<"in">(head)));
    expect(eq(gr::ConnectionResult::SUCCESS, head.out.connect(*packet_receiver.in)));
    expect(eq(gr::ConnectionResult::SUCCESS, packet_receiver.out->connect(file_sink.in)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, packet_receiver.out->connect(vector_sink.in)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });

    const auto ret = sched.runAndWait();
    stopper.join();
    expect(ret.has_value());
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    fmt::println("TAGS");
    for (const auto& _tag : vector_sink.tags()) {
        fmt::println("offset = {}, map = {}", _tag.index, _tag.map);
    }

    return 0;
}
