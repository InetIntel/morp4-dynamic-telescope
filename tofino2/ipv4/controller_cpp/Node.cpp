#include "Node.h"

Node::Node(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info) {
    this->session = session;
    this->dev_tgt = dev_tgt;

    // get the table
    bf_status = bf_rt_info->bfrtTableFromNameGet("$pre.node", &node);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // get key/data IDs
    bf_status = node->keyFieldIdGet("$MULTICAST_NODE_ID", &node_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = node->dataFieldIdGet("$MULTICAST_RID", &multicast_rid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = node->dataFieldIdGet("$DEV_PORT", &dev_port);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // allocate key and data
    bf_status = node->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = node->dataAllocate(&_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void Node::add_node(uint16_t rid, uint16_t port) {
    // reset
    bf_status = node->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = node->dataReset(_data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set values
    bf_status = _key->setValue(node_id, (uint64_t) rid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(multicast_rid, (uint64_t) rid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    bf_status = _data->setValue(dev_port, (vector<bf_rt_id_t>) {port});
    bf_sys_assert(bf_status == BF_SUCCESS);

    bf_status = node->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}