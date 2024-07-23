// See LICENSE for license details.
#ifndef _RISCV_PROCESSOR_H
#define _RISCV_PROCESSOR_H

#include "decode.h"
#include "config.h"
#include "devices.h"
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include "debug_rom/debug_rom_defines.h"

class processor_t;
class mmu_t;
typedef reg_t (*insn_func_t)(processor_t*, insn_t, reg_t);
class sim_t;
class trap_t;
class extension_t;
class disassembler_t;

struct insn_desc_t
{
  insn_bits_t match;
  insn_bits_t mask;
  insn_func_t rv32;
  insn_func_t rv64;
};

struct commit_log_reg_t
{
  reg_t addr;
  freg_t data;
};

typedef struct
{
  uint8_t prv;
  bool step;
  bool ebreakm;
  bool ebreakh;
  bool ebreaks;
  bool ebreaku;
  bool halt;
  uint8_t cause;
} dcsr_t;

typedef enum
{
  ACTION_DEBUG_EXCEPTION = MCONTROL_ACTION_DEBUG_EXCEPTION,
  ACTION_DEBUG_MODE = MCONTROL_ACTION_DEBUG_MODE,
  ACTION_TRACE_START = MCONTROL_ACTION_TRACE_START,
  ACTION_TRACE_STOP = MCONTROL_ACTION_TRACE_STOP,
  ACTION_TRACE_EMIT = MCONTROL_ACTION_TRACE_EMIT
} mcontrol_action_t;

typedef enum
{
  MATCH_EQUAL = MCONTROL_MATCH_EQUAL,
  MATCH_NAPOT = MCONTROL_MATCH_NAPOT,
  MATCH_GE = MCONTROL_MATCH_GE,
  MATCH_LT = MCONTROL_MATCH_LT,
  MATCH_MASK_LOW = MCONTROL_MATCH_MASK_LOW,
  MATCH_MASK_HIGH = MCONTROL_MATCH_MASK_HIGH
} mcontrol_match_t;

typedef struct
{
  uint8_t type;
  bool dmode;
  uint8_t maskmax;
  bool select;
  bool timing;
  mcontrol_action_t action;
  bool chain;
  mcontrol_match_t match;
  bool m;
  bool h;
  bool s;
  bool u;
  bool execute;
  bool store;
  bool load;
} mcontrol_t;

// architectural state of a RISC-V hart
struct state_t
{
  void reset();

  static const int num_triggers = 4;

  reg_t pc;
  regfile_t<reg_t, NXPR, true> XPR;
  regfile_t<freg_t, NFPR, false> FPR;

  // control and status registers
  reg_t prv;    // TODO: Can this be an enum instead?
  reg_t mstatus;
  reg_t mepc;
  reg_t mbadaddr;
  reg_t mscratch;
  reg_t mtvec;
  reg_t mcause;
  reg_t minstret;
  reg_t mie;
  reg_t mip;
  reg_t medeleg;
  reg_t mideleg;
  uint32_t mcounteren;
  uint32_t scounteren;
  reg_t sepc;
  reg_t sbadaddr;
  reg_t sscratch;
  reg_t stvec;
  reg_t sptbr;
  reg_t scause;
  reg_t dpc;
  reg_t dscratch;
  dcsr_t dcsr;
  reg_t tselect;
  mcontrol_t mcontrol[num_triggers];
  reg_t tdata2[num_triggers];

  uint32_t fflags;
  uint32_t frm;
  bool serialized; // whether timer CSRs are in a well-defined state

  // When true, execute a single instruction and then enter debug mode.  This
  // can only be set by executing dret.
  enum {
      STEP_NONE,
      STEP_STEPPING,
      STEP_STEPPED
  } single_step;

  reg_t load_reservation;

#ifdef RISCV_ENABLE_COMMITLOG
  commit_log_reg_t log_reg_write;
  reg_t last_inst_priv;
  int last_inst_xlen;
  int last_inst_flen;
#endif
};

typedef enum {
  OPERATION_EXECUTE,
  OPERATION_STORE,
  OPERATION_LOAD,
} trigger_operation_t;

// Count number of contiguous 1 bits starting from the LSB.
static int cto(reg_t val)
{
  int res = 0;
  while ((val & 1) == 1)
    val >>= 1, res++;
  return res;
}

struct pcode_region {
    uint32_t base;
    uint32_t mask;
	bool valid;
	bool lock;
};
class pcode_csr {
    public:
        uint64_t isolationSW;
		reg_t vpoffset;
		uint32_t pcodelock;
    struct pcode_region pcode_region[4];
		bool contains(reg_t addr) {
			int i = 0;
			for (i = 0; i < 4; i += 1) {
				if(pcode_lock() && pcode_region[i].valid && pcode_region[i].lock) {
					if ((pcode_region[i].base << 12) == (uint32_t)(addr & ((0xff << 24) | (pcode_region[i].mask << 14)))) {
						// fprintf(stderr, "[spike] valid, lock of %d: %x, %x\n",i, pcode_region[i].valid, pcode_region[i].lock);
            // fprintf(stderr, "[spike] base, mask of %d: %x, %x\n",i, pcode_region[i].base, pcode_region[i].mask );
            // fprintf(stderr, "[spike] ref of %d: %x\n",i, (uint32_t)(addr & ((0xff << 24) | (pcode_region[i].mask << 14))));
						return true;
					}
				}
			}
      return false;
		}
		bool pcode_lock(void) {
			for(int i=0; i<4; i+=1 ) {
				if(!pcode_region[i].lock) {
					return false;
				}
			}
			return true;
		}
    bool isw_enabled(void) {
        return (isolationSW & 0x1) == 0x1;
		} 
		bool isw_disabled(void) {
			return (isolationSW == 0x0);
		}
/*
		bool enabled(void) {
            return (pcodelock & (1 << 1)) == (1 << 1);
        }
        bool triggered_fetch_fault(void) {
            return (pcodelock & (1 << 8)) == (1 << 8);
        }
        bool triggered_load_fault(void) {
            return (pcodelock & (1 << 9)) == (1 << 9);
        }
        bool triggered_store_fault(void) {
            return (pcodelock & (1 << 10)) == (1 << 10);
        }
        void set_fetch_fault(void) {
            pcodelock |= (1 << 8);
        }
        void set_load_fault(void) {
            pcodelock |= (1 << 9);
        }
        void set_store_fault(void) {
            pcodelock |= (1 << 10);
        }
        void clear_fetch_fault(void) {
            pcodelock &= ~(1 << 8);
        }
        void clear_load_fault(void) {
            pcodelock &= ~(1 << 9);
        }
        void clear_store_fault(void) {
            pcodelock &= ~(1 << 10);
        }
*/
};

// this class represents one processor in a RISC-V machine.
class processor_t : public abstract_device_t
{
    
public:
  processor_t(const char* isa, sim_t* sim, uint32_t id, bool halt_on_reset=false);
  ~processor_t();

  std::ofstream pclout;
  std::ofstream pclout_m;
 void open_pcl_module_log_file() {
      std::stringstream ss;
      ss.str("");
      ss << "pcl-profile-module" << this->id << ".txt";
      pclout_m.open(ss.str());
  }
  
  void open_pcl_log_file() {
      std::stringstream ss;
      ss.str("");
      ss << "pcl-profile" << this->id << ".txt";
      pclout.open(ss.str());
  }
  void close_pcl_log_file() {
      if(pclout.is_open()) {
          pclout.close();
      }
  }
 void close_pcl_m_log_file() {
      if(pclout_m.is_open()) {
          pclout_m.close();
      }
  }

 void dump_to_pcl_m_log_file(char* buf) {
      if(!pclout_m.is_open()) {
          open_pcl_module_log_file();
      }
//fprintf(stdout, "test\n");
      pclout_m << buf << std::endl;
  }

void dump_to_pcl_log_file(char* buf) {
      if(!pclout.is_open()) {
          open_pcl_log_file();
      }
//fprintf(stdout, "test\n");
      pclout << buf << std::endl;
  }
  int m_index = 0;	
  int l_count = 0;
  int s_count = 0;
  int f_count = 0;
  reg_t mem_list[20]; // temp
  reg_t save_jtaget; 
  struct pcode_csr pcode;
  void set_debug(bool value);
  void set_histogram(bool value);
  void reset();
  void step(size_t n); // run for n cycles
  void set_csr(int which, reg_t val);
  reg_t get_csr(int which);
  mmu_t* get_mmu() { return mmu; }
  state_t* get_state() { return &state; }
  unsigned get_xlen() { return xlen; }
  unsigned get_flen() {
    return supports_extension('Q') ? 128 :
           supports_extension('D') ? 64 :
           supports_extension('F') ? 32 : 0;
  }
  extension_t* get_extension() { return ext; }
  bool supports_extension(unsigned char ext) {
    if (ext >= 'a' && ext <= 'z') ext += 'A' - 'a';
    bool ret = ext >= 'A' && ext <= 'Z' && ((isa >> (ext - 'A')) & 1);
    return ret;
  }
  void set_privilege(reg_t);
  void yield_load_reservation() { state.load_reservation = (reg_t)-1; }
  void update_histogram(reg_t pc);
  const disassembler_t* get_disassembler() { return disassembler; }

  void register_insn(insn_desc_t);
  void register_extension(extension_t*);

  // MMIO slave interface
  bool load(reg_t addr, size_t len, uint8_t* bytes);
  bool store(reg_t addr, size_t len, const uint8_t* bytes);

  // When true, display disassembly of each instruction that's executed.
  bool debug;
  // When true, take the slow simulation path.
  bool slow_path();
  bool halted() { return state.dcsr.cause ? true : false; }
  bool halt_request;

  // Return the index of a trigger that matched, or -1.
  inline int trigger_match(trigger_operation_t operation, reg_t address, reg_t data)
  {
    if (state.dcsr.cause)
      return -1;

    bool chain_ok = true;

    for (unsigned int i = 0; i < state.num_triggers; i++) {
      if (!chain_ok) {
        chain_ok |= !state.mcontrol[i].chain;
        continue;
      }

      if ((operation == OPERATION_EXECUTE && !state.mcontrol[i].execute) ||
          (operation == OPERATION_STORE && !state.mcontrol[i].store) ||
          (operation == OPERATION_LOAD && !state.mcontrol[i].load) ||
          (state.prv == PRV_M && !state.mcontrol[i].m) ||
          (state.prv == PRV_H && !state.mcontrol[i].h) ||
          (state.prv == PRV_S && !state.mcontrol[i].s) ||
          (state.prv == PRV_U && !state.mcontrol[i].u)) {
        continue;
      }

      reg_t value;
      if (state.mcontrol[i].select) {
        value = data;
      } else {
        value = address;
      }

      // We need this because in 32-bit mode sometimes the PC bits get sign
      // extended.
      if (xlen == 32) {
        value &= 0xffffffff;
      }

      switch (state.mcontrol[i].match) {
        case MATCH_EQUAL:
          if (value != state.tdata2[i])
            continue;
          break;
        case MATCH_NAPOT:
          {
            reg_t mask = ~((1 << cto(state.tdata2[i])) - 1);
            if ((value & mask) != (state.tdata2[i] & mask))
              continue;
          }
          break;
        case MATCH_GE:
          if (value < state.tdata2[i])
            continue;
          break;
        case MATCH_LT:
          if (value >= state.tdata2[i])
            continue;
          break;
        case MATCH_MASK_LOW:
          {
            reg_t mask = state.tdata2[i] >> (xlen/2);
            if ((value & mask) != (state.tdata2[i] & mask))
              continue;
          }
          break;
        case MATCH_MASK_HIGH:
          {
            reg_t mask = state.tdata2[i] >> (xlen/2);
            if (((value >> (xlen/2)) & mask) != (state.tdata2[i] & mask))
              continue;
          }
          break;
      }

      if (!state.mcontrol[i].chain) {
        return i;
      }
      chain_ok = true;
    }
    return -1;
  }

  void trigger_updated();

private:
  sim_t* sim;
  mmu_t* mmu; // main memory is always accessed via the mmu
  extension_t* ext;
  disassembler_t* disassembler;
  state_t state;
  uint32_t id;
  unsigned max_xlen;
  unsigned xlen;
  reg_t isa;
  reg_t max_isa;
  std::string isa_string;
  bool histogram_enabled;
  bool halt_on_reset;

  std::vector<insn_desc_t> instructions;
  std::map<reg_t,uint64_t> pc_histogram;

  static const size_t OPCODE_CACHE_SIZE = 8191;
  insn_desc_t opcode_cache[OPCODE_CACHE_SIZE];

  void take_pending_interrupt() { take_interrupt(state.mip & state.mie); }
  void take_interrupt(reg_t mask); // take first enabled interrupt in mask
  void take_trap(trap_t& t, reg_t epc); // take an exception
  void disasm(insn_t insn); // disassemble and print an instruction
  int paddr_bits();

  void enter_debug_mode(uint8_t cause);

  friend class sim_t;
  friend class mmu_t;
  friend class clint_t;
  friend class extension_t;

  void parse_isa_string(const char* isa);
  void build_opcode_map();
  void register_base_instructions();
  insn_func_t decode_insn(insn_t insn);

  // Track repeated executions for processor_t::disasm()
  uint64_t last_pc, last_bits, executions;
};

reg_t illegal_instruction(processor_t* p, insn_t insn, reg_t pc);

#define REGISTER_INSN(proc, name, match, mask) \
  extern reg_t rv32_##name(processor_t*, insn_t, reg_t); \
  extern reg_t rv64_##name(processor_t*, insn_t, reg_t); \
  proc->register_insn((insn_desc_t){match, mask, rv32_##name, rv64_##name});

#endif
