#ifndef MONITOREDTABLE_H // Include guards to prevent multiple inclusion

#define MONITOREDTABLE_H

#include <bf_rt/bf_rt.hpp>
#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_learn.hpp>
#include <bf_rt/bf_rt_session.hpp>
#include <bf_rt/bf_rt_table_attributes.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table.hpp>
#include <bf_rt/bf_rt_table_key.hpp>
#include <bf_rt/bf_rt_table_operations.hpp>

using namespace std;
using namespace bfrt;

uint32_t ipv4_to_bytes(const char* ipv4, uint8_t* pref_len=nullptr);

class MonitoredTable{
    private:
        bf_status_t bf_status;
        // keep session, dev_tgt since we need it in many funcs
        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;

        // monitored table info
        const BfRtTable *monitored_table;
        
        // for writing/reading data
        unique_ptr<BfRtTableKey> _key;
        unique_ptr<BfRtTableData> _data;

        // action/key/data IDs
        bf_rt_id_t calc_idx_id, meta_addr_id;
        bf_rt_id_t base_idx_id, mask_id, dark_base_idx_id;
    public:
        MonitoredTable(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        void add_entry(string &prefix,
                        string &length,
                        uint32_t &base_idx,
                        uint32_t &mask,
                        uint32_t &dark_base_idx);
};

#endif // MONITOREDTABLE_H