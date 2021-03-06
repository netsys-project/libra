/*
    Copyright (c) 2007-2016 Contributors as noted in the AUTHORS file

    This file is part of libzmq, the ZeroMQ core engine in C++.

    libzmq is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, the Contributors give you permission to link
    this library with independent modules to produce an executable,
    regardless of the license terms of these independent modules, and to
    copy and distribute the resulting executable under terms of your choice,
    provided that you also meet, for each linked independent module, the
    terms and conditions of the license of that module. An independent
    module is a module which is not derived from or based on this library.
    If you modify this library, you must extend this exception to your
    version of the library.

    libzmq is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __ZMQ_TCP_ADDRESS_HPP_INCLUDED__
#define __ZMQ_TCP_ADDRESS_HPP_INCLUDED__

//#if !defined ZMQ_HAVE_WINDOWS
//#include <sys/socket.h>
//#include <netinet/in.h>
//#endif
#include "lwip/sockets.h"

namespace zmq
{

    class tcp_address_t
    {
    public:

        tcp_address_t ();
        tcp_address_t (const lwip_sockaddr *sa, socklen_t sa_len);
        virtual ~tcp_address_t ();

        //  This function translates textual TCP address into an address
        //  structure. If 'local' is true, names are resolved as local interface
        //  names. If it is false, names are resolved as remote hostnames.
        //  If 'ipv6' is true, the name may resolve to IPv6 address.
        int resolve (const char *name_, bool local_, bool ipv6_, bool is_src_ = false);

        //  The opposite to resolve()
        virtual int to_string (std::string &addr_);

#if defined ZMQ_HAVE_WINDOWS
        unsigned short family () const;
#else
        lwip_sa_family_t family () const;
#endif
        const lwip_sockaddr *addr () const;
        socklen_t addrlen () const;

        const lwip_sockaddr *src_addr () const;
        socklen_t src_addrlen () const;
        bool has_src_addr () const;

    protected:
        int resolve_nic_name (const char *nic_, bool ipv6_, bool is_src_ = false);
        int resolve_interface (const char *interface_, bool ipv6_, bool is_src_ = false);
        int resolve_hostname (const char *hostname_, bool ipv6_, bool is_src_ = false);

#if defined ZMQ_HAVE_WINDOWS
		int get_interface_name(unsigned long index, char ** dest) const;
		int wchar_to_utf8(const WCHAR * src, char ** dest) const;
#endif

        union {
            struct lwip_sockaddr generic;
            struct lwip_sockaddr_in ipv4;
//            struct lwip_sockaddr_in6 ipv6;
        } address;

        union {
            struct lwip_sockaddr generic;
            struct lwip_sockaddr_in ipv4;
//            struct lwip_sockaddr_in6 ipv6;
        } source_address;
        bool _has_src_addr;
    };

    class tcp_address_mask_t : public tcp_address_t
    {
    public:
        tcp_address_mask_t ();

        // This function enhances tcp_address_t::resolve() with ability to parse
        // additional cidr-like(/xx) mask value at the end of the name string.
        // Works only with remote hostnames.
        int resolve (const char *name_, bool ipv6_);

        // The opposite to resolve()
        int to_string (std::string &addr_);

        int mask () const;

        bool match_address (const struct lwip_sockaddr *ss, const socklen_t ss_len) const;

    private:
        int address_mask;
    };

}

#endif
