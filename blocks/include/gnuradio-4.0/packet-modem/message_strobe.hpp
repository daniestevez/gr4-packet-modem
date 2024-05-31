#ifndef _GR4_PACKET_MODEM_MESSAGE_STROBE
#define _GR4_PACKET_MODEM_MESSAGE_STROBE

#include <condition_variable>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <atomic>
#include <mutex>
#include <thread>

namespace gr::packet_modem {

template <typename ClockSourceType = std::chrono::system_clock>
class MessageStrobe : public gr::Block<MessageStrobe<ClockSourceType>>
{
public:
    using Description = Doc<R""(
@brief Message Strobe. Sends a message with predefined contents periodically.

This block sends a fixed message specified in its constructor to its output
message port `strobe`, then sleeps for the `interval` indicated in the
constructor, and then repeats.

)"">;

public:
    std::condition_variable _cv;
    bool _stop;
    std::mutex _mutex;
    std::thread _thread;

private:
    void start_thread()
    {
        {
            std::lock_guard lock(_mutex);
            _stop = false;
        }
        _thread = std::thread([this]() {
            while (true) {
                // Use gr::message::Command::Invalid, since none of the OpenCMW
                // commands is appropriate for GNU Radio 3.10-style message passing.
                gr::sendMessage<gr::message::Command::Invalid>(strobe, "", "", message);
                // Sleep for interval_secs or until stop is set to true and the
                // condition variable is notified.
                std::unique_lock lock(_mutex);
                const auto interval = std::chrono::duration<double>(interval_secs);
                const auto wakeup = ClockSourceType::now() + interval;
                _cv.wait_for(lock, interval, [this, wakeup]() {
                    return _stop || ClockSourceType::now() >= wakeup;
                });
                if (_stop) {
                    return;
                }
            }
        });
    }

    void stop_thread()
    {
        {
            std::lock_guard lock(_mutex);
            _stop = true;
        }
        _cv.notify_one();
        _thread.join();
    }

public:
    gr::MsgPortOut strobe;
    gr::property_map message;
    double interval_secs = 1.0;

    void start() { start_thread(); }

    void stop() { stop_thread(); }

    void pause() { stop_thread(); }

    void resume() { start_thread(); }

    // If I don't include this 'fake_in' port and 'processOne' method, I get the
    // following compile errors:
    //
    // no member named 'merged_work_chunk_size' in 'gr::packet_modem::MessageDebug'
    //
    // static assertion failed due to requirement 'gr::meta::always_false<std::tuple<>>':
    // neither processBulk(...) nor processOne(...) implemented
    //
    gr::PortIn<int> fake_in;

    void processOne(int) { return; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::MessageStrobe, fake_in, strobe, message, interval_secs);

#endif // _GR4_PACKET_MODEM_MESSAGE_DEBUG
