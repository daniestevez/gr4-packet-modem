#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/crc.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/crc_check.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <ranges>

boost::ut::suite CrcTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "crc16"_test = [] {
        auto crc = Crc(16U, 0x1021U, 0xFFFFU, 0xFFFFU, true, true);
        const std::vector<uint8_t> v(10);
        expect(eq(crc.compute(v), 0x6378U));
    };

    "crc_append_one_packet"_test = [](size_t packet_len) {
        Graph fg;
        const std::vector<uint8_t> v(packet_len);
        const std::vector<Tag> t = { { 0, { { "packet_len", packet_len } } } };
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
        source.data = v;
        source.tags = t;
        auto& crc_append =
            fg.emplaceBlock<CrcAppend<>>({ { "num_bits", 16U },
                                           { "poly", uint64_t{ 0x1021 } },
                                           { "initial_value", uint64_t{ 0xFFFF } },
                                           { "final_xor", uint64_t{ 0xFFFF } },
                                           { "input_reflected", true },
                                           { "result_reflected", true } });
        auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(crc_append)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(crc_append).to<"in">(sink)));
        gr::scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), packet_len + 2U));
        std::vector<uint8_t> expected(v);
        auto crc_calc = Crc(16U, 0x1021U, 0xFFFFU, 0xFFFFU, true, true);
        const auto crc16 = crc_calc.compute(v);
        expected.push_back(static_cast<uint8_t>((crc16 >> 8) & 0xFF));
        expected.push_back(static_cast<uint8_t>(crc16 & 0xFF));
        expect(eq(data, expected));
        const auto tags = sink.tags();
        expect(eq(tags.size(), 1_u));
        expect(eq(tags[0].index, 0_i));
        expect(tags[0].map == property_map{ { "packet_len", packet_len + 2U } });
    } | std::vector<size_t>{ 1U, 4U, 10U, 100U, 65536U, 100000U };

    "crc_append_one_packet_pdu"_test = [](size_t packet_len) {
        Graph fg;
        const std::vector<uint8_t> v(packet_len);
        const Pdu<uint8_t> pdu = { v, {} };
        auto& source = fg.emplaceBlock<VectorSource<Pdu<uint8_t>>>();
        source.data = std::vector<Pdu<uint8_t>>{ pdu };
        auto& crc_append = fg.emplaceBlock<CrcAppend<Pdu<uint8_t>, uint64_t>>(
            { { "num_bits", 16U },
              { "poly", uint64_t{ 0x1021 } },
              { "initial_value", uint64_t{ 0xFFFF } },
              { "final_xor", uint64_t{ 0xFFFF } },
              { "input_reflected", true },
              { "result_reflected", true } });
        auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<Pdu<uint8_t>>>();
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(crc_append)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(crc_append).to<"in">(sink)));
        gr::scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), 1_u));
        const auto pdu_out = data.at(0);
        expect(eq(pdu_out.data.size(), packet_len + 2U));
        std::vector<uint8_t> expected(v);
        auto crc_calc = Crc(16U, 0x1021U, 0xFFFFU, 0xFFFFU, true, true);
        const auto crc16 = crc_calc.compute(v);
        expected.push_back(static_cast<uint8_t>((crc16 >> 8) & 0xFF));
        expected.push_back(static_cast<uint8_t>(crc16 & 0xFF));
        expect(eq(pdu_out.data, expected));
        expect(pdu_out.tags == pdu.tags);
    } | std::vector<size_t>{ 1U, 4U, 10U, 100U, 65536U, 100000U };

    "crc_check_one_packet"_test =
        [](auto args) {
            size_t packet_len;
            bool discard;
            std::tie(packet_len, discard) = args;
            Graph fg;
            std::vector<uint8_t> v(packet_len);
            auto crc_calc = Crc(16U, 0x1021U, 0xFFFFU, 0xFFFFU, true, true);
            const auto crc16 = crc_calc.compute(v);
            v.push_back(static_cast<uint8_t>((crc16 >> 8) & 0xFF));
            v.push_back(static_cast<uint8_t>(crc16 & 0xFF));
            const std::vector<Tag> t = { { 0, { { "packet_len", packet_len + 2U } } } };
            auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
            source.data = v;
            source.tags = t;
            auto& crc_check =
                fg.emplaceBlock<CrcCheck<>>({ { "num_bits", 16U },
                                              { "poly", uint64_t{ 0x1021 } },
                                              { "initial_value", uint64_t{ 0xFFFF } },
                                              { "final_xor", uint64_t{ 0xFFFF } },
                                              { "input_reflected", true },
                                              { "result_reflected", true },
                                              { "discard_crc", discard } });
            auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
            expect(eq(gr::ConnectionResult::SUCCESS,
                      fg.connect<"out">(source).to<"in">(crc_check)));
            expect(eq(gr::ConnectionResult::SUCCESS,
                      fg.connect<"out">(crc_check).to<"in">(sink)));
            gr::scheduler::Simple sched{ std::move(fg) };
            expect(sched.runAndWait().has_value());
            const auto data = sink.data();
            if (discard) {
                expect(eq(data.size(), packet_len));
                const auto r = v | std::views::take(packet_len);
                std::vector<uint8_t> expected_discard(r.begin(), r.end());
                expect(eq(data, expected_discard));
            } else {
                expect(eq(data.size(), packet_len + 2U));
                expect(eq(data, v));
            }
            const auto tags = sink.tags();
            expect(eq(tags.size(), 1_u));
            const auto& tag = tags.at(0);
            expect(eq(tag.index, 0_i));
            expect(tag.map == property_map{ { "packet_len", data.size() } });
        } |
        std::vector<std::tuple<size_t, bool>>{
            { 1U, false }, { 4U, false }, { 10U, false }, { 100U, false },
            /* { 100U, true },  this does not work because the data doesn't arrive to the
               vector sink */
            /* { 65536U, false}, this does not work because the data is
               never presented at once to the CrcCheck::processBulk() function */
        };
};

int main() {}
