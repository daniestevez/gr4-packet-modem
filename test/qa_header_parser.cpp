#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/header_parser.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite HeaderParserTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "header_parser_correct_headers"_test = [] {
        Graph fg;
        const std::vector<uint8_t> v = { 0x05, 0xdc, 0x00, 0x55, //
                                         0x08, 0x00, 0x00, 0x55 };
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
        source.data = v;
        auto& parser = fg.emplaceBlock<HeaderParser<>>();
        auto& sink = fg.emplaceBlock<VectorSink<Message>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(parser)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"metadata">(parser).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto messages = sink.data();
        expect(eq(messages.size(), 2_ul));
        for (const auto& msg : messages) {
            expect(msg.data.has_value());
        }
        expect(messages[0].data.value() ==
               property_map{ { "packet_length", 1500UZ },
                             { "constellation", "QPSK" },
                             { "packet_type", "USER_DATA" } });
        expect(messages[1].data.value() ==
               property_map{ { "packet_length", 2048UZ },
                             { "constellation", "QPSK" },
                             { "packet_type", "USER_DATA" } });
    };

    "header_parser_wrong_header"_test = [] {
        Graph fg;
        const std::vector<uint8_t> v = { 0x12, 0x34, 0x56, 0x78 };
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
        source.data = v;
        auto& parser = fg.emplaceBlock<HeaderParser<>>();
        auto& sink = fg.emplaceBlock<VectorSink<Message>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(parser)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"metadata">(parser).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto messages = sink.data();
        expect(eq(messages.size(), 1_ul));
        for (const auto& msg : messages) {
            expect(msg.data.has_value());
        }
        expect(messages[0].data.value() ==
               property_map{ { "invalid_header", pmtv::pmt_null() } });
    };
};

int main() {}
