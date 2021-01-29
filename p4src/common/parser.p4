// ---------------------------------------------------------------------------
// Ingress and egress parser
// ---------------------------------------------------------------------------
parser PsliteParser(
        packet_in pkt,
        out header_t hdr){
    state start {
        pkt.extract(hdr.pslite_key_header);
        transition select(hdr.pslite_key_header.key_size){
            0       : reject;
            default : parse_pslite_key;
        }
    }
    state parse_pslite_key {
        pkt.extract(hdr.pslite_key);
        transition select(hdr.pslite_key_header.key_size){
            1       : parse_pslite_value_header;
            2       : parse_pslite_key_1;
            3       : parse_pslite_key_2;
            4       : parse_pslite_key_3;
            5       : parse_pslite_key_4;
            6       : parse_pslite_key_5;
            7       : parse_pslite_key_6;
            8       : parse_pslite_key_7;
            9       : parse_pslite_key_8;
            10      : parse_pslite_key_9;
            default : reject;
        }
    }
    state parse_pslite_key_1 {
        pkt.extract(hdr.pslite_key_1);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_2 {
        pkt.extract(hdr.pslite_key_2);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_3 {
        pkt.extract(hdr.pslite_key_3);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_4 {
        pkt.extract(hdr.pslite_key_4);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_5 {
        pkt.extract(hdr.pslite_key_5);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_6 {
        pkt.extract(hdr.pslite_key_6);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_7 {
        pkt.extract(hdr.pslite_key_7);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_8 {
        pkt.extract(hdr.pslite_key_8);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_9 {
        pkt.extract(hdr.pslite_key_9);
        transition parse_pslite_value_header;
    }
    state parse_pslite_value_header {
        pkt.extract(hdr.pslite_value_header);
        transition parse_pslite_value;
    }
    state parse_pslite_value {
        pkt.extract(hdr.pslite_value);
        transition accept;
    }    
}

parser SwitchIngressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {

    TofinoIngressParser() tofino_parser;
    PsliteParser() pslite_parser;
    Checksum() tcp_csum;
    state start {
        tofino_parser.apply(pkt, ig_intr_md);
        transition parse_ethernet;
    }
    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            0x0800 : parse_ipv4;
        }
    }
    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        tcp_csum.subtract({hdr.ipv4.src_addr, hdr.ipv4.dst_addr});
        transition select(hdr.ipv4.protocol) {
            0x06 : parse_tcp;
            default : accept;
        }
    }
    state parse_tcp {
        pkt.extract(hdr.tcp);
        tcp_csum.subtract({hdr.tcp.checksum});
        tcp_csum.subtract({ hdr.tcp.src_port, 
                            hdr.tcp.dst_port,
                            hdr.tcp.seq_no,
                            hdr.tcp.ack_no,
                            hdr.tcp.data_offset,
                            hdr.tcp.flags,
                            hdr.tcp.ctrl_f,
                            hdr.tcp.ctrl_b});
        ig_md.checksum = tcp_csum.get();
        transition select(hdr.tcp.data_offset) {
            0x5 : parse_if_internal;
            default : parse_tcp_option;
        }
    }
    state parse_tcp_option {
        pkt.extract(hdr.tcp_option);
        transition select(hdr.tcp.flags) {
            COLD_DATA                      : parse_internal;
            RETRANSMISSION_COLD_DATA_UNCPU : parse_internal;
            FIRST_ARRIVE_HOT_DATA          : parse_internal;
            RETRANSMISSION_HOT_DATA_UNMATCH  : parse_internal;
            HOT_AGGREGATED_PACKET          : parse_internal;
            default : accept;
        }
    }
    state parse_if_internal{
        transition select(hdr.tcp.flags) {
            COLD_DATA                      : parse_internal;
            RETRANSMISSION_COLD_DATA_UNCPU : parse_internal;
            FIRST_ARRIVE_HOT_DATA          : parse_internal;
            RETRANSMISSION_HOT_DATA_UNMATCH  : parse_internal;
            HOT_AGGREGATED_PACKET          : parse_internal;
            default : accept;
        }        
    }
    state parse_internal {
        pkt.extract(hdr.internal);
        transition select(hdr.tcp.ctrl_f) {
            0x1 &&& 0x1: accept;
            default : parse_if_hotdata;
        }    
    }
    state parse_if_hotdata {
        transition select(hdr.tcp.flags) {
            FIRST_ARRIVE_HOT_DATA          : parse_pslite_route_and_meta_header;
            RETRANSMISSION_HOT_DATA_UNMATCH  : parse_pslite_route_and_meta_header;
            HOT_AGGREGATED_PACKET          : parse_pslite_route_and_meta_header;
            default : accept;
        }
    }
    state parse_pslite_route_and_meta_header {
        pkt.extract(hdr.pslite_route_and_meta_header);
        //pslite_parser.apply(pkt, hdr);
        //transition accept;
        transition parser_pslite_key_header;
    }
    state parser_pslite_key_header {
        pkt.extract(hdr.pslite_key_header);
        transition select(hdr.pslite_key_header.key_size){
            0       : reject;
            default : parse_pslite_key;
        }
    }
    state parse_pslite_key {
        pkt.extract(hdr.pslite_key);
        transition select(hdr.pslite_key_header.key_size){
            1       : parse_pslite_value_header;
            2       : parse_pslite_key_1;
            3       : parse_pslite_key_2;
            4       : parse_pslite_key_3;
            5       : parse_pslite_key_4;
            6       : parse_pslite_key_5;
            7       : parse_pslite_key_6;
            8       : parse_pslite_key_7;
            9       : parse_pslite_key_8;
            10      : parse_pslite_key_9;
            default : reject;
        }
    }
    state parse_pslite_key_1 {
        pkt.extract(hdr.pslite_key_1);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_2 {
        pkt.extract(hdr.pslite_key_2);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_3 {
        pkt.extract(hdr.pslite_key_3);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_4 {
        pkt.extract(hdr.pslite_key_4);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_5 {
        pkt.extract(hdr.pslite_key_5);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_6 {
        pkt.extract(hdr.pslite_key_6);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_7 {
        pkt.extract(hdr.pslite_key_7);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_8 {
        pkt.extract(hdr.pslite_key_8);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_9 {
        pkt.extract(hdr.pslite_key_9);
        transition parse_pslite_value_header;
    }
    state parse_pslite_value_header {
        pkt.extract(hdr.pslite_value_header);
        transition parse_pslite_value;
    }
    state parse_pslite_value {
        pkt.extract(hdr.pslite_value);
        transition accept;
    }   
}

parser EgressParser(
        packet_in pkt,
        out header_t hdr,
        out egress_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {
    TofinoEgressParser() tofino_parser;
    PsliteParser() pslite_parser;
    state start {
        tofino_parser.apply(pkt, eg_intr_md);
        transition parse_mirror;
    }

    state parse_mirror {
        pkt.extract(hdr.bridge_mirror);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            0x0800 : parse_ipv4;
        }
    }
    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            0x06 : parse_tcp;
            default : accept;
        }
    }
    state parse_tcp {
        pkt.extract(hdr.tcp);
        transition select(hdr.tcp.data_offset) {
            0x5 : parse_if_internal;
            default : parse_tcp_option;
        }
    }
    state parse_tcp_option {
        pkt.extract(hdr.tcp_option);
        transition select(hdr.tcp.flags) {
            COLD_DATA                      : parse_internal;
            RETRANSMISSION_COLD_DATA_UNCPU : parse_internal;
            FIRST_ARRIVE_HOT_DATA          : parse_internal;
            RETRANSMISSION_HOT_DATA_UNMATCH  : parse_internal;
            HOT_AGGREGATED_PACKET          : parse_internal;
            default : accept;
        }
    }
    state parse_if_internal{
        transition select(hdr.tcp.flags) {
            COLD_DATA                      : parse_internal;
            RETRANSMISSION_COLD_DATA_UNCPU : parse_internal;
            FIRST_ARRIVE_HOT_DATA          : parse_internal;
            RETRANSMISSION_HOT_DATA_UNMATCH  : parse_internal;
            HOT_AGGREGATED_PACKET          : parse_internal;
            default : accept;
        }        
    }
    state parse_internal {
        pkt.extract(hdr.internal);
        transition select(hdr.tcp.ctrl_f) {
            0x1 &&& 0x1: accept;
            default : parse_if_hotdata;
        }    
    }
    state parse_if_hotdata {
        transition select(hdr.tcp.flags) {
            FIRST_ARRIVE_HOT_DATA          : parse_pslite_route_and_meta_header;
            RETRANSMISSION_HOT_DATA_UNMATCH  : parse_pslite_route_and_meta_header;
            HOT_AGGREGATED_PACKET          : parse_pslite_route_and_meta_header;
            default : accept;
        }
    }
    state parse_pslite_route_and_meta_header {
        pkt.extract(hdr.pslite_route_and_meta_header);
        //pslite_parser.apply(pkt, hdr);
        //transition accept;
        transition parser_pslite_key_header;
    }
    state parser_pslite_key_header {
        pkt.extract(hdr.pslite_key_header);
        transition select(hdr.pslite_key_header.key_size){
            0       : reject;
            default : parse_pslite_key;
        }
    }
    state parse_pslite_key {
        pkt.extract(hdr.pslite_key);
        transition select(hdr.pslite_key_header.key_size){
            1       : parse_pslite_value_header;
            2       : parse_pslite_key_1;
            3       : parse_pslite_key_2;
            4       : parse_pslite_key_3;
            5       : parse_pslite_key_4;
            6       : parse_pslite_key_5;
            7       : parse_pslite_key_6;
            8       : parse_pslite_key_7;
            9       : parse_pslite_key_8;
            10      : parse_pslite_key_9;
            default : reject;
        }
    }
    state parse_pslite_key_1 {
        pkt.extract(hdr.pslite_key_1);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_2 {
        pkt.extract(hdr.pslite_key_2);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_3 {
        pkt.extract(hdr.pslite_key_3);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_4 {
        pkt.extract(hdr.pslite_key_4);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_5 {
        pkt.extract(hdr.pslite_key_5);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_6 {
        pkt.extract(hdr.pslite_key_6);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_7 {
        pkt.extract(hdr.pslite_key_7);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_8 {
        pkt.extract(hdr.pslite_key_8);
        transition parse_pslite_value_header;
    }
    state parse_pslite_key_9 {
        pkt.extract(hdr.pslite_key_9);
        transition parse_pslite_value_header;
    }
    state parse_pslite_value_header {
        pkt.extract(hdr.pslite_value_header);
        transition parse_pslite_value;
    }
    state parse_pslite_value {
        pkt.extract(hdr.pslite_value);
        transition accept;
    } 
}