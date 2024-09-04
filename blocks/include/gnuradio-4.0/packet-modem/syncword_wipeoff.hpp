#ifndef _GR4_PACKET_MODEM_SYNCWORD_WIPEOFF
#define _GR4_PACKET_MODEM_SYNCWORD_WIPEOFF

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <complex>
#include <numeric>
#include <vector>

namespace gr::packet_modem {

template <typename T = std::complex<float>, typename TSyncword = float>
class SyncwordWipeoff : public gr::Block<SyncwordWipeoff<T, TSyncword>>
{
public:
    using Description = Doc<R""(
@brief Syncword Wipe-off.

This block receives a stream of symbols marked with `"syncword_amplitude"` tags
at the locations where a syncword begins. It multiplies the syncword symbols by
the given `syncword` in order to wipe-off the symbols of the syncword. The
remaining symbols are passed through unaltered.

)"">;

public:
    bool _in_syncword = false;
    size_t _position = 0;

private:
    static constexpr char syncword_amplitude_key[] = "syncword_amplitude";

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    std::vector<TSyncword> syncword;

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
        assert(inSpan.size() == outSpan.size());
        assert(inSpan.size() > 0);

        if (!_in_syncword && this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(syncword_amplitude_key)) {
#ifdef TRACE
                fmt::println("{} got {} tag", this->name, syncword_amplitude_key);
#endif
                _in_syncword = true;
                _position = 0;
            }
        }

        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();
        if (_in_syncword) {
            const auto n = std::min(inSpan.size(), syncword.size() - _position);
            std::copy_n(in_item, n, out_item);
            for (auto _ : std::views::iota(0UZ, n)) {
                *out_item++ = *in_item++ * syncword[_position++];
            }
            if (_position == syncword.size()) {
                _in_syncword = false;
            }
        }

        if (!_in_syncword) {
            const auto n = inSpan.end() - in_item;
            std::copy_n(in_item, n, out_item);
            in_item += n;
            out_item += n;
        }

        if (!inSpan.consume(static_cast<size_t>(in_item - inSpan.begin()))) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(static_cast<size_t>(out_item - outSpan.begin()));

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::SyncwordWipeoff, in, out, syncword);

#endif // _GR4_PACKET_MODEM_SYNCWORD_WIPEOFF
