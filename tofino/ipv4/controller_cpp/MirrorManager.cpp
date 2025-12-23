#include "MirrorManager.h"

MirrorManager::MirrorManager(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info) {
    this->session = session;
    this->dev_tgt = dev_tgt;
    
    // get the mirror table
    bf_status = bf_rt_info->bfrtTableFromNameGet("$mirror.cfg", &mirror_cfg);
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    // key/data/action IDs
    bf_status = mirror_cfg->keyFieldIdGet("$sid", &sid_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->actionIdGet("$normal", &normal_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$direction", normal_id, &direction_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$session_enable", normal_id, &sess_en_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$ucast_egress_port", normal_id, &egress_port_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$ucast_egress_port_valid", normal_id, &egress_port_val_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$mcast_rid", normal_id, &mcast_rid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$mcast_grp_a", normal_id, &mcast_grp_a);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$mcast_grp_a_valid", normal_id, &mcast_grp_a_valid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$mcast_grp_b", normal_id, &mcast_grp_b);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$mcast_grp_b_valid", normal_id, &mcast_grp_b_valid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataFieldIdGet("$max_pkt_len", normal_id, &max_pkt_len);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // allocate key and data
    bf_status = mirror_cfg->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataAllocate(&_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void MirrorManager::add_mirror_port(uint16_t sid, uint16_t port) {
    // reset
    bf_status = mirror_cfg->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataReset(normal_id, _data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    // set values
    bf_status = _key->setValue(sid_id, (uint64_t) sid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(egress_port_id, (uint64_t) port);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(sess_en_id, true);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(egress_port_val_id, true);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(direction_id, (string) "BOTH");
    bf_sys_assert(bf_status == BF_SUCCESS);

    bf_status = mirror_cfg->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void MirrorManager::add_mirror_group(uint16_t sid, uint16_t grp_a, uint16_t grp_b, uint16_t pkt_len) {
    // reset
    bf_status = mirror_cfg->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = mirror_cfg->dataReset(normal_id, _data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set values
    bf_status = _key->setValue(sid_id, (uint64_t) sid);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(sess_en_id, true);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(mcast_grp_a, (uint64_t) grp_a);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(mcast_grp_a_valid, true);
    bf_status = _data->setValue(mcast_grp_b, (uint64_t) grp_b);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(mcast_grp_b_valid, true);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(direction_id, (string) "BOTH");
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(max_pkt_len, (uint64_t) pkt_len);
    bf_sys_assert(bf_status == BF_SUCCESS);

    bf_status = mirror_cfg->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}