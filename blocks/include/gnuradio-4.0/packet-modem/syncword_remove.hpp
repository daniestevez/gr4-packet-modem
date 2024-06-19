#ifndef _GR4_PACKET_MODEM_SYNCWORD_REMOVE
#define _GR4_PACKET_MODEM_SYNCWORD_REMOVE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <complex>

namespace gr::packet_modem {

template <typename T = std::complex<float>>
class SyncwordRemove : public gr::Block<SyncwordRemove<T>>
{
public:
    using Description = Doc<R""(
@brief Syncword Remove.

This block receives a stream of symbols marked with `"syncword_amplitude"` tags
at the locations where a syncword begins. It drops the symbols corresponding to
the syncword (whose length is indicated by the `syncword_size` parameter) and
passes the rest of the symbols to the output.

)"">;

public:
    bool _in_syncword = false;
    size_t _position = 0;

private:
    static constexpr char syncword_amplitude_key[] = "syncword_amplitude";

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    size_t syncword_size = 64;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "_in_syncword = {}, _position = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     _in_syncword,
                     _position);
#endif
        if (!_in_syncword && this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(syncword_amplitude_key)) {
#ifdef TRACE
                fmt::println("{} got {} tag", this->name, syncword_amplitude_key);
#endif
                _in_syncword = true;
                _position = 0;
            } else {
                // pass tag to output
                out.publishTag(tag.map);
            }
        }

        auto in_item = inSpan.begin();
        if (_in_syncword) {
            const auto n = std::min(inSpan.size(), syncword_size - _position);
            in_item += static_cast<ssize_t>(n);
            _position += n;
            if (_position >= syncword_size) {
                _in_syncword = false;
            }
        }

        if (!_in_syncword) {
            const auto n =
                std::min(static_cast<size_t>(inSpan.end() - in_item), outSpan.size());
            std::copy_n(in_item, n, outSpan.begin());
            outSpan.publish(n);
            in_item += static_cast<ssize_t>(n);
        } else {
            outSpan.publish(0);
            if (in_item != inSpan.begin()) {
                // Clear input tags. This is needed because the block doesn't
                // publish anything, so the input tags don't get cleared by the
                // runtime.
                this->_mergedInputTag.map.clear();
            }
        }

        if (!inSpan.consume(static_cast<size_t>(in_item - inSpan.begin()))) {
            throw gr::exception("consume failed");
        }

        // TODO: not sure why this is needed here, since some output is being published
        if (in_item != inSpan.begin()) {
            this->_mergedInputTag.map.clear();
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::SyncwordRemove, in, out, syncword_size);

#endif // _GR4_PACKET_MODEM_SYNCWORD_REMOVE
