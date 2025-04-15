#ifndef __CHANNEL_STATE_H
#define __CHANNEL_STATE_H

#include "bankstate.h"
#include "common.h"
#include "configuration.h"
#include "timing.h"
#include <vector>

namespace dramsim3 {

class ChannelState {
public:
  ChannelState(const Config &config, const Timing &timing);
  Command GetReadyCommand(const Command &cmd, uint64_t clk) const;
  void UpdateState(const Command &cmd);
  void UpdateTiming(const Command &cmd, uint64_t clk);
  void UpdateTimingAndStates(const Command &cmd, uint64_t clk);
  bool ActivationWindowOk(int rank, uint64_t curr_time) const;
  /// @brief When an activate command is issued,
  /// Not more than 4 ACTIVATE or SINGLE BANK REFRESH commands are allowed
  /// within tFAW period.
  /// In GDDR, no more than 32 banks may be activated in a rolling t 32AW
  /// window.
  void UpdateActivationTimes(int rank, uint64_t curr_time);
  bool IsRowOpen(int rank, int bankgroup, int bank) const {
    return bank_states_[rank][bankgroup][bank].IsRowOpen();
  }
  /// @brief Once there is a bank from this rank is in the state of ROW OPEN,
  /// this rank is not idle.
  bool IsAllBankIdleInRank(int rank) const;
  bool IsRankSelfRefreshing(int rank) const { return rank_is_sref_[rank]; }
  bool IsRefreshWaiting() const { return !refresh_q_.empty(); }
  bool IsRWPendingOnRef(const Command &cmd) const;
  const Command &PendingRefCommand() const { return refresh_q_.front(); }
  void BankNeedRefresh(int rank, int bankgroup, int bank, bool need);
  void RankNeedRefresh(int rank, bool need);
  int OpenRow(int rank, int bankgroup, int bank) const {
    return bank_states_[rank][bankgroup][bank].OpenRow();
  }
  int RowHitCount(int rank, int bankgroup, int bank) const {
    return bank_states_[rank][bankgroup][bank].RowHitCount();
  };

  /// @brief Each rank has a cycle counter for idle cycles.
  std::vector<int> rank_idle_cycles;

private:
  const Config &config_;
  const Timing &timing_;

  /// @brief Each rank has a flag indicates if the rank is in the state of the
  /// refresh. Only after the self refresh command is issued, the rank will set
  /// rank_is_sref_[.] to be true, and after the self exit command is issued,
  /// the rank can set this flag to be false.
  std::vector<bool> rank_is_sref_;
  /// @brief Rank -> Bank Group -> Bank.
  std::vector<std::vector<std::vector<BankState>>> bank_states_;
  /// @brief Put the refresh command into the refresh_q.
  std::vector<Command> refresh_q_;

  /// @brief See the defination of function of IsFAWReady. It stores up to 4
  /// cycles that during this period from the current cycle that cannot issue
  /// activation commands greater than 4. Why up to 4, because in this window,
  /// at most 4 activation commands can be issued, and the simulator will
  /// control that once there has been 4 commands issued, it will not issue new
  /// commands.
  std::vector<std::vector<uint64_t>> four_aw_;
  /// @brief See the defination of function of IsFAWReady. Same as fout_aw_.
  std::vector<std::vector<uint64_t>> thirty_two_aw_;

  bool IsFAWReady(int rank, uint64_t curr_time) const;
  bool Is32AWReady(int rank, uint64_t curr_time) const;
  // Update timing of the bank the command corresponds to
  void UpdateSameBankTiming(
      const Address &addr,
      const std::vector<std::pair<CommandType, int>> &cmd_timing_list,
      uint64_t clk);

  // Update timing of the other banks in the same bankgroup as the command
  void UpdateOtherBanksSameBankgroupTiming(
      const Address &addr,
      const std::vector<std::pair<CommandType, int>> &cmd_timing_list,
      uint64_t clk);

  // Update timing of banks in the same rank but different bankgroup as the
  // command
  void UpdateOtherBankgroupsSameRankTiming(
      const Address &addr,
      const std::vector<std::pair<CommandType, int>> &cmd_timing_list,
      uint64_t clk);

  // Update timing of banks in a different rank as the command
  void UpdateOtherRanksTiming(
      const Address &addr,
      const std::vector<std::pair<CommandType, int>> &cmd_timing_list,
      uint64_t clk);

  // Update timing of the entire rank (for rank level commands)
  void UpdateSameRankTiming(
      const Address &addr,
      const std::vector<std::pair<CommandType, int>> &cmd_timing_list,
      uint64_t clk);
};

}  // namespace dramsim3
#endif
