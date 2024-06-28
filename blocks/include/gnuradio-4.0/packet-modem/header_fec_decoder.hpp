#ifndef _GR4_PACKET_MODEM_HEADER_FEC_DECODER
#define _GR4_PACKET_MODEM_HEADER_FEC_DECODER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ldpc_toolbox.h>
#include <algorithm>
#include <array>

namespace gr::packet_modem {

class HeaderFecDecoder : public gr::Block<HeaderFecDecoder, gr::Resampling<1U, 64U, true>>
{
public:
    using Description = Doc<R""(
@brief Header FEC Decoder.

Decodes the FEC of a header. See HeaderFecEncoder for details about the header
FEC. The input of this block is LLRs (soft symbols), with the conventions that
a positive LLR represents that the bit 0 is more likely. The output is the
decoded header packed as 8 bits per byte.

)"">;

private:
    static constexpr size_t header_num_llrs = 256;
    static constexpr size_t header_ldpc_n = 128;
    static constexpr size_t header_ldpc_k = 32;
    static constexpr size_t header_num_bytes = 4;
    static constexpr char alist[] = R""(128 96
3 5 
3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 
4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 3 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 5 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 
21 66 86 
25 26 65 
5 6 52 
35 47 56 
1 58 70 
72 77 80 
14 41 83 
12 69 74 
24 57 84 
4 16 82 
17 55 63 
9 18 20 
62 75 85 
19 59 64 
25 29 42 
33 45 86 
15 23 76 
3 39 78 
37 49 91 
10 67 79 
27 73 90 
21 28 81 
40 71 89 
61 87 93 
22 34 92 
36 38 51 
43 66 96 
11 32 88 
30 31 60 
8 13 68 
2 46 53 
7 78 94 
18 77 88 
65 67 84 
38 52 85 
4 30 45 
8 12 56 
14 39 60 
24 26 61 
42 89 94 
2 36 78 
51 71 75 
10 43 64 
16 25 79 
22 29 68 
13 21 72 
27 32 57 
35 44 58 
9 17 73 
49 76 95 
62 86 93 
28 59 74 
70 81 82 
50 63 87 
11 41 55 
5 23 96 
3 54 66 
34 37 83 
19 48 69 
6 20 31 
15 53 90 
47 91 92 
33 40 80 
1 7 46 
3 9 16 
45 77 91 
23 32 71 
41 61 79 
40 74 92 
81 85 89 
46 84 96 
1 64 90 
2 17 86 
48 57 58 
14 50 59 
29 49 93 
31 37 62 
19 68 80 
6 42 72 
10 12 36 
39 55 82 
4 5 83 
52 67 73 
35 43 60 
8 18 94 
51 70 76 
34 38 87 
24 56 63 
15 25 33 
7 21 22 
20 26 44 
11 30 69 
27 47 66 
53 75 88 
28 65 95 
13 57 78 
54 59 94 
12 80 82 
1 11 38 
14 23 29 
44 67 88 
43 72 95 
27 31 55 
9 64 91 
33 35 83 
5 28 36 
3 34 76 
13 15 63 
22 61 73 
4 90 93 
45 78 85 
20 46 74 
16 48 75 
17 42 58 
8 62 84 
7 51 56 
39 53 92 
40 49 52 
77 79 96 
19 47 65 
24 37 69 
41 89 95 
6 50 70 
30 54 71 
2 18 60 
26 32 81 
10 48 87 
44 54 68 
5 64 72 99 
31 41 73 125 
18 57 65 107 
10 36 82 110 
3 56 82 106 
3 60 79 123 
32 64 90 116 
30 37 85 115 
12 49 65 104 
20 43 80 127 
28 55 92 99 
8 37 80 98 
30 46 96 108 
7 38 75 100 
17 61 89 108 
10 44 65 113 
11 49 73 114 
12 33 85 125 
14 59 78 120 
12 60 91 112 
1 22 46 90 
25 45 90 109 
17 56 67 100 
9 39 88 121 
2 15 44 89 
2 39 91 126 
21 47 93 103 
22 52 95 106 
15 45 76 100 
29 36 92 124 
29 60 77 103 
28 47 67 126 
16 63 89 105 
25 58 87 107 
4 48 84 105 
26 41 80 106 
19 58 77 121 
26 35 87 99 
18 38 81 117 
23 63 69 118 
7 55 68 122 
15 40 79 114 
27 43 84 102 
48 91 101 128 
16 36 66 111 
31 64 71 112 
4 62 93 120 
59 74 113 127 
19 50 76 118 
54 75 123 
26 42 86 116 
3 35 83 118 
31 61 94 117 
57 97 124 128 
11 55 81 103 
4 37 88 116 
9 47 74 96 
5 48 74 114 
14 52 75 97 
29 38 84 125 
24 39 68 109 
13 51 77 115 
11 54 88 108 
14 43 72 104 
2 34 95 120 
1 27 57 93 
20 34 83 101 
30 45 78 128 
8 59 92 121 
5 53 86 123 
23 42 67 124 
6 46 79 102 
21 49 83 109 
8 52 69 112 
13 42 94 113 
17 50 86 107 
6 33 66 119 
18 32 41 96 111 
20 44 68 119 
6 63 78 98 
22 53 70 126 
10 53 81 98 
7 58 82 105 
9 34 71 115 
13 35 70 111 
1 16 51 73 
24 54 87 127 
28 33 94 101 
23 40 70 122 
21 61 72 110 
19 62 66 104 
25 62 69 117 
24 51 76 110 
32 40 85 97 
50 95 102 122 
27 56 71 119 

)"";

public:
    std::array<float, header_ldpc_n> _llrs;
    std::array<uint8_t, header_ldpc_k> _bits;
    void* _ldpc_decoder = nullptr;

public:
    gr::PortIn<float> in;
    gr::PortOut<uint8_t> out;

    void start()
    {
        if (_ldpc_decoder != nullptr) {
            throw gr::exception("an LDPC decoder already exists");
        }
        _ldpc_decoder = ldpc_toolbox_decoder_ctor_alist_string(alist, "HLAminstari8", "");
        if (_ldpc_decoder == nullptr) {
            throw gr::exception("could not build LDPC decoder");
        }
    }

    void stop()
    {
        if (_ldpc_decoder != nullptr) {
            ldpc_toolbox_decoder_dtor(_ldpc_decoder);
            _ldpc_decoder = nullptr;
        }
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        const auto codewords =
            std::min(inSpan.size() / header_num_llrs, outSpan.size() / header_num_bytes);
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "codewords = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     codewords);
#endif

        if (codewords == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return inSpan.size() < header_num_llrs
                       ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                       : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();
        for (size_t j = 0; j < codewords; ++j) {
            // accumulate LLRs for repetition coding
            std::copy_n(in_item, header_ldpc_n, _llrs.begin());
            for (size_t k = 0; k < header_ldpc_n; ++k) {
                _llrs[k] += in_item[static_cast<ssize_t>(header_ldpc_n + k)];
            }
            in_item += header_num_llrs;

            // perform LDPC decoding
            const uint32_t max_iterations = 25;
            const int32_t ret = ldpc_toolbox_decoder_decode_f32(_ldpc_decoder,
                                                                _bits.data(),
                                                                header_ldpc_k,
                                                                _llrs.data(),
                                                                header_ldpc_n,
                                                                max_iterations);
            if (ret < 0) {
                // LDPC decoding failed
                const gr::property_map tag = { { "invalid_header", pmtv::pmt_null() } };
                out.publishTag(tag, out_item - outSpan.begin());
            }

            // Pack bits into output
            for (size_t k = 0; k < header_num_bytes; ++k) {
                uint8_t byte = 0;
                for (size_t n = 0; n < 8; ++n) {
                    byte = static_cast<uint8_t>(byte << 1) | _bits[8 * k + n];
                }
                *out_item++ = byte;
            }
        }

        if (!inSpan.consume(codewords * header_num_llrs)) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(codewords * header_num_bytes);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION(gr::packet_modem::HeaderFecDecoder, in, out);

#endif // _GR4_PACKET_MODEM_HEADER_FEC_DECODER
