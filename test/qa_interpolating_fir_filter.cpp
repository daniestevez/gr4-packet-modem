#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite InterpolatingFirFilterTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "interpolating_fir_filter"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        auto& source = fg.emplaceBlock<RandomSource<int>>(
            { { "minimum", -8 },
              { "maximum", 8 },
              { "num_items", static_cast<size_t>(num_items) },
              { "repeat", false } });
        auto& input_sink = fg.emplaceBlock<VectorSink<int>>();
        const size_t interpolation = 5U;
        const std::vector<int> taps = { 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                                        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
        auto& fir = fg.emplaceBlock<InterpolatingFirFilter<int>>(
            { { "interpolation", interpolation }, { "taps", taps } });
        auto& output_sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(input_sink)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(fir)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(fir).to<"in">(output_sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto input_data = input_sink.data();
        const auto output_data = output_sink.data();
        expect(eq(input_data.size(), num_items));
        expect(eq(output_data.size(), static_cast<size_t>(num_items) * interpolation));
        std::vector<int> input_zero_packed(output_data.size());
        for (size_t j = 0; j < input_data.size(); ++j) {
            input_zero_packed[interpolation * j] = input_data[j];
        }
        for (size_t j = 0; j < output_data.size(); ++j) {
            int y = 0;
            for (size_t k = 0; k < taps.size(); ++k) {
                if (j >= k) {
                    y += taps[k] * input_zero_packed[j - k];
                }
            }
            expect(eq(y, output_data[j]));
        }
    };

    "interpolating_fir_filter_pdu"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        auto& source = fg.emplaceBlock<RandomSource<int>>(
            { { "minimum", -8 },
              { "maximum", 8 },
              { "num_items", static_cast<size_t>(num_items) },
              { "repeat", false } });
        auto& input_sink = fg.emplaceBlock<VectorSink<int>>();
        auto& stream_to_tagged = fg.emplaceBlock<StreamToTaggedStream<int>>(
            { { "packet_length", uint64_t{ 100 } } });
        auto& tagged_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<int>>();
        const size_t interpolation = 5U;
        const std::vector<int> taps = { 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                                        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
        auto& fir = fg.emplaceBlock<InterpolatingFirFilter<Pdu<int>, Pdu<int>, int>>(
            { { "interpolation", interpolation }, { "taps", taps } });
        auto& pdu_to_tagged =
            fg.emplaceBlock<PduToTaggedStream<int>>({ { "packet_len_tag_key", "" } });
        auto& output_sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(input_sink)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(stream_to_tagged)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(stream_to_tagged).to<"in">(tagged_to_pdu)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(tagged_to_pdu).to<"in">(fir)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(fir).to<"in">(pdu_to_tagged)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pdu_to_tagged).to<"in">(output_sink)));
        scheduler::Simple sched{ std::move(fg) };
        // this test doesn't terminate on its own because there is a
        // PduToTaggedStream; stop it manually
        gr::MsgPortOut toScheduler;
        expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            gr::sendMessage<gr::message::Command::Set>(
                toScheduler,
                "",
                gr::block::property::kLifeCycleState,
                { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto input_data = input_sink.data();
        const auto output_data = output_sink.data();
        expect(eq(input_data.size(), num_items));
        expect(eq(output_data.size(), static_cast<size_t>(num_items) * interpolation));
        std::vector<int> input_zero_packed(output_data.size());
        for (size_t j = 0; j < input_data.size(); ++j) {
            input_zero_packed[interpolation * j] = input_data[j];
        }
        for (size_t j = 0; j < output_data.size(); ++j) {
            int y = 0;
            for (size_t k = 0; k < taps.size(); ++k) {
                if (j >= k) {
                    y += taps[k] * input_zero_packed[j - k];
                }
            }
            expect(eq(y, output_data[j]));
        }
    };
};

int main() {}
