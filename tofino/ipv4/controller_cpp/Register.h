#ifndef REGISTER_H // Include guards to prevent multiple inclusion

#define REGISTER_H

#include <mutex>
#include <condition_variable>

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

// struct to use as cookie to make sure that sync is completed
// before reading from sw
struct RegisterSync {
    mutex register_sync_lock;
    condition_variable register_sync_completed;
};

class Register {
    private:
        bf_status_t bf_status;
        // keep session, dev_tgt since we need it in many funcs
        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;
        
        // register info
        const BfRtTable *register_table;

        // for syncing register
        BfRtTable::BfRtTableGetFlag _flag;
        struct RegisterSync cookie;
        unique_ptr<BfRtTableOperations> table_ops;
        const TableOperationsType op_type;

        // for writing/reading data
        unique_ptr<BfRtTableKey> _key;
        unique_ptr<BfRtTableData> _data;
        bf_rt_id_t _register_index_id, _register_value_id;
        bf_rt_id_t _f1_id;
    public:
        Register(const string &name, shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        vector<vector<uint64_t>> get_entries(const uint32_t start_idx, const uint32_t end_idx);

        void add_entries(vector<uint32_t> keys, int value);

        static void sync_callback(const bf_rt_target_t &, void *cookie);

        unique_lock<mutex> start_sync();

        void end_sync(unique_lock<mutex> &lck);
};

#endif