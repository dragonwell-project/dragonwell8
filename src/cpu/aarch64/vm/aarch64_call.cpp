#include <stdio.h>
#include <sys/types.h>
#include "asm/assembler.hpp"
#include "../../../../../../simulator/cpustate.hpp"
#include "../../../../../../simulator/simulator.hpp"

/*
 * we maintain a simulator per-thread and provide it with 8 Mb of
 * stack space
 */
#define SIM_STACK_SIZE (1024 * 1024) // in units of u_int64_t

extern "C" u_int64_t get_alt_stack()
{
  return AArch64Simulator::altStack();
}

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
  // | xmm0   |
  // +--------+
  // | xmm1   |
  // +--------+
  // | xmm2   |
  // +--------+
  // | xmm3   |
  // +--------+
  // | xmm4   |
  // +--------+
  // | xmm5   |
  // +--------+
  // | xmm6   |
  // +--------+
  // | xmm7   |
  // +--------+
  // | fp     |
  // +--------+
  // | caller |
  // | ret ip |
  // +--------+
  // | arg0   | <-- any extra call args start here
  // +--------+     offset = 18 * wordSize
  // | . . .  |     (i.e. 1 * calladdr + 1 * rax  + 6 * gp call regs
  //                      + 8 * fp call regs + 2 * frame words)
  //
  // TODO : for now we create a sim at every call and give it a fresh
  // stack eventually we may need to reuse a sim/stack per thread
  const int cursor2_offset = 18;
  const int fp_offset = 16;
  u_int64_t *cursor = (u_int64_t *)sp;
  u_int64_t *cursor2 = ((u_int64_t *)sp) + cursor2_offset;
  u_int64_t *fp = ((u_int64_t *)sp) + fp_offset;
  int gp_arg_count = calltype & 0xf;
  int fp_arg_count = (calltype >> 4) & 0xf;
  int return_type = (calltype >> 8) & 0x3;
  if (UseSimulatorCache) {
    AArch64Simulator::useCache = 1;
  }
  AArch64Simulator *sim = AArch64Simulator::current();
  // set up start pc
  sim->init(*cursor++, (u_int64_t)sp, (u_int64_t)fp);
  u_int64_t *return_slot = cursor++;

  // if we need to pass the sim extra args on the stack then bump
  // the stack pointer now
  u_int64_t *cursor3 = (u_int64_t *)sim->getCPUState().xreg(SP, 1);
  if (gp_arg_count > 8) {
    cursor3 -= gp_arg_count - 8;
  }
  if (fp_arg_count > 8) {
    cursor3 -= fp_arg_count - 8;
  }
  sim->getCPUState().xreg(SP, 1) = (u_int64_t)(cursor3++);

  for (int i = 0; i < gp_arg_count; i++) {
    if (i < 6) {
      // copy saved register to sim register
      GReg reg = (GReg)i;
      sim->getCPUState().xreg(reg, 0) = *cursor++;
    } else if (i < 8) {
      // copy extra int arg to sim register
      GReg reg = (GReg)i;
      sim->getCPUState().xreg(reg, 0) = *cursor2++;
    } else {
      // copy extra fp arg to sim stack      
      *cursor3++ = *cursor2++;
    }
  }
  for (int i = 0; i < fp_arg_count; i++) {
    if (i < 8) {
      // copy saved register to sim register
      GReg reg = (GReg)i;
      sim->getCPUState().xreg(reg, 0) = *cursor++;
    } else {
      // copy extra arg to sim stack      
      *cursor3++ = *cursor2++;
    }
  }
  AArch64Simulator::status_t return_status = sim->run();
  if (return_status != AArch64Simulator::STATUS_RETURN){
    sim->simPrint0();
    fatal("invalid status returned from simulator.run()\n");
  }
  switch (return_type) {
  case MacroAssembler::ret_type_void:
  default:
    break;
  case MacroAssembler::ret_type_integral:
  // this overwrites the saved rax
    *return_slot = sim->getCPUState().xreg(R0, 0);
    break;
  case MacroAssembler::ret_type_float:
    *(float *)return_slot = sim->getCPUState().sreg(V0);
    break;
  case MacroAssembler::ret_type_double:
    *(double *)return_slot = sim->getCPUState().dreg(V0);
    break;
  }
}
