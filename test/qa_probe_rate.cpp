#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <boost/ut.hpp>

boost::ut::suite ProbeRateTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "probe_rate"_test = [] {
        Graph fg;
        const double samp_rate = 10e3;
        auto& source = fg.emplaceBlock<NullSource<int>>();
        auto& throttle = fg.emplaceBlock<Throttle<int>>(
            { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 100UZ } });
        auto& probe_rate =
            fg.emplaceBlock<ProbeRate<int>>({ { "min_update_time_secs", 1.0 } });
        auto& msg_debug = fg.emplaceBlock<MessageDebug>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(throttle)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(throttle).to<"in">(probe_rate)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"rate">(probe_rate).to<"store">(msg_debug)));
        scheduler::Simple sched{ std::move(fg) };
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto messages = msg_debug.messages();
        // nominally we expect 5 messages
        expect(messages.size() >= 4_u && messages.size() <= 6_u);
        for (const auto& message : messages) {
            expect(message.data.has_value());
            const auto data = message.data.value();
            const auto rate_now = pmtv::cast<double>(data.at("rate_now"));
            const auto rate_avg = pmtv::cast<double>(data.at("rate_avg"));
            const double tol = 250.0;
            expect(std::abs(rate_now - samp_rate) < tol);
            expect(std::abs(rate_avg - samp_rate) < tol);
        }
    };
};

int main() {}
