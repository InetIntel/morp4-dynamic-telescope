#include "PortsTable.h"

PortsTable::PortsTable(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info){
    this->session = session;
    this->dev_tgt = dev_tgt;

    // get the ports table
    bf_status = bf_rt_info->bfrtTableFromNameGet("pipe.Ingress.ports", &ports_table);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // confirm it is a match direct table
    BfRtTable::TableType table_type;
    bf_status = ports_table->tableTypeGet(&table_type);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_sys_assert(table_type == BfRtTable::TableType::MATCH_DIRECT);

    // get key/action IDs
    bf_status = ports_table->keyFieldIdGet("ig_intr_md.ingress_port", &ingress_port_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = ports_table->actionIdGet("Ingress.set_incoming", &incoming_action_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = ports_table->actionIdGet("Ingress.set_outgoing", &outgoing_action_id);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // allocate key and data
    bf_status = ports_table->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = ports_table->dataAllocate(&_incoming_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = ports_table->dataAllocate(&_outgoing_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void PortsTable::add_entry(const uint16_t &port, bool direction){
    // reset
    bf_status = ports_table->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = ports_table->dataReset(incoming_action_id, _incoming_data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = ports_table->dataReset(outgoing_action_id, _outgoing_data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set values
    bf_status = _key->setValue(ingress_port_id, port);
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    if (!direction){
        bf_status = ports_table->tableEntryAdd(*session, dev_tgt, *_key, *_incoming_data);
    }
    else{
        bf_status = ports_table->tableEntryAdd(*session, dev_tgt, *_key, *_outgoing_data);
    }
    
    bf_sys_assert(bf_status == BF_SUCCESS);
}