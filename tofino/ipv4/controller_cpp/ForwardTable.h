#ifndef FORWARDTABLE_H // Include guards to prevent multiple inclusion

#define FORWARDTABLE_H

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

class ForwardTable {
    private:
        bf_status_t bf_status;
        // keep session, dev_tgt since we need it in many funcs
        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;

        const BfRtTable *forward_table;

        // for writing/reading data
        unique_ptr<BfRtTableKey> _key;
        unique_ptr<BfRtTableData> _data;
        // action/key/data ID
        bf_rt_id_t set_port_id, ingress_port_id, egress_port_id;
    public:
        ForwardTable(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        void add_entry(const uint16_t &ingress_port,
                        const uint16_t &egress_port);
};

#endif // FORWARDTABLE_H