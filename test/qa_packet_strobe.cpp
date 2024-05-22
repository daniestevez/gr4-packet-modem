#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite PacketStrobeTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "packet_strobe"_test = [] {
        Graph fg;
        const size_t packet_len = 1000;
        auto& strobe = fg.emplaceBlock<PacketStrobe<uint8_t>>(
            packet_len, std::chrono::milliseconds(10), "packet_len");
        auto& stream_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<uint8_t>>();
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<uint8_t>>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(strobe).to<"in">(stream_to_pdu)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(stream_to_pdu).to<"in">(sink)));
        // a multi-threaded scheduler is required for this test because the
        // TaggedStreamToPdu processBulk() needs to be called multiple times while
        // the PacketStrobe processBulk() is sleeping.
        scheduler::Simple<scheduler::ExecutionPolicy::multiThreaded> sched{ std::move(
            fg) };
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        expect(sink.tags().empty());
        const auto pdus = sink.data();
        // nominally we expect 100 PDUs
        expect(pdus.size() > 80_ul && pdus.size() < 120_ul);
        for (const auto& pdu : pdus) {
            expect(eq(pdu.data.size(), packet_len));
            for (const auto x : pdu.data) {
                expect(eq(x, 0));
            }
            expect(pdu.tags.empty());
        }
    };
};

int main() {}
