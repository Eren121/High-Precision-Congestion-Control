#include "ns3/ag-shared.h"
#include "ns3/ag-runtime.h"
#include "ns3/rdma-network.h"
#include <fstream>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(AgShared);

TypeId AgShared::GetTypeId()
{
  static TypeId tid = TypeId("ns3::AgShared")
    .SetParent<Object>()
  ;
  return tid;
}

AgShared::AgShared(Ptr<AgConfig> config, NodeContainer servers)
  : m_config{config},
    m_servers{servers}
{
  m_start = Simulator::Now();
}
  
void AgShared::AddMissedChunk(block_id_t block, chunk_id_t chunk)
{
  MissedChunkRecord record;
  record.block = block;
  record.chunk = chunk;

  m_missed.push_back(record);
}

void AgShared::RegisterStateTransition(AgState state)
{
  if(state == AgState::Finished) {
    m_completed_apps++;
    if(m_completed_apps == m_config->GetBlockCount()) {
      m_config->OnAllFinished();
      DumpStats();
      DumpMissedChunks();
    }
  }
  else if(state == AgState::Recovery) {
    m_completed_mcasts++;
    if(m_completed_mcasts == m_config->GetBlockCount()) {
      m_mcast_elapsed = Simulator::Now() - m_start;
    }
  }
}

void AgShared::DumpStats() const
{
  if(m_config->dump_stats.empty()) {
    return;
  }

  const RdmaConfig& rdma_config{RdmaNetwork::GetInstance().GetConfig()};
  const std::string& out_path{rdma_config.FindFile(m_config->dump_stats)};
  std::ofstream ofs{out_path.c_str()};

  json info;
  info["total_elapsed_time"] = Simulator::Now().GetSeconds() - m_start.GetSeconds();
  info["mcast_elapsed_time"] = m_mcast_elapsed.GetSeconds();
  info["lost_chunk_count"] = m_missed.size();
  info["lost_chunk_percent"] = double(m_missed.size()) / m_config->GetTotalChunkCount();  
  
  info["total_chunk_count"] = m_config->GetTotalChunkCount();
  info["chunk_size"] = m_config->GetChunkByteSize();
  info["block_count"] = m_config->GetBlockCount();
  
  ofs << info.dump(4);
}

void AgShared::DumpMissedChunks() const
{
  if(m_config->dump_missed_chunks.empty()) {
    return;
  }

  const RdmaConfig& rdma_config{RdmaNetwork::GetInstance().GetConfig()};
  const std::string& out_path{rdma_config.FindFile(m_config->dump_missed_chunks)};
  std::ofstream ofs{out_path.c_str()};

  ofs << rfl::json::write(m_missed);
}

void AgShared::RegisterNode(Ptr<AgRuntime> node)
{
  m_nodes[node->GetBlock()] = node;
}

void AgShared::NotifyCutoffTimerTriggered(block_id_t)
{
  m_cutoff_triggered++;
}

Ptr<AgRuntime> AgShared::GetNode(block_id_t node) const
{
  return m_nodes.at(node);
}

Ptr<AgConfig> AgShared::GetConfig() const
{
  return m_config;
}

NodeContainer AgShared::GetServers() const
{
  return m_servers;
}

} // namespace ns3