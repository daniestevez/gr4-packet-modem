#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>
#include <array>

static constexpr std::array<uint8_t, 255> ccsds_scrambling_sequence = {
    1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0,
    1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
    1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0,
    0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0,
    1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0,
    1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0,
    1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0
};

boost::ut::suite AdditiveScramblerTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "additive_scrambler_ccsds"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        auto& source = fg.emplaceBlock<RandomSource<uint8_t>>(
            uint8_t{ 0 }, uint8_t{ 1 }, static_cast<size_t>(num_items), false);
        auto& input_sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        auto& scrambler = fg.emplaceBlock<AdditiveScrambler<uint8_t>>(0xA9U, 0xFFU, 7U);
        auto& output_sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(input_sink)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(scrambler)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(scrambler).to<"in">(output_sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto input_data = input_sink.data();
        expect(eq(input_data.size(), num_items));
        const auto output_data = output_sink.data();
        expect(eq(output_data.size(), num_items));
        for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
            expect(
                eq(output_data[j], input_data[j] ^ ccsds_scrambling_sequence[j % 255]));
        }
    };

    // test for the new CCSDS scrambler defined in CCSDS 131.0-B-5 (September 2023)
    "additive_scrambler_ccsds_2023"_test = [] {
        Graph fg;
        const std::vector<uint8_t> expected = { 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0,
                                                0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1,
                                                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1 };
        const std::vector<uint8_t> v(expected.size());
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>(v);
        auto& scrambler =
            fg.emplaceBlock<AdditiveScrambler<uint8_t>>(0x4001U, 0x18E38U, 16U);
        auto& sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(scrambler)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(scrambler).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        expect(eq(sink.data(), expected));
    };

    "additive_scrambler_reset"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        constexpr auto num_reset = 100_ul;
        auto& source = fg.emplaceBlock<NullSource<uint8_t>>();
        auto& head = fg.emplaceBlock<Head<uint8_t>>(static_cast<uint64_t>(num_items));
        auto& add_tags = fg.emplaceBlock<StreamToTaggedStream<uint8_t>>(
            static_cast<uint64_t>(num_reset));
        auto& scrambler = fg.emplaceBlock<AdditiveScrambler<uint8_t>>(
            0xA9U, 0xFFU, 7U, 0U, "packet_len");
        auto& sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(add_tags)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(add_tags).to<"in">(scrambler)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(scrambler).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
            expect(eq(data[j],
                      ccsds_scrambling_sequence[j % static_cast<size_t>(num_reset)]));
        }
    };
};

int main() {}
