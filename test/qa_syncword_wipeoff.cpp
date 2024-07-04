#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/syncword_wipeoff.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite SyncwordWipeoffTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "syncword_wipeoff"_test = [] {
        Graph fg;
        constexpr auto num_items = 1000_ul;
        std::vector<int> v(static_cast<size_t>(num_items));
        std::iota(v.begin(), v.end(), 0);
        const auto expected = v;
        const std::vector<int> syncword = { 1,  1,  1,  1,  1,  1,  -1, -1, 1,  -1, 1,
                                            1,  1,  -1, -1, -1, 1,  -1, -1, -1, 1,  -1,
                                            -1, 1,  -1, -1, 1,  1,  1,  -1, -1, -1, 1,
                                            1,  -1, 1,  1,  -1, -1, -1, 1,  1,  -1, 1,
                                            -1, 1,  1,  1,  -1, 1,  1,  -1, 1,  -1, 1,
                                            -1, -1, 1,  -1, -1, 1,  1,  1,  1 };
        auto& source = fg.emplaceBlock<VectorSource<int>>();
        const std::vector<size_t> syncword_positions = { 10, 100, 250 };
        for (auto position : syncword_positions) {
            for (size_t j = 0; j < syncword.size(); ++j) {
                v[position + j] *= syncword[j];
            }
            source.tags.push_back(
                { static_cast<ssize_t>(position), { { "syncword_amplitude", 1.0f } } });
        }
        source.data = v;
        auto& syncword_wipeoff =
            fg.emplaceBlock<gr::packet_modem::SyncwordWipeoff<int, int>>(
                { { "syncword", syncword } });
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(syncword_wipeoff)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(syncword_wipeoff).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        expect(eq(data, expected));
    };
};

int main() {}
