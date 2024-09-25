/*
 * This file contains code adapted from GNU Radio 4.0, which is licensed as
 * follows:
 *
 * Copyright (C) 2001-September 2020 GNU Radio Project -- managed by Free Software Foundation, Inc.
 * Copyright (C) September 2020-2024 GNU Radio Project -- managed by SETI Institute
 * Copyright (C) 2018-2024 FAIR -- Facility for Antiproton & Ion Research, Darmstadt, Germany
 *
 * SPDX-License-Identifier: LGPL-3.0-linking-exception
 *
 */

#ifndef _GR4_PACKET_MODEM_REGISTER_HELPERS
#define _GR4_PACKET_MODEM_REGISTER_HELPERS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>

// adapted from gnuradio-4.0/Block.hpp
template <template <auto, typename, typename> typename TBlock,
          auto Value,
          typename Tuple1,
          typename Tuple2,
          typename TRegisterInstance>
inline constexpr int registerBlockVTT(TRegisterInstance& registerInstance)
{
    using namespace gr;
    auto addBlockType = [&]<typename Type1, typename Type2> {
        using ThisBlock = TBlock<Value, Type1, Type2>;
        registerInstance.template addBlockType<ThisBlock>(
            detail::blockBaseName<ThisBlock>(),
            detail::nttpToString<Value>() + "," + detail::reflFirstTypeName<Type1>() +
                "," + detail::reflFirstTypeName<Type2>());
    };

    std::apply(
        [&]<typename... T1>(T1...) { // iterate over first type
            std::apply(
                [&]<typename... T2>(T2...) { // iterate over second type
                    (([&]<typename Type1>() {
                         ((addBlockType.template operator()<Type1, T2>()), ...);
                     }.template operator()<T1>()),
                     ...);
                },
                Tuple2{});
        },
        Tuple1{});

    return {};
}

// adapted from gnuradio-4.0/Block.hpp
template <template <typename, typename, typename> typename TBlock,
          typename... TBlockParameters,
          typename TRegisterInstance>
inline constexpr int registerBlock(TRegisterInstance& registerInstance)
{
    using namespace gr;
    auto addBlockType = [&]<typename Type> {
        using ThisBlock = TBlock<typename Type::template at<0>,
                                 typename Type::template at<1>,
                                 typename Type::template at<2>>;
        static_assert(meta::is_instantiation_of<Type, BlockParameters>);
        static_assert(Type::size == 3);
        registerInstance.template addBlockType<ThisBlock>(
            detail::blockBaseName<ThisBlock>(), Type::toString());
    };
    ((addBlockType.template operator()<TBlockParameters>()), ...);
    return {};
}

// adapted from gnuradio-4.0/Block.hpp
template <template <typename, typename, typename, typename> typename TBlock,
          typename... TBlockParameters,
          typename TRegisterInstance>
inline constexpr int registerBlock(TRegisterInstance& registerInstance)
{
    using namespace gr;
    auto addBlockType = [&]<typename Type> {
        using ThisBlock = TBlock<typename Type::template at<0>,
                                 typename Type::template at<1>,
                                 typename Type::template at<2>,
                                 typename Type::template at<3>>;
        static_assert(meta::is_instantiation_of<Type, BlockParameters>);
        static_assert(Type::size == 4);
        registerInstance.template addBlockType<ThisBlock>(
            detail::blockBaseName<ThisBlock>(), Type::toString());
    };
    ((addBlockType.template operator()<TBlockParameters>()), ...);
    return {};
}

template <template <typename> typename BlockT>
inline constexpr void register_all_float_types()
{
    gr::registerBlock<BlockT, double, float>(gr::globalBlockRegistry());
}

template <template <typename> typename BlockT>
inline constexpr void register_all_float_and_complex_types()
{
    gr::registerBlock<BlockT, double, float, std::complex<double>, std::complex<float>>(
        gr::globalBlockRegistry());
}

template <template <typename> typename BlockT>
inline constexpr void register_all_uint_types()
{
    gr::registerBlock<BlockT, uint64_t, uint32_t, uint16_t, uint8_t>(
        gr::globalBlockRegistry());
}

template <template <typename> typename BlockT>
inline constexpr void register_all_scalar_types()
{
    gr::registerBlock<BlockT,
                      std::complex<double>,
                      std::complex<float>,
                      double,
                      float,
                      int64_t,
                      int32_t,
                      int16_t,
                      int8_t,
                      uint64_t,
                      uint32_t,
                      uint16_t,
                      uint8_t>(gr::globalBlockRegistry());
}

template <template <typename> typename BlockT>
inline constexpr void register_all_pdu_types()
{
    gr::registerBlock<BlockT,
                      gr::packet_modem::Pdu<std::complex<double>>,
                      gr::packet_modem::Pdu<std::complex<float>>,
                      gr::packet_modem::Pdu<double>,
                      gr::packet_modem::Pdu<float>,
                      gr::packet_modem::Pdu<int64_t>,
                      gr::packet_modem::Pdu<int32_t>,
                      gr::packet_modem::Pdu<int16_t>,
                      gr::packet_modem::Pdu<int8_t>,
                      gr::packet_modem::Pdu<uint64_t>,
                      gr::packet_modem::Pdu<uint32_t>,
                      gr::packet_modem::Pdu<uint16_t>,
                      gr::packet_modem::Pdu<uint8_t>>(gr::globalBlockRegistry());
}

template <template <typename> typename BlockT>
inline constexpr void register_all_types()
{
    register_all_scalar_types<BlockT>();
    register_all_pdu_types<BlockT>();
}

template <template <typename, typename, typename> typename BlockT>
inline constexpr void register_scalar_mult()
{
    using namespace gr;
    using namespace gr::packet_modem;
    registerBlock<
        BlockT,
        BlockParameters<float, float, float>,
        BlockParameters<std::complex<float>, std::complex<float>, std::complex<float>>,
        BlockParameters<std::complex<float>, std::complex<float>, float>,
        BlockParameters<float, std::complex<float>, std::complex<float>>,
        BlockParameters<double, double, double>,
        BlockParameters<std::complex<double>, std::complex<double>, std::complex<double>>,
        BlockParameters<std::complex<double>, std::complex<double>, double>,
        BlockParameters<double, std::complex<double>, std::complex<double>>,
        BlockParameters<Pdu<float>, Pdu<float>, float>,
        BlockParameters<Pdu<std::complex<float>>,
                        Pdu<std::complex<float>>,
                        std::complex<float>>,
        BlockParameters<Pdu<std::complex<float>>, Pdu<std::complex<float>>, float>,
        BlockParameters<Pdu<float>, Pdu<std::complex<float>>, std::complex<float>>,
        BlockParameters<Pdu<double>, Pdu<double>, double>,
        BlockParameters<Pdu<std::complex<double>>,
                        Pdu<std::complex<double>>,
                        std::complex<double>>,
        BlockParameters<Pdu<std::complex<double>>, Pdu<std::complex<double>>, double>,
        BlockParameters<Pdu<double>, Pdu<std::complex<double>>, std::complex<double>>>(
        globalBlockRegistry());
}

#endif // _GR4_PACKET_MODEM_REGISTER_HELPERS
