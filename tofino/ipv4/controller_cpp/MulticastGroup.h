#ifndef MULTICASTGROUP_H // Include guards to prevent multiple inclusion

#define MULTICASTGROUP_H

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

class MulticastGroup{
    private:
        bf_status_t bf_status;
        // keep session, dev_tgt since we need it in many funcs
        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;

        const BfRtTable *group;

        // for writing/reading data
        unique_ptr<BfRtTableKey> _key;
        unique_ptr<BfRtTableData> _data;
        // key/data ID
        bf_rt_id_t mgid;
        bf_rt_id_t mcast_node_id, mcast_node_xid_valid, mcast_node_xid;
    public:
        MulticastGroup(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        void add_group(uint16_t group_id, vector<uint16_t> rids);
};

#endif // MULTICASTGROUP_H