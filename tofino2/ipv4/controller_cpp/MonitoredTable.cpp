#include "MonitoredTable.h"

uint32_t ipv4_to_bytes(const char* ipv4, uint8_t* pref_len) {
    uint8_t byte0, byte1, byte2, byte3;
    if (pref_len == nullptr) {
        sscanf(ipv4, "%hhu.%hhu.%hhu.%hhu", &byte3, &byte2, &byte1, &byte0);
    }
    else {
        sscanf(ipv4, "%hhu.%hhu.%hhu.%hhu/%hhu", &byte3, &byte2, &byte1, &byte0, pref_len);
    }
    return ((uint32_t)byte3 << 24) + ((uint32_t)byte2 << 16) + ((uint32_t)byte1 << 8) + (uint32_t)byte0;
}

 MonitoredTable::MonitoredTable(shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info){
    this->session = session;
    this->dev_tgt = dev_tgt;

    // get the table
    bf_status = bf_rt_info->bfrtTableFromNameGet("pipe.Ingress.monitored", &monitored_table);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // confirm it is a match direct table
    BfRtTable::TableType table_type;
    bf_status = monitored_table->tableTypeGet(&table_type);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_sys_assert(table_type == BfRtTable::TableType::MATCH_DIRECT);

    // get key/data/action IDs
    bf_status = monitored_table->keyFieldIdGet("meta.addr", &meta_addr_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = monitored_table->actionIdGet("Ingress.calc_idx", &calc_idx_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = monitored_table->dataFieldIdGet("base_idx", calc_idx_id, &base_idx_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = monitored_table->dataFieldIdGet("mask", calc_idx_id, &mask_id);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = monitored_table->dataFieldIdGet("dark_base_idx", calc_idx_id, &dark_base_idx_id);
    bf_sys_assert(bf_status == BF_SUCCESS);

    // allocate key and data
    bf_status = monitored_table->keyAllocate(&_key);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = monitored_table->dataAllocate(&_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

void MonitoredTable::add_entry(string &prefix, string &length, uint32_t &base_idx,
                uint32_t &mask, uint32_t &dark_base_idx){
    // reset
    bf_status = monitored_table->keyReset(_key.get());
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = monitored_table->dataReset(calc_idx_id, _data.get());
    bf_sys_assert(bf_status == BF_SUCCESS);

    // set values
    uint8_t fixed_length = (uint8_t) stoi(length);
    uint32_t fixed_prefix = ipv4_to_bytes(prefix.c_str(), &fixed_length);

    bf_status = _key->setValueLpm(meta_addr_id, (uint64_t) fixed_prefix, (uint16_t) fixed_length);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(base_idx_id, (uint64_t) base_idx);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(mask_id, (uint64_t) mask);
    bf_sys_assert(bf_status == BF_SUCCESS);
    bf_status = _data->setValue(dark_base_idx_id, (uint64_t) dark_base_idx);
    bf_sys_assert(bf_status == BF_SUCCESS);

    bf_status = monitored_table->tableEntryAdd(*session, dev_tgt, *_key, *_data);
    bf_sys_assert(bf_status == BF_SUCCESS);
}