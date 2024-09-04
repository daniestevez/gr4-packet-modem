#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/pack_bits.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/unpack_bits.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite PackUnpackTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "pack_8bits_fixed"_test = [] {
        Graph fg;
        const std::vector<uint8_t> v = {
            0, 0, 0, 1, 0, 1, 1, 0, //
            0, 1, 0, 1, 1, 1, 0, 0, //
            1, 1, 0, 0, 0, 1, 0, 1, //
            1, 1, 1, 1, 1, 1, 1, 1, //
            0, 0, 0, 0, 0, 0, 0, 0, //
            1, 0, 1, 0, 1, 0, 1, 0, //
            0, 1, 0, 1, 0, 1, 0, 1, //
        };
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
        source.data = v;
        auto& pack_msb = fg.emplaceBlock<PackBits<>>({ { "inputs_per_output", 8UZ } });
        auto& pack_lsb =
            fg.emplaceBlock<PackBits<Endianness::LSB>>({ { "inputs_per_output", 8UZ } });
        auto& sink_msb = fg.emplaceBlock<VectorSink<uint8_t>>();
        auto& sink_lsb = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack_msb)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack_lsb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pack_msb).to<"in">(sink_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pack_lsb).to<"in">(sink_lsb)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const std::vector<uint8_t> expected_msb = { 0b0010110,  0b01011100, 0b11000101,
                                                    0b11111111, 0b00000000, 0b10101010,
                                                    0b01010101 };
        const std::vector<uint8_t> expected_lsb = { 0b01101000, 0b00111010, 0b10100011,
                                                    0b11111111, 0b0000000,  0b01010101,
                                                    0b10101010 };
        expect(eq(sink_msb.data(), expected_msb));
        expect(eq(sink_lsb.data(), expected_lsb));
    };

    "pack_8bits_fixed_pdu"_test = [] {
        Graph fg;
        const std::vector<uint8_t> v = {
            0, 0, 0, 1, 0, 1, 1, 0, //
            0, 1, 0, 1, 1, 1, 0, 0, //
            1, 1, 0, 0, 0, 1, 0, 1, //
            1, 1, 1, 1, 1, 1, 1, 1, //
            0, 0, 0, 0, 0, 0, 0, 0, //
            1, 0, 1, 0, 1, 0, 1, 0, //
            0, 1, 0, 1, 0, 1, 0, 1, //
        };
        const Pdu<uint8_t> pdu = { v, {} };
        auto& source = fg.emplaceBlock<VectorSource<Pdu<uint8_t>>>();
        source.data = std::vector<Pdu<uint8_t>>{ pdu };
        auto& pack_msb =
            fg.emplaceBlock<PackBits<Endianness::MSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "inputs_per_output", 8UZ } });
        auto& pack_lsb =
            fg.emplaceBlock<PackBits<Endianness::LSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "inputs_per_output", 8UZ } });
        auto& sink_msb = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        auto& sink_lsb = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack_msb)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack_lsb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pack_msb).to<"in">(sink_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pack_lsb).to<"in">(sink_lsb)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const std::vector<uint8_t> expected_msb = { 0b0010110,  0b01011100, 0b11000101,
                                                    0b11111111, 0b00000000, 0b10101010,
                                                    0b01010101 };
        const std::vector<uint8_t> expected_lsb = { 0b01101000, 0b00111010, 0b10100011,
                                                    0b11111111, 0b0000000,  0b01010101,
                                                    0b10101010 };
        expect(eq(sink_msb.data().size(), 1_u));
        expect(eq(sink_lsb.data().size(), 1_u));
        expect(eq(sink_msb.data().at(0).data, expected_msb));
        expect(eq(sink_lsb.data().at(0).data, expected_lsb));
    };

    "unpack_8bits_fixed"_test = [] {
        Graph fg;
        const std::vector<uint8_t> v = { 0xab, 0x00, 0xff, 0x12, 0x34, 0x55 };
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
        source.data = v;
        auto& unpack_msb =
            fg.emplaceBlock<UnpackBits<>>({ { "outputs_per_input", 8UZ } });
        auto& unpack_lsb = fg.emplaceBlock<UnpackBits<Endianness::LSB>>(
            { { "outputs_per_input", 8UZ } });
        auto& sink_msb = fg.emplaceBlock<VectorSink<uint8_t>>();
        auto& sink_lsb = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(unpack_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(unpack_lsb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(unpack_msb).to<"in">(sink_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(unpack_lsb).to<"in">(sink_lsb)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const std::vector<uint8_t> expected_msb = {
            1, 0, 1, 0, 1, 0, 1, 1, //
            0, 0, 0, 0, 0, 0, 0, 0, //
            1, 1, 1, 1, 1, 1, 1, 1, //
            0, 0, 0, 1, 0, 0, 1, 0, //
            0, 0, 1, 1, 0, 1, 0, 0, //
            0, 1, 0, 1, 0, 1, 0, 1, //
        };
        const std::vector<uint8_t> expected_lsb = {
            1, 1, 0, 1, 0, 1, 0, 1, //
            0, 0, 0, 0, 0, 0, 0, 0, //
            1, 1, 1, 1, 1, 1, 1, 1, //
            0, 1, 0, 0, 1, 0, 0, 0, //
            0, 0, 1, 0, 1, 1, 0, 0, //
            1, 0, 1, 0, 1, 0, 1, 0, //
        };
        expect(eq(sink_msb.data(), expected_msb));
        expect(eq(sink_lsb.data(), expected_lsb));
    };

    "unpack_8bits_fixed_pdu"_test = [] {
        Graph fg;
        const std::vector<uint8_t> v = { 0xab, 0x00, 0xff, 0x12, 0x34, 0x55 };
        const Pdu<uint8_t> pdu = { v, {} };
        auto& source = fg.emplaceBlock<VectorSource<Pdu<uint8_t>>>();
        source.data = std::vector<Pdu<uint8_t>>{ pdu };
        auto& unpack_msb =
            fg.emplaceBlock<UnpackBits<Endianness::MSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "outputs_per_input", 8UZ } });
        auto& unpack_lsb =
            fg.emplaceBlock<UnpackBits<Endianness::LSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "outputs_per_input", 8UZ } });
        auto& sink_msb = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        auto& sink_lsb = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(unpack_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(unpack_lsb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(unpack_msb).to<"in">(sink_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(unpack_lsb).to<"in">(sink_lsb)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const std::vector<uint8_t> expected_msb = {
            1, 0, 1, 0, 1, 0, 1, 1, //
            0, 0, 0, 0, 0, 0, 0, 0, //
            1, 1, 1, 1, 1, 1, 1, 1, //
            0, 0, 0, 1, 0, 0, 1, 0, //
            0, 0, 1, 1, 0, 1, 0, 0, //
            0, 1, 0, 1, 0, 1, 0, 1, //
        };
        const std::vector<uint8_t> expected_lsb = {
            1, 1, 0, 1, 0, 1, 0, 1, //
            0, 0, 0, 0, 0, 0, 0, 0, //
            1, 1, 1, 1, 1, 1, 1, 1, //
            0, 1, 0, 0, 1, 0, 0, 0, //
            0, 0, 1, 0, 1, 1, 0, 0, //
            1, 0, 1, 0, 1, 0, 1, 0, //
        };
        expect(eq(sink_msb.data().size(), 1_u));
        expect(eq(sink_lsb.data().size(), 1_u));
        expect(eq(sink_msb.data().at(0).data, expected_msb));
        expect(eq(sink_lsb.data().at(0).data, expected_lsb));
    };

    "pack_unpack_random_data"_test = [](auto args) {
        size_t packed_nibbles;
        uint8_t bits_per_nibble;
        std::tie(packed_nibbles, bits_per_nibble) = args;
        Graph fg;
        constexpr auto num_items = 120000_ul;
        auto& source = fg.emplaceBlock<RandomSource<uint8_t>>(
            { { "minimum", uint8_t{ 0 } },
              { "maximum", uint8_t{ 255 } },
              { "num_items", static_cast<size_t>(num_items) },
              { "repeat", false } });
        auto& data_sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        auto& pack_msb = fg.emplaceBlock<PackBits<Endianness::MSB>>(
            { { "inputs_per_output", packed_nibbles },
              { "bits_per_input", bits_per_nibble } });
        auto& pack_lsb = fg.emplaceBlock<PackBits<Endianness::LSB>>(
            { { "inputs_per_output", packed_nibbles },
              { "bits_per_input", bits_per_nibble } });
        auto& unpack_msb = fg.emplaceBlock<UnpackBits<Endianness::MSB>>(
            { { "outputs_per_input", packed_nibbles },
              { "bits_per_output", bits_per_nibble } });
        auto& unpack_lsb = fg.emplaceBlock<UnpackBits<Endianness::LSB>>(
            { { "outputs_per_input", packed_nibbles },
              { "bits_per_output", bits_per_nibble } });
        auto& sink_msb = fg.emplaceBlock<VectorSink<uint8_t>>();
        auto& sink_lsb = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(data_sink)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack_msb)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack_lsb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pack_msb).to<"in">(unpack_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pack_lsb).to<"in">(unpack_lsb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(unpack_msb).to<"in">(sink_msb)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(unpack_lsb).to<"in">(sink_lsb)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto input_data = data_sink.data();
        std::vector<uint8_t> expected;
        expected.reserve(input_data.size());
        for (const auto in : input_data) {
            expected.push_back(static_cast<uint8_t>(in & ((1U << bits_per_nibble) - 1)));
        }
        expect(eq(input_data.size(), num_items));
        expect(eq(sink_msb.data(), expected));
        expect(eq(sink_lsb.data(), expected));
    } | std::vector<std::tuple<size_t, uint8_t>>{ { 8U, 1U }, { 4U, 1U }, { 4U, 2U },
                                                  { 3U, 1U }, { 3U, 2U }, { 2U, 1U },
                                                  { 2U, 4U }, { 1U, 1U }, { 1U, 2U },
                                                  { 1U, 3U } };
};

int main() {}
