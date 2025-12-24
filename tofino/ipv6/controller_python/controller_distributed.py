# This software is Copyright (c) 2024 Georgia Tech Research Corporation. All
# Rights Reserved. Permission to copy, modify, and distribute this software and
# its documentation for academic research and education purposes, without fee,
# and without a written agreement is hereby granted, provided that the above
# copyright notice, this paragraph and the following three paragraphs appear in
# all copies. Permission to make use of this software for other than academic
# research and education purposes may be obtained by contacting:
#
#  Office of Technology Licensing
#  Georgia Institute of Technology
#  926 Dalney Street, NW
#  Atlanta, GA 30318
#  404.385.8066
#  techlicensing@gtrc.gatech.edu
#
# This software program and documentation are copyrighted by Georgia Tech
# Research Corporation (GTRC). The software program and documentation are 
# supplied "as is", without any accompanying services from GTRC. GTRC does
# not warrant that the operation of the program will be uninterrupted or
# error-free. The end-user understands that the program was developed for
# research purposes and is advised not to rely exclusively on the program for
# any reason.
#
# IN NO EVENT SHALL GEORGIA TECH RESEARCH CORPORATION BE LIABLE TO ANY PARTY FOR
# DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
# LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
# EVEN IF GEORGIA TECH RESEARCH CORPORATION HAS BEEN ADVISED OF THE POSSIBILITY
# OF SUCH DAMAGE. GEORGIA TECH RESEARCH CORPORATION SPECIFICALLY DISCLAIMS ANY
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED
# HEREUNDER IS ON AN "AS IS" BASIS, AND  GEORGIA TECH RESEARCH CORPORATION HAS
# NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
# MODIFICATIONS.

import os
import sys
import logging
import time

SDE_INSTALL   = os.environ['SDE_INSTALL']

PYTHON3_VER   = '{}.{}'.format(
    sys.version_info.major,
    sys.version_info.minor)
SDE_PYTHON3   = os.path.join(SDE_INSTALL, 'lib', 'python' + PYTHON3_VER,
                             'site-packages')
sys.path.append(SDE_PYTHON3)
sys.path.append(os.path.join(SDE_PYTHON3, 'tofino'))
sys.path.append(os.path.join(SDE_PYTHON3, 'tofino', 'bfrt_grpc'))

LOG_PORT = 6
# LOG_PORT = 140 # 2
RECIRCULATE_PORT = 68
NUM_PIPES = 2

import bfrt_grpc.client as gc
import argparse, time, ipaddress
from concurrent.futures import ThreadPoolExecutor, as_completed

class LocalClient:
    def __init__(self, time_interval, global_table_size, alpha, monitored_path, ports, switch_ips=None):        
        self.time_interval = time_interval
        self.global_table_size = global_table_size
        self.alpha = alpha
        self.counters = [self.alpha]*self.global_table_size*4
        self.monitored_path = monitored_path
        self.index_prefix_mapping = []
        self.ports = ports
        self.switch_ips = switch_ips if switch_ips else ['localhost']

        self.addr_cnt = 0
        self._setup()

    def parse_monitored(self, path):
        monitored_prefixes = []
        with open(path, 'r') as f:
            for line in f:
                if line.startswith('#'):
                    continue
                monitored_prefixes.append(line.strip())
        return monitored_prefixes

    def _setup(self):
        self.interfaces = {}
        self.bfrt_infos = {}
        self.dev_tgts = {}
        self.ports_tables = {}
        self.monitored_tables = {}
        self.global_tables_dict = {}
        self.flag_tables_dict = {}
        
        for idx, switch_ip in enumerate(self.switch_ips):
            self._setup_switch(switch_ip, idx)
    
    def _setup_switch(self, switch_ip, client_id):
        grpc_addr = f'{switch_ip}:50052'
        
        self.interfaces[switch_ip] = gc.ClientInterface(
            grpc_addr = grpc_addr,
            client_id = client_id,
            device_id = 0,
            num_tries = 1)

        self.bfrt_infos[switch_ip] = self.interfaces[switch_ip].bfrt_info_get()
        self.dev_tgts[switch_ip] = gc.Target(0)
        print(f'Switch {switch_ip}: The target runs the program {self.bfrt_infos[switch_ip].p4_name_get()}')

        self.ports_tables[switch_ip] = self.bfrt_infos[switch_ip].table_get('pipe.Ingress.ports')
        self.monitored_tables[switch_ip] = self.bfrt_infos[switch_ip].table_get('pipe.Ingress.monitored')
        self.monitored_tables[switch_ip].info.key_field_annotation_add('meta.addr', 'ipv6')
        
        # Get global and flag tables for this switch
        self.global_tables_dict[switch_ip] = [
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.global_table0'),
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.global_table1'),
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.global_table2'),
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.global_table3')
        ]
        self.flag_tables_dict[switch_ip] = [
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.flag_table0'),
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.flag_table1'),
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.flag_table2'),
            self.bfrt_infos[switch_ip].table_get('pipe.Ingress.flag_table3')
        ]
        
        self.interfaces[switch_ip].bind_pipeline_config(self.bfrt_infos[switch_ip].p4_name_get())
        self.add_mirroring(switch_ip, [5, 5, 6], 1, 2)  # set up mirroring
        monitored_prefixes = self.parse_monitored(self.monitored_path)   # populate monitored table
        self.populate_monitored(switch_ip, monitored_prefixes)
        self.add_ports(switch_ip, self.ports)
 
    def add_ports(self, switch_ip, ports):
        for port in ports['incoming']:
            _keys = self.ports_tables[switch_ip].make_key([gc.KeyTuple('ig_intr_md.ingress_port', port)])
            _data = self.ports_tables[switch_ip].make_data([], 'Ingress.set_incoming')
            try:
                self.ports_tables[switch_ip].entry_add(self.dev_tgts[switch_ip], [_keys], [_data])
            except:
                pass

        for port in ports['outgoing']:
            _keys = self.ports_tables[switch_ip].make_key([gc.KeyTuple('ig_intr_md.ingress_port', port)])
            _data = self.ports_tables[switch_ip].make_data([], 'Ingress.set_outgoing')
            try:
                self.ports_tables[switch_ip].entry_add(self.dev_tgts[switch_ip], [_keys], [_data])
            except:
                pass

    def populate_monitored(self, switch_ip, entries):
        base_idx = 0

        for entry in entries:
            prefix, length = entry.split('/')
            mask = 2**(54 - int(length)) - 1
            _keys = self.monitored_tables[switch_ip].make_key([gc.KeyTuple('meta.addr', prefix, None, int(length))])
            _data = self.monitored_tables[switch_ip].make_data([
                gc.DataTuple('base_idx', base_idx),
                gc.DataTuple('mask', mask)
            ], 'Ingress.calc_idx')
            try:
                self.monitored_tables[switch_ip].entry_add(self.dev_tgts[switch_ip], [_keys], [_data])
            except:
                pass
            
            # save in local dictionary (shared across all switches)
            ipnet = ipaddress.IPv6Network(entry)
            netws = list(ipnet.subnets(new_prefix=56))
            self.index_prefix_mapping.extend(netws)

            base_idx += mask + 1
            self.addr_cnt += len(netws)

    def add_mirroring(self, switch_ip, eg_ports, mc_session_id, log_session_id):
        mirror_table = self.bfrt_infos[switch_ip].table_get('$mirror.cfg')
        pre_node_table = self.bfrt_infos[switch_ip].table_get('$pre.node')
        pre_mgid_table = self.bfrt_infos[switch_ip].table_get('$pre.mgid')
        rec_ports = [RECIRCULATE_PORT + 128*x for x in range(NUM_PIPES)]

        rid = 1
        # multicast nodes
        for port in eg_ports:
            for _ in range(3): # 3 packets per router
                l1_node_key = pre_node_table.make_key([gc.KeyTuple('$MULTICAST_NODE_ID', rid)])
                l2_node = pre_node_table.make_data([
                    gc.DataTuple('$MULTICAST_RID', rid),
                    gc.DataTuple('$DEV_PORT', int_arr_val=[port])
                ])
                rid += 1
                try:
                    pre_node_table.entry_add(self.dev_tgt, [l1_node_key], [l2_node])   
                except:
                    pass
        
        # multicast group
        mg_id_key = pre_mgid_table.make_key([gc.KeyTuple('$MGID', 1)])
        mg_id_data = pre_mgid_table.make_data([
            gc.DataTuple('$MULTICAST_NODE_ID', int_arr_val=list(range(1, rid))),
            gc.DataTuple('$MULTICAST_NODE_L1_XID_VALID', bool_arr_val=[False]*(rid-1)),
            gc.DataTuple('$MULTICAST_NODE_L1_XID', int_arr_val=[0]*(rid-1)),
        ])
        try:
            pre_mgid_table.entry_add(self.dev_tgt, [mg_id_key], [mg_id_data])
        except:
            pass

        init_rid = rid
        # recirculation nodes
        for port in rec_ports:
            l1_node_key = pre_node_table.make_key([gc.KeyTuple('$MULTICAST_NODE_ID', rid)])
            l2_node = pre_node_table.make_data([
                gc.DataTuple('$MULTICAST_RID', rid),
                gc.DataTuple('$DEV_PORT', int_arr_val=[port])
            ])
            rid += 1
            try:
                pre_node_table.entry_add(self.dev_tgt, [l1_node_key], [l2_node])   
            except:
                pass
        
        # multicast group
        mg_id_key = pre_mgid_table.make_key([gc.KeyTuple('$MGID', 2)])
        mg_id_data = pre_mgid_table.make_data([
            gc.DataTuple('$MULTICAST_NODE_ID', int_arr_val=list(range(init_rid, rid))),
            gc.DataTuple('$MULTICAST_NODE_L1_XID_VALID', bool_arr_val=[False]*(rid-init_rid)),
            gc.DataTuple('$MULTICAST_NODE_L1_XID', int_arr_val=[0]*(rid-init_rid)),
        ])
        try:
            pre_mgid_table.entry_add(self.dev_tgt, [mg_id_key], [mg_id_data])
        except:
            pass

        mirror_key  = mirror_table.make_key([gc.KeyTuple('$sid', mc_session_id)])
        mirror_data = mirror_table.make_data([
            gc.DataTuple('$direction', str_val="BOTH"),
            gc.DataTuple('$session_enable', bool_val=True),
            gc.DataTuple('$mcast_rid', 1),
            gc.DataTuple('$mcast_grp_a', 1),
            gc.DataTuple('$mcast_grp_a_valid', bool_val=True),
            gc.DataTuple('$mcast_grp_b', 2),
            gc.DataTuple('$mcast_grp_b_valid', bool_val=True),
            gc.DataTuple('$max_pkt_len', 71)
        ], "$normal")

        try:
            mirror_table.entry_add(self.dev_tgt, [mirror_key], [mirror_data])
        except:
            pass

        mirror_key  = mirror_table.make_key([gc.KeyTuple('$sid', log_session_id)])
        mirror_data = mirror_table.make_data([
            gc.DataTuple('$direction', str_val="BOTH"),
            gc.DataTuple('$session_enable', bool_val=True),
            gc.DataTuple('$ucast_egress_port', LOG_PORT),
            gc.DataTuple('$ucast_egress_port_valid', bool_val=True)
            ], "$normal")

        try:
            mirror_table.entry_add(self.dev_tgt, [mirror_key], [mirror_data])
        except:
            pass

    def read_register(self, table, index_range, switch_ip='localhost', flags={"from_hw": False}):
        _keys = [table.make_key([gc.KeyTuple("$REGISTER_INDEX", index)]) for index in index_range]
        data_name = table.info.data_dict_allname["f1"]
        results = []
        for entry in table.entry_get(self.dev_tgts[switch_ip], _keys, flags=flags):
            data = entry[0].to_dict()
            results.append(data[data_name])

        return results

    def write_register(self, table, keys_1, keys_0, switch_ip='localhost'):
        _keys = [table.make_key([gc.KeyTuple("$REGISTER_INDEX", index)]) for index in keys_1]
        _keys.extend([table.make_key([gc.KeyTuple("$REGISTER_INDEX", index)]) for index in keys_0])

        data_name = table.info.data_dict_allname["f1"]
        _data = [table.make_data([gc.DataTuple(data_name, 1)])]*len(keys_1)
        _data.extend([table.make_data([gc.DataTuple(data_name, 0)])]*len(keys_0))

        table.entry_add(self.dev_tgts[switch_ip], _keys, _data)

    def _read_and_sync_switch(self, switch_ip):
        try:
            flags_all_tables = {}
            for t in range(4):
                logging.info(f'Syncing flag table {t} on {switch_ip}')
                self.flag_tables_dict[switch_ip][t].operations_execute(self.dev_tgts[switch_ip], 'Sync')
                
                logging.info(f'Reading flag table {t} from {switch_ip}')
                flags = self.read_register(self.flag_tables_dict[switch_ip][t], range(int(self.addr_cnt / 4)), switch_ip)
                flags_all_tables[t] = flags
            return switch_ip, flags_all_tables
        except Exception as e:
            logging.error(f'Error reading from switch {switch_ip}: {e}')
            return switch_ip, None

    def _write_updates_to_switch(self, switch_ip, global_indices_per_table, inactive_indices_per_table, flag_indices_per_table):
        try:
            for t in range(4):
                logging.info(f'Writing into table {t} on {switch_ip}')
                global_indices = global_indices_per_table.get(t, [])
                inactive_indices = inactive_indices_per_table.get(t, [])
                flag_indices = flag_indices_per_table.get(t, [])
                
                # write global table updates (mark as active or inactive)
                if global_indices or inactive_indices:
                    self.write_register(self.global_tables_dict[switch_ip][t], global_indices, inactive_indices, switch_ip)
                # reset flags
                if flag_indices:
                    self.write_register(self.flag_tables_dict[switch_ip][t], [], flag_indices, switch_ip)
            logging.info(f'Completed writing to {switch_ip}')
        except Exception as e:
            logging.error(f'Error writing to switch {switch_ip}: {e}')

    def run(self):
        while True:
            logging.info('Starting collecting values...')
            iter_time = time.monotonic() 

            # read flags from all switches in parallel
            all_switch_flags = {}
            with ThreadPoolExecutor(max_workers=len(self.switch_ips)) as executor:
                futures = {executor.submit(self._read_and_sync_switch, switch_ip): switch_ip for switch_ip in self.switch_ips}
                for future in as_completed(futures):
                    switch_ip, flags_all_tables = future.result()
                    if flags_all_tables is not None:
                        all_switch_flags[switch_ip] = flags_all_tables
            logging.info(f'Collecting from all {len(self.switch_ips)} switches took {time.monotonic() - iter_time}s')

            flags_to_reset_per_switch = {switch_ip: {t: [] for t in range(4)} for switch_ip in self.switch_ips}
            global_indices_per_switch = {switch_ip: {t: [] for t in range(4)} for switch_ip in self.switch_ips}
            inactive_indices_per_switch = {switch_ip: {t: [] for t in range(4)} for switch_ip in self.switch_ips}
            
            for i in range(int(self.addr_cnt / 4)):
                for t in range(4):
                    active = False
                    for switch_ip in self.switch_ips:
                        if switch_ip not in all_switch_flags or t not in all_switch_flags[switch_ip]:
                            continue
                        flags = all_switch_flags[switch_ip][t]
                        t_val = flags[i]
                        if any(t_val):
                            active = True
                            break
                    
                    if active:
                        # prefix is active on at least one switch
                        if not self.counters[4*i+t]:
                            for switch_ip in self.switch_ips:
                                global_indices_per_switch[switch_ip][t].append(i)
                            logging.warning(f'Prefix {self.index_prefix_mapping[4*i+t]} became active.')
                        self.counters[4*i+t] = self.alpha + 1
                        # mark for flag reset on all switches where it was flagged
                        for switch_ip in self.switch_ips:
                            if switch_ip in all_switch_flags and t in all_switch_flags[switch_ip]:
                                flags = all_switch_flags[switch_ip][t]
                                if any(flags[i]):
                                    flags_to_reset_per_switch[switch_ip][t].append(i)
                    else:
                        # prefix is inactive on all switches
                        if self.counters[4*i+t] > 1:
                            self.counters[4*i+t] -= 1
                        else:
                            if self.counters[4*i+t] == 1:
                                for switch_ip in self.switch_ips:
                                    inactive_indices_per_switch[switch_ip][t].append(i)
                                self.counters[4*i+t] = 0

            # write updates to all switches in parallel
            write_time = time.monotonic()
            with ThreadPoolExecutor(max_workers=len(self.switch_ips)) as executor:
                futures = {executor.submit(self._write_updates_to_switch, switch_ip, global_indices_per_switch[switch_ip], inactive_indices_per_switch[switch_ip], flags_to_reset_per_switch[switch_ip]): switch_ip for switch_ip in self.switch_ips}
                for future in as_completed(futures):
                    future.result()
            logging.info(f'Writing to all {len(self.switch_ips)} switches took {time.monotonic() - write_time}s')

            logging.info(f'Iteration took {time.monotonic() - iter_time}s')

            logging.info(f'Waiting for {self.time_interval} seconds...')
            time.sleep(self.time_interval - (time.monotonic() - iter_time))

if __name__ == "__main__":
    print("Start Controller....")
    
    logging.basicConfig(level="DEBUG",
                        format="%(asctime)s|%(levelname)s: %(message)s",
                        datefmt="%Y-%m-%d %H:%M:%S")

    parser = argparse.ArgumentParser()
    parser.add_argument('--interval', default=100, type=int, help='Interval in seconds')
    parser.add_argument('--global-table-size', default=4194304, type=int, help='Size of the global table')
    parser.add_argument('--alpha', default=216, type=int, help='Alpha value')
    parser.add_argument('--monitored', default='../input_files/monitored.txt', type=str)
    parser.add_argument('--outgoing', nargs='*', default=[1], type=int)
    parser.add_argument('--incoming', nargs='*', default=[2], type=int)
    parser.add_argument('--switches', nargs='*', default=['localhost'], type=str, help='IP addresses of switches to monitor')

    args = parser.parse_args()

    client = LocalClient(args.interval, args.global_table_size, args.alpha, args.monitored, {'incoming': args.incoming, 'outgoing': args.outgoing}, args.switches)
    client.run()