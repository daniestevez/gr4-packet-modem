#ifndef _GR4_PACKET_MODEM_CONSTELLATION_LLR_DECODER
#define _GR4_PACKET_MODEM_CONSTELLATION_LLR_DECODER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <complex>

namespace gr::packet_modem {

template <typename T = float>
class ConstellationLLRDecoder
    : public gr::Block<ConstellationLLRDecoder<T>, gr::ResamplingRatio<>>
{
public:
    using Description = Doc<R""(
@brief Constellation LLR decoder.

This block converts symbols from a constellation into LLRs (log-likelihood
ratios) for each bit. The constellation can be updated on the fly by using
`"constellation"` tags.

This block uses the convention that in BPSK and QPSK a negative amplitude
encodes the bit 1. It uses the LLR convention that a positive LLR means that the
bit 0 is more likely.

)"">;

public:
    bool _in_syncword = false;
    size_t _position = 0;

private:
    static constexpr char syncword_amplitude_key[] = "syncword_amplitude";

public:
    T _scale = T{ 2 };

public:
    gr::PortIn<std::complex<T>> in;
    gr::PortOut<T> out;
    // standard deviation of the noise in the real part (and also imaginary
    // part) of the complex input AGWN
    T noise_sigma = T{ 1 };
    std::string constellation = "BPSK";

    // use custom tag propagation policy because the runtime isn't smart enough
    // to propagate tags correctly with the `this->numerator` changes done by
    // this block
    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
#ifdef TRACE
        fmt::println(
            "{}::settingsChanged(), constellation = {}", this->name, constellation);
#endif
        // set resampling ratio for the scheduler
        if (constellation == "BPSK") {
            this->numerator = 1;
        } else if (constellation == "QPSK") {
            this->numerator = 2;
        } else {
            throw gr::exception(
                fmt::format("constellation {} not supported", constellation));
        }
        _scale = T{ 2 } / (noise_sigma * noise_sigma);
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "constellation = {}, numerator = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     constellation,
                     this->numerator);
#endif
        if (this->input_tags_present()) {
            const auto tag = this->mergedInputTag();
#ifdef TRACE
            fmt::println("{} tag = {}, index = {}", this->name, tag.map, tag.index);
#endif
            out.publishTag(tag.map);
        }

        const auto n = std::min(inSpan.size(), outSpan.size() / this->numerator);

        auto out_item = outSpan.begin();
        auto input = inSpan | std::views::take(n);
        if (constellation == "BPSK") {
            for (auto& in_item : input) {
                *out_item++ = _scale * in_item.real();
            }
        } else if (constellation == "QPSK") {
            for (auto& in_item : input) {
                *out_item++ = _scale * in_item.real();
                *out_item++ = _scale * in_item.imag();
            }
        } else {
            throw gr::exception(
                fmt::format("constellation {} not supported", constellation));
        }

        if (!inSpan.consume(n)) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(n * this->numerator);
#ifdef TRACE
        fmt::println(
            "{} consumed = {}, produced = {}", this->name, n, n * this->numerator);
#endif

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::ConstellationLLRDecoder, in, out, noise_sigma, constellation);

#endif // _GR4_PACKET_MODEM_CONSTELLATION_LLR_DECODER
