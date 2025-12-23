#ifndef PORTMGR_H // Include guards to prevent multiple inclusion

#define PORTMGR_H

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

class PortManager {
    private:
        bf_status_t bf_status;
        // keep session, dev_tgt since we need it in many funcs
        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;

        const BfRtTable *port_manager;

        // for writing/reading data
        unique_ptr<BfRtTableKey> _key;
        unique_ptr<BfRtTableData> _data;

        // action/key/data ID
        bf_rt_id_t dev_port_id, speed_id, fec_id, port_enable_id, auto_neg_id;
        bf_rt_id_t register_index_id, register_value_id;
    public:
        PortManager(shared_ptr<BfRtSession> sess, bf_rt_target_t tgt, const BfRtInfo *bf_rt_info);

        void port_enable(const uint16_t &port, const string &speed);
};

#endif // PORTMGR_H