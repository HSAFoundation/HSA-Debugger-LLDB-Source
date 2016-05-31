//===-- index_sequence.h ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

template <std::size_t... Idx>
struct index_sequence{};

template<std::size_t N, std::size_t...S>
struct make_index_sequence : make_index_sequence<N-1, N-1, S...> {};

template<std::size_t... S>
struct make_index_sequence<0, S...> : index_sequence<S...> {};
