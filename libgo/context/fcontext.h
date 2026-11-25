#pragma once
#include "../common/config.h"

namespace co {

struct StackTraits
{
    static stack_malloc_fn_t& MallocFunc();

    static stack_free_fn_t& FreeFunc();

    static int & GetProtectStackPageSize();

    static bool ProtectStack(void* stack, std::size_t size, int pageSize);

    static void UnprotectStack(void* stack, int pageSize);
};

} // namespace co

//#include "../../third_party/boost.context/boost/context/fcontext.hpp"
//using boost::context::fcontext_t;
//using boost::context::jump_fcontext;
//using boost::context::make_fcontext;

// extern "C"
// {

typedef void* fcontext_t;
typedef void (FCONTEXT_CALL *fn_t)(intptr_t);

// 每次 jump 随参数传递给"被跳入方"的桥接载荷。
//
// 旧版 Boost: jump_fcontext(fcontext_t* ofc, nfc, vp) 在切换之前，把"跳入者自己"
//   挂起后的新句柄写入 *ofc。
// 新版 Boost: jump_fcontext(to, data) 把"跳入者"的新句柄作为 transfer_t.fctx
//   交给"被跳入方"。
// 因此要复现旧语义，必须由被跳入方把 t.fctx 写入跳入者指定的 slot_。
// slot_ 就是旧版那个 ofc 出参。
struct ContextBridge
{
    fcontext_t* slot_ = nullptr;
    intptr_t    vp_   = 0;
    fn_t        fn_   = nullptr; // 仅首次进入协程时有效（新句柄的入口点）
};

intptr_t libgo_jump_fcontext(fcontext_t * ofc, fcontext_t nfc,
        intptr_t vp, bool preserve_fpu = false);

// 带入口函数的版本：首次进入某个协程时必须使用，否则 trampoline 没有入口可调用。
intptr_t libgo_jump_context(fcontext_t * ofc, fcontext_t nfc, ContextBridge* bridge);

fcontext_t libgo_make_fcontext(void* stack, std::size_t size, fn_t fn);

// } // extern "C"
