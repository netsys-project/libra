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

#include "precompiled.hpp"
#include "epoll.hpp"
#if defined ZMQ_USE_EPOLL

#include <sys/epoll.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <new>

#include "macros.hpp"
#include "epoll.hpp"
#include "err.hpp"
#include "config.hpp"
#include "i_poll_events.hpp"

zmq::epoll_t::epoll_t (const zmq::ctx_t &ctx_, bool lwip) :
    ctx(ctx_),
    stopping (false),
	maxfd (0)
{
#ifdef ZMQ_USE_EPOLL_CLOEXEC
    //  Setting this option result in sane behaviour when exec() functions
    //  are used. Old sockets are closed and don't block TCP ports, avoid
    //  leaks, etc.
    epoll_fd = epoll_create1 (EPOLL_CLOEXEC);
#else
    epoll_fd = epoll_create (1);
#endif
    errno_assert (epoll_fd != -1);
	is_lwip = lwip;
}

zmq::epoll_t::~epoll_t ()
{
    //  Wait till the worker thread exits.
    worker.stop ();
//	lwip_worker.stop();

    close (epoll_fd);
//    for (retired_t::iterator it = retired.begin (); it != retired.end (); ++it) {
//        LIBZMQ_DELETE(*it);
//    }

    for (retired_t::iterator it = retired.begin (); it != retired.end (); ++it) {
        LIBZMQ_DELETE(*it);
    }

	for (lwip_entries_t::iterator it = lwip_entries.begin(); it != lwip_entries.end(); ++it) {
		LIBZMQ_DELETE(*it);
	}
}

zmq::epoll_t::handle_t zmq::epoll_t::add_fd (fd_t fd_, i_poll_events *events_)
{
    poll_entry_t *pe = new (std::nothrow) poll_entry_t;
    alloc_assert (pe);

    //  The memset is not actually needed. It's here to prevent debugging
    //  tools to complain about using uninitialised memory.
    memset (pe, 0, sizeof (poll_entry_t));

    pe->fd = fd_;
    pe->ev.events = 0;
    pe->ev.data.ptr = pe;
    pe->events = events_;

    int rc = epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_, &pe->ev);
    errno_assert (rc != -1);

    //  Increase the load metric of the thread.
    adjust_load (1);

    return pe;
}

zmq::epoll_t::handle_t zmq::epoll_t::add_fd_lwip (fd_t fd_, i_poll_events *events_)
{
    poll_entry_t *pe = new (std::nothrow) poll_entry_t;
    alloc_assert (pe);

    //  The memset is not actually needed. It's here to prevent debugging
    //  tools to complain about using uninitialised memory.
    memset (pe, 0, sizeof (poll_entry_t));

    pe->fd = fd_;
    pe->events = events_;

	fds_sync.lock();
	lwip_entries.push_back(pe);
	FD_SET(fd_, &fds_set.error);

	if (maxfd < (fd_+1))
		maxfd = fd_ + 1;
	fds_sync.unlock();

    //  Increase the load metric of the thread.
    adjust_load (1);

    return pe;
}


void zmq::epoll_t::rm_fd (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;
    int rc = epoll_ctl (epoll_fd, EPOLL_CTL_DEL, pe->fd, &pe->ev);
    errno_assert (rc != -1);
    pe->fd = retired_fd;
    retired_sync.lock ();
    retired.push_back (pe);
    retired_sync.unlock ();

    //  Decrease the load metric of the thread.
    adjust_load (-1);
}

zmq::epoll_t::poll_entry_t *
zmq::epoll_t::find_entry_by_fd (lwip_entries_t &entries, fd_t fd)
{
	for (unsigned i = 0; i < entries.size(); i++) {
		if (entries[i]->fd == fd)
			return entries[i];
	}

	return NULL;
}

void zmq::epoll_t::rm_fd_lwip (handle_t handle_)
{
	lwip_entries_t::iterator it;
	fd_t fd;
	poll_entry_t *pe = (poll_entry_t *)handle_;

	fds_sync.lock();

	fd = pe->fd;

	for (it = lwip_entries.begin(); it != lwip_entries.end(); it++) {
		if ((*it)->fd == fd)
			break;
	}

	if (it == lwip_entries.end()) {
		fds_sync.unlock();
		return;
	}

    (*it)->fd = retired_fd;
	fds_set.remove_fd(fd);
	lwip_entries.erase(it);

	fds_sync.unlock();

    retired_sync.lock ();
    retired.push_back (pe);
    retired_sync.unlock ();

    //  Decrease the load metric of the thread.
    adjust_load (-1);
}

void zmq::epoll_t::set_pollin (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;
    pe->ev.events |= EPOLLIN;
    int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
    errno_assert (rc != -1);
}

void zmq::epoll_t::set_pollin_lwip (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;

	fds_sync.lock();
	FD_SET(pe->fd, &fds_set.read);
	fds_sync.unlock();
}

void zmq::epoll_t::reset_pollin (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;
    pe->ev.events &= ~((short) EPOLLIN);
    int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
    errno_assert (rc != -1);
}

void zmq::epoll_t::reset_pollin_lwip (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;
	fds_sync.lock();
	FD_CLR(pe->fd, &fds_set.read);
	fds_sync.unlock();
}

void zmq::epoll_t::set_pollout (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;
    pe->ev.events |= EPOLLOUT;
    int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
    errno_assert (rc != -1);
}

void zmq::epoll_t::set_pollout_lwip (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;

	fds_sync.lock();
	FD_SET(pe->fd, &fds_set.write);
	fds_sync.unlock();
}

void zmq::epoll_t::reset_pollout (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;
    pe->ev.events &= ~((short) EPOLLOUT);
    int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
    errno_assert (rc != -1);
}

void zmq::epoll_t::reset_pollout_lwip (handle_t handle_)
{
    poll_entry_t *pe = (poll_entry_t*) handle_;

	fds_sync.lock();
	FD_CLR(pe->fd, &fds_set.write);
	fds_sync.unlock();
}

void zmq::epoll_t::start ()
{
    ctx.start_thread (worker, worker_routine, this);

//	if (is_lwip)
//		ctx.start_thread(lwip_worker, lwip_routine, this);
}

void zmq::epoll_t::stop ()
{
    stopping = true;
}

int zmq::epoll_t::max_fds ()
{
    return -1;
}

void zmq::epoll_t::handle_epoll()
{
	int n = 0;
	epoll_event ev_buf[max_io_events];

	n = epoll_wait(epoll_fd, ev_buf, max_io_events, 0);
	if (n == -1) {
		errno_assert(errno == EINTR);
		return;
	}

    for (int i = 0; i < n; i ++) {
        struct poll_entry_t *pe = ((poll_entry_t*) ev_buf [i].data.ptr);

        if (pe->fd == retired_fd)
  			continue;
        if (ev_buf [i].events & (EPOLLERR | EPOLLHUP)) {
            pe->events->in_event ();
		}
        if (pe->fd == retired_fd)
           continue;
        if (ev_buf [i].events & EPOLLOUT) {
            pe->events->out_event ();
		}
        if (pe->fd == retired_fd)
            continue;
        if (ev_buf [i].events & EPOLLIN) {
            pe->events->in_event ();
		}
    }
}

void zmq::epoll_t::handle_lwip()
{
	if (lwip_entries.empty())
		return;

	struct fd_ev {
		struct poll_entry_t *pe;
		int type;
	};

	enum {
		FD_EV_READ = 0,
		FD_EV_WRITE,
		FD_EV_ERROR
	};

	struct fd_ev events[max_io_events];
	int n = 0;

	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	fds_sync.lock();

	fds_set_t local_fds = fds_set;
	int rc = lwip_select (maxfd+1, &local_fds.read, &local_fds.write, &local_fds.error, &tv);

	if (rc == -1) {
		errno_assert(errno == EINTR);
		fds_sync.unlock();
		return;
	}
	if (rc == 0) {
		fds_sync.unlock();
		return;
	}

	for (unsigned i = 0; i < lwip_entries.size(); i++) {
		poll_entry_t *entry = lwip_entries[i];

		if (entry->fd == retired_fd)
			continue;

		if (entry->fd > maxfd)
			continue;

		if (FD_ISSET(entry->fd, &local_fds.read)) {
			events[n].type = FD_EV_READ;
			events[n].pe = entry;
			n++;
			if (n == max_io_events) {
				fprintf(stdout, "[%s][%d]: exceed max event limit\n",
								__FILE__, __LINE__);
				break;
			}
		}
		if (FD_ISSET(entry->fd, &local_fds.write)) {
			events[n].type = FD_EV_WRITE;
			events[n].pe = entry;
			n++;
			if (n == max_io_events) {
				fprintf(stdout, "[%s][%d]: exceed max event limit\n",
								__FILE__, __LINE__);
				break;
			}
		}
		if (FD_ISSET(entry->fd, &local_fds.error)) {
			events[n].type = FD_EV_ERROR;
			events[n].pe = entry;
			n++;
			if (n == max_io_events) {
				fprintf(stdout, "[%s][%d]: exceed max event limit\n",
								__FILE__, __LINE__);
				break;
			}
		}
	}
	fds_sync.unlock();

	for (int i = 0; i < n; i++) {
		struct fd_ev *ev = &events[i];

		if (ev->pe->fd == retired_fd)
			continue;
		if (ev->type == FD_EV_READ || ev->type == FD_EV_ERROR)
			ev->pe->events->in_event();
		else
			ev->pe->events->out_event();
	}
}

void zmq::epoll_t::loop_epoll ()
{
    while (!stopping) {

        //  Execute any due timers.
//        int timeout = (int) execute_timers ();

		handle_epoll();

		if (is_lwip)
			handle_lwip();

        //  Destroy retired event sources.
        retired_sync.lock ();
        for (retired_t::iterator it = retired.begin (); it != retired.end (); ++it) {
            LIBZMQ_DELETE(*it);
        }
        retired.clear ();
        retired_sync.unlock ();
    }
}

void zmq::epoll_t::loop_lwip()
{
	while (!stopping) {
		handle_lwip();

        //  Destroy retired event sources.
        retired_sync.lock ();
        for (retired_t::iterator it = retired.begin (); it != retired.end (); ++it) {
            LIBZMQ_DELETE(*it);
        }
        retired.clear ();
        retired_sync.unlock ();
	}
}

void zmq::epoll_t::worker_routine (void *arg_)
{
	prctl(PR_SET_NAME, "iothread_epoll");
    ((epoll_t*) arg_)->loop_epoll ();
}

void zmq::epoll_t::lwip_routine (void *arg_)
{
	prctl(PR_SET_NAME, "iothread_lwip");
	((epoll_t *)arg_)->loop_lwip();
}

zmq::epoll_t::fds_set_t::fds_set_t ()
{
    FD_ZERO (&read);
    FD_ZERO (&write);
    FD_ZERO (&error);
}

zmq::epoll_t::fds_set_t::fds_set_t (const fds_set_t &other_)
{
    memcpy (&read, &other_.read, sizeof other_.read);
    memcpy (&write, &other_.write, sizeof other_.write);
    memcpy (&error, &other_.error, sizeof other_.error);
}

zmq::epoll_t::fds_set_t &zmq::epoll_t::fds_set_t::
operator= (const fds_set_t &other_)
{
    memcpy (&read, &other_.read, sizeof other_.read);
    memcpy (&write, &other_.write, sizeof other_.write);
    memcpy (&error, &other_.error, sizeof other_.error);
    return *this;
}

void zmq::epoll_t::fds_set_t::remove_fd (const fd_t &fd_)
{
    FD_CLR (fd_, &read);
    FD_CLR (fd_, &write);
    FD_CLR (fd_, &error);
}


#endif
