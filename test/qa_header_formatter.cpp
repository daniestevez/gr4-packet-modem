#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/header_formatter.hpp>
#include <gnuradio-4.0/packet-modem/item_strobe.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite HeaderFormatterTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "header_formatter"_test = [] {
        Graph fg;
        auto& strobe =
            fg.emplaceBlock<ItemStrobe<gr::Message>>({ { "interval_secs", 0.1 } });
        strobe.item.data = gr::property_map{ { "packet_length", uint64_t{ 1234 } },
                                             { "packet_type", "user_data" } };
        auto& header_formatter = fg.emplaceBlock<HeaderFormatter<>>();
        auto& stream_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<uint8_t>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(strobe).to<"metadata">(header_formatter)));
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
        auto& strobe =
            fg.emplaceBlock<ItemStrobe<gr::Message>>({ { "interval_secs", 0.1 } });
        strobe.item.data = gr::property_map{ { "packet_length", uint64_t{ 1234 } },
                                             { "packet_type", "user_data" } };
        auto& header_formatter = fg.emplaceBlock<HeaderFormatter<Pdu<uint8_t>>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(strobe).to<"metadata">(header_formatter)));
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
