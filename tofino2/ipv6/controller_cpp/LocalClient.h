#ifndef LOCALCLIENT_H // Include guards to prevent multiple inclusion

#define LOCALCLIENT_H

#include <cmath>
#include <chrono>
#include <thread> 
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <stdio.h>

#include <bf_rt/bf_rt.hpp>
#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_learn.hpp>
#include <bf_rt/bf_rt_session.hpp>
#include <bf_rt/bf_rt_table_attributes.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table.hpp>
#include <bf_rt/bf_rt_table_key.hpp>
#include <bf_rt/bf_rt_table_operations.hpp>

#include "Register.h"
#include "MonitoredTable.h"
#include "ForwardTable.h"
#include "PortsTable.h"
#include "PortManager.h"
#include "Node.h"
#include "Meter.h"
#include "MulticastGroup.h"
#include "MirrorManager.h"

#define NUM_PIPES 2
#define RECIRCULATE_PORT 6

using namespace std;
using namespace bfrt;

struct Args {
    uint16_t time_interval = 100;
    uint32_t global_table_size = 2097152;
    uint32_t dark_meter_size = 16384;
    uint16_t alpha = 216;
    uint32_t max_pkt_rate = 1174405;
    uint32_t avg_pkt_rate = 343933;
    string monitored_path = "monitored.txt";
    vector<uint16_t> outgoing = {8};
    vector<uint16_t> incoming = {9};
};

class LocalClient{
    public:
        uint32_t global_table_size;
        uint32_t dark_meter_size;
        string monitored_path;
        vector<string> monitored_prefixes;
        uint32_t addr_cnt;
        unordered_map<uint32_t, uint32_t> dark_prefix_index_mapping;

        vector<uint16_t> counters;
        uint16_t alpha;
        uint16_t time_interval;
        uint32_t max_pkt_rate;
        uint32_t avg_pkt_rate;

        unordered_map<string, vector<uint16_t>> ports;
        unordered_map<uint16_t, uint16_t> port_pairs;

        shared_ptr<BfRtSession> session;
        bf_rt_target_t dev_tgt;
        const BfRtInfo *bf_rt_info;
        PortManager *port_mgr;
        Node *node;
        MulticastGroup *mc_group;
        MirrorManager *mirror;
        PortsTable *ports_table;
        MonitoredTable *monitored_table;
        ForwardTable *forward_table;
        Register *global_table0, *global_table1, *global_table2, *global_table3, *global_table4, *global_table5, *global_table6, *global_table7;
        Register *flag_table0, *flag_table1, *flag_table2, *flag_table3, *flag_table4, *flag_table5, *flag_table6, *flag_table7;
        vector<Register *> global_tables;
        vector<Register *> flag_tables;
        Meter *dark_meter;
        Meter *dark_global_meter;
    public:
        LocalClient(Args* args, shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        void add_mirroring(vector<uint16_t> router_ports, uint16_t mc_session_id, uint16_t log_session_id, uint16_t pkt_len, uint16_t log_port);

        vector<string> parse_monitored(string path);

        void populate_monitored(vector<string> entries);

        void add_ports(unordered_map<string, vector<uint16_t>> ports);

        void set_forward(unordered_map<uint16_t, uint16_t> port_pairs);

        void set_rates();

        void update_rates(unordered_map<uint32_t, uint32_t> inactive_pfxs, uint32_t inactive_addr);



        void setup();

        void run();
};

#endif // LOCALCLIENT_H