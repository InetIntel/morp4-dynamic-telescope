# DPDK Buffer Packet Capture Application

### Compile the application

Run the following commands to cleanly compile the application
* `make clean`
* `make`

### Running the application

1. Identify which NIC is to be used for the application from `ifconfig`.
2. Get the PCI bus address of that NIC using `ethtool -i <NIC name>`. The PCI bus
   address is given by the `bus-info` key in the output.
3. Identify which 2 CPU cores you want the application to run on. Run `numactl -H` to select two of the available CPU cores.
   Do not pick two hyperthread siblings. Pick two different physical cores. (Always pick the cores from the NUMA node local to the NIC if you have multiple NUMA nodes).
3. Once we know the `<pci bus address>`, `<IP address>` of the required NIC, and the two CPU cores run
   the following command to start the application
   * `sudo ./build/delayed_capture -l 0,1 0000:41:00.0 10.10.1.1` (Here PCI bus address is 0000:41:00.0, IP address of the NIC is 10.10.1.1, and the two cores
     on which we want the application to run on are core 0 and core 1).
4. The application will run forever. To stop the application press `Ctrl+C` and wait for the application to shutdown gracefully.
5. The package capture file will be stored in repository with the name `packets.[PCAP_FILE_INDEX].pcap`.