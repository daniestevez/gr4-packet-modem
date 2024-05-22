#ifndef _GR4_PACKET_MODEM_HEAD
#define _GR4_PACKET_MODEM_HEAD

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <cstddef>
#include <ranges>

namespace gr::packet_modem {

template <typename T>
class Head : public gr::Block<Head<T>>
{
public:
    using Description = Doc<R""(
@brief Head block. Only lets the first N items go through.

This block passes the first `num_items` items from the input to the
output. After `num_items` items have been produced, the block signals that it is
done.

)"">;

private:
    const size_t d_num_items;
    size_t d_published = 0;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;

    Head(size_t num_items) : d_num_items(num_items) {}

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {}), "
                     "d_published = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     d_published);
#endif
        if (d_published == d_num_items) {
#ifdef TRACE
            fmt::println("{}::processBulk returning DONE", this->name);
#endif
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::DONE;
        }
        if (outSpan.size() == 0) {
#ifdef TRACE
            fmt::println("{}::processBulk returning INSUFFICIENT_OUTPUT_ITEMS",
                         this->name);
#endif
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }
        if (inSpan.size() == 0) {
#ifdef TRACE
            fmt::println("{}::processBulk returning INSUFFICIENT_INPUT_ITEMS",
                         this->name);
#endif
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        const size_t can_publish =
            std::min({ d_num_items - d_published, outSpan.size(), inSpan.size() });
        std::ranges::copy_n(
            inSpan.begin(), static_cast<ssize_t>(can_publish), outSpan.begin());
        std::ignore = inSpan.consume(can_publish);
        outSpan.publish(can_publish);
        d_published += can_publish;
#ifdef TRACE
        if (d_published == d_num_items) {
            fmt::println("{}::processBulk returning DONE", this->name);
        } else {
            fmt::println("{}::processBulk returning OK", this->name);
        }
#endif
        return d_published == d_num_items ? gr::work::Status::DONE : gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::Head, in, out);

#endif // _GR4_PACKET_MODEM_HEAD
