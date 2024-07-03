#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/payload_metadata_insert.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>

boost::ut::suite PayloadMetadataInsertTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "payload_metadata_insert_header_only"_test = [] {
        Graph fg;
        const size_t num_items = 100000;
        // although in principle it should be possible to instantiate
        // PayloadMetadataInsert<int>, doing so gives compiler errors
        using c64 = std::complex<float>;
        std::vector<c64> v(num_items);
        std::iota(v.begin(), v.end(), 0);
        const size_t syncword_index = 12345;
        const std::vector<Tag> tags = { { static_cast<ssize_t>(syncword_index),
                                          { { "syncword_amplitude", 0.1f } } } };
        const size_t syncword_size = 64;
        const size_t header_size = 128;
        auto& source = fg.emplaceBlock<VectorSource<c64>>();
        source.data = v;
        source.tags = tags;
        auto& payload_metadata_insert = fg.emplaceBlock<PayloadMetadataInsert<>>(
            { { "syncword_size", syncword_size }, { "header_size", header_size } });
        auto& sink = fg.emplaceBlock<VectorSink<c64>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(payload_metadata_insert)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(payload_metadata_insert).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        // this test needs to be stopped with a message, because the
        // PayloadMetadataInsert keeps waiting for a message containing the
        // header decode
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto data = sink.data();
        expect(eq(data.size(), syncword_size + header_size));
        for (size_t j = 0; j < data.size(); ++j) {
            expect(eq(data[j], v[syncword_index + j]));
        }
        const auto sink_tags = sink.tags();
        expect(eq(sink_tags.size(), 2_ul));
        const auto& syncword_tag = sink_tags.at(0);
        expect(eq(syncword_tag.index, 0_l));
        expect(eq(pmtv::cast<std::string>(syncword_tag.map.at("constellation")),
                  std::string("PILOT")));
        expect(syncword_tag.map.contains("loop_bandwidth"));
        expect(eq(pmtv::cast<float>(syncword_tag.map.at("syncword_amplitude")), 0.1f));
        const auto& header_tag = sink_tags.at(1);
        expect(eq(header_tag.index, static_cast<ssize_t>(syncword_size)));
        expect(eq(pmtv::cast<std::string>(header_tag.map.at("constellation")),
                  std::string("QPSK")));
        expect(header_tag.map.contains("header_start"));
        expect(header_tag.map.at("header_start") == pmtv::pmt_null());
        expect(header_tag.map.contains("loop_bandwidth"));
    };

    "payload_metadata_insert_with_payload"_test = [] {
        Graph fg;
        const size_t num_items = 100000;
        // although in principle it should be possible to instantiate
        // PayloadMetadataInsert<int>, doing so gives compiler errors
        using c64 = std::complex<float>;
        std::vector<c64> v(num_items);
        std::iota(v.begin(), v.end(), 0);
        const size_t syncword_index = 12345;
        const std::vector<Tag> tags = { { static_cast<ssize_t>(syncword_index),
                                          { { "syncword_amplitude", 0.1f } } } };
        const size_t syncword_size = 64;
        const size_t header_size = 128;
        auto& source = fg.emplaceBlock<VectorSource<c64>>();
        source.data = v;
        source.tags = tags;
        auto& payload_metadata_insert = fg.emplaceBlock<PayloadMetadataInsert<>>(
            { { "syncword_size", syncword_size }, { "header_size", header_size } });
        auto& sink = fg.emplaceBlock<VectorSink<c64>>();
        // repeat set to true in this block because otherwise it says it's DONE
        // and this causes the flowgraph to terminate
        auto& parsed_source =
            fg.emplaceBlock<VectorSource<Message>>({ { "repeat", true } });
        const size_t packet_length = 100;
        // +32 because of the CRC-32
        const size_t payload_bits = 8 * packet_length + 32;
        const size_t payload_symbols = payload_bits / 2;
        Message parsed;
        parsed.data = property_map{ { "packet_length", packet_length } };
        parsed_source.data = std::vector<Message>{ std::move(parsed) };
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(payload_metadata_insert)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(payload_metadata_insert).to<"in">(sink)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(parsed_source)
                      .to<"parsed_header">(payload_metadata_insert)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), syncword_size + header_size + payload_symbols));
        for (size_t j = 0; j < data.size(); ++j) {
            expect(eq(data[j], v[syncword_index + j]));
        }
        const auto sink_tags = sink.tags();
        for (const auto& tag : sink_tags) {
            fmt::println("{} {}", tag.index, tag.map);
        }
        expect(eq(sink_tags.size(), 3_ul));
        const auto& syncword_tag = sink_tags.at(0);
        expect(eq(syncword_tag.index, 0_l));
        expect(eq(pmtv::cast<std::string>(syncword_tag.map.at("constellation")),
                  std::string("PILOT")));
        expect(syncword_tag.map.contains("loop_bandwidth"));
        expect(eq(pmtv::cast<float>(syncword_tag.map.at("syncword_amplitude")), 0.1f));
        const auto& header_tag = sink_tags.at(1);
        expect(eq(header_tag.index, static_cast<ssize_t>(syncword_size)));
        expect(eq(pmtv::cast<std::string>(header_tag.map.at("constellation")),
                  std::string("QPSK")));
        expect(header_tag.map.contains("header_start"));
        expect(header_tag.map.at("header_start") == pmtv::pmt_null());
        expect(header_tag.map.contains("loop_bandwidth"));
        const auto& payload_tag = sink_tags.at(2);
        expect(eq(payload_tag.index, static_cast<ssize_t>(syncword_size + header_size)));
        expect(eq(pmtv::cast<size_t>(payload_tag.map.at("payload_bits")), payload_bits));
        expect(eq(pmtv::cast<size_t>(payload_tag.map.at("payload_symbols")),
                  payload_symbols));
        expect(!payload_tag.map.contains("constellation"));
        expect(payload_tag.map.contains("loop_bandwidth"));
    };
};

int main() {}
