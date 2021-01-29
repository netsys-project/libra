/*******************************************************************************
 * BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
 *
 * Copyright (c) 2019-present Barefoot Networks, Inc.
 *
 * All Rights Reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of
 * Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
 * technical concepts contained herein are proprietary to Barefoot Networks, Inc.
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination of
 * this information or reproduction of this material is strictly forbidden unless
 * prior written permission is obtained from Barefoot Networks, Inc.
 *
 * No warranty, explicit or implicit is provided, unless granted under a written
 * agreement with Barefoot Networks, Inc.
 *
 ******************************************************************************/

#ifndef _HEADERS_
#define _HEADERS_
//no refers to seqNo, ackNo, and interNo                   
typedef bit<32> no_related_t;
typedef bit<16> no_register_index_related_t;
const   bit<32> NO_REGISTER_INSTANCE_COUNTER  = 1024;
typedef bit<32> pslite_size_t;

typedef bit<16>  pslite_flag_t;
typedef bit<32>  ps_value_t;
typedef bit<384> pslite_route_and_meta_length_t;

typedef bit<1> chksum_type_t;
const   chksum_type_t ACK_PACKET_CHECKSUM  = 1; 

const   PortId_t CPU_PORT = 192;
const   PortId_t RECIRCULATE_PORT  = 196;

typedef bit<16> bool_var_t; 
const   bool_var_t TRUE  = 1;
const   bool_var_t FALSE = 0;

/*******************************************************************************
 * packet type is a very important flag. We determine how to handle packet 
 * according to this flag. 
 * we infer packet type from the reserved field of tcp and ingress port.
 ******************************************************************************/
typedef bit<4> packet_type_t;
const packet_type_t BYPASS                           = 8;
const packet_type_t CONTROL                          = 0;
const packet_type_t SYN_FROM_CPU                     = 3;
const packet_type_t COLD_DATA                        = 4;
const packet_type_t RETRANSMISSION_COLD_DATA_UNCPU   = 6;    //ingress port != 192
const packet_type_t RETRANSMISSION_COLD_DATA_UNMATCH = 6;    //ingress port == 192
const packet_type_t RETRANSMISSION_COLD_DATA_MATCH   = 4;    //ingress port == 192
const packet_type_t CONTROL_FROM_CPU                 = 1;
const packet_type_t FIRST_ARRIVE_HOT_DATA            = 7;
const packet_type_t RETRANSMISSION_HOT_DATA_UNCPU    = 2;
const packet_type_t RETRANSMISSION_HOT_DATA_MATCH    = 7;
const packet_type_t RETRANSMISSION_HOT_DATA_UNMATCH  = 2;
const packet_type_t RETRANSMISSION_HOT_AGGREGATED    = 12;
const packet_type_t HOT_AGGREGATED_ACK               = 13;
const packet_type_t HOT_AGGREGATED_PACKET            = 14;

/*******************************************************************************
 * The following packet types are about how ingress pipeline deals with packets
 ******************************************************************************/
const packet_type_t ARRIVED_HOT_DATA                         = 5;
const packet_type_t DIRECT_SEND_TO_CPU                       = 6; 
const packet_type_t FORWARD_DATA_PACKET                      = 4;
const packet_type_t ACK_NEED_FIELD_TRANSFER                  = 7;
const packet_type_t INGRESS_RETRANSMISSION_HOT_DATA_MATCH    = 3;

/*******************************************************************************
 * The following packet types are about how egress pipeline deals with packets
 ******************************************************************************/
const packet_type_t NO_TRANSFER                      = 1; 
const packet_type_t FIELD_TRANSFER                   = 2;
const packet_type_t ACK_PACKET                       = 3;
const packet_type_t HOT_DATA_RECIRCULATE             = 4;
const packet_type_t AGGREGATE_PACKET                 = 5;
const packet_type_t DO_NOTHING                       = 6;
const packet_type_t MIRROR_TO_CPU                    = 7;

const   bit<32>    ST_INSTANCE_COUNTER_BIT_WIDTH     = 16; 
const   bit<32>    ST_INSTANCE_COUNTER               = 65536;           
typedef bit<16>    st_index_t;
typedef bit<32>    st_counter_t;

typedef bit<3> mirror_type_t;
const mirror_type_t CPU_PORT_MIRROR                  = 1;
const mirror_type_t RECIRCULATE_PORT_MIRROR          = 2;
const mirror_type_t RECIRCULATE_AND_CPU_PORT_MIRROR  = 3;

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;
typedef bit<16> tcp_port_t;
typedef bit<16> ether_type_t;

/*******************************************************************************
 * static network configuration
 ******************************************************************************/
typedef bit<64>   pslite_key_t;
const ipv4_addr_t  SWITCH_IPV4_ADDR =  0xaca80148;
const mac_addr_t   SWITCH_MAC_ADDR  =  0x000200000300;
const tcp_port_t   SWITCH_TCP_PORT  =  0x1388;
const no_register_index_related_t SERVER_ID = 8; 
const pslite_size_t  PSLITE_KEY_SIZE        = 64;
const pslite_size_t  PSLITE_VALUE_SIZE      = 32;
const st_counter_t NUM_WORKER               = 10;


const ether_type_t ETHERTYPE_IPV4 = 16w0x0800;
const ether_type_t ETHERTYPE_ARP  = 16w0x0806;
const ether_type_t ETHERTYPE_IPV6 = 16w0x86dd;
const ether_type_t ETHERTYPE_VLAN = 16w0x8100;
 
typedef bit<8> ip_protocol_t;
const ip_protocol_t IP_PROTOCOLS_ICMP = 1;
const ip_protocol_t IP_PROTOCOLS_TCP  = 6;
const ip_protocol_t IP_PROTOCOLS_UDP  = 17;

header ethernet_h {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> ether_type;
}

header ipv4_h {
    bit<4>  version;
    bit<4>  ihl;
    bit<8>  diffserv;
    bit<16> total_len;
    bit<16> identification;
    bit<3>  flags;
    bit<13> frag_offset;
    bit<8>  ttl;
    bit<8>  protocol;
    bit<16> hdr_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}

header tcp_h {
    tcp_port_t src_port;
    tcp_port_t dst_port;
    no_related_t seq_no;
    no_related_t ack_no;
    bit<4>  data_offset;
    bit<4>  flags;
    bit<4>  ctrl_f;
    bit<4>  ctrl_b;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}
header tcp_option_h {
    bit<32> pad;
}

header internal_h {
    no_register_index_related_t dst_id;
    no_register_index_related_t src_id;
    no_related_t interNo;
    no_related_t tunnel_no;
}
header pslite_route_and_meta_header_h {
    /*********************************************************************
    * This header contains pslite router and metadata fields.
    * We don't need to handle these fields. 
    **********************************************************************/ 
    pslite_route_and_meta_length_t padding;
}
header pslite_key_header_h {
    pslite_flag_t  flag;
    pslite_size_t  key_size;
}
header pslite_key_h {
    pslite_key_t  key;
}
/*********************************************************************
* The variable-width header function is not well implemented by tofino,
* so we have to achieve this function by listing all possible situations. 
**********************************************************************/ 
header pslite_key_1_h {
    pslite_key_t  key_1;
}
header pslite_key_2_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
}
header pslite_key_3_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
    pslite_key_t  key_3;
}
header pslite_key_4_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
    pslite_key_t  key_3;
    pslite_key_t  key_4;
}
header pslite_key_5_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
    pslite_key_t  key_3;
    pslite_key_t  key_4;
    pslite_key_t  key_5;
}
header pslite_key_6_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
    pslite_key_t  key_3;
    pslite_key_t  key_4;
    pslite_key_t  key_5;
    pslite_key_t  key_6;
}
header pslite_key_7_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
    pslite_key_t  key_3;
    pslite_key_t  key_4;
    pslite_key_t  key_5;
    pslite_key_t  key_6;
    pslite_key_t  key_7;
}
header pslite_key_8_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
    pslite_key_t  key_3;
    pslite_key_t  key_4;
    pslite_key_t  key_5;
    pslite_key_t  key_6;
    pslite_key_t  key_7;
    pslite_key_t  key_8;
}
header pslite_key_9_h {
    pslite_key_t  key_1;
    pslite_key_t  key_2;
    pslite_key_t  key_3;
    pslite_key_t  key_4;
    pslite_key_t  key_5;
    pslite_key_t  key_6;
    pslite_key_t  key_7;
    pslite_key_t  key_8;
    pslite_key_t  key_9;
}
header pslite_value_header_h {
    pslite_flag_t  flag;
    pslite_size_t  value_size;
}
header pslite_value_h {
    ps_value_t value;
}

header recirculate_meta_h{
    /*********************************************************************
    * This header is used to transporting metadata for recirculation
    **********************************************************************/
    bit<16>                     checksum; 
}

header bridge_mirror_h{
    /*********************************************************************
    * This header is used to transporting metadata from ingress to egress
    **********************************************************************/
    bit<16>                     checksum; 
    no_register_index_related_t register_index;
    no_related_t                seqNo;
    no_related_t                interNo;
    no_related_t                ackNo;
    packet_type_t               packet_type;
    bit<4>                      padding;
    ps_value_t                  value;
}


struct header_t {
    //recirculate_meta_h        recirculate_meta;
    bridge_mirror_h           bridge_mirror;
    bridge_mirror_h           egress_mirror;
    ethernet_h                ethernet;
    ipv4_h                    ipv4;
    tcp_h                     tcp;
    tcp_option_h              tcp_option;
    internal_h                internal;
    pslite_route_and_meta_header_h     pslite_route_and_meta_header;
    pslite_key_header_h       pslite_key_header;
    pslite_key_h              pslite_key;
    pslite_key_1_h            pslite_key_1;
    pslite_key_2_h            pslite_key_2;
    pslite_key_3_h            pslite_key_3;
    pslite_key_4_h            pslite_key_4;
    pslite_key_5_h            pslite_key_5;
    pslite_key_6_h            pslite_key_6;
    pslite_key_7_h            pslite_key_7;
    pslite_key_8_h            pslite_key_8;
    pslite_key_9_h            pslite_key_9;
    pslite_value_header_h     pslite_value_header;
    pslite_value_h            pslite_value;
}

// ---------------------------------------------------------------------------
//                             Approximate Calculation
// z=x+y 
// frac_z=z, frac_x=|x|, frac_y=|y|
// log_i=log(frac_x), log_j=log(frac_y)
// log_k=log_j-log_i
// log_m=log(±1±2^(j-i))
// n=i+log(±1±2^(j-i))
// sign_z=0x0 if z>0 else sign_z=0x8000  
// info: ___________________________ ______ _____ _________  
//      |  five bits reserved       |  x>0 | y>0 | |x|>|y| |      let 0 be ture, and 1 be false 
//      |___________________________|______|_____|_________|
// flag:
//      x+y= ±2^(i+log(±1±2^(j-i)))
//      the first two bits of flag correspond to the last two ±, and the others are reserved. 
//
//      Only suppport float16 !!!
// ---------------------------------------------------------------------------
struct approximate_calculation_metadata_t {
    bit<16> frac_x;
    bit<16> frac_y;
    bit<16> sign;
    bit<16> frac_z;
    int<16> log_i;
    int<16> log_j;
    int<16> log_k;
    int<16> log_m;
    int<16> n;
    bit<16> sign_z;
    bit<8>  info;
    bit<8>  flag;
}

struct connection_aggregation_metadata_t {
    no_related_t seqNo;
    no_related_t interNo;
    no_related_t ackNo;
    no_register_index_related_t register_index;
    bit<32>      tcp_data_len;
    bit<16>      port;
}

struct storage_metadata_t {
    ps_value_t  value;
    bool_var_t  is_aggregated_condition_meet;
}

struct metadata_t {
    connection_aggregation_metadata_t  ca_md;
    bit<16>                            checksum; 
    bool_var_t                         is_malloc_seqno;
    packet_type_t                      packet_type;
    MirrorId_t                         mirror_id;
    MirrorId_t                         recirculate_mirror_id;
    storage_metadata_t                 st_md;
    bool_var_t                         is_syn_packet;
    bit<16>                            ipv4_and_tcp_header_len;
    bool_var_t                         is_need_ack;
    st_index_t                         index;
}
struct egress_metadata_t {
    bit<16>        checksum; 
    packet_type_t  packet_type;
    bit<4>         padding;
    chksum_type_t  chksum_type;
}

#endif /* _HEADERS_ */
