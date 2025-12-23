#include "Meter.h"

Meter::Meter(const string &name, shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info){
    this->session = session;
    this->dev_tgt = dev_tgt;

    // get the meter table
    bf_status = bf_rt_info->bfrtTableFromNameGet(name, &meter_table);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // confirm it is a meter
    BfRtTable::TableType table_type;
    bf_status = meter_table->tableTypeGet(&table_type);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_sys_assert(table_type == BfRtTable::TableType::METER);

    // get key/data IDs
    bf_status = meter_table->keyFieldIdGet("$METER_INDEX", &meter_index_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = meter_table->dataFieldIdGet("$METER_SPEC_CIR_PPS", &cir_pps_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = meter_table->dataFieldIdGet("$METER_SPEC_PIR_PPS", &pir_pps_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = meter_table->dataFieldIdGet("$METER_SPEC_CBS_PKTS", &cbs_pkts_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = meter_table->dataFieldIdGet("$METER_SPEC_PBS_PKTS", &pbs_pkts_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    // allocate key and data
    bf_status = meter_table->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = meter_table->dataAllocate(&_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void Meter::add_entry(const uint32_t &avg_pkt_rate, const uint32_t &max_pkt_rate, const uint32_t &idx){
    // reset
    bf_status = meter_table->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = meter_table->dataReset(_data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set values
    bf_status = _key->setValue(meter_index_id, idx);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(cir_pps_id, (uint64_t) avg_pkt_rate);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(pir_pps_id, (uint64_t) max_pkt_rate);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(cbs_pkts_id, (uint64_t) 100);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(pbs_pkts_id, (uint64_t) 100);
    bf_sys_assert(bf_status == BF_SUCCESS);

    bf_status = meter_table->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}