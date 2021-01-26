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

#ifndef __ZMQ_EPOLL_HPP_INCLUDED__
#define __ZMQ_EPOLL_HPP_INCLUDED__

//  poller.hpp decides which polling mechanism to use.
#include "poller.hpp"
#if defined ZMQ_USE_EPOLL

#include <vector>
#include <sys/epoll.h>

#include "ctx.hpp"
#include "fd.hpp"
#include "thread.hpp"
#include "poller_base.hpp"
#include "mutex.hpp"

namespace zmq
{

    struct i_poll_events;

    //  This class implements socket polling mechanism using the Linux-specific
    //  epoll mechanism.

    class epoll_t : public poller_base_t
    {
    public:

        typedef void* handle_t;

        epoll_t (const ctx_t &ctx_, bool lwip = false);
        ~epoll_t ();

        //  "poller" concept.
        handle_t add_fd (fd_t fd_, zmq::i_poll_events *events_);
        void rm_fd (handle_t handle_);
        void set_pollin (handle_t handle_);
        void reset_pollin (handle_t handle_);
        void set_pollout (handle_t handle_);
        void reset_pollout (handle_t handle_);
        void start ();
        void stop ();

        static int max_fds ();

		handle_t add_fd_lwip (fd_t fd_, zmq::i_poll_events *events_);
        void rm_fd_lwip (handle_t handle_);
        void set_pollin_lwip (handle_t handle_);
        void reset_pollin_lwip (handle_t handle_);
        void set_pollout_lwip (handle_t handle_);
        void reset_pollout_lwip (handle_t handle_);

    private:

        //  Main worker thread routine.
        static void worker_routine (void *arg_);

		static void lwip_routine (void *arg_);

        //  Main event loop.
        void loop_epoll ();
        void loop_lwip ();

        // Reference to ZMQ context.
        const ctx_t &ctx;

        //  Main epoll file descriptor
        fd_t epoll_fd;

        struct poll_entry_t
        {
            fd_t fd;
            epoll_event ev;
            zmq::i_poll_events *events;
        };

        //  List of retired event sources.
        typedef std::vector <poll_entry_t*> retired_t;
        retired_t retired;
		retired_t retired_lwip;

        //  If true, thread is in the process of shutting down.
        bool stopping;

        //  Handle of the physical thread doing the I/O work.
        thread_t worker;
		thread_t lwip_worker;

        //  Synchronisation of retired event sources
        mutex_t retired_sync;
		mutex_t retired_lwip_sync;


		/*  lwip thread */
		fd_t maxfd;

		bool is_lwip;

    	//  Internal state.
    	struct fds_set_t
    	{
        	fds_set_t ();
        	fds_set_t (const fds_set_t &other_);
        	fds_set_t &operator= (const fds_set_t &other_);
        	//  Convenience method to descriptor from all sets.
        	void remove_fd (const fd_t &fd_);

        	fd_set read;
        	fd_set write;
        	fd_set error;
    	};

		struct fds_set_t fds_set;

		typedef std::vector<poll_entry_t *> lwip_entries_t;
		lwip_entries_t lwip_entries;

		mutex_t fds_sync;

		static poll_entry_t *
				find_entry_by_fd (lwip_entries_t &entries, fd_t fd);

		void handle_epoll();
		void handle_lwip();

        epoll_t (const epoll_t&);
        const epoll_t &operator = (const epoll_t&);
    };

    typedef epoll_t poller_t;

}

#endif

#endif
