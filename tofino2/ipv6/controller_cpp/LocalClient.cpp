#include "LocalClient.h"

string getCurrentDateTimeUTC() {
    time_t now = time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", gmtime(&now));
    return string(buf);
}

void IPv6ToBytes(const std::string& ip, uint8_t* bytes) {
    memset(bytes, 0, 16);
    istringstream ss(ip);
    string part;
    int index = 0;
    uint16_t value;

    while (getline(ss, part, ':')) {
        if (part.empty()) {
            // Handle "::" by skipping to the appropriate position
            index += (8 - std::count_if(istream_iterator<string>(ss),
                                        istream_iterator<string>(),
                                        [](const string&) { return true; })) * 2;
            continue;
        }
        value = stoi(part, nullptr, 16);
        bytes[index++] = (value >> 8) & 0xFF; // High byte
        bytes[index++] = value & 0xFF;       // Low byte
    }
}

string bytesToIPv6(const uint8_t* bytes) {
    ostringstream oss;
    uint16_t segment;
    for (int i = 0; i < 16; i += 2) {
        if (i > 0) oss << ":";
        segment = (bytes[i] << 8) | bytes[i + 1];
        oss << hex << segment;
    }
    return oss.str();
}

void generate_IPv6_addresses(const string& prefix, int length){
    ofstream file("prefixes.txt", ios::app);
    if (!file.is_open()) {
        cerr << "Error: Could not open file for writing.\n";
        exit(1);
    }

    uint64_t total_pfxes = 1ULL << (53 - length);
    uint8_t base_addr[16] = {0};
    uint8_t addr[16];

    IPv6ToBytes(prefix, base_addr);
    
    for (uint64_t i = 0; i < total_pfxes; i++) {
        memcpy(addr, base_addr, 16);
        uint64_t tmp_i = i << 3;
        for (int j = 6; j >= 0; --j) {
            addr[j] += tmp_i & 0xFF;
            tmp_i >>= 8;
        }
        file << bytesToIPv6(addr) << "\n";
    }
    file.close();
}

LocalClient::LocalClient(Args* args, shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info) {
    this->session = session;
    this->dev_tgt = dev_tgt;
    this->bf_rt_info = bf_rt_info;

    // parse args
    time_interval = args->time_interval;
    global_table_size = args->global_table_size;
    dark_meter_size = args->dark_meter_size;
    alpha = args->alpha;
    max_pkt_rate = args->max_pkt_rate;
    avg_pkt_rate = args->avg_pkt_rate;
    monitored_path = args->monitored_path;
    ports["incoming"] = args->incoming; //{133};
    ports["outgoing"] = args->outgoing; //{132};

    counters = vector<uint16_t> (global_table_size*8, alpha);

    cout << "outgoing size " << ports["outgoing"].size() << endl;
    cout << "incoming size " << ports["incoming"].size() << endl;
    cout << "alpha: " + to_string(alpha) << endl;

    // track total number of monitored addresses
    addr_cnt = 0;

    setup();
}

void LocalClient::add_mirroring(vector<uint16_t> router_ports, uint16_t mc_session_id, uint16_t log_session_id, uint16_t pkt_len, uint16_t log_port){
    /* border routers*/
    uint16_t rid = 1;
    vector<uint16_t> rids;

    for(auto val: router_ports){
        for(int i = 0; i < 3; i++){ // 3 pkts per router
            cout << "Adding node: " << rid << " with port: " << val << endl;
            node->add_node(rid, val);
            rids.push_back(rid);
            rid++;
        }
    }

    mc_group->add_group(1, rids);

    /* recirculation nodes */
    vector<uint16_t> rec_ports;
    for(int i = 0; i < NUM_PIPES; i++){
        rec_ports.push_back(RECIRCULATE_PORT + 128*i);
    }

    rids.clear();
    for(auto val: rec_ports){
        node->add_node(rid, val);
        rids.push_back(rid);
        rid++;
    }

    mc_group->add_group(2, rids);
    mirror->add_mirror_group(mc_session_id, 1, 2, pkt_len);
    mirror->add_mirror_port(log_session_id, log_port);
}

vector<string> LocalClient::parse_monitored(string path){
    vector<string> monitored_pfx;

    ifstream file(path);
    if(file.is_open()){
        string prefix;
        while(getline(file, prefix)){
            monitored_pfx.push_back(prefix);
        }
    } else{
        printf("Error in opening monitored prefixes file");
        exit(1);
    }
    file.close();
    return monitored_pfx;
}

void LocalClient::populate_monitored(vector<string> entries){
    static uint32_t base_idx = 0;
    static uint32_t dark_base_idx = 0;
    uint32_t mask;
    string prefix, length;
    size_t pos;

    for(string entry: entries){
        pos = entry.find('/');
        if (pos != string::npos) {
            prefix = entry.substr(0, pos);
            length = entry.substr(pos + 1);
        }
        else {
            continue;
        }
        
        mask = (1ULL << (53 - stoi(length))) - 1;
        
        monitored_table->add_entry(prefix, length, base_idx, mask);
        generate_IPv6_addresses(prefix, stoi(length));

        cout << "Prefix: " << prefix << " Length: " << length << endl;
        cout << "Mask " << mask << endl;
        base_idx += (mask + 1);
        addr_cnt += (mask + 1) * 8;
        
        dark_base_idx += pow(2, 46 - stoi(length));
    }
}

void LocalClient::add_ports(unordered_map<string, vector<uint16_t>> ports){
    for(auto port: ports["incoming"]){
        ports_table->add_entry(port, false);
    }
    for(auto port: ports["outgoing"]){
        ports_table->add_entry(port, true);
    }
}

void LocalClient::set_forward(unordered_map<uint16_t, uint16_t> port_pairs){
    for(auto port_pair: port_pairs){
        forward_table->add_entry(port_pair.first, port_pair.second);
    }
}

void LocalClient::set_rates(){
    dark_global_meter->add_entry(avg_pkt_rate, max_pkt_rate, 0);
    // initially it's fine to have all meters with the same rate; they will be updated accordingly later
    for(uint32_t i = 0; i < dark_meter_size; i++){
        dark_meter->add_entry(avg_pkt_rate, max_pkt_rate, i);
    }
}

void LocalClient::update_rates(unordered_map<uint32_t, uint32_t> inactive_pfxs, uint32_t inactive_addr){
    if (inactive_addr == 0) {
        return;
    }
    uint32_t addr_avg_pkt_rate = ceil(avg_pkt_rate / (double) inactive_addr);
    uint32_t addr_max_pkt_rate = ceil(max_pkt_rate / (double) inactive_addr);
    uint32_t prefix_max_pkt_rate, prefix_avg_pkt_rate;

    cout << avg_pkt_rate << " " << inactive_addr << " " << addr_avg_pkt_rate << endl;
    cout << max_pkt_rate << " " << inactive_addr << " " << addr_max_pkt_rate << endl;

    for(auto& [mtr_idx, in_addr]: inactive_pfxs){
        prefix_max_pkt_rate = ceil(addr_max_pkt_rate * in_addr);
        prefix_avg_pkt_rate = ceil(addr_avg_pkt_rate * in_addr);
        
        dark_meter->add_entry(prefix_avg_pkt_rate, prefix_max_pkt_rate, mtr_idx);
    }
}

void LocalClient::setup(){
    // enable switch ports
    port_mgr = new PortManager(session, dev_tgt, bf_rt_info);
    port_mgr->port_enable(164, "BF_SPEED_100G");
    port_mgr->port_enable(172, "BF_SPEED_100G");
    port_mgr->port_enable(180, "BF_SPEED_100G");
    port_mgr->port_enable(188, "BF_SPEED_100G");
    port_mgr->port_enable(56, "BF_SPEED_100G");
    port_mgr->port_enable(48, "BF_SPEED_100G");
    port_mgr->port_enable(40, "BF_SPEED_100G");
    port_mgr->port_enable(32, "BF_SPEED_100G");
    port_mgr->port_enable(24, "BF_SPEED_100G");

    // get objects
    ports_table = new PortsTable(session, dev_tgt, bf_rt_info);
    monitored_table = new MonitoredTable(session, dev_tgt, bf_rt_info);
    forward_table = new ForwardTable(session, dev_tgt, bf_rt_info);
    node = new Node(session, dev_tgt, bf_rt_info);
    mc_group = new MulticastGroup(session, dev_tgt, bf_rt_info);
    mirror = new MirrorManager(session, dev_tgt, bf_rt_info);
    global_table0 = new Register("pipe.Ingress.global_table0", session, dev_tgt, bf_rt_info);
    global_table1 = new Register("pipe.Ingress.global_table1", session, dev_tgt, bf_rt_info);
    global_table2 = new Register("pipe.Ingress.global_table2", session, dev_tgt, bf_rt_info);
    global_table3 = new Register("pipe.Ingress.global_table3", session, dev_tgt, bf_rt_info);
    global_table4 = new Register("pipe.Ingress.global_table4", session, dev_tgt, bf_rt_info);
    global_table5 = new Register("pipe.Ingress.global_table5", session, dev_tgt, bf_rt_info);
    global_table6 = new Register("pipe.Ingress.global_table6", session, dev_tgt, bf_rt_info);
    global_table7 = new Register("pipe.Ingress.global_table7", session, dev_tgt, bf_rt_info);
    global_tables.push_back(global_table0);
    global_tables.push_back(global_table1);
    global_tables.push_back(global_table2);
    global_tables.push_back(global_table3);
    global_tables.push_back(global_table4);
    global_tables.push_back(global_table5);
    global_tables.push_back(global_table6);
    global_tables.push_back(global_table7);
    flag_table0 = new Register("pipe.Ingress.flag_table0", session, dev_tgt, bf_rt_info);
    flag_table1 = new Register("pipe.Ingress.flag_table1", session, dev_tgt, bf_rt_info);
    flag_table2 = new Register("pipe.Ingress.flag_table2", session, dev_tgt, bf_rt_info);
    flag_table3 = new Register("pipe.Ingress.flag_table3", session, dev_tgt, bf_rt_info);
    flag_table4 = new Register("pipe.Ingress.flag_table4", session, dev_tgt, bf_rt_info);
    flag_table5 = new Register("pipe.Ingress.flag_table5", session, dev_tgt, bf_rt_info);
    flag_table6 = new Register("pipe.Ingress.flag_table6", session, dev_tgt, bf_rt_info);
    flag_table7 = new Register("pipe.Ingress.flag_table7", session, dev_tgt, bf_rt_info);
    flag_tables.push_back(flag_table0);
    flag_tables.push_back(flag_table1);
    flag_tables.push_back(flag_table2);
    flag_tables.push_back(flag_table3);
    flag_tables.push_back(flag_table4);
    flag_tables.push_back(flag_table5);
    flag_tables.push_back(flag_table6);
    flag_tables.push_back(flag_table7);

    dark_meter = new Meter("pipe.Ingress.dark_meter", session, dev_tgt, bf_rt_info);
    dark_global_meter = new Meter("pipe.Ingress.dark_global_meter", session, dev_tgt, bf_rt_info);

    vector<uint16_t> router_ports;

    cout<<"Setting mirroring\n";
    add_mirroring(router_ports, 1, 2, 73, 16);
    cout<<"Done with mirroring setup\n";
    monitored_prefixes = parse_monitored(monitored_path);
    cout << "Populating monitored IPv6\n";
    populate_monitored(monitored_prefixes);
    add_ports(ports);
    set_rates();

    set_forward(port_pairs);
}

void LocalClient::run(){
    while(true){
        auto start = chrono::steady_clock::now();

        cout << "[" << getCurrentDateTimeUTC() << "]: Start of iteration\n";

        uint32_t cur_active_addr_cnt = 0;
        uint32_t active_addr_cnt = 0;
        uint32_t inactive_addr = 0;
        unordered_map<uint32_t, uint32_t> inactive_pfxs;
        
        for (int x = 0; x < 8; x++){
            unique_lock<mutex> flag_lock = flag_tables[x]->start_sync();
            flag_tables[x]->end_sync(flag_lock);

            vector<vector<uint64_t>> flags = flag_tables[x]->get_entries(0, addr_cnt / 8 - 1);

            vector<uint32_t> global_indices;
            vector<uint32_t> flag_indices;
            vector<uint32_t> inactive_indices;

            for(uint32_t i = 0; i < addr_cnt / 8; i++){
                vector<uint64_t> t_val = flags[i];
                uint32_t actual_idx = 8*i + x;
                if (find(t_val.begin(), t_val.end(), 1) != t_val.end()){
                    cur_active_addr_cnt++;
                    cout << "Flag " << to_string(actual_idx) << endl;
                    if(counters[actual_idx] == 0){
                        global_indices.push_back(i);
                    }
                    flag_indices.push_back(i);
                    counters[actual_idx] = alpha + 1;
                    active_addr_cnt++;
                }
                else {
                    if(counters[actual_idx] > 1){
                        counters[actual_idx]--;
                        active_addr_cnt++;
                        cout << "Global " << to_string(actual_idx) << endl;
                    }
                    else if (counters[actual_idx] == 1){
                        inactive_indices.push_back(i);
                        counters[actual_idx] = 0;
                        inactive_addr++;
                        uint32_t mtr_idx = actual_idx / 1024;
                        if (inactive_pfxs.find(mtr_idx) == inactive_pfxs.end()) {
                            inactive_pfxs[mtr_idx] = 0;
                        }
                        inactive_pfxs[mtr_idx]++;
                    }
                }
            }
            cout << "Start writing\n";
            cout << "Size of global to active: " << global_indices.size() << endl;
            global_tables[x]->add_entries(global_indices, 1);
            cout << "Written global_indices \n";

            cout << "Size of global to inactive: " << inactive_indices.size() << endl;
            global_tables[x]->add_entries(inactive_indices, 0);
            cout << "Written inactive_indices \n";

            cout << "Size of flags: " << flag_indices.size() << endl;
            flag_tables[x]->add_entries(flag_indices, 0);
            cout << "End of writing\n";
        }

        cout << "Cur active addr: " << cur_active_addr_cnt << endl;
        cout << "Active addr: " << active_addr_cnt << " out of " << addr_cnt << endl;

        update_rates(inactive_pfxs, inactive_addr);

        auto final_stop = chrono::steady_clock::now();
        auto final_duration = chrono::duration_cast<chrono::microseconds>(final_stop - start);

        cout << "[" << getCurrentDateTimeUTC() << "]: Time taken by function: " << final_duration.count() / 1000000 << " seconds" << endl;

        cout << "Waiting for " + to_string(time_interval) + " seconds...\n";
        this_thread::sleep_for(chrono::milliseconds(time_interval * 1000) -
                                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start));
    }
}