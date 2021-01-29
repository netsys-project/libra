/*************************************************************************************
 *                          DATA AND CONNECTION AGGREGATION
 * The author of this program is cuipenglai (ICT)
 * this program realized the following two functions:
 *      1) tcp connection aggregation: 
 *         we deploy a modified TCP stack in control plane to complete three handshakes.
 *         And we exploit stateful resources in P4 switch to store connection state.
 *         Thus we can complete data packet forward in dataplane.
 *      2) data aggregation: 
 *         we devide data into two categories, hot data and cold data. For cold data,
 *         we forward it according to id in internal header (a self-defined header).
 *         For hot data, we accumulate and it in switch registers until arrive a fixed
 *         number of times.  
 ************************************************************************************/
#include <tna.p4>
#include "common/headers.p4"
#include "common/util.p4"
#include "common/parser.p4"
#include "plugin/field_transfer.p4"

control SwitchIngress(
    inout header_t hdr,
    inout metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    Register<no_related_t, no_register_index_related_t>(NO_REGISTER_INSTANCE_COUNTER) seq_register;
    RegisterAction<no_related_t, no_register_index_related_t, no_related_t>(seq_register) seq_register_update = {
        void apply(inout no_related_t origin_seqNo, out no_related_t out_seqNo) {
            out_seqNo = origin_seqNo;
            origin_seqNo = origin_seqNo + ig_md.ca_md.tcp_data_len;
        }
    };
    action seq_register_update_action(no_register_index_related_t index){
        ig_md.ca_md.seqNo = seq_register_update.execute(index);
    }
    RegisterAction<no_related_t, no_register_index_related_t, no_related_t>(seq_register) seq_register_read = {
        void apply(inout no_related_t origin_seqNo, out no_related_t out_seqNo) {
            out_seqNo = origin_seqNo;
        }
    };
    action seq_register_read_action(no_register_index_related_t index){
        ig_md.ca_md.seqNo = seq_register_read.execute(index);
    }
    RegisterAction<no_related_t, no_register_index_related_t, no_related_t>(seq_register) seq_register_init = {
        void apply(inout no_related_t origin_seqNo, out no_related_t out_seqNo) {
            origin_seqNo = hdr.tcp.seq_no+1;
        }
    };
    action seq_register_init_action(no_register_index_related_t index){
        seq_register_init.execute(index);
    }
    Register<no_related_t, no_register_index_related_t>(NO_REGISTER_INSTANCE_COUNTER) ack_register;
    RegisterAction<no_related_t, no_register_index_related_t, no_related_t>(ack_register) ack_register_init = {
        void apply(inout no_related_t origin_ackNo, out no_related_t out_ackNo) {
            origin_ackNo = hdr.tcp.ack_no;
        }
    };
    action ack_register_init_action(no_register_index_related_t index){
        ack_register_init.execute(index);
    }
    RegisterAction<no_related_t, no_register_index_related_t, no_related_t>(ack_register) ack_register_read = {
        void apply(inout no_related_t origin_ackNo, out no_related_t out_ackNo) {
            out_ackNo = origin_ackNo;
        }
    };
    action ack_register_read_action(no_register_index_related_t index){
        ig_md.ca_md.ackNo = ack_register_read.execute(index);
    }
    Register<st_counter_t, st_index_t>(ST_INSTANCE_COUNTER) st_counter_register;
    RegisterAction<st_counter_t, _, bool_var_t>(st_counter_register) st_counter_update = {
        void apply(inout st_counter_t count, out bool_var_t flag) {
            if (count < NUM_WORKER-1){
                count = count+1;
                flag  = FALSE;
            }
            else {
                count = 0;
                flag  = TRUE;
            }
        }
    };
    action st_counter_action(){
        ig_md.st_md.is_aggregated_condition_meet = st_counter_update.execute(ig_md.index);
    }
    Register<ps_value_t, st_index_t>(ST_INSTANCE_COUNTER) st_value_register;
    RegisterAction<ps_value_t, st_index_t, ps_value_t>(st_value_register) st_value_update = {
        void apply(inout ps_value_t origin_value, out ps_value_t out_value) {
            out_value = origin_value+hdr.pslite_value.value;
            if(ig_md.st_md.is_aggregated_condition_meet == TRUE){
                origin_value = 0;
            }
            else{
                origin_value = out_value;
            }
            
        }
    };
    action st_value_update_action(){
        hdr.bridge_mirror.value = st_value_update.execute(ig_md.index);
    }
    action forward_data_packet_action(packet_type_t packet_type, bool_var_t is_malloc_seqno){
        ig_md.packet_type = packet_type;
        ig_md.is_malloc_seqno = is_malloc_seqno;
    }
    action direct_send_to_cpu_action(packet_type_t packet_type){
        ig_md.packet_type = packet_type;
    }
    action first_arrive_hot_data_action(packet_type_t packet_type){
        ig_md.packet_type = packet_type;
    }
    action arrived_hot_data_action(packet_type_t packet_type){
        ig_md.packet_type = packet_type;
    }
    action retransmission_hot_data_match_action(packet_type_t packet_type){
        ig_md.packet_type = packet_type;
    }
    table data_branch_selection_table {
        key = {
           hdr.tcp.flags : exact;
           ig_intr_md.ingress_port : exact;
        }
        actions = {
            forward_data_packet_action;
            direct_send_to_cpu_action;
            first_arrive_hot_data_action;
            arrived_hot_data_action();
            retransmission_hot_data_match_action();   
        }
        size = 4096; 
    }
    action control_packet_action(packet_type_t packet_type){
        ig_md.packet_type = packet_type;
    }
    action bypass_packet_action(packet_type_t packet_type, bool_var_t is_syn_packet){
        ig_md.packet_type   = packet_type;
        ig_md.is_syn_packet = is_syn_packet;
    }
    table control_branch_selection_table {
        key = {
           hdr.tcp.ctrl_b : exact;
           hdr.tcp.flags  : exact;
        }
        actions = {
            control_packet_action;
            bypass_packet_action;
        }

        size = 256; 
    }

    action ip_forward_action(bit<9> egress_port){
        ig_tm_md.ucast_egress_port = egress_port;
    }

    table ip_forward_table{
        key = {
            hdr.ipv4.dst_addr : exact;
        }
        actions ={
            ip_forward_action;
        }
        size = 256; 
    }

    action dst_ip_to_register_index_action(no_register_index_related_t register_index){
        ig_md.ca_md.register_index = register_index;
    }
    @pragma stage 2
    table dst_ip_to_register_index_table{
        key = {
            hdr.ipv4.dst_addr : exact;
        }
        actions = {
            dst_ip_to_register_index_action;
        }       
        size = 256;  
    }

    action src_ip_to_register_index_action(no_register_index_related_t register_index){
        ig_md.ca_md.register_index = register_index;
    }

    table src_ip_to_register_index_table{
        key = {
            hdr.ipv4.src_addr : exact;
        }
        actions = {
            src_ip_to_register_index_action;
        }       
        size = 256;  
    }

    action dstId_to_egress_port_action(PortId_t egress_port){
        ig_tm_md.ucast_egress_port = egress_port;
    }

    table dstId_to_egress_port_table{
        key = {
            hdr.internal.dst_id : exact;
        }
        actions = {
            dstId_to_egress_port_action;
        }
        size = 256; 
    }

    action srcId_to_egress_port_1_action(PortId_t egress_port){
        ig_tm_md.ucast_egress_port = egress_port;
    }

    table srcId_to_egress_port_1_table{
        key = {
            hdr.internal.src_id : exact;
        }
        actions = {
            srcId_to_egress_port_1_action;
        }
        size = 256; 
    }
    action srcId_to_egress_port_2_action(PortId_t egress_port){
        ig_tm_md.ucast_egress_port = egress_port;
    }

    table srcId_to_egress_port_2_table{
        key = {
            hdr.internal.src_id : exact;
        }
        actions = {
            srcId_to_egress_port_2_action;
        }
        size = 256; 
    }

    action hotdata_key_to_index_action(st_index_t index){
        ig_md.index = index;
    }

    table hotdata_key_to_index_table {
        key = {
            hdr.pslite_key.key : exact;
        }
        actions = {
            hotdata_key_to_index_action;
        }
        size = 32768; 
    }

    action get_packet_length_action(bit<32> tcp_data_len){
        ig_md.ca_md.tcp_data_len = tcp_data_len;
    }

    table get_packet_length_table{
        key ={
            hdr.ipv4.total_len  :exact;
            hdr.tcp.data_offset :exact;
        }
        actions = {
            get_packet_length_action();
        }
        size = 400000;
    }
    action set_ack_packet_action(){
        hdr.tcp.ack_no  = hdr.tcp.seq_no + ig_md.ca_md.tcp_data_len;
        hdr.tcp.seq_no  = ig_md.ca_md.seqNo;
        hdr.tcp.ctrl_f  = 0x1;
        hdr.tcp.ctrl_b  = 0x0;
    }
    apply {
        if(hdr.internal.isValid()){
            get_packet_length_table.apply();
            data_branch_selection_table.apply();
            /************************************************************************************
                * This branch handles the following three conditions:
                *   1. cold data packet
                *   2. retransmission cold data packet which has been sent to cpu but unmatched
                *   3. retransmission cold data packet which has been sent to cpu and match
                *      (don't need malloc seqNo) 
                *   4. aggregated hot data packet
                *   5. retransmission aggregated hot data packet (don't need malloc seqNo)
                **********************************************************************************/
            if(ig_md.packet_type == FORWARD_DATA_PACKET){
                hdr.bridge_mirror.setValid();
                hdr.bridge_mirror.packet_type = FORWARD_DATA_PACKET;                                                                                                                                                                                                                                                    
                dstId_to_egress_port_table.apply();
                hdr.bridge_mirror.checksum = ig_md.checksum;
                if(ig_md.is_malloc_seqno == TRUE){
                    ig_md.mirror_id = 100;
                    ig_dprsr_md.mirror_type = CPU_PORT_MIRROR;
                    if(hdr.internal.interNo == 1){
                        hdr.internal.interNo = hdr.tcp.seq_no;
                        ack_register_read_action(hdr.internal.dst_id);
                        seq_register_update_action(hdr.internal.dst_id);
                        hdr.tcp.seq_no = ig_md.ca_md.seqNo;
                        hdr.tcp.ack_no = ig_md.ca_md.ackNo;
                    }
                    else{
                        seq_register_update_action(hdr.internal.dst_id); 
                        ig_md.ca_md.interNo   = hdr.internal.interNo;
                        hdr.internal.interNo  = hdr.tcp.seq_no;
                        hdr.tcp.seq_no        = ig_md.ca_md.seqNo;
                        ig_md.ca_md.ackNo     = hdr.tcp.ack_no;
                        hdr.tcp.ack_no        = ig_md.ca_md.interNo;
                    }
                    hdr.bridge_mirror.seqNo   = hdr.tcp.seq_no;
                    hdr.bridge_mirror.ackNo   = hdr.tcp.ack_no;
                    hdr.bridge_mirror.interNo = hdr.internal.interNo;
                }
            }
            /*******************************************************************************
            * 1. retransmission cold data which has not been sent to cpu. 
            * 2. the ack packet of aggregated packet.
            * 3. retransmission hot data which has not been sent to cpu.
            ******************************************************************************/
            else if(ig_md.packet_type == DIRECT_SEND_TO_CPU){        
                ig_tm_md.ucast_egress_port = CPU_PORT;
                ig_tm_md.bypass_egress     = 1w1;
            }
            /*******************************************************************************
            * 1. hot data packet which arrives P4 switch for the first time. 
            * 2. retransmission hot data which unmatched.
            ******************************************************************************/
            else if(ig_md.packet_type == FIRST_ARRIVE_HOT_DATA){
                seq_register_read_action(hdr.internal.src_id);
                set_ack_packet_action();
                srcId_to_egress_port_1_table.apply();
                hdr.bridge_mirror.setValid();
                ig_md.mirror_id               = 100;
                ig_md.recirculate_mirror_id   = 101;
                ig_dprsr_md.mirror_type       = RECIRCULATE_AND_CPU_PORT_MIRROR;
                hdr.bridge_mirror.checksum    = ig_md.checksum;
                hdr.bridge_mirror.packet_type = FIRST_ARRIVE_HOT_DATA;
            }
            else if(ig_md.packet_type == ARRIVED_HOT_DATA){
                //ig_tm_md.ucast_egress_port  =  192;//= RECIRCULATE_PORT;
                //hdr.ipv4.identification     = (bit<16>)ig_intr_md.ingress_port;
                hotdata_key_to_index_table.apply(); 
                st_counter_action();
                hdr.bridge_mirror.setValid();
                st_value_update_action();
                ig_tm_md.ucast_egress_port        = RECIRCULATE_PORT;
                hdr.bridge_mirror.checksum        = ig_md.checksum;
                hdr.bridge_mirror.packet_type     = ARRIVED_HOT_DATA;
                if(ig_md.st_md.is_aggregated_condition_meet == TRUE){
                    hdr.tcp.flags                 = HOT_AGGREGATED_PACKET;
                    ig_md.recirculate_mirror_id   = 101;
                    ig_dprsr_md.mirror_type       = RECIRCULATE_PORT_MIRROR; 
                }
            }
            else if(ig_md.packet_type == INGRESS_RETRANSMISSION_HOT_DATA_MATCH){
                seq_register_read_action(hdr.internal.src_id);
                set_ack_packet_action(); 
                srcId_to_egress_port_2_table.apply();
                hdr.bridge_mirror.setValid();
                hdr.bridge_mirror.checksum       = ig_md.checksum;
                hdr.bridge_mirror.packet_type    = RETRANSMISSION_HOT_DATA_MATCH; 
            }    
        }
        else{
            control_branch_selection_table.apply();
            if(ig_md.packet_type == BYPASS){
                /*******************************************************************************
                * 1. bypass packet
                * 2. control packet from cpu
                ******************************************************************************/
                ip_forward_table.apply();
                ig_tm_md.bypass_egress = 1w1;
                if(ig_md.is_syn_packet == TRUE){
                    dst_ip_to_register_index_table.apply();
                    ack_register_init_action(ig_md.ca_md.register_index);
                    seq_register_init_action(ig_md.ca_md.register_index);
                }
            }
            else if(ig_md.packet_type == CONTROL){    
                //control packet from zmq
                hdr.bridge_mirror.setValid();
                hdr.bridge_mirror.checksum = ig_md.checksum;
                ig_tm_md.ucast_egress_port = 192;
                src_ip_to_register_index_table.apply();
                hdr.bridge_mirror.register_index = ig_md.ca_md.register_index;
            }               
        }
    }
}

// ---------------------------------------------------------------------------
// Ingress Deparser
// ---------------------------------------------------------------------------
control SwitchIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md
        ) {
    Mirror() recirculate_mirror;
    Mirror() cpu_mirror; 
    apply {
        if(ig_dprsr_md.mirror_type == CPU_PORT_MIRROR){
            cpu_mirror.emit<bridge_mirror_h>(ig_md.mirror_id,hdr.bridge_mirror);
        }
        else if(ig_dprsr_md.mirror_type == RECIRCULATE_PORT_MIRROR){
            recirculate_mirror.emit<bridge_mirror_h>(ig_md.recirculate_mirror_id,hdr.bridge_mirror);
        }
        else if(ig_dprsr_md.mirror_type == RECIRCULATE_AND_CPU_PORT_MIRROR){
            recirculate_mirror.emit<bridge_mirror_h>(ig_md.recirculate_mirror_id,hdr.bridge_mirror);
            cpu_mirror.emit<bridge_mirror_h>(ig_md.mirror_id,hdr.bridge_mirror);
        }
        pkt.emit(hdr);
    }
}

control EgressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in egress_metadata_t eg_md,
        in egress_intrinsic_metadata_for_deparser_t eg_intr_md_for_dprsr) {
    Checksum() ipv4_checksum;
    Checksum() tcp_csum;
    apply {
        hdr.ipv4.hdr_checksum = ipv4_checksum.update({
            hdr.ipv4.version,
            hdr.ipv4.ihl,
            hdr.ipv4.diffserv,
            hdr.ipv4.total_len,
            hdr.ipv4.identification,
            hdr.ipv4.flags,
            hdr.ipv4.frag_offset,
            hdr.ipv4.ttl,
            hdr.ipv4.protocol,
            hdr.ipv4.src_addr,
            hdr.ipv4.dst_addr});       
        hdr.tcp.checksum = tcp_csum.update({
            hdr.ipv4.src_addr,
            hdr.ipv4.dst_addr,
            hdr.tcp.src_port,
            hdr.tcp.dst_port,
            hdr.tcp.seq_no,
            hdr.tcp.ack_no,
            hdr.tcp.data_offset,
            hdr.tcp.flags,
            hdr.tcp.ctrl_f,
            hdr.tcp.ctrl_b,
            hdr.tcp_option,
            hdr.internal,
            hdr.pslite_route_and_meta_header,
            hdr.pslite_key_header,
            hdr.pslite_key,
            hdr.pslite_value_header,
            hdr.pslite_value,
            eg_md.checksum});
        pkt.emit(hdr);
    }
   
}

control Egress(
        inout header_t hdr,
        inout egress_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_md_for_dprsr,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {
    Field_transfer() field_transfer_1;

    Register<tcp_port_t, bit<16>>(NO_REGISTER_INSTANCE_COUNTER) port_register;
    RegisterAction<tcp_port_t, bit<16>, tcp_port_t>(port_register) port_register_read = {
        void apply(inout tcp_port_t origin_port, out tcp_port_t out_port) {
            out_port = origin_port;
        }
    };
    action port_register_read_action(no_register_index_related_t index){
        hdr.tcp.dst_port = port_register_read.execute(index);
    }

    RegisterAction<tcp_port_t, bit<16>, tcp_port_t>(port_register) port_register_init = {
        void apply(inout tcp_port_t origin_port, out tcp_port_t out_port) {
            origin_port = hdr.tcp.src_port;
        }
    };
    action port_register_init_action(no_register_index_related_t index){
        port_register_init.execute(index);
    }
    action ipv4_and_tcp_header_len_action(bit<16> total_len){
        hdr.ipv4.total_len = total_len;
    }

    table ipv4_and_tcp_header_len_table{
        key = {
            hdr.tcp.data_offset : exact;
        }
        actions = {
            ipv4_and_tcp_header_len_action;
        }
        const entries = {
            (0x5) : ipv4_and_tcp_header_len_action(52);
            (0x6) : ipv4_and_tcp_header_len_action(56);
        }
        size = 64; 
    }
    action pslite_aggregated_packet_len_action(bit<16> total_len){
        hdr.ipv4.total_len = total_len;
    }

    table pslite_aggregated_packet_len_table{
        key = {
            hdr.tcp.data_offset                : exact;
            //hdr.pslite_value_header.value_size : exact;
        }
        actions = {
            pslite_aggregated_packet_len_action;
        }
        /*const entries = {
            (0x5) : pslite_aggregated_packet_len_table(124);
            (0x6) : pslite_aggregated_packet_len_table(128);   
        }*/     
        size = 32; 
    }
    action no_transfer_action(packet_type_t packet_type){
        eg_md.packet_type = packet_type;
    }
    action field_transfer_action(packet_type_t packet_type){
        eg_md.packet_type = packet_type;
    }
    action ack_packet_action(packet_type_t packet_type){
        eg_md.packet_type = packet_type;
    }
    action hot_data_recirculate_action(packet_type_t packet_type){
        eg_md.packet_type = packet_type;
    }
    action do_nothing(){
        eg_md.packet_type = DO_NOTHING;
    }
    table egress_branch_selection_table {
        key = {
           hdr.bridge_mirror.packet_type : exact;
           eg_intr_md.egress_port        : exact;
        }
        actions = {
            no_transfer_action();
            field_transfer_action();
            ack_packet_action();
            hot_data_recirculate_action();
            do_nothing();
        }
        default_action = do_nothing;
        size = 8192; 
    }
    apply {
        if(hdr.internal.isValid()) {
            egress_branch_selection_table.apply();
            if(eg_md.packet_type == NO_TRANSFER){
                hdr.tcp.seq_no        = hdr.bridge_mirror.seqNo;
                hdr.tcp.ack_no        = hdr.bridge_mirror.ackNo;
                hdr.internal.interNo  = hdr.bridge_mirror.interNo;                    
            }
            else if(eg_md.packet_type == FIELD_TRANSFER) {
                field_transfer_1.apply(hdr);
                port_register_read_action(hdr.internal.dst_id);
            }
            else if(eg_md.packet_type == ACK_PACKET) {
                hdr.ipv4.dst_addr     = hdr.ipv4.src_addr;
                hdr.ipv4.src_addr     = SWITCH_IPV4_ADDR; 
                hdr.ethernet.dst_addr = hdr.ethernet.src_addr;
                hdr.ethernet.src_addr = SWITCH_MAC_ADDR;
                hdr.tcp.src_port      = SWITCH_TCP_PORT; 
                port_register_read_action(hdr.internal.src_id);
                ipv4_and_tcp_header_len_table.apply();
                hdr.internal.dst_id = hdr.internal.src_id;
                hdr.internal.src_id = SERVER_ID;
                eg_md.chksum_type   = ACK_PACKET_CHECKSUM;  
                hdr.pslite_route_and_meta_header.setInvalid();
                hdr.pslite_key_header.setInvalid();
                hdr.pslite_key.setInvalid();
                hdr.pslite_key_1.setInvalid();
                hdr.pslite_key_2.setInvalid();
                hdr.pslite_key_3.setInvalid();
                hdr.pslite_key_4.setInvalid();
                hdr.pslite_key_5.setInvalid();
                hdr.pslite_key_6.setInvalid();
                hdr.pslite_key_7.setInvalid();
                hdr.pslite_key_8.setInvalid();
                hdr.pslite_key_9.setInvalid();
                hdr.pslite_value_header.setInvalid();
                hdr.pslite_value.setInvalid();
            }
            else if(eg_md.packet_type == HOT_DATA_RECIRCULATE){
                if(hdr.tcp.flags == HOT_AGGREGATED_PACKET){
                    hdr.pslite_key_header.key_size     = 1;  
                    hdr.pslite_value_header.value_size = 4;
                    hdr.pslite_value.value             = hdr.bridge_mirror.value;
                    hdr.pslite_key_1.setInvalid();
                    hdr.pslite_key_2.setInvalid();
                    hdr.pslite_key_3.setInvalid();
                    hdr.pslite_key_4.setInvalid();
                    hdr.pslite_key_5.setInvalid();
                    hdr.pslite_key_6.setInvalid();
                    hdr.pslite_key_7.setInvalid();
                    hdr.pslite_key_8.setInvalid();
                    hdr.pslite_key_9.setInvalid();
                    pslite_aggregated_packet_len_table.apply();   
                }
                else{
                    hdr.pslite_key_header.key_size = hdr.pslite_key_header.key_size-1;
                    hdr.pslite_value_header.value_size = hdr.pslite_value_header.value_size-4;
                    hdr.pslite_key.setInvalid();
                    hdr.pslite_value.setInvalid();
                }
            }
            else{}
        }
        else{
            if(hdr.tcp.flags == CONTROL){
                port_register_init_action(hdr.bridge_mirror.register_index);
            }
            else{}
        }
        eg_md.checksum = hdr.bridge_mirror.checksum;
        hdr.bridge_mirror.setInvalid();
    }
}

Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         EgressParser(),
         Egress(),
         EgressDeparser()) pipe;
Switch(pipe) main;