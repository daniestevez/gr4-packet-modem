#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/syncword_detection_filter.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>

boost::ut::suite SyncwordDetectionFilterTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "syncword_detection_filter"_test = [] {
        Graph fg;
        const size_t num_items = 100000;
        using c64 = std::complex<float>;
        std::vector<c64> v(num_items);
        std::iota(v.begin(), v.end(), 0);
        const size_t syncword_index = 12345;
        const size_t packet_length = 1500;
        // the syncword + header is nominally 768 samples long at 4
        // samples/symbol
        const size_t second_syncword_index = syncword_index + 2000;
        const std::vector<Tag> tags = { { static_cast<ssize_t>(syncword_index),
                                          { { "syncword_amplitude", 1.0f } } },
                                        { static_cast<ssize_t>(second_syncword_index),
                                          { { "syncword_amplitude", 2.0f } } } };
        auto& source = fg.emplaceBlock<VectorSource<c64>>();
        source.data = v;
        source.tags = tags;
        auto& syncword_detection_filter = fg.emplaceBlock<SyncwordDetectionFilter<>>();
        auto& sink = fg.emplaceBlock<VectorSink<c64>>();
        // repeat set to true in this block because otherwise it says it's DONE
        // and this causes the flowgraph to terminate
        auto& parsed_source =
            fg.emplaceBlock<VectorSource<Message>>({ { "repeat", true } });
        Message parsed;
        parsed.data = property_map{ { "packet_length", packet_length } };
        parsed_source.data = std::vector<Message>{ std::move(parsed) };
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(syncword_detection_filter)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(syncword_detection_filter).to<"in">(sink)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(parsed_source)
                      .to<"parsed_header">(syncword_detection_filter)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        expect(eq(data, v));
        const auto sink_tags = sink.tags();
        for (const auto& tag : sink_tags) {
            fmt::println("{} {}", tag.index, tag.map);
        }
        expect(eq(sink_tags.size(), 1_ul));
        const auto& syncword_tag = sink_tags.at(0);
        expect(eq(syncword_tag.index, static_cast<ssize_t>(syncword_index)));
        expect(syncword_tag.map == tags[0].map);
    };
};

int main() {}
