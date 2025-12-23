#ifndef MIRRORMGR_H // Include guards to prevent multiple inclusion

#define MIRRORMGR_H

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

class MirrorManager {
    private:
        bf_status_t bf_status;
        // keep session, dev_tgt since we need it in many funcs
        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;

        const BfRtTable *mirror_cfg;

        // for writing/reading data
        unique_ptr<BfRtTableKey> _key;
        unique_ptr<BfRtTableData> _data;
        // action/key/data ID
        bf_rt_id_t sid_id, normal_id;
        bf_rt_id_t direction_id, sess_en_id, egress_port_id, egress_port_val_id;
        bf_rt_id_t mcast_rid, mcast_grp_a, mcast_grp_a_valid, mcast_grp_b, mcast_grp_b_valid, max_pkt_len;
    public:
        MirrorManager(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        void add_mirror_port(uint16_t sid, uint16_t port);

        void add_mirror_group(uint16_t sid, uint16_t grp_a, uint16_t grp_b, uint16_t pkt_len);
};

#endif