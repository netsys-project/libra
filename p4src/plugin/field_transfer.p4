/*******************************************************************************
 *                          FIELD TRANSFER
 * The author of this program is cuipenglai (ICT)
 ******************************************************************************/

control Field_transfer(
        inout header_t hdr) {
    action dstId_to_dstIp_action(ipv4_addr_t dst_ip){
        hdr.ipv4.dst_addr = dst_ip;
    }
    table dstId_to_dstIp_table{
        key = {
            hdr.internal.dst_id : exact;
        }
        actions = {
            dstId_to_dstIp_action;
        }
        size = 256; 
    }
    action dstId_to_dstMac_action(mac_addr_t dst_mac){
        hdr.ethernet.dst_addr = dst_mac;
    }
    table dstId_to_dstMac_table{
        key = {
            hdr.internal.dst_id : exact;
        }
        actions = {
            dstId_to_dstMac_action;
        }
        size = 256;
    }

    apply {
        hdr.ipv4.src_addr     = SWITCH_IPV4_ADDR; 
        dstId_to_dstIp_table.apply();
        hdr.ethernet.src_addr = SWITCH_MAC_ADDR;
        dstId_to_dstMac_table.apply();
        hdr.tcp.src_port      = SWITCH_TCP_PORT;        
    }
}