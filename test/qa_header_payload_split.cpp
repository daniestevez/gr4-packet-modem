#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/header_payload_split.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite HeaderPayloadSplitTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "header_payload_split"_test = [](auto args) {
        Graph fg;
        size_t header_size;
        size_t payload_bits;
        std::tie(header_size, payload_bits) = args;
        std::vector<int> v(header_size + payload_bits);
        std::iota(v.begin(), v.end(), 0);
        const std::vector<Tag> tags = { { static_cast<ssize_t>(header_size),
                                          { { "payload_bits", payload_bits } } } };
        auto& source = fg.emplaceBlock<VectorSource<int>>();
        source.data = v;
        source.tags = tags;
        auto& split =
            fg.emplaceBlock<HeaderPayloadSplit<int>>({ { "header_size", header_size } });
        auto& header_sink = fg.emplaceBlock<VectorSink<int>>();
        auto& payload_sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(split)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"header">(split).to<"in">(header_sink)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"payload">(split).to<"in">(payload_sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto header = header_sink.data();
        expect(eq(header.size(), header_size));
        for (size_t j = 0; j < header_size; ++j) {
            expect(eq(header[j], v[j]));
        }
        const auto payload = payload_sink.data();
        expect(eq(payload.size(), payload_bits));
        for (size_t j = 0; j < payload_bits; ++j) {
            expect(eq(payload[j], v[header_size + j]));
        }
    } | std::vector<std::tuple<size_t, size_t>>{ { 256, 1500 }, { 128, 1500 },
                                                 { 256, 1 },    { 256, 23 },
                                                 { 64, 17321 }, { 256, 16385 } };
};

int main() {}
