#include <stdio.h>
#include <sys/types.h>
#include "../../../../../../simulator/cpustate.hpp"
#include "../../../../../../simulator/simulator.hpp"

extern "C" void setup_arm_sim(void *sp, u_int64_t calltype)
{
  // stack layout is as below - the number of registers
  // needed for the ARM call is determined by calltype
  //
  // +--------+  
  // | fnptr  |  <--- argument sp points here
  // +--------+  |
  // | rax    |  | return slot if we need to return a value
  // +--------+  |
  // | rdi    |  increasing
  // +--------+  address
  // | rsi    |  |
  // +--------+  V
  // | rdx    |
  // +--------+
  // | rcx    |
  // +--------+
  // | r8     |
  // +--------+
  // | r9     |
  // +--------+
  // | caller |
  // | ret ip |
  // +--------+
  // | arg0   | <-- any extra call args start here
  // +--------+
  // | . . .  |
  //
  //
  // TODO : cater for floating point arguments
  // TODO : for now we create a sim at every call and give it a fresh
  // stack eventually we may need to reuse a sim/stack per thread
  AArch64Simulator sim;
  u_int64_t *cursor = (u_int64_t *)sp;
  u_int64_t *returnSlot = 0;
  u_int64_t stack[100];
  int argCount = calltype & 0xff;
  u_int64_t doReturn = (calltype >> 63);
  sim.init(*cursor++, (u_int64_t)(stack + sizeof(stack)), (u_int64_t)stack);
  returnSlot = cursor++;
  // TODO : need to deal with cases where there are more args than registers
  for (int i = 0; i < argCount; i++) {
    GReg reg = (GReg)i;
    if (i == 6)
      cursor += 2;
    sim.getCPUState().xreg(reg, 0) = *cursor++;
  }
  sim.getCPUState().xreg((GReg)31, 0) = (u_int64_t)sp;
  sim.run();
  if (doReturn) {
    // this overwrites the saved rax
    *returnSlot = sim.getCPUState().xreg(R0, 0);
  }
}
