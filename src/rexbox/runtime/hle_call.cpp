#include "rexbox/runtime/hle_call.h"

#include "rexbox/runtime/environment.h"

namespace rexbox::runtime
{

HleCall::HleCall() : cpu_(Env().executor->Cpu()), memory_(*Env().memory)
{
}

uint32_t HleCall::Arg(int index) const
{
    return memory_.Read32(cpu_.Get(Gpr::kEsp) + 4 + 4 * static_cast<uint32_t>(index));
}

void HleCall::Return(uint32_t value, int stack_args)
{
    cpu_.Set(Gpr::kEax, value);
    cpu_.Set(Gpr::kEsp, cpu_.Get(Gpr::kEsp) + 4 + 4 * static_cast<uint32_t>(stack_args));
}

void HleCall::Return64(uint64_t value, int stack_args)
{
    cpu_.Set(Gpr::kEdx, static_cast<uint32_t>(value >> 32));
    Return(static_cast<uint32_t>(value), stack_args);
}

}  // namespace rexbox::runtime
