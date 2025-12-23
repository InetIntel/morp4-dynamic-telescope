#include "MulticastGroup.h"

MulticastGroup::MulticastGroup(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info) {
    this->session = session;
    this->dev_tgt = dev_tgt;
    
    // get the table
    bf_status = bf_rt_info->bfrtTableFromNameGet("$pre.mgid", &group);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // get key/data IDs
    bf_status = group->keyFieldIdGet("$MGID", &mgid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = group->dataFieldIdGet("$MULTICAST_NODE_ID", &mcast_node_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = group->dataFieldIdGet("$MULTICAST_NODE_L1_XID_VALID", &mcast_node_xid_valid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = group->dataFieldIdGet("$MULTICAST_NODE_L1_XID", &mcast_node_xid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    // allocate key and data
    bf_status = group->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = group->dataAllocate(&_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void MulticastGroup::add_group(uint16_t group_id, vector<uint16_t> rids) {
    // reset
    bf_status = group->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = group->dataReset(_data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set key and data
    bf_status = _key->setValue(mgid, (uint64_t) group_id);
    bf_sys_assert(bf_status == BF_SUCCESS);

    vector<bf_rt_id_t> v_ids(rids.begin(), rids.end());
    vector<bf_rt_id_t> xids(rids.size(), 0);
    vector<bool> xids_valid(rids.size(), false);

    bf_status = _data->setValue(mcast_node_id, v_ids);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(mcast_node_xid, xids);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(mcast_node_xid_valid, xids_valid);
    bf_sys_assert(bf_status == BF_SUCCESS);

    bf_status = group->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}