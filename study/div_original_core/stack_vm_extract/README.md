# Stack/VM extract (original DIV code)

Este diretorio tem apenas recortes literais do codigo original, sem reescrita.

## Ordem de leitura

1. `01_opcodes_base_from_inter.h`
2. `02_stack_registers_from_inter.h`
3. `03_vm_loop_and_stack_persistence_from_i.cpp`
4. `04_kernel_core_cases_from_kernel.cpp`

## Onde estao no original

- `../inter.h`
- `../i.cpp`
- `../kernel.cpp`

## Dica rapida de estudo

- Primeiro entende `pila`, `sp`, `ip`, `bp`.
- Depois segue `interprete() -> exec_process() -> nucleo_exec()`.
- Por fim, percorre os `case l...` no `kernel.cpp`.
