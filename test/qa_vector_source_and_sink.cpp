#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <ranges>

boost::ut::suite VectorSourceAndSinkTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "vector_source_fixed_data"_test = [] {
        Graph fg;
        std::vector<int> v(100);
        std::iota(v.begin(), v.end(), 0);
        const std::vector<Tag> tags = {
            { 0, { { "a", pmtv::pmt_null() } } },
            { 10, { { "b", 0.1234 }, { "c", 12345U } } },
            { 73, { { "d", std::vector<int>{ 1, 2, 3 } }, { "e", 0.0f } } },
            { std::ssize(v) - 1, { { "f", pmtv::pmt_null() } } }
        };
        auto& source = fg.emplaceBlock<VectorSource<int>>(v, false, tags);
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        expect(eq(sink.data(), v));
        // do not use eq(), because tags cannot be formatted
        expect(sink.tags() == tags);
    };

    "vector_source_long_vector"_test = [] {
        Graph fg;
        constexpr auto num_items = 1000000_ul;
        std::vector<int> v(static_cast<size_t>(num_items));
        std::iota(v.begin(), v.end(), 0);
        auto& source = fg.emplaceBlock<VectorSource<int>>(v);
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        expect(eq(sink.data(), v));
    };

    "vector_source_repeat"_test = [] {
        Graph fg;
        constexpr auto num_items = 100_ul;
        std::vector<int> v(static_cast<size_t>(num_items));
        std::iota(v.begin(), v.end(), 0);
        const std::vector<Tag> tags = { { 0, { { "begin", pmtv::pmt_null() } } },
                                        { std::ssize(v) - 1,
                                          { { "end", pmtv::pmt_null() } } } };
        const auto repetitions = 1000UZ;
        auto& source = fg.emplaceBlock<VectorSource<int>>(v, true, tags);
        auto& head =
            fg.emplaceBlock<Head<int>>(static_cast<size_t>(num_items) * repetitions);
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };

        // TODO: remove this once https://github.com/fair-acc/gnuradio4/pull/342 is merged
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        //

        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto sink_data = sink.data();
        for (size_t j = 0; j < repetitions; ++j) {
            const auto r = sink_data |
                           std::views::drop(j * static_cast<size_t>(num_items)) |
                           std::views::take(static_cast<size_t>(num_items));
            const std::vector<int> rep(r.begin(), r.end());
            expect(eq(rep, v));
        }
        const auto sink_tags = sink.tags();
        expect(eq(sink_tags.size(), tags.size() * repetitions));
        for (size_t j = 0; j < repetitions; ++j) {
            const auto& tag_begin = sink_tags[2 * j];
            const auto& tag_end = sink_tags[2 * j + 1];
            expect(eq(tag_begin.index, static_cast<ssize_t>(j * v.size())));
            expect(eq(tag_end.index, static_cast<ssize_t>((j + 1) * v.size() - 1)));
            expect(tag_begin.map == tags[0].map);
            expect(tag_end.map == tags[1].map);
        }
    };
};

int main() {}
