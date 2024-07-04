#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/syncword_remove.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite SyncwordRemoveTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "syncword_remove"_test = [] {
        Graph fg;
        constexpr auto num_items = 1000_ul;
        std::vector<int> v(static_cast<size_t>(num_items));
        std::iota(v.begin(), v.end(), 0);
        auto& source = fg.emplaceBlock<VectorSource<int>>();
        source.data = v;
        source.tags = std::vector<Tag>{ { 10, { { "syncword_amplitude", 1.0f } } },
                                        { 100, { { "syncword_amplitude", 0.1f } } },
                                        { 250, { { "syncword_amplitude", 2.3f } } } };
        const size_t syncword_size = 64;
        auto& syncword_remove = fg.emplaceBlock<gr::packet_modem::SyncwordRemove<int>>(
            { { "syncword_size", syncword_size } });
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(syncword_remove)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(syncword_remove).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        const size_t num_syncwords = source.tags.size();
        expect(eq(data.size(),
                  static_cast<size_t>(num_items) - num_syncwords * syncword_size));
        auto expected = v;
        expected.erase(expected.begin() + 250, expected.begin() + 250 + syncword_size);
        expected.erase(expected.begin() + 100, expected.begin() + 100 + syncword_size);
        expected.erase(expected.begin() + 10, expected.begin() + 10 + syncword_size);
        expect(eq(data, expected));
    };
};

int main() {}
