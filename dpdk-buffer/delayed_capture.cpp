/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <iostream>

#include <rte_arp.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_pcapng.h>


/* Ethernet MTU size in bytes */
#define ETH_MTU                  (1500u)

/**
 *  Number of objects (packets) in the packet memory pool
 *
 * For 100Gbps NIC, we can receive packets at the rate of 100Gbps. If we
 * buffer every packet for 1 second, and then process the packets after 1
 * second, then we need enough memory to buffer the packets for 1 whole second.
 * This is equal to (100Gbps * 1 second) worth of data. This equates to
 * ~12.5GB or ~8333334 MTU-sized (1500 bytes) packets.
 *
 * After the first second, our pool will hold ~8333334 packets, and we will receive
 * even more from the network. This means we need to have enough memory in the pool
 * to buffer the packets for 1 second, and also to allow receiving more packets
 * from the network.
 *
 * Hence, the packet buffer memory pool size will be set to double of ~8333334
 * packets, viz., ~16 million packets. We will round it up to 2^24
 *
 * NOTE: Make sure to allocate enough hugepages for the above.
 */
#define MBUF_POOL_SIZE           (1 << 24u)
//#define MBUF_POOL_SIZE              (8192)

/**
 * Number of objects in the packet ring (between the first and second lcore)
 *
 * This should be equal to the pool size limit.
 */
#define MBUF_RING_SIZE           (MBUF_POOL_SIZE)

/* TX/RX ring configuration */
#define NB_TX_RINGS              (1)
#define NB_RX_RINGS              (2)
#define NB_TX_DESC               (1024)
#define NB_RX_DESC               (1024)

/* TX/RX burst size for transmitting or receiving packets */
#define BURST_SIZE               (32)

/* ARP IPv4 settings */
/*
#define ARP_PROTOCOL_IPV4        (0x0800)
#define ARP_HARDWARE_ADDR_LEN    (6)
#define ARP_PROTOCOL_ADDR_LEN    (4)
*/

/* IP Protocol numbers we need to handle */
#define CTL_IP_PROTO          (146u)

/**
 * Time for which a packet should be buffered (in seconds)
 *
 * NOTE: If this value is changed, then the mbuf pool size above has to be
 *       changed as well.
 */
#define BUFFER_PACKETS_WAIT_TIME (1u)


/* Time after which the state of the prefix can be updated (in seconds) from active to inactive*/
#define BUFFER_STATE_UPDATE_TIME (3600u)

/* Packet capture file name */
#define PCAP_FILE_NAME           "packets."
#define PCAP_FILE_EXT            ".pcap"

/* Time when the packet arrived */
struct __attribute__((aligned(RTE_MBUF_PRIV_ALIGN))) mbuf_priv_data {
    uint64_t arrival_ts;
    bool is_ipv6;
};


/* Argument structure for the PCAP file */
struct pcap_args {
    int pcap_fd;
    rte_pcapng_t *pcap_hdl;
    struct rte_mempool *pcap_mbuf_pool;
};

/* Argument structure for the lcores */
struct lcore_args {
    uint16_t port_id;
    uint32_t port_ip;
    struct rte_mempool *mbuf_pool;
    struct rte_ring *mbuf_ring;
    struct pcap_args *pcap_args;
};

/**
 * The IP payload structure. This is application specific.
 */
struct __attribute__((packed)) ip_payload {
    uint32_t target_ip;
};
struct __attribute__((packed)) ip6_payload {
    uint8_t target_ip[16];
};

/**
 * Global array. This is application specific.
 */
std::vector<bool> g_state_arr;
std::vector<uint64_t> g_state_last_changed_ts;

/**
* Mapping of IPv4 addresses to indices
*/
std::unordered_map<uint32_t, int> ip_to_idx;

/**
* Mapping of IPv6 addresses to indices
*/
std::unordered_map<uint64_t, int> ipv6_to_idx; // we will never go beyond /64

/**
 * Initialize the given ethernet port
 */
int port_init(uint16_t port_id, struct rte_mempool *mbuf_pool) {

    int status;
    struct rte_eth_conf port_conf;

    /* Check if the port ID is valid */
    if (!rte_eth_dev_is_valid_port(port_id)) {
        return -1;
    }

    /* Configure the port */
    memset(&port_conf, 0, sizeof(struct rte_eth_conf));
    status = rte_eth_dev_configure(port_id, NB_RX_RINGS, NB_TX_RINGS, &port_conf);
    if (status) {
        return status;
    }

    /* Configure the RX queue */
    status = rte_eth_rx_queue_setup(port_id, 0, NB_RX_DESC,
                                    rte_eth_dev_socket_id(port_id), NULL, mbuf_pool);
    if (status) {
        return status;
    }
    status = rte_eth_rx_queue_setup(port_id, 1, NB_RX_DESC,
                                    rte_eth_dev_socket_id(port_id), NULL, mbuf_pool);
    if (status) {
        return status;
    }

    /* Configure the TX queue */
    status = rte_eth_tx_queue_setup(port_id, 0, NB_TX_DESC,
                                    rte_eth_dev_socket_id(port_id), NULL);
    if (status) {
        return status;
    }

    /* Set the MTU */
    status = rte_eth_dev_set_mtu(port_id, ETH_MTU);
    if (status) {
        return status;
    }
    printf("Set the MTU for port %d\n", port_id);

    /* Start the port */
    status = rte_eth_dev_start(port_id);
    if (status) {
        return status;
    }
    
    status = rte_eth_promiscuous_enable(port_id);
    if (status) {
    	return status;
    }
    printf("Enabled promiscuous mode for port %d\n", port_id);


    return 0;
}

/**
 * De-initialize the given ethernet port
 */
int port_deinit(uint16_t port_id) {

    int status;

    /* Stop the port */
    status = rte_eth_dev_stop(port_id);
    if (status) {
        return status;
    }

    /* Close the port (to free any resources) */
    status = rte_eth_dev_close(port_id);
    if (status) {
        return status;
    }

    return 0;
}

/**
* Convert IPv4 address to integer
*/

uint32_t IPv4ToInt(const std::string& ip) {
    uint32_t result = 0;
    std::stringstream ss(ip);
    std::string ip_byte;

    for (int i = 3; i >= 0; --i) {
        getline(ss, ip_byte, '.');
        result |= (stoi(ip_byte) << (i * 8));
    }
    return result;
}

/**
* Convert IPv6 address to bytes
*/

uint64_t IPv6To64Int(const std::string& ip) {
    uint8_t bytes[16];
    memset(bytes, 0, 16);
    std::istringstream ss(ip);
    std::string part;
    int index = 0;
    uint16_t value;
    uint64_t block_64 = 0;

    while (getline(ss, part, ':')) {
        if (part.empty()) {
            // Handle "::" by skipping to the appropriate position
            index += (8 - std::count_if(std::istream_iterator<std::string>(ss),
                                        std::istream_iterator<std::string>(),
                                        [](const std::string&) { return true; })) * 2;
            continue;
        }
        value = stoi(part, nullptr, 16);
        bytes[index++] = (value >> 8) & 0xFF; // High byte
        bytes[index++] = value & 0xFF;       // Low byte
    }

    // get the 64 bit block to use as key
    for (int i = 0; i < 8; i++) {
        block_64 |= bytes[i] << (i * 8);
    }

    return block_64;
}

/**
* Read mapping of addresses to indices from a file
*/
void read_mapping(const char* filename) {
    std::ifstream file(filename);
    std::string line;
    int idx = 0;

    while (getline(file, line)) {
        // check if it is an IPv4 or IPv6 address
        if (line.find(':') != std::string::npos) {
            uint64_t ip6 = IPv6To64Int(line);
            ipv6_to_idx[ip6] = idx;
        } else {
            uint32_t int_ipv4 = IPv4ToInt(line);
            ip_to_idx[int_ipv4] = idx;
        }
        idx++;
    }
    g_state_arr.resize(idx, false);
    g_state_last_changed_ts.resize(idx, 0);
    printf("Read %d prefixes from the file\n", idx);
}

/**
* Initialize the PCAP file
*/
struct pcap_args *pcap_init(const char* filename, uint16_t port_id) {
    int pcap_fd;
    rte_pcapng_t *pcap_hdl;
    struct rte_mempool *pcap_mbuf_pool;
    struct pcap_args *file_args = new struct pcap_args;

    /* Setup the packet capture files and required resources */
    pcap_fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (pcap_fd < 0) {
        rte_exit(EXIT_FAILURE, "Failed to create the packet capture file %s",
                 filename);
    }
    pcap_hdl = rte_pcapng_fdopen(pcap_fd, NULL, NULL, NULL, NULL);
    if (!pcap_hdl) {
        rte_exit(EXIT_FAILURE, "Failed to create the DPDK packet capture handle");
    }
    
    pcap_mbuf_pool = rte_pktmbuf_pool_create("PCAP_MBUF_POOL", 2048,
                                             0, 0,
                                             rte_pcapng_mbuf_size(RTE_MBUF_DEFAULT_BUF_SIZE),
                                             rte_socket_id());
    if (!pcap_mbuf_pool) {
        rte_exit(EXIT_FAILURE, "Cannot create PCAP mbuf pool\n");
    }

    if (rte_pcapng_add_interface(pcap_hdl, port_id, NULL, NULL, NULL) < 0) {
        rte_exit(EXIT_FAILURE, "Failed to add port %d to the DPDK "
                 "packet capture handle", port_id);
    }
    file_args->pcap_fd = pcap_fd;
    file_args->pcap_hdl = pcap_hdl;
    file_args->pcap_mbuf_pool = pcap_mbuf_pool;

    return file_args;
}

/**
* Perform the PCAP-specific cleanup
*/

void close_pcap_file(struct pcap_args *file_args) {
    rte_pcapng_close(file_args->pcap_hdl);
    rte_mempool_free(file_args->pcap_mbuf_pool);
    close(file_args->pcap_fd);
    delete file_args;
}

/**
 * Read the IP packet payload to get the required field
 */
int get_arr_idx_from_ip_packet(struct rte_ipv4_hdr *ip_hdr, bool is_not_normal) {

    uint32_t ip_hdr_len = (ip_hdr->version_ihl & 0x0F) * 4;
    uint32_t int_addr = 0;
    if (!is_not_normal){
        int_addr = rte_be_to_cpu_32(ip_hdr->dst_addr);
    }
    else {
        /* Typecast data as required to extract the array index */
        struct ip_payload *payload = (struct ip_payload *)((char *)ip_hdr + ip_hdr_len);
        int_addr = payload->target_ip;
    }

    printf("payload->target_ip: %u\n", int_addr);
    if (ip_to_idx.find(int_addr) == ip_to_idx.end()) {
        return -1;
    }
    else {
        return ip_to_idx[int_addr];
    }
}

int get_arr_idx_from_ip6_packet(struct rte_ipv6_hdr *ip6_hdr, bool is_not_normal) {

    uint32_t ip6_hdr_len = 40; // we know that the header length is 40 bytes (minimum size)
    uint8_t int_addr[16];
    if (!is_not_normal){
        memcpy(int_addr, &(ip6_hdr->dst_addr), 16);
    }
    else {
        /* Typecast data as required to extract the array index */
        struct ip6_payload *payload = (struct ip6_payload *)((char *)ip6_hdr + ip6_hdr_len);
        memcpy(int_addr, payload->target_ip, 16);
    }
    
    // get the /64 block to use as key
    // by default it is /56, so we zero out the last 8 bits of the 8th byte
    int_addr[7] &= 0xF0; // zero out last 4 bits
    uint64_t block_64 = 0;
    for (int i = 0; i < 8; i++) {
        block_64 |= int_addr[i] << (i * 8);
    }
    
    if (ipv6_to_idx.find(block_64) == ipv6_to_idx.end()) {
        return -1;
    }
    else {
        return ipv6_to_idx[block_64];
    }
}


/**
 * Loop to run on the second lcore.
 *
 * This loop receives packets from the first lcore. It will process the packets
 * only after the packets sent by the first lcore were buffered for some
 * fixed amount of time (say 1 second).
 *
 */
int second_half_loop(void *_args) {

    int status;

    struct lcore_args *args = (struct lcore_args *)_args;

    uint16_t port_id = args->port_id;
    struct rte_mempool *mbuf_pool = args->mbuf_pool;
    struct rte_ring *mbuf_ring = args->mbuf_ring;
    struct pcap_args *pcap_args = args->pcap_args;

    struct rte_mbuf *mbuf;
    uint64_t curr_ts;
    uint64_t arrival_ts;
    bool is_ipv6;
    int arr_idx;
    int captured_pkts = 0;
    int pcap_num = 1;

    struct rte_ipv4_hdr *ip_hdr;
    struct rte_ipv6_hdr *ip6_hdr;

    while (1) {

        /* Wait till there are any elements in the ring */
        if (rte_ring_empty(mbuf_ring)) {
            continue;
        }

        /* Get an object from the ring */
        status = rte_ring_dequeue(mbuf_ring, (void **)&mbuf);
        if (status) {
            continue;
        }

        /* Get the packet's arrival time in the ring */
        struct mbuf_priv_data *pdata = (struct mbuf_priv_data*) rte_mbuf_to_priv(mbuf);
        arrival_ts = pdata->arrival_ts;
        is_ipv6 = pdata->is_ipv6;

        /* Get the current time */
        curr_ts = rte_get_timer_cycles();

        /* Check if the object has exceeded the buffer time threshold */
        while ((curr_ts - arrival_ts) <
               (BUFFER_PACKETS_WAIT_TIME * rte_get_timer_hz())) {
            /**
             * We can be smart here and probably sleep here, instead
             * of busy polling. Check rte_delay_us_sleep(). But
             * no need to have that level of complexity here.
             */
            curr_ts = rte_get_timer_cycles();
        }

        /* Process the packet */
        if (!is_ipv6){
            ip_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
                                         sizeof(struct rte_ether_hdr));
            arr_idx = get_arr_idx_from_ip_packet(ip_hdr, false);
        }
        else{
            ip6_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv6_hdr *,
                                         sizeof(struct rte_ether_hdr));
            arr_idx = get_arr_idx_from_ip6_packet(ip6_hdr, false);
        }
        
        /* Store the packet only if the state of the prefix is inactive */
        if (!g_state_arr[arr_idx]) {

            /* Format the packet according to the PCAP format */
            struct rte_mbuf *pcap_mbuf = rte_pcapng_copy(port_id, 0, mbuf,
                                                         pcap_args->pcap_mbuf_pool,
                                                         UINT32_MAX,
                                                         RTE_PCAPNG_DIRECTION_IN,
                                                         NULL);
            if (pcap_mbuf) {
                /* Write packet to the PCAP file */
                if (rte_pcapng_write_packets(pcap_args->pcap_hdl, &pcap_mbuf, 1) == -1) {
                    printf("Failed to write the packet to the PCAP file\n");
                    rte_pktmbuf_free(pcap_mbuf);
                }
            } else {
                printf("Failed to allocate PCAP mbuf\n");
            }
            captured_pkts++;
            if (captured_pkts % 1000 == 0) {
                printf("Captured %d packets\n", captured_pkts);
                close_pcap_file(pcap_args);

                const char* filename = (PCAP_FILE_NAME + std::to_string(pcap_num) + PCAP_FILE_EXT).c_str();
                pcap_args = pcap_init(filename, port_id);
                pcap_num++;
            }
        }

        /* Free the packet */
        rte_pktmbuf_free(mbuf);
    }

    return 0;
}

/**
 * Loop to run on the first lcore.
 *
 * This loop receives the packets from the network. It either processes the
 * packet immediately or passes the packet to the second lcore for
 * further processing.
 */
int first_half_loop(void *_args) {

    int status;

    struct lcore_args *args = (struct lcore_args *)_args;

    uint16_t port_id = args->port_id;
    uint32_t port_ip = args->port_ip;;
    struct rte_mempool *mbuf_pool = args->mbuf_pool;
    struct rte_ring *mbuf_ring = args->mbuf_ring;

    struct rte_mbuf *mbufs[BURST_SIZE];
    uint32_t nb_rx;
    uint32_t nb_tx;

    struct rte_ether_hdr *eth_hdr;
    struct rte_arp_hdr *arp_hdr;
    struct rte_arp_ipv4 *arp_payload;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_ipv6_hdr *ip6_hdr;
    uint16_t ether_type;

    /* Get the MAC address of the given port */
    struct rte_ether_addr my_mac_addr;
    status = rte_eth_macaddr_get(port_id, &my_mac_addr);
    if (status != 0) {
        rte_exit(EXIT_FAILURE, "Failed to get the MAC address for port %d\n",
                 port_id);
    }

    while (1) {

        /* Receive the packets from the network */
        nb_rx = rte_eth_rx_burst(port_id, 0, mbufs, BURST_SIZE);
	    // printf("Num packets: %d\n", nb_rx);
        /* Poll again if we did not receive any packets */
        if (unlikely(nb_rx <= 0)) {
            continue;
        }

        /* Process each received packet */
        for (int i = 0; i < nb_rx; i++) {
            /* Get the type of ethernet packet */
            eth_hdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr *);
            ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);
            
            // NO NEED FOR ARP, ONLY IPV4 AND IPV6 ARE SHOULD BE RECEIVED AND PROCESSED;
            // WE DO NOT HAVE/NEED AN IP ADDR ON OUR END
            if (ether_type == RTE_ETHER_TYPE_IPV4) {

                /* Get the IP header */
                ip_hdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_ipv4_hdr *,
                                                 sizeof(struct rte_ether_hdr));
                
                if (ip_hdr->next_proto_id != CTL_IP_PROTO) {
                    printf("not ctl\n");
                    /* Set the current timestamp in the packet */
                    struct mbuf_priv_data *pdata = (struct mbuf_priv_data*) rte_mbuf_to_priv(mbufs[i]);
                    pdata->arrival_ts = rte_get_timer_cycles();
                    pdata->is_ipv6 = false;

                    int arr_idx = get_arr_idx_from_ip_packet(ip_hdr, false);
                    if (arr_idx == -1) {
                        rte_pktmbuf_free(mbufs[i]);
                    }
                    else {
                        printf("arr_idx: %d\n", arr_idx);
                        if ((rte_get_timer_cycles() - g_state_last_changed_ts[arr_idx]) >
                            (BUFFER_STATE_UPDATE_TIME * rte_get_timer_hz())) {
                            g_state_arr[arr_idx] = 0;
                        }
                        /* Pass the packet to the second lcore */
                        rte_ring_enqueue(mbuf_ring, mbufs[i]);
                    }
                    

                } else if (ip_hdr->next_proto_id == CTL_IP_PROTO) {

                    int arr_idx = get_arr_idx_from_ip_packet(ip_hdr, true);
                    if (arr_idx == -1) {
                        rte_pktmbuf_free(mbufs[i]);
                    }
                    else {
                        printf("arr_idx: %d\n", arr_idx);
                        g_state_arr[arr_idx] = 1;
                        g_state_last_changed_ts[arr_idx] = rte_get_timer_cycles();
                        rte_pktmbuf_free(mbufs[i]);
                    }
                } else {
                    /* Other un-handled types of IP protos */
                    rte_pktmbuf_free(mbufs[i]);
                }
            } 
            else if (ether_type == RTE_ETHER_TYPE_IPV6){

                /* Get the IP header */
                ip6_hdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_ipv6_hdr *,
                                                 sizeof(struct rte_ether_hdr));

                if (ip6_hdr->proto != CTL_IP_PROTO) {

                    /* Set the current timestamp in the packet */
                    struct mbuf_priv_data *pdata = (struct mbuf_priv_data*) rte_mbuf_to_priv(mbufs[i]);
                    pdata->arrival_ts = rte_get_timer_cycles();
                    pdata->is_ipv6 = true;

                    int arr_idx = get_arr_idx_from_ip6_packet(ip6_hdr, false);
                    if (arr_idx == -1) {
                        rte_pktmbuf_free(mbufs[i]);
                    }
                    else {
                        /* Set the state of the array to inactive only if enough seconds have passed
                        since its last update to active*/
                        if ((rte_get_timer_cycles() - g_state_last_changed_ts[arr_idx]) >
                            (BUFFER_STATE_UPDATE_TIME * rte_get_timer_hz())) {
                            g_state_arr[arr_idx] = 0;
                        }
                        printf("arr_idx: %d\n", arr_idx);
                        /* Pass the packet to the second lcore */
                        rte_ring_enqueue(mbuf_ring, mbufs[i]);
                    }

                } else if (ip6_hdr->proto == CTL_IP_PROTO) {

                    int arr_idx = get_arr_idx_from_ip6_packet(ip6_hdr, true);
                    if (arr_idx == -1){
                        rte_pktmbuf_free(mbufs[i]);
                    }
                    else {
                        printf("arr_idx: %d\n", arr_idx);
                        g_state_arr[arr_idx] = 1;
                        g_state_last_changed_ts[arr_idx] = rte_get_timer_cycles();
                        rte_pktmbuf_free(mbufs[i]);
                    }
                } else {
                    /* Other un-handled types of IP protos */
                    rte_pktmbuf_free(mbufs[i]);
                }

            }
            else {
                /* Other un-handled types of ethernet types */
                rte_pktmbuf_free(mbufs[i]);
            }
        }
    }

    return 0;
}

/**
 * Entry function
 */
int main(int argc, char *argv[]) {

    char *port_name;
    uint32_t port_ip;
    uint16_t port_id;
    struct rte_mempool *mbuf_pool;
    struct rte_ring *mbuf_ring;
    uint32_t first_lcore_id;
    uint32_t second_lcore_id;
    struct lcore_args args;
    int pcap_fd;
    rte_pcapng_t *pcap_hdl;
    struct rte_mempool *pcap_mbuf_pool;

    /* Initialize the DPDK environment */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }

    /* Skip the parsed arguments */
    argc -= ret;
    argv += ret;

    /* Check if at least two lcores are provided */
    if (rte_lcore_count() != 2) {
        rte_exit(EXIT_FAILURE, "Need exactly two lcores to run this application\n");
    }

    /* Parse the command line arguments */
    if (argc < 3) {
        rte_exit(EXIT_FAILURE, "Usage: sudo ./delayed_capture.c <DPDK EAL args...> "
                 "<iface PCI address> <iface IP address>\n");
    }

    /* Get the port ID of the required network interface */
    port_name = argv[1];
    if (rte_eth_dev_get_port_by_name(port_name, &port_id)) {
        rte_exit(EXIT_FAILURE, "Cannot find port - %s\n", port_name);
    }
    printf("Port name %s corresponds to port ID %d\n", port_name, port_id);

    /* Get the port's IP address */
    if (inet_pton(AF_INET, argv[2], &port_ip) != 1) {
        rte_exit(EXIT_FAILURE, "Invalid IPv4 address: %s\n", argv[2]);
    }
    printf("Port %s binded with IP %s\n", port_name, argv[2]);

    /* Allocate the memory for the packet buffers */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", MBUF_POOL_SIZE,
                                        0, sizeof(struct mbuf_priv_data),
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (!mbuf_pool) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    }
    printf("Allocated the memory for packet buffers\n");

    /* Allocate a ring to hold the mbufs being passed between the two lcores */
    mbuf_ring = rte_ring_create("MBUF_RING", MBUF_RING_SIZE, rte_socket_id(),
                                RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (!mbuf_ring) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf ring\n");
    }
    printf("Created the ring for the mbufs\n");

    /* Initialize the given port */
    if (port_init(port_id, mbuf_pool)) {
        rte_exit(EXIT_FAILURE, "Failed to initialize port %s\n", port_name);
    }

    printf("Initialized port %s\n", port_name);

    /* Initialize the PCAP file */
    const char* filename = (PCAP_FILE_NAME + std::to_string(0) + PCAP_FILE_EXT).c_str();
    printf("Opening the packet capture file %s\n", filename);
    struct pcap_args *first_pcap_args = pcap_init(filename, port_id);
    printf("Initialized the packet capture structures\n");

    /* Get the lcore IDs */
    first_lcore_id = rte_get_next_lcore(LCORE_ID_ANY, 0, 1);
    second_lcore_id = rte_get_next_lcore(first_lcore_id, 0, 1);

    /* Initialize the arguments to the lcore functions */
    args.port_id = port_id;
    args.port_ip = port_ip;
    args.mbuf_pool = mbuf_pool;
    args.mbuf_ring = mbuf_ring;
    args.pcap_args = first_pcap_args;

    /* Read the mapping of IP addresses to indices */
    read_mapping("prefixes.txt");

    /* Launch the work on the second lcore */
    if (rte_eal_remote_launch(second_half_loop, &args, second_lcore_id)) {
        rte_exit(EXIT_FAILURE, "Failed to launch work on lcore %d\n",
                 second_lcore_id);
    }
    printf("Launched the second half of the loop on lcore %d\n",
           second_lcore_id);

    /* Launch the work on the first (main) lcore */
    printf("Launching the first half of the loop on lcore %d\n",
           first_lcore_id);
    first_half_loop(&args);

    /* Wait for the lcores */
    rte_eal_wait_lcore(second_lcore_id);

    /* Close pcap file */
    close_pcap_file(first_pcap_args);
    delete first_pcap_args;

    /* Deinitialize the port */
    if (port_deinit(port_id)) {
        rte_exit(EXIT_FAILURE, "Failed to de-initialize port %s\n", port_name);
    }
    printf("De-initialized port %s\n", port_name);

    /* Deinitialize the mbuf ring */
    rte_ring_free(mbuf_ring);
    printf("Destroyed the ring for mbufs\n");

    /* Deinitialize the mbuf pool */
    rte_mempool_free(mbuf_pool);
    printf("Freed the memory for packet buffers\n");

    /* Clean up the DPDK environment */
    rte_eal_cleanup();

    return 0;
}
