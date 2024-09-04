#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite StreamToTaggedStreamTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "stream_to_tagged_stream"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        constexpr auto packet_len = 100_ul;
        std::vector<int> v(static_cast<size_t>(num_items));
        std::iota(v.begin(), v.end(), 0);
        auto& source = fg.emplaceBlock<VectorSource<int>>();
        source.data = v;
        auto& stream_to_tagged = fg.emplaceBlock<StreamToTaggedStream<int>>(
            { { "packet_length", static_cast<uint64_t>(packet_len) } });
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(stream_to_tagged)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(stream_to_tagged).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        expect(eq(sink.data(), v));
        const auto tags = sink.tags();
        expect(eq(tags.size(),
                  (static_cast<size_t>(num_items) + static_cast<size_t>(packet_len) - 1) /
                      static_cast<size_t>(packet_len)));
        const gr::property_map expected_tag = { { "packet_len",
                                                  static_cast<uint64_t>(packet_len) } };
        ssize_t index = 0;
        for (const auto& tag : tags) {
            expect(eq(tag.index, index));
            expect(tag.map == expected_tag);
            index += static_cast<ssize_t>(static_cast<size_t>(packet_len));
        }
    };
};

int main() {}
