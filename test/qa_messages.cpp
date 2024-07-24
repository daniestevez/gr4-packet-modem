#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/message_strobe.hpp>
#include <boost/ut.hpp>
#include <chrono>
#include <thread>

boost::ut::suite MessageTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;
    using namespace std::string_literals;

    "message_strobe"_test = [] {
        Graph fg;
        const property_map message = { { "test", "test data" } };
        auto& strobe = fg.emplaceBlock<MessageStrobe<>>(
            { { "message", message }, { "interval_secs", 0.01 } });
        auto& debug = fg.emplaceBlock<MessageDebug>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect(strobe, "strobe"s, debug, "print"s)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect(strobe, "strobe"s, debug, "store"s)));
        scheduler::Simple sched{ std::move(fg) };
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
        const auto messages = debug.messages();
        // nominally we expect to receive 100 messages
        expect(messages.size() > 80_u && messages.size() < 120_u);
        for (const auto& msg : messages) {
            expect(msg.data.has_value());
            expect(msg.data.value() == message);
        }
    };
};

int main() {}
