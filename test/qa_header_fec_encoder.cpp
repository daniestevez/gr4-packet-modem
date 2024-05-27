#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/header_fec_encoder.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite HeadTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "head"_test = [] {
        Graph fg;
        const std::vector<uint8_t> v = { 0xeb, 0x59, 0x29, 0xd6, //
                                         0x6b, 0x11, 0x2d, 0x72 };
        const std::vector<Tag> tags = { { 0, { { "packet_len", 4UZ } } },
                                        { 4, { { "packet_len", 4UZ } } } };
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>(v, false, tags);
        auto& fec = fg.emplaceBlock<HeaderFecEncoder>();
        auto& sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(fec)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(fec).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const std::vector<uint8_t> expected_output = {
            0xeb, 0x59, 0x29, 0xd6, //
            0x35, 0xf1, 0xca, 0x33, 0x98, 0x48, 0xd8, 0x4d, 0x41, 0x24, 0xc1, 0x57,
            0xeb, 0x59, 0x29, 0xd6,                                                 //
            0x35, 0xf1, 0xca, 0x33, 0x98, 0x48, 0xd8, 0x4d, 0x41, 0x24, 0xc1, 0x57, //
            0x6b, 0x11, 0x2d, 0x72,                                                 //
            0x1b, 0x59, 0xe3, 0x4f, 0xe7, 0x71, 0x91, 0x5f, 0xe1, 0x37, 0x6e, 0x2b, //
            0x6b, 0x11, 0x2d, 0x72,                                                 //
            0x1b, 0x59, 0xe3, 0x4f, 0xe7, 0x71, 0x91, 0x5f, 0xe1, 0x37, 0x6e, 0x2b
        };
        expect(eq(sink.data(), expected_output));
        const std::vector<Tag> expected_tags = { { 0, { { "packet_len", 32UZ } } },
                                                 { 32, { { "packet_len", 32UZ } } } };
        expect(sink.tags() == expected_tags);
    };
};

int main() {}
