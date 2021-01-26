#ifndef __LWIP_NETML_H__
#define __LWIP_NETML_H__

#if LWIP_NETML

#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/err.h"
#include "mlib/hmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal header: flags */
//#define TCP_BYPASS	0X8U
//#define TCP_DAT		0X4U
//#define TCP_RET		0X2U
//#define TCP_HOT		0X1U

#define TCP_OFFSET_FLAGS 0xfU

#define NETML_INVALID_ID UINT16_MAX

enum {
	NETML_BYPASS	= 0x08,		// 1000 RX TX
	NETML_CTL		= 0x00,		// 0000 RX TX
	NETML_CTL_SW	= 0x01,		// 0001 RX
	NETML_COLD		= 0x04,		// 0100 RX TX
	NETML_COLD_RE	= 0x06,		// 0110 RX TX
	NETML_HOT		= 0x07,		// 0111    TX
	NETML_HOT_RE	= 0x02,		// 0010    TX
	NETML_AGG_ACK	= 0x0d,		// 1101    TX
	NETML_AGG		= 0x0e		// 1110 RX
};

/* Internal header */
PACK_STRUCT_BEGIN
struct internal_hdr {
  PACK_STRUCT_FIELD(u16_t dst_id);
  PACK_STRUCT_FIELD(u16_t src_id);
  PACK_STRUCT_FIELD(u32_t int_seqno);
  PACK_STRUCT_FIELD(u32_t int_tunlno);
//  PACK_STRUCT_FIELD(u32_t int_flags);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

#define TCPH_OFFSET_FLAGS(phdr) ((lwip_ntohs((phdr)->_hdrlen_rsvd_flags) >> 8) & TCP_OFFSET_FLAGS)

#define TCPH_OFFSET_SETBIT(phdr, offset) \
	(phdr)->_hdrlen_rsvd_flags = ((phdr)->_hdrlen_rsvd_flags | lwip_htons((offset) << 8))

#define TCPH_OFFSET_CLEAR(phdr) \
		(phdr)->_hdrlen_rsvd_flags = ((phdr)->_hdrlen_rsvd_flags & 0xf0ff)

#define INTH_BYPASS(flags)	(flags == NETML_BYPASS)
#define INTH_CTL(flags)		(flags == NETML_CTL || flags == NETML_CTL_SW)

struct kv_pair {
	struct hmap_node node;
	u64_t value;
};

struct tcp_internal_id {
  u32_t nxtwish;
  u32_t intseq;
  u32_t intack;
  u32_t inttunl;
  u16_t inid;
#if TCP_QUEUE_OOSEQ
  struct tcp_seg *ooseq;    /* Received out of sequence segments. */
#endif /* TCP_QUEUE_OOSEQ */
};

#ifdef __cplusplus
}
#endif

#endif /* LWIP_NETML  */

#endif /* __LWIP_NETML_H__ */
