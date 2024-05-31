#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/mapper.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <ranges>

boost::ut::suite MapperTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "mapper_fixed_input"_test = [] {
        Graph fg;
        const std::vector<float> map = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f };
        std::vector<uint8_t> v(16);
        std::iota(v.begin(), v.end(), uint8_t{ 0 });
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
        source.data = v;
        auto& mapper = fg.emplaceBlock<Mapper<uint8_t, float>>({ { "map", map } });
        auto& sink = fg.emplaceBlock<VectorSink<float>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(mapper)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(mapper).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const std::vector<float> expected = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f,
                                              0.7f, 0.8f, 0.1f, 0.2f, 0.3f, 0.4f,
                                              0.5f, 0.6f, 0.7f, 0.8f };
        expect(eq(sink.data(), expected));
    };
};

int main() {}
