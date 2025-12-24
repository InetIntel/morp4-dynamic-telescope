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
#include "Meter.h"
#include "PortManager.h"
#include "Node.h"
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
    uint32_t max_pkt_rate = 1174405;
    uint32_t avg_pkt_rate = 343933;
    uint32_t max_byte_rate = 338102845;
    uint32_t avg_byte_rate = 17758683;
    uint16_t alpha = 216;
    string monitored_path = "monitored.txt";
    vector<uint16_t> outgoing = {8};
    vector<uint16_t> incoming = {9};
};

class LocalClient{
    public:
        uint32_t global_table_size;
        string monitored_path;
        vector<string> monitored_prefixes;
        uint32_t addr_cnt;

        vector<uint16_t> counters;
        uint16_t alpha;
        uint16_t time_interval;

        unordered_map<string, vector<uint16_t>> ports;
        unordered_map<uint16_t, uint16_t> port_pairs;

        uint32_t dark_meter_size;
        uint32_t max_pkt_rate;
        uint32_t avg_pkt_rate;
        uint32_t max_byte_rate;
        uint32_t avg_byte_rate;

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
        vector<Register *> global_tables;
        vector<Register *> flag_tables;
        Meter *dark_meter;
        Meter *dark_global_meter;
    public:
        LocalClient(Args* args, shared_ptr<BfRtSession> session, bf_rt_target_t dev_tgt, const BfRtInfo *bf_rt_info);

        void add_mirroring(vector<uint16_t> router_ports, uint16_t mc_session_id, uint16_t log_session_id, 
                            uint16_t pkt_len, uint16_t log_port);

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