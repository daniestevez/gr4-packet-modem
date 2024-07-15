#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_ingress.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite PacketIngressTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "packet_ingress"_test = [] {
        Graph fg;
        const std::vector<size_t> packet_lengths = { 1,     3,     7,  25,     100, 10000,
                                                     65535, 65536, 47, 100000, 23,  18 };
        std::vector<Pdu<uint8_t>> pdus;
        pdus.reserve(packet_lengths.size());
        for (const auto packet_length : packet_lengths) {
            std::vector<uint8_t> v(packet_length);
            std::iota(v.begin(), v.end(), 0);
            const Pdu<uint8_t> pdu = { std::move(v), {} };
            pdus.push_back(std::move(pdu));
        }
        auto& source = fg.emplaceBlock<VectorSource<Pdu<uint8_t>>>();
        source.data = pdus;
        auto& pdu_to_stream = fg.emplaceBlock<PduToTaggedStream<uint8_t>>();
        auto& packet_ingress = fg.emplaceBlock<PacketIngress<>>();
        auto& stream_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<uint8_t>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        auto& meta_sink = fg.emplaceBlock<VectorSink<Message>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(pdu_to_stream)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pdu_to_stream).to<"in">(packet_ingress)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(packet_ingress).to<"in">(stream_to_pdu)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(stream_to_pdu).to<"in">(sink)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"metadata">(packet_ingress).to<"in">(meta_sink)));
        scheduler::Simple sched{ std::move(fg) };
        // this flowgraph doesn't terminate on its own (perhaps because the
        // PduToTaggedStream output is Async)
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        std::vector<size_t> expected_packet_lengths;
        expected_packet_lengths.reserve(packet_lengths.size());
        for (const auto len : packet_lengths) {
            if (len < 65536U) {
                expected_packet_lengths.push_back(len);
            }
        }
        const auto pdus_out = sink.data();
        expect(eq(pdus_out.size(), expected_packet_lengths.size()));
        for (size_t j = 0; j < pdus_out.size(); ++j) {
            const auto& pdu = pdus_out[j];
            expect(eq(pdu.data.size(), expected_packet_lengths[j]));
            std::vector<uint8_t> v(pdu.data.size());
            std::iota(v.begin(), v.end(), 0);
            expect(eq(pdu.data, v));
        }
        const auto messages = meta_sink.data();
        expect(eq(messages.size(), expected_packet_lengths.size()));
        for (size_t j = 0; j < messages.size(); ++j) {
            const auto& message = messages[j];
            expect(message.data.has_value());
            const property_map expected_map = { { "packet_length",
                                                  expected_packet_lengths[j] },
                                                { "packet_type", "USER_DATA" } };
            expect(message.data.value() == expected_map);
        }
    };

    "packet_ingress_pdu"_test = [] {
        Graph fg;
        const std::vector<size_t> packet_lengths = { 1,     3,     7,  25,     100, 10000,
                                                     65535, 65536, 47, 100000, 23,  18 };
        std::vector<Pdu<uint8_t>> pdus;
        pdus.reserve(packet_lengths.size());
        for (const auto packet_length : packet_lengths) {
            std::vector<uint8_t> v(packet_length);
            std::iota(v.begin(), v.end(), 0);
            const Pdu<uint8_t> pdu = { std::move(v), {} };
            pdus.push_back(std::move(pdu));
        }
        auto& source = fg.emplaceBlock<VectorSource<Pdu<uint8_t>>>();
        source.data = pdus;
        auto& packet_ingress = fg.emplaceBlock<PacketIngress<Pdu<uint8_t>>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        auto& meta_sink = fg.emplaceBlock<VectorSink<Message>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(packet_ingress)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(packet_ingress).to<"in">(sink)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"metadata">(packet_ingress).to<"in">(meta_sink)));
        scheduler::Simple sched{ std::move(fg) };
        // this flowgraph doesn't terminate on its own (perhaps because the
        // PduToTaggedStream output is Async)
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        std::vector<size_t> expected_packet_lengths;
        expected_packet_lengths.reserve(packet_lengths.size());
        for (const auto len : packet_lengths) {
            if (len < 65536U) {
                expected_packet_lengths.push_back(len);
            }
        }
        const auto pdus_out = sink.data();
        expect(eq(pdus_out.size(), expected_packet_lengths.size()));
        for (size_t j = 0; j < pdus_out.size(); ++j) {
            const auto& pdu = pdus_out[j];
            expect(eq(pdu.data.size(), expected_packet_lengths[j]));
            std::vector<uint8_t> v(pdu.data.size());
            std::iota(v.begin(), v.end(), 0);
            expect(eq(pdu.data, v));
        }
        const auto messages = meta_sink.data();
        expect(eq(messages.size(), expected_packet_lengths.size()));
        for (size_t j = 0; j < messages.size(); ++j) {
            const auto& message = messages[j];
            expect(message.data.has_value());
            const property_map expected_map = { { "packet_length",
                                                  expected_packet_lengths[j] },
                                                { "packet_type", "USER_DATA" } };
            expect(message.data.value() == expected_map);
        }
    };
};

int main() {}
