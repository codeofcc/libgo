#pragma once
#include "../common/config.h"
#include "fcontext.h"

#if defined(LIBGO_SYS_Windows)
# include "fiber/context.h"
#else
namespace co {

class Context
{
public:
    Context(fn_t fn, intptr_t vp, std::size_t stackSize)
        : fn_(fn), vp_(vp), stackSize_(stackSize), entered_(false)
    {
        stack_ = (char*)StackTraits::MallocFunc()(stackSize_);
        DebugPrint(dbg_task, "valloc stack. size=%u ptr=%p",
                stackSize_, stack_);

        ctx_ = libgo_make_fcontext(stack_ + stackSize_, stackSize_, fn_);

        int protectPage = StackTraits::GetProtectStackPageSize();
        if (protectPage && StackTraits::ProtectStack(stack_, stackSize_, protectPage))
            protectPage_ = protectPage;
    }
    ~Context()
    {
        if (stack_) {
            DebugPrint(dbg_task, "free stack. ptr=%p", stack_);
            if (protectPage_)
                StackTraits::UnprotectStack(stack_, protectPage_);
            StackTraits::FreeFunc()(stack_);
            stack_ = NULL;
        }
    }

    ALWAYS_INLINE void SwapIn()
    {
        ContextBridge bridge;
        bridge.slot_ = &GetTlsContext();                 // 调度线程挂起后的句柄存放处
        bridge.vp_   = vp_;
        bridge.fn_   = entered_ ? nullptr : fn_;         // 仅首次进入需要提供入口函数
        libgo_jump_context(&GetTlsContext(), ctx_, &bridge);
        entered_ = true;
    }

    ALWAYS_INLINE void SwapTo(Context & other)
    {
        ContextBridge bridge;
        bridge.slot_ = &ctx_;
        bridge.vp_   = other.vp_;
        bridge.fn_   = nullptr;
        libgo_jump_context(&ctx_, other.ctx_, &bridge);
    }

    ALWAYS_INLINE void SwapOut()
    {
        ContextBridge bridge;
        bridge.slot_ = &ctx_;                            // 我方句柄由调度线程写回，供下次 SwapIn 作为跳转目标
        bridge.vp_   = 0;
        bridge.fn_   = nullptr;
        libgo_jump_context(&ctx_, GetTlsContext(), &bridge);
    }

    fcontext_t& GetTlsContext()
    {
        static thread_local fcontext_t tls_context;
        return tls_context;
    }

private:
    fcontext_t ctx_;
    fn_t fn_;
    intptr_t vp_;
    char* stack_ = nullptr;
    uint32_t stackSize_ = 0;
    int protectPage_ = 0;
    bool entered_ = false; // 首次 SwapIn 需要把入口函数交给 trampoline
};
} // namespace co

#endif
