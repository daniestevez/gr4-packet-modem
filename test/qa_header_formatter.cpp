#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/header_formatter.hpp>
#include <gnuradio-4.0/packet-modem/message_strobe.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite HeaderFormatterTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "header_formatter"_test = [] {
        Graph fg;
        const gr::property_map message = { { "packet_length", 1234 } };
        auto& strobe = fg.emplaceBlock<MessageStrobe<>>(
            { { "message", message }, { "interval_secs", 0.1 } });
        auto& header_formatter = fg.emplaceBlock<HeaderFormatter<>>();
        auto& stream_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<uint8_t>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        expect(eq(gr::ConnectionResult::SUCCESS,
                  strobe.strobe.connect(header_formatter.metadata)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(header_formatter).to<"in">(stream_to_pdu)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(stream_to_pdu).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        MsgPortOut toScheduler;
        expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto pdus = sink.data();
        // nominally, 10 PDUs are expected
        expect(8_u <= pdus.size() && pdus.size() <= 12_u);
        std::vector<uint8_t> expected_header = { 0x04, 0xd2, 0x00, 0x55 };
        for (const auto& pdu : pdus) {
            expect(eq(pdu.data, expected_header));
        }
    };

    "header_formatter_pdu"_test = [] {
        Graph fg;
        const gr::property_map message = { { "packet_length", 1234 } };
        auto& strobe = fg.emplaceBlock<MessageStrobe<>>(
            { { "message", message }, { "interval_secs", 0.1 } });
        auto& header_formatter = fg.emplaceBlock<HeaderFormatter<Pdu<uint8_t>>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        expect(eq(gr::ConnectionResult::SUCCESS,
                  strobe.strobe.connect(header_formatter.metadata)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(header_formatter).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        MsgPortOut toScheduler;
        expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto pdus = sink.data();
        // nominally, 10 PDUs are expected
        expect(8_u <= pdus.size() && pdus.size() <= 12_u);
        std::vector<uint8_t> expected_header = { 0x04, 0xd2, 0x00, 0x55 };
        for (const auto& pdu : pdus) {
            expect(eq(pdu.data, expected_header));
        }
    };
};

int main() {}
