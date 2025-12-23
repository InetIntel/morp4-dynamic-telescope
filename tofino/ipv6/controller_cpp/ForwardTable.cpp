#include "ForwardTable.h"

 ForwardTable::ForwardTable(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info){
    this->session = session;
    this->dev_tgt = dev_tgt;

    // get the table
    bf_status = bf_rt_info->bfrtTableFromNameGet("pipe.Ingress.forward", &forward_table);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // confirm it is a match direct table
    BfRtTable::TableType table_type;
    bf_status = forward_table->tableTypeGet(&table_type);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_sys_assert(table_type == BfRtTable::TableType::MATCH_DIRECT);
    
    // get key/data/action IDs
    bf_status = forward_table->keyFieldIdGet("ig_intr_md.ingress_port", &ingress_port_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = forward_table->actionIdGet("Ingress.set_port", &set_port_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = forward_table->dataFieldIdGet("port", set_port_id, &egress_port_id);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // allocate key and data
    bf_status = forward_table->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = forward_table->dataAllocate(&_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void ForwardTable::add_entry(const uint16_t &ingress_port, const uint16_t &egress_port){
    // reset
    bf_status = forward_table->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = forward_table->dataReset(set_port_id, _data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set values
    bf_status = _key->setValue(ingress_port_id, ingress_port);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(egress_port_id, (uint64_t) egress_port);
    bf_sys_assert(bf_status == BF_SUCCESS);

    bf_status = forward_table->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}