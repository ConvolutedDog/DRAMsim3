#ifndef __CONFIG_H
#define __CONFIG_H

#include "common.h"
#include <fstream>
#include <string>

#include "INIReader.h"

namespace dramsim3 {

enum class DRAMProtocol {
  DDR3,
  DDR4,
  GDDR5,
  GDDR5X,
  GDDR6,
  LPDDR,
  LPDDR3,
  LPDDR4,
  HBM,
  HBM2,
  HMC,
  SIZE
};

enum class RefreshPolicy {
  RANK_LEVEL_SIMULTANEOUS,  // impractical due to high power requirement
  RANK_LEVEL_STAGGERED,
  BANK_LEVEL_STAGGERED,
  SIZE
};

class Config {
public:
  Config(std::string config_file, std::string out_dir);
  Address AddressMapping(uint64_t hex_addr) const;
  // DRAM physical structure
  DRAMProtocol protocol;
  int channel_size;
  int channels;
  int ranks;
  int banks;
  int bankgroups;
  int banks_per_group;
  int rows;
  int columns;
  int device_width;
  int bus_width;
  int devices_per_rank;
  int BL;

  // Address mapping numbers
  int shift_bits;
  int ch_pos, ra_pos, bg_pos, ba_pos, ro_pos, co_pos;
  uint64_t ch_mask, ra_mask, bg_mask, ba_mask, ro_mask, co_mask;

  // Generic DRAM timing parameters
  double tCK;
  int burst_cycle;  // seperate BL with timing since for GDDRx it's not BL/2

  /// @ref
  /// https://www.intel.com/content/www/us/en/docs/programmable/683106/21-1-19-2-0/additive-latency.html
  /// Additive latency increases the efficiency of the command and data bus for
  /// sustainable bandwidths. You may issue the commands externally but the
  /// device holds the commands internally for the duration of additive latency
  /// before executing, to improve the system scheduling. The delay helps to
  /// avoid collision on the command bus and gaps in data input or output
  /// bursts. Additive latency allows the controller to issue the row and column
  /// address commands—activate, and read or write—in consecutive clock cycles,
  /// so that the controller need not hold the column address for several (tRCD)
  /// cycles. This gap between the activate and the read or write command can
  /// cause bubbles in the data stream.
  int AL;
  /// @ref https://en.wikipedia.org/wiki/CAS_latency
  /// Column address strobe latency, also called CAS latency or CL, is the delay
  /// in clock cycles between the READ command and the moment data is available.
  /// In asynchronous DRAM, the interval is specified in nanoseconds (absolute
  /// time). In synchronous DRAM, the interval is specified in clock cycles.
  /// Because the latency is dependent upon a number of clock ticks instead of
  /// absolute time, the actual time for an SDRAM module to respond to a CAS
  /// event might vary between uses of the same module if the clock rate
  /// differs.
  int CL;
  /// CAS Write Latency, also called CWL, is the number of clock cycles between
  /// the assertion of a WRITE command and the moment the memory controller must
  /// deliver the first data word to the DRAM via the DQ bus.
  int CWL;
  /// Read Latency, also called RL, ​Relationship with AL:
  /// RL = AL (Additive Latency) + CL. If AL = 0 (default), then RL = CL.
  /// Full Workflow:
  /// READ command → (AL cycles) → CL cycles → Data output from DRAM.
  int RL;
  /// Write Latency, also called WL, ​Relationship with CWL:
  /// WL = AL (Additive Latency) + CWL. If AL = 0 (default), then WL = CWL.
  /// Full Workflow:
  /// WRITE command → (AL cycles) → CWL cycles → Data input to DRAM.
  int WL;
  /// @ref JESD79-4B Figure 60 — tCCD Timing (WRITE to WRITE Example)
  /// @ref JESD79-4B Figure 61 — tCCD Timing (READ to READ Example)
  /// tCCD_L : CAS_n-to-CAS_n delay (long) : Applies to consecutive CAS_n
  /// (Column Address Strobe, Active Low) to the same Bank Group (i.e., T4 to
  /// T10).
  int tCCD_L;
  /// @ref JESD79-4B Figure 60 — tCCD Timing (WRITE to WRITE Example)
  /// @ref JESD79-4B Figure 61 — tCCD Timing (READ to READ Example)
  /// tCCD_S : CAS_n-to-CAS_n delay (short) : Applies to consecutive CAS_n
  /// (Column Address Strobe, Active Low) to different Bank Group (i.e., T0 to
  /// T4).
  int tCCD_S;
  /// @ref https://www.globalsino.com/ICsAndMaterials/page4933.html
  /// Rank-to-rank switching time, which is used in DDR and DDR2 SDRAM memory
  /// systems and is not used in SDRAM or Direct RDRAM memory systems. It is one
  /// full cycle in DDR SDRAM.
  int tRTRS;
  /// Read to Precharge Latency, the ​minimum number of clock cycles (tCK)
  /// required between the end of a READ command (deassertion of CAS_n) and the
  /// start of a subsequent PRECHARGE command for the same bank.
  int tRTP;
  /// @ref JESD79-4B Figure 65 — tWTR_L Timing (WRITE to READ, Same Bank Group,
  /// CRC and DM Disabled)
  /// tWTR_L: Delay from start of internal write transaction to internal read
  /// command to the same Bank Group. When AL is non-zero, the external read
  /// command at Tb0 can be pulled in by AL.
  int tWTR_L;
  /// @ref JESD79-4B Figure 64 — tWTR_S Timing (WRITE to READ, Different Bank
  /// Group, CRC and DM Disabled)
  /// tWTR_S : Delay from start of internal write transaction to internal read
  /// command to a different Bank Group. When AL is non-zero, the external read
  /// command at Tb0 can be pulled in by AL.
  int tWTR_S;
  /// Write Recovery Time, the ​minimum delay required between the last data
  /// input of a WRITE operation and issuing a PRECHARGE command to the same
  /// bank. This ensures written data is properly stored in memory cells before
  /// the bank is closed.
  int tWR;
  /// Row Precharge Time, the ​minimum time required to deactivate (precharge)
  /// an open row and prepare the bank for a new row activation. This is
  /// measured from the start of a PRECHARGE command to when a new ACT
  /// (Activate) command can be issued to the same bank.
  int tRP;
  /// @ref JESD79-4B Figure 62 — tRRD Timing
  /// tRRD_L: ACTIVATE to ACTIVATE Command period (long) : Applies to
  /// consecutive ACTIVATE Commands to the different Banks of the same Bank
  /// Group (i.e., T4 to T10)
  int tRRD_L;
  /// @ref JESD79-4B Figure 62 — tRRD Timing
  /// tRRD_S: ACTIVATE to ACTIVATE Command period (short) : Applies to
  /// consecutive ACTIVATE Commands to different Bank Group (i.e., T0 to T4)
  int tRRD_S;
  /// Row Active Time, the ​minimum duration a DRAM row must remain open
  /// (active) before it can be precharged.
  int tRAS;
  /// RAS to CAS Delay, the ​minimum delay required between activating a row
  /// (RAS) and issuing a column access command (CAS) for read/write operations.
  int tRCD;
  /// Refresh Cycle Time, the ​minimum time required to complete a refresh
  /// operation across all banks in a DDR memory module. This parameter ensures
  /// data integrity by periodically recharging DRAM cells before their charge
  /// dissipates.
  int tRFC;
  /// Row Cycle Time, the ​minimum time required between successive ACTIVATE
  /// (ACT) commands to the same bank in DDR SDRAM (DDR4/DDR5). This parameter
  /// governs how frequently a bank can be accessed.
  int tRC;
  // tCKSRE and tCKSRX are only useful for changing clock freq after entering
  // SRE mode we are not doing that, so tCKESR is sufficient
  int tCKE;
  /// @ref JESD79-4B section 4.27
  /// The minimum time that the DDR4 SDRAM must remain in Self-Refresh mode is
  /// tCKESR.
  int tCKESR;
  /// Exit Time from Self-Refresh, the ​minimum time required for a memory
  /// device to become fully operational after exiting Self-Refresh mode. This
  /// latency period ensures all internal circuits stabilize before normal
  /// commands can be processed.
  int tXS;
  /// Exit Time from Power-Down Mode, the ​minimum delay required after
  /// deasserting CKE (Clock Enable) before valid commands can be issued when
  /// exiting Power-Down mode. This ensures stable clock synchronization and
  /// internal circuit recovery.
  int tXP;
  /// Bank-Level Refresh Cycle Time, the ​minimum time required to refresh a
  /// single bank or bank group during per-bank refresh operations. This
  /// represents a fundamental architectural shift from DDR4's global refresh
  /// mechanism.
  int tRFCb;
  /// Refresh Interval, the average time between automatic refresh commands.
  int tREFI;
  /// Per-bank refresh interval.
  int tREFIb;
  /// @ref JESD79-4B Figure 63 — tFAW Timing
  /// tFAW: Four activate window.
  int tFAW;
  /// Read Preamble Ready Time, Setup time before DQS strobe for reads.
  int tRPRE;  // read preamble and write preamble are important
  /// Write Preamble Ready Time, DQS lead-in time before write data.
  int tWPRE;
  int read_delay;
  int write_delay;

  // LPDDR4 and GDDR5
  /// Post-Package Repair Delay, Recovery time after fuse programming.
  int tPPD;
  // GDDR5
  /// 32-bit Activate Window, Minimum time between 32-byte activates.
  int t32AW;
  /// RAS-to-CAS Read Delay, ACT to READ delay.
  int tRCDRD;
  /// RAS-to-CAS Write Delay, ACT to WRITE delay.
  int tRCDWR;

  // pre calculated power parameters
  double act_energy_inc;
  double pre_energy_inc;
  double read_energy_inc;
  double write_energy_inc;
  double ref_energy_inc;
  double refb_energy_inc;
  double act_stb_energy_inc;
  double pre_stb_energy_inc;
  double pre_pd_energy_inc;
  double sref_energy_inc;

  // HMC
  int num_links;
  int num_dies;
  int link_width;
  int link_speed;
  int num_vaults;
  int block_size;  // block size in bytes
  int xbar_queue_depth;

  // System
  std::string address_mapping;
  std::string queue_structure;
  std::string row_buf_policy;
  RefreshPolicy refresh_policy;
  int cmd_queue_size;
  bool unified_queue;
  int trans_queue_size;
  int write_buf_size;
  bool enable_self_refresh;
  int sref_threshold;
  bool aggressive_precharging_enabled;
  bool enable_hbm_dual_cmd;

  int epoch_period;
  int output_level;
  std::string output_dir;
  std::string output_prefix;
  std::string json_stats_name;
  std::string json_epoch_name;
  std::string txt_stats_name;

  // Computed parameters
  int request_size_bytes;

  bool IsGDDR() const {
    return (protocol == DRAMProtocol::GDDR5 ||
            protocol == DRAMProtocol::GDDR5X ||
            protocol == DRAMProtocol::GDDR6);
  }
  bool IsHBM() const {
    return (protocol == DRAMProtocol::HBM || protocol == DRAMProtocol::HBM2);
  }
  bool IsHMC() const { return (protocol == DRAMProtocol::HMC); }
  // yzy: add another function
  bool IsDDR4() const { return (protocol == DRAMProtocol::DDR4); }

  int ideal_memory_latency;

#ifdef THERMAL
  std::string loc_mapping;
  int num_row_refresh;  // number of rows to be refreshed for one time
  double amb_temp;      // the ambient temperature in [C]
  double const_logic_power;

  double chip_dim_x;
  double chip_dim_y;
  int num_x_grids;
  int num_y_grids;
  int mat_dim_x;
  int mat_dim_y;
  // 0: x-direction priority, 1: y-direction priority
  int bank_order;
  // 0; low-layer priority, 1: high-layer priority
  int bank_layer_order;
  int row_tile;
  int tile_row_num;
  double bank_asr;  // the aspect ratio of a bank: #row_bits / #col_bits
#endif              // THERMAL

private:
  INIReader *reader_;
  void CalculateSize();
  DRAMProtocol GetDRAMProtocol(std::string protocol_str);
  int GetInteger(const std::string &sec, const std::string &opt,
                 int default_val) const;
  void InitDRAMParams();
  void InitOtherParams();
  void InitPowerParams();
  void InitSystemParams();
#ifdef THERMAL
  void InitThermalParams();
#endif  // THERMAL
  void InitTimingParams();
  void SetAddressMapping();
};

}  // namespace dramsim3
#endif
