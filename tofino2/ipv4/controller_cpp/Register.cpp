#include "Register.h"

Register::Register(const string &name, shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info)
        : _flag(BfRtTable::BfRtTableGetFlag::GET_FROM_SW), op_type(TableOperationsType::REGISTER_SYNC) {
            this->session = session;
            this->dev_tgt = dev_tgt;
            
            // get the register table
            bf_status = bf_rt_info->bfrtTableFromNameGet(name, &register_table);
            bf_sys_assert(bf_status == BF_SUCCESS);
            
            // confirm it is a register
            BfRtTable::TableType table_type;
            bf_status = register_table->tableTypeGet(&table_type);
            bf_sys_assert(bf_status == BF_SUCCESS);
            bf_sys_assert(table_type == BfRtTable::TableType::REGISTER);
            
            // allocate register operations
            bf_status = register_table->operationsAllocate(op_type, &table_ops);
            bf_sys_assert(bf_status == BF_SUCCESS);
            bf_status = table_ops->registerSyncSet(*session, dev_tgt, Register::sync_callback, &cookie);
            bf_sys_assert(bf_status == BF_SUCCESS);
            
            // get key/data IDs
            bf_status = register_table->keyFieldIdGet("$REGISTER_INDEX", &_register_index_id);
            bf_sys_assert(bf_status == BF_SUCCESS);
            string data_field_name = name.substr(name.find_first_of(".") + 1) + ".f1";
            bf_status = register_table->dataFieldIdGet(data_field_name, &_f1_id);
            bf_sys_assert(bf_status == BF_SUCCESS);

            // allocate key and data
            bf_status = register_table->keyAllocate(&_key);
            bf_sys_assert(bf_status == BF_SUCCESS); 
            bf_status = register_table->dataAllocate(&_data);
            bf_sys_assert(bf_status == BF_SUCCESS);
        }

// Sync callback function
void Register::sync_callback(const bf_rt_target_t &, void *cookie) {
    struct RegisterSync* local_cookie = (struct RegisterSync*) cookie;

    unique_lock<mutex> lck(local_cookie->register_sync_lock);
    local_cookie->register_sync_completed.notify_all();
}

// start syncing the register
unique_lock<mutex> Register::start_sync() {
    unique_lock<mutex> lck(cookie.register_sync_lock);

    // execute sync operations
    bf_status = register_table->tableOperationsExecute(*table_ops);
    bf_sys_assert(bf_status == BF_SUCCESS);

    return lck;
}

// wait for syncing to complete
void Register::end_sync(unique_lock<mutex> &lck) {
    cookie.register_sync_completed.wait(lck);
    lck.unlock();
}

vector<vector<uint64_t>> Register::get_entries(const uint32_t start_idx,
                                    const uint32_t end_idx) {
    vector<vector<uint64_t>> output;
    output.reserve(end_idx - start_idx);

    for(uint32_t index = start_idx; index < end_idx + 1; index++){
        // reset
        bf_status = register_table->keyReset(_key.get());
        bf_sys_assert(bf_status == BF_SUCCESS);
        bf_status = register_table->dataReset(_data.get());
        bf_sys_assert(bf_status == BF_SUCCESS);

        // set value
        bf_status = _key->setValue(_register_index_id, index);
        bf_sys_assert(bf_status == BF_SUCCESS);
        
        bf_status = register_table->tableEntryGet(*session, dev_tgt, *_key, _flag, _data.get());
        bf_sys_assert(bf_status == BF_SUCCESS);
        
        vector<uint64_t> temp_val;
        bf_status = _data->getValue(_f1_id, &temp_val);
        bf_sys_assert(bf_status == BF_SUCCESS);
        output.push_back(temp_val);
    }
    
    return output;
}

void Register::add_entries(vector<uint32_t> keys, int value){
    // begin batch
    bf_status = session->beginBatch();
    bf_sys_assert(bf_status == BF_SUCCESS);

    for(auto index: keys){
        // reset key and data
        bf_status = register_table->keyReset(_key.get());
        bf_sys_assert(bf_status == BF_SUCCESS);
        bf_status = register_table->dataReset(_data.get());
        bf_sys_assert(bf_status == BF_SUCCESS);
        
        bf_status = _key->setValue(_register_index_id, index);
        bf_sys_assert(bf_status == BF_SUCCESS);
        bf_status = _data->setValue(_f1_id, (uint64_t) value);
        bf_sys_assert(bf_status == BF_SUCCESS);

        bf_status = register_table->tableEntryAdd(*session, dev_tgt, *_key, *_data);
        bf_sys_assert(bf_status == BF_SUCCESS);
    }

    // end batch
    bf_status = session->endBatch(true);
    bf_sys_assert(bf_status == BF_SUCCESS);
}

