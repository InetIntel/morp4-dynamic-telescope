#ifndef NODE_H // Include guards to prevent multiple inclusion

#define NODE_H

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

class Node {
    private:
        bf_status_t bf_status;
        // keep session, dev_tgt since we need it in many funcs
        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;

        const BfRtTable *node;

        // for writing/reading data
        unique_ptr<BfRtTableKey> _key;
        unique_ptr<BfRtTableData> _data;

        // action/key/data ID
        bf_rt_id_t node_id;
        bf_rt_id_t multicast_rid, dev_port;
        bf_rt_id_t mcast_rid, mcast_grp_a, mcast_grp_a_valid, mcast_grp_b, mcast_grp_b_valid, max_pkt_len;
    public:
        Node(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        void add_node(uint16_t rid, uint16_t port);
};

#endif // NODE_H