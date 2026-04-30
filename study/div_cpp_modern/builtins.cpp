#include "builtins.hpp"

#include <cstdio>

#include "vm.hpp"

namespace {

void builtin_write(VM& vm)
{
    std::printf("  write -> ");
    vm.print_value(vm.top());
    std::printf("\n");
    vm.pop_discard();
}

} // namespace

namespace Builtins {

CoreIds register_core(VM& vm)
{
    CoreIds ids;
    ids.write = vm.add_builtin(builtin_write);
    return ids;
}

} // namespace Builtins