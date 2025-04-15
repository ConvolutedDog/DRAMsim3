#include "command_queue.h"

namespace dramsim3 {

CommandQueue::CommandQueue(int channel_id, const Config &config,
                           const ChannelState &channel_state,
                           SimpleStats &simple_stats)
    : rank_q_empty(config.ranks, true), config_(config),
      channel_state_(channel_state), simple_stats_(simple_stats),
      is_in_ref_(false),
      queue_size_(static_cast<size_t>(config_.cmd_queue_size)), queue_idx_(0),
      clk_(0) {
  if (config_.queue_structure == "PER_BANK") {
    queue_structure_ = QueueStructure::PER_BANK;
    num_queues_ = config_.banks * config_.ranks;
  } else if (config_.queue_structure == "PER_RANK") {
    queue_structure_ = QueueStructure::PER_RANK;
    num_queues_ = config_.ranks;
  } else {
    std::cerr << "Unsupportted queueing structure " << config_.queue_structure
              << std::endl;
    AbruptExit(__FILE__, __LINE__);
  }

  queues_.reserve(num_queues_);
  for (int i = 0; i < num_queues_; i++) {
    auto cmd_queue = std::vector<Command>();
    cmd_queue.reserve(config_.cmd_queue_size);
    queues_.push_back(cmd_queue);
  }
}

Command CommandQueue::GetCommandToIssue() {
  for (int i = 0; i < num_queues_; i++) {
    auto &queue = GetNextQueue();
    // if we're refresing, skip the command queues that are involved
    if (is_in_ref_) {
      if (ref_q_indices_.find(queue_idx_) != ref_q_indices_.end()) {
        continue;
      }
    }
    auto cmd = GetFirstReadyInQueue(queue);
    if (cmd.IsValid()) {
      if (cmd.IsReadWrite()) {
        EraseRWCommand(cmd);
      }
      return cmd;
    }
  }
  return Command();
}

Command CommandQueue::FinishRefresh() {
  // we can do something fancy here like clearing the R/Ws
  // that already had ACT on the way but by doing that we
  // significantly pushes back the timing for a refresh
  // so we simply implement an ASAP approach

  /// Extract a refresh request form the front of refresh_q_. Here, cause that
  /// the refresh controller will only send REFRESH or REFRESH_BANK command to
  /// the refresh_q_, so ref here can only be REFRESH or REFRESH_BANK command.
  auto ref = channel_state_.PendingRefCommand();
  if (!is_in_ref_) {
    /// Cause the refresh controller has send the address with the refresh
    /// command, for REFRESH command the address is the rank index, and for the
    /// REFRESH_BANK command the address is the rank index, the group index, the
    /// bank index. But for the REFRESH_BANK command, there has two scenarios,
    /// if the command queue is PER_RANK, the function GetRefQIndices should
    /// extract the rank index, and if the command queue is PER_BANK, this
    /// function should extract the indexes of all the banks that belongs to
    /// this rank. And for the REFRESH_BANK command, this function just extract
    /// the bank.
    ///
    /// Another thing is that at the stage of the refresh, there are three
    /// refresh policies, RANK_LEVEL_SIMULTANEOUS, RANK_LEVEL_STAGGERED, and
    /// BANK_LEVEL_STAGGERED. There is no doubt that fot the third policy, the
    /// refresh controller send the REFRESH_BANK command, so we here just
    /// extract the bank index. But for the former two policies, they all send
    /// REFRESH command, so if we have to judge the orgnization of the command
    /// queue. And for the PER_BANK command queue, we should extract the indexes
    /// of all the banks that belongs to the rank.
    ///
    /// If this cycle has not been in the state of refresh, it indicates that
    /// this cycle can do refresh, this function extracts the above mentioned
    /// indexes and put them into ref_q_indices_. Here we set ref_q_indices_ to
    /// be true to permitted the next cycle to continue to entre the if
    /// statement.
    GetRefQIndices(ref);
    is_in_ref_ = true;
  }

  // either precharge or refresh
  /// This step is to check if all the banks has been ready for the refresh
  /// command. For example, if one of the banks is in the state of open, it
  /// should pre-charge before the refresh command, and this function will
  /// return the precharge command.
  auto cmd = channel_state_.GetReadyCommand(ref, clk_);

  /// If the refresh command is ok for all the banks (not the precharge
  /// command), just executed the command. It means that we should release the
  /// ref_q_indices_ and set is_in_self_ to be false.
  if (cmd.IsRefresh()) {
    ref_q_indices_.clear();
    is_in_ref_ = false;
  }
  /// Else return the precharge command.
  return cmd;
}

bool CommandQueue::ArbitratePrecharge(const CMDIterator &cmd_it,
                                      const CMDQueue &queue) const {
  auto cmd = *cmd_it;

  /// Iterate if there are previously uncompleted commands, if true, permit the
  /// precharge command.
  for (auto prev_itr = queue.begin(); prev_itr != cmd_it; prev_itr++) {
    if (prev_itr->Rank() == cmd.Rank() &&
        prev_itr->Bankgroup() == cmd.Bankgroup() &&
        prev_itr->Bank() == cmd.Bank()) {
      return false;
    }
  }

  bool pending_row_hits_exist = false;
  int open_row =
      channel_state_.OpenRow(cmd.Rank(), cmd.Bankgroup(), cmd.Bank());
  /// Check if there are following commands that have will access the same open
  /// row on the same bank with the precharge command. pending_row_hits_exist
  /// records if this open row will be used by the commands after the precharge
  /// command in the queue.
  for (auto pending_itr = cmd_it; pending_itr != queue.end(); pending_itr++) {
    if (pending_itr->Row() == open_row && pending_itr->Bank() == cmd.Bank() &&
        pending_itr->Bankgroup() == cmd.Bankgroup() &&
        pending_itr->Rank() == cmd.Rank()) {
      pending_row_hits_exist = true;
      break;
    }
  }

  bool rowhit_limit_reached =
      channel_state_.RowHitCount(cmd.Rank(), cmd.Bankgroup(), cmd.Bank()) >= 4;
  /// Only if there are no commands that will access the open row or the row
  /// hit number of this row does exceed 4, will continue to execute the
  /// precharge command.
  if (!pending_row_hits_exist || rowhit_limit_reached) {
    simple_stats_.Increment("num_ondemand_pres");
    return true;
  }
  return false;
}

bool CommandQueue::WillAcceptCommand(int rank, int bankgroup, int bank) const {
  int q_idx = GetQueueIndex(rank, bankgroup, bank);
  return queues_[q_idx].size() < queue_size_;
}

bool CommandQueue::QueueEmpty() const {
  for (const auto q : queues_) {
    if (!q.empty()) {
      return false;
    }
  }
  return true;
}

bool CommandQueue::AddCommand(Command cmd) {
  auto &queue = GetQueue(cmd.Rank(), cmd.Bankgroup(), cmd.Bank());
  if (queue.size() < queue_size_) {
    queue.push_back(cmd);
    rank_q_empty[cmd.Rank()] = false;
    return true;
  } else {
    return false;
  }
}

CMDQueue &CommandQueue::GetNextQueue() {
  queue_idx_++;
  if (queue_idx_ == num_queues_) {
    queue_idx_ = 0;
  }
  return queues_[queue_idx_];
}

void CommandQueue::GetRefQIndices(const Command &ref) {
  /// Only REFRESH_BANK or REFRESH can be stored in the refresh queue.
  if (ref.cmd_type == CommandType::REFRESH) {
    if (queue_structure_ == QueueStructure::PER_BANK) {
      /// Append all the banks of this rank.
      for (int i = 0; i < num_queues_; i++) {
        if (i / config_.banks == ref.Rank()) {
          ref_q_indices_.insert(i);
        }
      }
    } else {
      // Just append this rank.
      ref_q_indices_.insert(ref.Rank());
    }
  } else {  // refb
    /// Append the bank index to the ref_q_indexes_.
    int idx = GetQueueIndex(ref.Rank(), ref.Bankgroup(), ref.Bank());
    ref_q_indices_.insert(idx);
  }
  return;
}

int CommandQueue::GetQueueIndex(int rank, int bankgroup, int bank) const {
  if (queue_structure_ == QueueStructure::PER_RANK) {
    return rank;
  } else {
    return rank * config_.banks + bankgroup * config_.banks_per_group + bank;
  }
}

CMDQueue &CommandQueue::GetQueue(int rank, int bankgroup, int bank) {
  int index = GetQueueIndex(rank, bankgroup, bank);
  return queues_[index];
}

Command CommandQueue::GetFirstReadyInQueue(CMDQueue &queue) const {
  for (auto cmd_it = queue.begin(); cmd_it != queue.end(); cmd_it++) {
    Command cmd = channel_state_.GetReadyCommand(*cmd_it, clk_);
    if (!cmd.IsValid()) {
      continue;
    }
    if (cmd.cmd_type == CommandType::PRECHARGE) {
      if (!ArbitratePrecharge(cmd_it, queue)) {
        continue;
      }
    } else if (cmd.IsWrite()) {
      if (HasRWDependency(cmd_it, queue)) {
        continue;
      }
    }
    return cmd;
  }
  return Command();
}

void CommandQueue::EraseRWCommand(const Command &cmd) {
  auto &queue = GetQueue(cmd.Rank(), cmd.Bankgroup(), cmd.Bank());
  for (auto cmd_it = queue.begin(); cmd_it != queue.end(); cmd_it++) {
    if (cmd.hex_addr == cmd_it->hex_addr && cmd.cmd_type == cmd_it->cmd_type) {
      queue.erase(cmd_it);
      return;
    }
  }
  std::cerr << "cannot find cmd!" << std::endl;
  exit(1);
}

int CommandQueue::QueueUsage() const {
  int usage = 0;
  for (auto i = queues_.begin(); i != queues_.end(); i++) {
    usage += i->size();
  }
  return usage;
}

bool CommandQueue::HasRWDependency(const CMDIterator &cmd_it,
                                   const CMDQueue &queue) const {
  // Read after write has been checked in controller so we only
  // check write after read here
  for (auto it = queue.begin(); it != cmd_it; it++) {
    if (it->IsRead() && it->Row() == cmd_it->Row() &&
        it->Column() == cmd_it->Column() && it->Bank() == cmd_it->Bank() &&
        it->Bankgroup() == cmd_it->Bankgroup()) {
      return true;
    }
  }
  return false;
}

}  // namespace dramsim3
