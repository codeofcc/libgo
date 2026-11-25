#include "fcontext.h"
#include <memory>
#include <string.h>

#if defined(LIBGO_SYS_Unix)
#include <sys/mman.h>
#endif

namespace co
{
    stack_malloc_fn_t &StackTraits::MallocFunc()
    {
        static stack_malloc_fn_t fn = &::std::malloc;
        return fn;
    }
    stack_free_fn_t &StackTraits::FreeFunc()
    {
        static stack_free_fn_t fn = &::std::free;
        return fn;
    }
    int &StackTraits::GetProtectStackPageSize()
    {
        static int size = 0;
        return size;
    }
#if defined(LIBGO_SYS_Unix)
    bool StackTraits::ProtectStack(void *stack, std::size_t size, int pageSize)
    {
        if (!pageSize)
            return false;

        if ((int)size <= getpagesize() * (pageSize + 1))
            return false;

        void *protect_page_addr = ((std::size_t)stack & 0xfff) ? (void *)(((std::size_t)stack & ~(std::size_t)0xfff) + 0x1000) : stack;
        if (-1 == mprotect(protect_page_addr, getpagesize() * pageSize, PROT_NONE))
        {
            DebugPrint(dbg_task, "origin_addr:%p, align_addr:%p, page_size:%d, protect_page:%u, protect stack stack error: %s",
                       stack, protect_page_addr, getpagesize(), pageSize, strerror(errno));
            return false;
        }
        else
        {
            DebugPrint(dbg_task, "origin_addr:%p, align_addr:%p, page_size:%d, protect_page:%u, protect stack success.",
                       stack, protect_page_addr, pageSize, getpagesize());
            return true;
        }
    }
    void StackTraits::UnprotectStack(void *stack, int pageSize)
    {
        if (!pageSize)
            return;

        void *protect_page_addr = ((std::size_t)stack & 0xfff) ? (void *)(((std::size_t)stack & ~(std::size_t)0xfff) + 0x1000) : stack;
        if (-1 == mprotect(protect_page_addr, getpagesize() * pageSize, PROT_READ | PROT_WRITE))
        {
            DebugPrint(dbg_task, "origin_addr:%p, align_addr:%p, page_size:%d, protect_page:%u, protect stack stack error: %s",
                       stack, protect_page_addr, getpagesize(), pageSize, strerror(errno));
        }
        else
        {
            DebugPrint(dbg_task, "origin_addr:%p, align_addr:%p, page_size:%d, protect_page:%u, protect stack success.",
                       stack, protect_page_addr, pageSize, getpagesize());
        }
    }
#else  // defined(LIBGO_SYS_Unix)
    bool StackTraits::ProtectStack(void *stack, std::size_t size, int pageSize)
    {
        return false;
    }

    void StackTraits::UnprotectStack(void *stack, int pageSize)
    {
        return;
    }
#endif // defined(LIBGO_SYS_Unix)

} // namespace co


#ifndef _WIN32

#include <cstdio>
#include <cstdlib>
#include <cstddef>
namespace {

// ============================================================
// Boost.Context 桥接层（不依赖 Boost 头文件）
//
// libgo 只需要 Boost.Context 的 fcontext 三件套，它们由
// third_party/boost_context/src/asm/*.S 提供，是纯 C ABI：
//   void*      make_fcontext(void* sp, size_t size, void(*fn)(transfer_t));
//   transfer_t jump_fcontext(void* to, void* vp);
//   transfer_t ontop_fcontext(void* to, void* vp, transfer_t(*fn)(transfer_t));
//
// 这里用 extern "C" 直接声明，不再 #include <boost/context/detail/fcontext.hpp>。
// 好处：不需要 Boost 的头文件体系（boost/config.hpp 等），构建时也就不必
// 下载/配置整个 Boost 仓库（原本 691MB 只为拿 context 一个库）。
//
// transfer_t 与 Boost 的定义逐字节一致（{ void* fctx; void* data; }），
// 在 Itanium C++ ABI 下单个聚合类型成员按 by-value 传递，
// 因此与 Boost 编译出来的调用约定兼容。
//
// 汇编入口是隐藏符号（.globl + .hidden），只能被同一链接单元内的代码调用，
// 所以必须在本文件内声明并包装，不对外暴露。
// ============================================================

struct FctxTransfer
{
    void* fctx;
    void* data;
};

extern "C" {

// 与 boostorg/context src/asm/*.S 中的定义一致
void* make_fcontext(void* sp, std::size_t size, void (* fn)(FctxTransfer));

FctxTransfer jump_fcontext(void* to, void* vp);

FctxTransfer ontop_fcontext(void* to, void* vp, void (* fn)(FctxTransfer));

} // extern "C"

// 新版 Boost 入口签名: void(FctxTransfer)
// 作为旧版 fn_t(intptr_t) 与新汇编入口之间的桥梁，仅在协程首次被激活时执行一次
//
// 语义要点（fork 原实现踩到的两个坑）：
//
// 1) 入口函数不能用 thread_local 传递。
//    vpc 用的是 Scheduler::goStart()：Task 构造（libgo_make_fcontext）发生在调用 go
//    的线程，而首次 SwapIn 发生在调度线程，两者不是同一线程，thread_local 会读到
//    nullptr 并跳转到地址 0。入口函数改为随首次 jump 一起传入（ContextBridge::fn_）。
//
// 2) "跳入者挂起后的新句柄"必须由被跳入方写回跳入者指定的槽位。
//    旧版 jump_fcontext(&ofc, nfc, vp) 在切换前由跳入者自己写 *ofc；
//    新版 jump_fcontext(to, data) 把这个句柄作为 transfer_t.fctx 交给被跳入方。
//    fork 原来在外层 libgo_jump_fcontext 里写 *ofc = t.fctx，那是"我方被跳回之后"，
//    写进去的是对方的句柄，位置也错，导致首次 SwapOut 时 GetTlsContext() 仍为 0，
//    jump_fcontext 跳到空句柄 -> SIGSEGV。
static void FCONTEXT_CALL trampoline(FctxTransfer t) noexcept
{
    auto* bridge = reinterpret_cast<ContextBridge*>(t.data);
    if (!bridge || !bridge->fn_ || !bridge->slot_) {
        fprintf(stderr, "libgo: coroutine entry bridge invalid (bridge=%p)\n", (void*)bridge);
        abort();
    }

    // 等价于旧版 jump_fcontext 的 *ofc 写入：把跳入者（调度线程）挂起后的
    // 新句柄交还给跳入者，供它之后 jump 回来。
    *bridge->slot_ = t.fctx;

    const fn_t user_fn = bridge->fn_;
    const intptr_t user_vp = bridge->vp_;
    bridge->fn_ = nullptr; // 入口函数只允许消费一次

    // 以旧版语义调用用户协程入口
    user_fn(user_vp);

    // 协程函数不应正常返回（它只能通过 jump_fcontext 离开）
    fprintf(stderr, "libgo: coroutine entry function must not return\n");
    abort();
}

} // anonymous namespace

// 我方作为跳入者：把 bridge 交给对方，由对方负责把 t.fctx 写入 bridge->slot_。
// mine 位于本函数栈帧内，挂起期间依然有效，生命周期覆盖整个挂起期。
intptr_t libgo_jump_context(fcontext_t *ofc, fcontext_t nfc, ContextBridge* bridge)
{
    ContextBridge mine = *bridge;
    mine.slot_ = ofc;

    // 走到下一行时，说明我方已被对方 jump 回来：
    //   t.fctx = 对方挂起后的新句柄, t.data = 对方传来的 ContextBridge*
    // 按旧版语义，把对方句柄写入对方要求的槽位（即对方的 *ofc），并返回对方的 vp。
    FctxTransfer t = jump_fcontext(nfc, &mine);

    auto* peer = reinterpret_cast<ContextBridge*>(t.data);
    if (peer) {
        if (peer->slot_)
            *peer->slot_ = t.fctx;
        return peer->vp_;
    }
    return 0;
}

intptr_t libgo_jump_fcontext(fcontext_t *ofc, fcontext_t nfc,
                             intptr_t vp, bool /*preserve_fpu*/)
{
    ContextBridge bridge;
    bridge.slot_ = ofc;
    bridge.vp_   = vp;
    bridge.fn_   = nullptr;
    return libgo_jump_context(ofc, nfc, &bridge);
}

fcontext_t libgo_make_fcontext(void *stack, std::size_t size, fn_t fn)
{
    // 入口函数不再在这里 smuggle；首次进入协程时由 Context 通过 ContextBridge::fn_ 传入。
    (void)fn;
    return make_fcontext(stack, size, &trampoline);
}
#endif