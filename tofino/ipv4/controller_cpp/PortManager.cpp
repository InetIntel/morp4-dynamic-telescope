#include "PortManager.h"

PortManager::PortManager(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info){
    this->session = session;
    this->dev_tgt = dev_tgt;

    // get the table
    bf_status = bf_rt_info->bfrtTableFromNameGet("$PORT", &port_manager);
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    // key/data IDs
    bf_status = port_manager->keyFieldIdGet("$DEV_PORT", &dev_port_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = port_manager->dataFieldIdGet("$SPEED", &speed_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = port_manager->dataFieldIdGet("$FEC", &fec_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = port_manager->dataFieldIdGet("$PORT_ENABLE", &port_enable_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = port_manager->dataFieldIdGet( "$AUTO_NEGOTIATION", &auto_neg_id);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // allocate key and data
    bf_status = port_manager->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = port_manager->dataAllocate(&_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

// Enable port and set speed
void PortManager::port_enable(const uint16_t &port, const string &speed) {
    // reset
    bf_status = port_manager->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = port_manager->dataReset(_data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set values
    bf_status = _key->setValue(dev_port_id, (uint64_t) port);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(port_enable_id, true);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(speed_id, speed);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(fec_id, (string) "BF_FEC_TYP_NONE");
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    bf_status = port_manager->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}