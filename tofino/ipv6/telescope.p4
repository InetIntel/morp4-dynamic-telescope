// This software is Copyright (c) 2024 Georgia Tech Research Corporation. All
// Rights Reserved. Permission to copy, modify, and distribute this software and
// its documentation for academic research and education purposes, without fee,
// and without a written agreement is hereby granted, provided that the above
// copyright notice, this paragraph and the following three paragraphs appear in
// all copies. Permission to make use of this software for other than academic
// research and education purposes may be obtained by contacting:
//
//  Office of Technology Licensing
//  Georgia Institute of Technology
//  926 Dalney Street, NW
//  Atlanta, GA 30318
//  404.385.8066
//  techlicensing@gtrc.gatech.edu
//
// This software program and documentation are copyrighted by Georgia Tech
// Research Corporation (GTRC). The software program and documentation are 
// supplied "as is", without any accompanying services from GTRC. GTRC does
// not warrant that the operation of the program will be uninterrupted or
// error-free. The end-user understands that the program was developed for
// research purposes and is advised not to rely exclusively on the program for
// any reason.
//
// IN NO EVENT SHALL GEORGIA TECH RESEARCH CORPORATION BE LIABLE TO ANY PARTY FOR
// DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
// LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
// EVEN IF GEORGIA TECH RESEARCH CORPORATION HAS BEEN ADVISED OF THE POSSIBILITY
// OF SUCH DAMAGE. GEORGIA TECH RESEARCH CORPORATION SPECIFICALLY DISCLAIMS ANY
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED
// HEREUNDER IS ON AN "AS IS" BASIS, AND  GEORGIA TECH RESEARCH CORPORATION HAS
// NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
// MODIFICATIONS.

#include <core.p4>
#include <tna.p4>

#include "./include/constants.p4"
#include "./include/headers.p4"

parser IngressParser(packet_in        pkt,
    out my_ingress_headers_t          hdr,
    out my_ingress_metadata_t         meta,
    out ingress_intrinsic_metadata_t  ig_intr_md)
{

    state start {
        pkt.extract(ig_intr_md);
        pkt.advance(PORT_METADATA_SIZE);

        meta.addr = 0;
        meta.offset = 0;
        meta.idx = 0;
        meta.incoming = 0;
        meta.outgoing = 0;
        meta.notify = 0;
        meta.mirror_session = 0;
        meta.bridge.header_type = HEADER_NORMAL;
        meta.mirror_header_type = 0;

        transition parse_ethernet;
    }
    
    state parse_ethernet {       
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ether_type_t.IPV6 : parse_ipv6;
            default : accept;
        }
    }

    state parse_ipv6 {
        pkt.extract(hdr.ipv6);
        transition select(hdr.ipv6.nextHdr) {
            ip_protocol_t.CTL: parse_ctl;
            default: accept;
        }
    }

    state parse_ctl {
        pkt.extract(hdr.ctl);
        transition accept;
    }

}

control Ingress(
    inout my_ingress_headers_t                       hdr,
    inout my_ingress_metadata_t                      meta,
    in    ingress_intrinsic_metadata_t               ig_intr_md,
    in    ingress_intrinsic_metadata_from_parser_t   ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t        ig_tm_md)
{      
    action drop_exit_ingress () {
        ig_dprsr_md.drop_ctl = 1;
        exit;
    }

    action set_port(PortId_t port) {
        ig_tm_md.ucast_egress_port = port;
    }

    table forward {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            set_port; 
            @defaultonly NoAction;
        }
        size = 1024;
        default_action = NoAction();
    }    

    action calc_idx(bit<GLOBAL_TABLE_INDEX_WIDTH> base_idx, bit<GLOBAL_TABLE_INDEX_WIDTH> mask) {
            meta.offset = ((bit<GLOBAL_TABLE_INDEX_WIDTH>) (meta.addr >> 74)) & mask; //assuming /32; monitoring at /54 (/56 actually)
            meta.idx = base_idx;
    }

    action set_incoming(){
        meta.addr = hdr.ipv6.dst_addr;
        meta.incoming = 1;
    }

    action set_outgoing(){
        meta.addr = hdr.ipv6.src_addr;
        meta.outgoing = 1;
    }

    table ports {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            set_incoming;
            set_outgoing;
            @defaultonly NoAction;
        }
        size = 64;
        default_action = NoAction();
    }

    table monitored {
        key = {
            meta.addr: lpm;
        }
        actions = {
            calc_idx;
            @defaultonly NoAction;
        }
        size = 1024;
        default_action = NoAction();
    }

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 0) flag_table0;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table0)
    read_update_flag_table0 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = ~value;
            value = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table0)
    read_flag_table0 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 1) global_table0;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table0)
    update_global_table0 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            value = 1;
            rv = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table0)
    read_global_table0 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 0) flag_table1;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table1)
    read_update_flag_table1 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = ~value;
            value = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table1)
    read_flag_table1 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 1) global_table1;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table1)
    update_global_table1 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            value = 1;
            rv = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table1)
    read_global_table1 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 0) flag_table2;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table2)
    read_update_flag_table2 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = ~value;
            value = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table2)
    read_flag_table2 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 1) global_table2;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table2)
    update_global_table2 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            value = 1;
            rv = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table2)
    read_global_table2 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 0) flag_table3;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table3)
    read_update_flag_table3 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = ~value;
            value = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(flag_table3)
    read_flag_table3 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    Register<bit<1>, global_reg_index_t>(GLOBAL_TABLE_ENTRIES, 1) global_table3;
    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table3)
    update_global_table3 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            value = 1;
            rv = 1;
        }
    };

    RegisterAction<bit<1>, global_reg_index_t, bit<1>>(global_table3)
    read_global_table3 = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
        }
    };

    apply {
        if (hdr.ipv6.isValid()){
            meta.bridge.setValid();
            if (hdr.ctl.isValid()){
                // if it's notification pkt from the other routers/pipes
                meta.addr = hdr.ctl.targetAddr;
                meta.outgoing = 1;
            }
            else{
                ports.apply();
            } 
            meta.pos = (bit<2>) (meta.addr>>72); // pick correct table (55-56 bits)
            if (monitored.apply().hit){
                meta.idx = meta.idx + meta.offset;
                if (meta.outgoing == 1){
                    bit<1> g_val;
                    if (meta.pos == 0){
                        g_val = update_global_table0.execute(meta.idx);
                        if(g_val == 1 || g_val == 0){
                            meta.notify = read_update_flag_table0.execute(meta.idx);
                        }
                    }
                    else if (meta.pos == 1){
                        g_val = update_global_table1.execute(meta.idx);
                        if (g_val == 1 || g_val == 0){
                            meta.notify = read_update_flag_table1.execute(meta.idx);
                        }
                    }
                    else if (meta.pos == 2){
                        g_val = update_global_table2.execute(meta.idx);
                        if (g_val == 1 || g_val == 0){
                            meta.notify = read_update_flag_table2.execute(meta.idx);
                        }
                    }
                    else if (meta.pos == 3){
                        g_val = update_global_table3.execute(meta.idx);
                        if (g_val == 1 || g_val == 0){
                            meta.notify = read_update_flag_table3.execute(meta.idx);
                        }
                    }
                    if (!hdr.ctl.isValid() && meta.notify == 1){
                        meta.mirror_header_type = HEADER_CONTROL;
                        ig_dprsr_md.mirror_type = 1; 
                        meta.mirror_session = (MirrorId_t) 1;
                    }
                }
                else if (meta.incoming == 1){
                    bit<1> g_value;
                    bit<1> t_value;

                    if (meta.pos == 0){
                        g_value = read_global_table0.execute(meta.idx);
                        t_value = read_flag_table0.execute(meta.idx);
                    }
                    else if (meta.pos == 1){
                        g_value = read_global_table1.execute(meta.idx);
                        t_value = read_flag_table1.execute(meta.idx);
                    }
                    else if (meta.pos == 2){
                        g_value = read_global_table2.execute(meta.idx);
                        t_value = read_flag_table2.execute(meta.idx);
                    }
                    else if (meta.pos == 3){
                        g_value = read_global_table3.execute(meta.idx);
                        t_value = read_flag_table3.execute(meta.idx);
                    }

                    if (g_value == 0 && t_value == 0){
                        meta.mirror_header_type = HEADER_MIRROR;
                        ig_dprsr_md.mirror_type = 2; 
                        meta.mirror_session = (MirrorId_t) 2;                     
                    }
                }
            }
            if (hdr.ctl.isValid()){
                set_port((PortId_t) LOG_PORT);
            }
            else {
                forward.apply(); // Basic forwarding logic
            }
        }
    }
}

control IngressDeparser(packet_out pkt,
    inout my_ingress_headers_t                       hdr,
    in    my_ingress_metadata_t                      meta,
    in    ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md)
{   
    Mirror() mirror;

    apply {
        if (ig_dprsr_md.mirror_type == 1){
            mirror.emit<darknet_control_mirror_h>(meta.mirror_session, {meta.mirror_header_type, meta.addr}); 
        }
        if (ig_dprsr_md.mirror_type == 2){
            mirror.emit<normal_h>(meta.mirror_session, {meta.mirror_header_type});
        }
        pkt.emit(meta.bridge);
        pkt.emit(hdr);
    }
}

parser EgressParser(packet_in        pkt,
    out my_egress_headers_t          hdr,
    out my_egress_metadata_t         meta,
    out egress_intrinsic_metadata_t  eg_intr_md)
{

    state start {
        pkt.extract(eg_intr_md);
        meta = {0, 0};
        meta.header_type = pkt.lookahead<header_type_t>(); // check type
        pkt.advance(32w8);
        transition select(meta.header_type) {
            HEADER_NORMAL: parse_ethernet;
            HEADER_CONTROL: parse_control;
            HEADER_MIRROR: accept;
            default: reject;
        }
    }

    state parse_control {
        meta.addr = pkt.lookahead<ipv6_addr_t>();
        pkt.advance(32w128);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition parse_ipv6;
    }

    state parse_ipv6 {
        pkt.extract(hdr.ipv6);
        transition accept;
    }

}

control Egress(
    inout my_egress_headers_t                          hdr,
    inout my_egress_metadata_t                         meta,
    in    egress_intrinsic_metadata_t                  eg_intr_md,
    in    egress_intrinsic_metadata_from_parser_t      eg_prsr_md,
    inout egress_intrinsic_metadata_for_deparser_t     eg_dprsr_md,
    inout egress_intrinsic_metadata_for_output_port_t  eg_oport_md)
{
    action set_nhop_r(mac_addr_t dstMACAddr, ipv6_addr_t dstIpAddr){
        hdr.ethernet.dst_addr = dstMACAddr;
        hdr.ipv6.dst_addr = dstIpAddr;
    }

    table mcast_routers{
        key = {
            eg_intr_md.egress_rid: exact;
        }
        actions = {
            set_nhop_r;
            @defaultonly NoAction;
        }
        size = 1024;
        default_action = NoAction();
    }

    apply {
        if(meta.header_type == HEADER_CONTROL){
            mcast_routers.apply();
            hdr.ipv6.nextHdr = ip_protocol_t.CTL;
            hdr.ctl.setValid();
            hdr.ctl.targetAddr = meta.addr;
            hdr.ipv6.hopLimit = 255;
            hdr.ipv6.payloadLen = 16;
        }
    }
}

control EgressDeparser(packet_out pkt,
    inout my_egress_headers_t                       hdr,
    in    my_egress_metadata_t                      meta,
    in    egress_intrinsic_metadata_for_deparser_t  eg_dprsr_md)
{
    apply {
        pkt.emit(hdr);
    }
}

Pipeline(
    IngressParser(),
    Ingress(),
    IngressDeparser(),
    EgressParser(),
    Egress(),
    EgressDeparser()
) pipe;

Switch(pipe) main;