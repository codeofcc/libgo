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

#include <boost/context/detail/fcontext.hpp>
namespace {

// 新版 Boost 入口签名: void(transfer_t)
// 作为旧版 fn_t(intptr_t) 与新 Boost 之间的桥梁
// 仅在协程首次被激活时执行一次
static void FCONTEXT_CALL trampoline(boost::context::detail::transfer_t t) noexcept
{
    // 从 thread_local 获取用户函数（由 make_fcontext 设置）
    extern thread_local fn_t g_pending_fn;
    fn_t user_fn = g_pending_fn;
    g_pending_fn = nullptr; // 用完即清

    // t.data 就是首次 jump_fcontext(ctx, vp) 中的 vp
    intptr_t user_vp = reinterpret_cast<intptr_t>(t.data);

    // 以旧版语义调用用户协程入口
    user_fn(user_vp);

    // 协程函数不应正常返回
    assert(false && "libgo: coroutine entry function must not return");
    std::abort();
}

thread_local fn_t g_pending_fn = nullptr;

} // anonymous namespace

intptr_t libgo_jump_fcontext(fcontext_t *ofc, fcontext_t nfc,
                             intptr_t vp, bool /*preserve_fpu*/)
{
    // 新版: transfer_t jump_fcontext(fcontext_t to, void* data)
    // 旧版的 ofc 现在通过返回值传回
    auto t = boost::context::detail::jump_fcontext(
        nfc, reinterpret_cast<void *>(vp));

    if (ofc) {
        *ofc = t.fctx;
    }

    // 旧版返回对方 jump 时传入的 vp，新版通过 t.data 传回，语义一致
    return reinterpret_cast<intptr_t>(t.data);
}

fcontext_t libgo_make_fcontext(void *stack, std::size_t size, fn_t fn)
{
    // make_fcontext 和紧随其后的首次 jump 总是在同一线程顺序执行
    // 因此 thread_local 传递是安全的
    g_pending_fn = fn;

    return boost::context::detail::make_fcontext(stack, size, &trampoline);
}
#endif