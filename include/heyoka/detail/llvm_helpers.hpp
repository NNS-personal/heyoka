// Copyright 2020, 2021 Francesco Biscani (bluescarni@gmail.com), Dario Izzo (dario.izzo@gmail.com)
//
// This file is part of the heyoka library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef HEYOKA_DETAIL_LLVM_HELPERS_HPP
#define HEYOKA_DETAIL_LLVM_HELPERS_HPP

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <typeinfo>
#include <vector>

#include <heyoka/detail/fwd_decl.hpp>
#include <heyoka/detail/llvm_fwd.hpp>
#include <heyoka/detail/visibility.hpp>

namespace heyoka::detail
{

HEYOKA_DLL_PUBLIC llvm::Type *to_llvm_type_impl(llvm::LLVMContext &, const std::type_info &);

// Helper to associate a C++ type to an LLVM type.
template <typename T>
inline llvm::Type *to_llvm_type(llvm::LLVMContext &c)
{
    return to_llvm_type_impl(c, typeid(T));
}

HEYOKA_DLL_PUBLIC llvm::Type *make_vector_type(llvm::Type *, std::uint32_t);

// Helper to construct an LLVM vector type of size batch_size with elements
// of the LLVM type tp corresponding to the C++ type T. If batch_size is 1, tp
// will be returned. batch_size cannot be zero.
template <typename T>
inline llvm::Type *to_llvm_vector_type(llvm::LLVMContext &c, std::uint32_t batch_size)
{
    return make_vector_type(to_llvm_type<T>(c), batch_size);
}

HEYOKA_DLL_PUBLIC llvm::Value *load_vector_from_memory(ir_builder &, llvm::Value *, std::uint32_t);
HEYOKA_DLL_PUBLIC void store_vector_to_memory(ir_builder &, llvm::Value *, llvm::Value *);

HEYOKA_DLL_PUBLIC llvm::Value *vector_splat(ir_builder &, llvm::Value *, std::uint32_t);

HEYOKA_DLL_PUBLIC std::vector<llvm::Value *> vector_to_scalars(ir_builder &, llvm::Value *);

HEYOKA_DLL_PUBLIC llvm::Value *scalars_to_vector(ir_builder &, const std::vector<llvm::Value *> &);

HEYOKA_DLL_PUBLIC llvm::Value *pairwise_sum(ir_builder &, std::vector<llvm::Value *> &);

HEYOKA_DLL_PUBLIC llvm::Value *llvm_invoke_intrinsic(llvm_state &, const std::string &,
                                                     const std::vector<llvm::Type *> &,
                                                     const std::vector<llvm::Value *> &);

HEYOKA_DLL_PUBLIC llvm::Value *llvm_invoke_external(llvm_state &, const std::string &, llvm::Type *,
                                                    const std::vector<llvm::Value *> &,
                                                    // NOTE: this is going to be converted into
                                                    // a vector of llvm attributes (represented
                                                    // as an enum) in the implementation.
                                                    const std::vector<int> & = {});

HEYOKA_DLL_PUBLIC llvm::Value *llvm_invoke_internal(llvm_state &, const std::string &,
                                                    const std::vector<llvm::Value *> &);

HEYOKA_DLL_PUBLIC void llvm_loop_u32(llvm_state &, llvm::Value *, llvm::Value *,
                                     const std::function<void(llvm::Value *)> &,
                                     const std::function<llvm::Value *(llvm::Value *)> & = {});

HEYOKA_DLL_PUBLIC void llvm_if_then_else(llvm_state &, llvm::Value *, const std::function<void()> &,
                                         const std::function<void()> &);

HEYOKA_DLL_PUBLIC llvm::Type *pointee_type(llvm::Value *);

HEYOKA_DLL_PUBLIC std::string llvm_type_name(llvm::Type *);

HEYOKA_DLL_PUBLIC bool compare_function_signature(llvm::Function *, llvm::Type *, const std::vector<llvm::Type *> &);

HEYOKA_DLL_PUBLIC llvm::Value *make_global_zero_array(llvm::Module &, llvm::ArrayType *);

HEYOKA_DLL_PUBLIC llvm::Value *call_extern_vec(llvm_state &, llvm::Value *, const std::string &);

} // namespace heyoka::detail

#endif
