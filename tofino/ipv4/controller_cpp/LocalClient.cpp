#include "LocalClient.h"

string getCurrentDateTimeUTC() {
    time_t now = time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", gmtime(&now));
    return string(buf);
}

uint32_t IPv4ToInt(const std::string& ip) {
    uint32_t result = 0;
    stringstream ss(ip);
    string ip_byte;

    for (int i = 3; i >= 0; --i) {
        getline(ss, ip_byte, '.');
        result |= (stoi(ip_byte) << (i * 8));
    }
    return result;
}

string IntToIPv4(const uint32_t ip) {
    ostringstream oss;
    oss << ((ip >> 24) & 0xFF) << "."
        << ((ip >> 16) & 0xFF) << "."
        << ((ip >> 8) & 0xFF) << "."
        << (ip & 0xFF);
    return oss.str();
}

void generate_IPv4_addresses(const string& prefix, int length){
    ofstream file("prefixes.txt", ios::app);
    if (!file.is_open()) {
        cerr << "Error: Could not open file for writing.\n";
        exit(1);
    }

    uint64_t total_pfxes = 1ULL << (32 - length);
    uint32_t addr = IPv4ToInt(prefix);
    
    for (uint64_t i = 0; i < total_pfxes; i++) {
        file << IntToIPv4(addr) << "\n";
        addr += 1;
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
    max_pkt_rate = args->max_pkt_rate;
    avg_pkt_rate = args->avg_pkt_rate;
    max_byte_rate = args->max_byte_rate;
    avg_byte_rate = args->avg_byte_rate;
    alpha = args->alpha;
    monitored_path = args->monitored_path;
    ports["incoming"] = args->incoming; //{133};
    ports["outgoing"] = args->outgoing; //{132};

    counters = vector<uint16_t> (global_table_size, alpha);

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
        mask = pow(2, 32 - stoi(length)) - 1;

        monitored_table->add_entry(prefix, length, base_idx, mask, dark_base_idx);
        generate_IPv4_addresses(prefix, stoi(length));

        cout << "Prefix: " << prefix << " Length: " << length << endl;
        cout << "Mask " << mask << endl;
        base_idx += (uint64_t) mask + 1;
        dark_base_idx += pow(2, 24 - stoi(length));
    }
    addr_cnt = base_idx;
}

void LocalClient::add_ports(unordered_map<string, vector<uint16_t>> ports){
    for(auto port: ports["incoming"]){
        ports_table->add_entry(port, false);
    }
    for(auto port: ports["outgoing"]){
        ports_table->add_entry(port, true);
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
    if (inactive_addr == 0)
        return;
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

void LocalClient::set_forward(unordered_map<uint16_t, uint16_t> port_pairs){
    for(auto port_pair: port_pairs){
        forward_table->add_entry(port_pair.first, port_pair.second);
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
    
    global_table = new Register("pipe.Ingress.global_table", session, dev_tgt, bf_rt_info);
    flag_table = new Register("pipe.Ingress.flag_table", session, dev_tgt, bf_rt_info);

    dark_meter = new Meter("pipe.Ingress.dark_meter", session, dev_tgt, bf_rt_info);
    dark_global_meter = new Meter("pipe.Ingress.dark_global_meter", session, dev_tgt, bf_rt_info);

    vector<uint16_t> router_ports;

    cout<<"Setting mirroring\n";
    add_mirroring(router_ports, 1, 2, 43, 24);
    cout<<"Done with mirroring setup\n";
    monitored_prefixes = parse_monitored(monitored_path);
    cout << "Populating monitored IPv4\n";
    populate_monitored(monitored_prefixes);
    add_ports(ports);
    set_forward(port_pairs);
    set_rates();
}

void LocalClient::run(){
    while(true){
        auto start = chrono::steady_clock::now();

        unique_lock<mutex> flag_lock = flag_table->start_sync();
        flag_table->end_sync(flag_lock);

        cout << "[" << getCurrentDateTimeUTC() << "]: Sync done; Start of iteration\n";

        vector<vector<uint64_t>> flags = flag_table->get_entries(0, addr_cnt - 1);

        vector<uint32_t> global_indices;
        vector<uint32_t> flag_indices;
        vector<uint32_t> inactive_indices;
        uint32_t cur_active_addr_cnt = 0;
        uint32_t active_addr_cnt = 0;
        unordered_map<uint32_t, uint32_t> inactive_pfxs;
        uint32_t inactive_addr = 0;
        uint32_t mtr_idx = 0;

        for(uint32_t i = 0; i < addr_cnt; i++){
            vector<uint64_t> t_val = flags[i];
            if (find(t_val.begin(), t_val.end(), 1) != t_val.end()){
                cur_active_addr_cnt++;
                cout << "Flag " << to_string(i) << endl;
                if(counters[i] == 0){
                    global_indices.push_back(i);
                }
                flag_indices.push_back(i);
                counters[i] = alpha + 1;
                active_addr_cnt++;
            }
            else {
                if(counters[i] > 1){
                    counters[i]--;
                    active_addr_cnt++;
                    cout << "Global " << to_string(i) << endl;
                }
                else{
                    inactive_addr++;
                    mtr_idx = i / 256;
                    if (inactive_pfxs.find(mtr_idx) == inactive_pfxs.end()){
                        inactive_pfxs[mtr_idx] = 0;
                    }
                    inactive_pfxs[mtr_idx]++;
                    
                    if (counters[i] == 1){
                        inactive_indices.push_back(i);
                        counters[i] = 0;
                    }
                }
            }
        }
        cout << "Cur active addr: " << cur_active_addr_cnt << endl;
        cout << "Active addr: " << active_addr_cnt << " out of " << addr_cnt << endl;

        auto stop = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);

        cout << "[" << getCurrentDateTimeUTC() << "]: Time taken by iteration: " << duration.count() / 1000000 << " seconds" << endl;

        cout << "Size of global to active: " << global_indices.size() << endl;
        global_table->add_entries(global_indices, 1);
        cout << "Written global_indices \n";
        
        cout << "Size of global to inactive: " << inactive_indices.size() << endl;
        global_table->add_entries(inactive_indices, 0);
        cout << "Written inactive_indices \n";
        
        cout << "Size of flags: " << flag_indices.size() << endl;
        flag_table->add_entries(flag_indices, 0);
        cout << "End of writing\n";

        update_rates(inactive_pfxs, inactive_addr);
        cout << "Finished rates\n";

        auto final_stop = chrono::steady_clock::now();
        auto final_duration = chrono::duration_cast<chrono::microseconds>(final_stop - start);

        cout << "[" << getCurrentDateTimeUTC() << "]: Time taken by function: " << final_duration.count() / 1000000 << " seconds" << endl;

        this_thread::sleep_for(chrono::milliseconds(time_interval * 1000) -
                                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start));
    }
}