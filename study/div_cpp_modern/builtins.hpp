#pragma once

class VM;

namespace Builtins {

struct CoreIds {
    int write = -1;
};

CoreIds register_core(VM& vm);

}