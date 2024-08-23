#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/add.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/mapper.hpp>
#include <gnuradio-4.0/packet-modem/noise_source.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/syncword_detection.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>

boost::ut::suite LoopbackTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "loopback"_test =
        [](auto args) {
            float freq_error;
            bool stream_mode;
            std::tie(freq_error, stream_mode) = args;
            Graph fg;
            using c64 = std::complex<float>;
            const std::vector<size_t> packet_lengths = {
                10,
                25,
                100,
                1500,
                27,
                38,
                243,
                514,
                1500,
                1500,
                1024,
                1024,
                42,
                34,
                // long packet: it will not appear in the output because its end
                // doesn't make it through the decoder completely
                4096,
            };
            auto& source = fg.emplaceBlock<VectorSource<Pdu<uint8_t>>>();
            for (auto len : packet_lengths) {
                std::vector<uint8_t> v(len);
                std::iota(v.begin(), v.end(), 0);
                source.data.emplace_back(std::move(v));
            }
            const size_t samples_per_symbol = 4U;
            const size_t max_in_samples = 1U;
            // note that buffer size is rounded up to a multiple of
            // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
            // the same number give slightly different performance
            const size_t out_buff_size = 1U;
            auto packet_transmitter_pdu = PacketTransmitterPdu(
                fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
            auto& rotator = fg.emplaceBlock<Rotator<>>({ { "phase_incr", freq_error } });
            auto& noise_source = fg.emplaceBlock<NoiseSource<c64>>(
                { { "noise_type", "gaussian" }, { "amplitude", 0.05f } });
            auto& add_noise = fg.emplaceBlock<Add<c64>>();
            const bool header_debug = false;
            const bool zmq_output = false;
            const bool log = true;
            auto packet_receiver = PacketReceiver(
                fg, samples_per_symbol, "packet_len", header_debug, zmq_output, log);
            auto& sink = fg.emplaceBlock<VectorSink<uint8_t>>();
            if (stream_mode) {
                expect(eq(gr::ConnectionResult::SUCCESS,
                          fg.connect<"out">(*packet_transmitter_pdu.rrc_interp)
                              .to<"in">(rotator)));
            } else {
                auto& pdu_to_stream =
                    fg.emplaceBlock<gr::packet_modem::PduToTaggedStream<c64>>(
                        { { "packet_len_tag_key", "" } });
                if (max_in_samples) {
                    pdu_to_stream.in.max_samples = max_in_samples;
                }
                expect(eq(gr::ConnectionResult::SUCCESS,
                          fg.connect<"out">(*packet_transmitter_pdu.burst_shaper)
                              .to<"in">(pdu_to_stream)));
                expect(eq(gr::ConnectionResult::SUCCESS,
                          fg.connect<"out">(pdu_to_stream).to<"in">(rotator)));
            }
            expect(
                eq(ConnectionResult::SUCCESS,
                   fg.connect<"out">(source).to<"in">(*packet_transmitter_pdu.ingress)));
            expect(eq(ConnectionResult::SUCCESS,
                      fg.connect<"out">(rotator).to<"in0">(add_noise)));
            expect(eq(ConnectionResult::SUCCESS,
                      fg.connect<"out">(noise_source).to<"in1">(add_noise)));
            expect(eq(ConnectionResult::SUCCESS,
                      fg.connect<"out">(add_noise).to<"in">(
                          *packet_receiver.syncword_detection)));
            expect(
                eq(ConnectionResult::SUCCESS,
                   fg.connect<"out">(*packet_receiver.payload_crc_check).to<"in">(sink)));
            scheduler::Simple sched{ std::move(fg) };
            MsgPortOut toScheduler;
            expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
            std::thread stopper([&toScheduler]() {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                sendMessage<message::Command::Set>(toScheduler,
                                                   "",
                                                   block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
            });
            expect(sched.runAndWait().has_value());
            stopper.join();
            const auto data = sink.data();
            std::vector<uint8_t> expected_data;
            for (const auto& packet : source.data) {
                if (packet.data.size() != 4096) {
                    expected_data.insert(
                        expected_data.end(), packet.data.cbegin(), packet.data.cend());
                }
            }
            expect(eq(data.size(), expected_data.size()));
            expect(eq(data, expected_data));
            const auto tags = sink.tags();
            expect(eq(tags.size(), packet_lengths.size() - 1));
            for (size_t j = 0; j < tags.size(); ++j) {
                const auto& tag = tags[j];
                const auto packet_len = pmtv::cast<uint64_t>(tag.at("packet_len"));
                expect(eq(packet_len, static_cast<uint64_t>(packet_lengths[j])));
            }
        } |
        // arguments are { freq_error, stream_mode }
        std::vector<std::tuple<float, bool>>({ { 0.0f, false },
                                               { 0.0f, true },
                                               { 0.006f, false },
                                               { 0.006f, true },
                                               { -0.02f, false },
                                               { -0.02f, true } });
};

int main() {}
