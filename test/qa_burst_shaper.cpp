#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/burst_shaper.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <ranges>

boost::ut::suite BurstShaperTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "burst_shaper"_test = [] {
        Graph fg;
        const std::vector<size_t> packet_lengths = { 1,      2, 3,  4,   5,   6,    7,
                                                     8,      9, 10, 100, 250, 1000, 10000,
                                                     100000, 7, 3,  25,  14,  28,   178 };
        std::vector<Pdu<float>> v;
        v.reserve(packet_lengths.size());
        for (const auto packet_length : packet_lengths) {
            const Pdu<float> pdu = { std::vector(packet_length, 1.0f), {} };
            v.push_back(std::move(pdu));
        }
        auto& source = fg.emplaceBlock<VectorSource<Pdu<float>>>(v);
        auto& pdu_to_tagged = fg.emplaceBlock<PduToTaggedStream<float>>();
        const std::vector<float> leading = { 0.1f, 0.5f, 0.9f };
        const std::vector<float> trailing = { 0.8f, 0.2f };
        auto& burst_shaper = fg.emplaceBlock<BurstShaper<float>>(leading, trailing);
        auto& tagged_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<float>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<float>>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(pdu_to_tagged)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pdu_to_tagged).to<"in">(burst_shaper)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(burst_shaper).to<"in">(tagged_to_pdu)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(tagged_to_pdu).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto pdus = sink.data();
        expect(eq(pdus.size(), packet_lengths.size()));
        for (size_t j = 0; j < packet_lengths.size(); ++j) {
            const auto& pdu = pdus[j];
            expect(eq(pdu.data.size(), packet_lengths[j]));
            const auto r1 = leading | std::views::take(packet_lengths[j]);
            const std::vector<float> expected_leading(r1.begin(), r1.end());
            const auto r2 = pdu.data | std::views::take(leading.size());
            const std::vector<float> pdu_leading(r2.begin(), r2.end());
            expect(eq(pdu_leading, expected_leading));
            if (packet_lengths[j] > leading.size() + trailing.size()) {
                const std::vector<float> middle(pdu.data.cbegin() + std::ssize(leading),
                                                pdu.data.cend() - std::ssize(trailing));
                const std::vector<float> expected_middle(
                    packet_lengths[j] - leading.size() - trailing.size(), 1.0f);
                expect(eq(middle, expected_middle));
            }
            if (packet_lengths[j] > leading.size()) {
                const auto n = static_cast<ssize_t>(
                    std::min(packet_lengths[j] - leading.size(), trailing.size()));
                const std::vector<float> pdu_trailing(pdu.data.cend() - n,
                                                      pdu.data.cend());
                const std::vector<float> expected_trailing(trailing.cend() - n,
                                                           trailing.cend());
                expect(eq(pdu_trailing, expected_trailing));
            }
        }
    };
};

int main() {}
