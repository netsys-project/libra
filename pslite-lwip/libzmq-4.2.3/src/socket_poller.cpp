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
#include "socket_poller.hpp"
#include "err.hpp"

zmq::socket_poller_t::socket_poller_t () :
    tag (0xCAFEBABE),
    signaler (NULL),
    need_rebuild (true),
    use_signaler (false),
    poll_size(0)
#if defined ZMQ_POLL_BASED_ON_POLL
    ,
    pollfds (NULL)
#elif defined ZMQ_POLL_BASED_ON_SELECT
    ,
    maxfd(0)
#endif
{
#if defined ZMQ_POLL_BASED_ON_SELECT
#if defined ZMQ_HAVE_WINDOWS
    // On Windows fd_set contains array of SOCKETs, each 4 bytes.
    // For large fd_sets memset() could be expensive and it is unnecessary.
    // It is enough to set fd_count to 0, exactly what FD_ZERO() macro does.
    FD_ZERO (&pollset_in);
    FD_ZERO (&pollset_out);
    FD_ZERO (&pollset_err);
#else
    memset(&pollset_in, 0, sizeof(pollset_in));
    memset(&pollset_out, 0, sizeof(pollset_out));
    memset(&pollset_err, 0, sizeof(pollset_err));
#endif
#endif
}

zmq::socket_poller_t::~socket_poller_t ()
{
    //  Mark the socket_poller as dead
    tag = 0xdeadbeef;

    for (items_t::iterator it = items.begin(); it != items.end(); ++it) {
        if (it->socket && it->socket->check_tag()) {
            int thread_safe;
            size_t thread_safe_size = sizeof(int);

            if (it->socket->getsockopt (ZMQ_THREAD_SAFE, &thread_safe, &thread_safe_size) == 0 && thread_safe)
                it->socket->remove_signaler (signaler);
        }
    }

    if (signaler != NULL) {
        delete signaler;
        signaler = NULL;
    }

#if defined ZMQ_POLL_BASED_ON_POLL
    if (pollfds) {
        xfree (pollfds);
        pollfds = NULL;
    }
#endif
}

bool zmq::socket_poller_t::check_tag ()
{
    return tag == 0xCAFEBABE;
}

int zmq::socket_poller_t::add (socket_base_t *socket_, void* user_data_, short events_)
{
    for (items_t::iterator it = items.begin (); it != items.end (); ++it) {
        if (it->socket == socket_) {
            errno = EINVAL;
            return -1;
        }
    }

    int thread_safe;
    size_t thread_safe_size = sizeof(int);

    int rc =
      socket_->getsockopt (ZMQ_THREAD_SAFE, &thread_safe, &thread_safe_size);
    zmq_assert (rc == 0);

    if (thread_safe) {
        if (signaler == NULL)
            signaler = new signaler_t ();

        rc = socket_->add_signaler (signaler);
        zmq_assert (rc == 0);
    }

    item_t item = {socket_, 0, user_data_, events_
#if defined ZMQ_POLL_BASED_ON_POLL
                   ,-1
#endif
    };
    items.push_back (item);
    need_rebuild = true;

    return 0;
}

int zmq::socket_poller_t::add_fd (fd_t fd_, void *user_data_, short events_)
{
   for (items_t::iterator it = items.begin (); it != items.end (); ++it) {
        if (!it->socket && it->fd == fd_) {
            errno = EINVAL;
            return -1;
        }
    }

    item_t item = {NULL, fd_, user_data_, events_
#if defined ZMQ_POLL_BASED_ON_POLL
                   ,-1
#endif
                   };
    items.push_back (item);
    need_rebuild = true;

    return 0;
}

int zmq::socket_poller_t::modify (socket_base_t  *socket_, short events_)
{
    items_t::iterator it;

    for (it = items.begin (); it != items.end (); ++it) {
        if (it->socket == socket_)
            break;
    }

    if (it == items.end()) {
        errno = EINVAL;
        return -1;
    }

    it->events = events_;
    need_rebuild = true;

    return 0;
}


int zmq::socket_poller_t::modify_fd (fd_t fd_, short events_)
{
    items_t::iterator it;

    for (it = items.begin (); it != items.end (); ++it) {
        if (!it->socket && it->fd == fd_)
            break;
    }

    if (it == items.end()) {
        errno = EINVAL;
        return -1;
    }

    it->events = events_;
    need_rebuild = true;

    return 0;
}


int zmq::socket_poller_t::remove (socket_base_t *socket_)
{
    items_t::iterator it;

    for (it = items.begin (); it != items.end (); ++it) {
        if (it->socket == socket_)
            break;
    }

    if (it == items.end()) {
        errno = EINVAL;
        return -1;
    }

    items.erase(it);
    need_rebuild = true;

    int thread_safe;
    size_t thread_safe_size = sizeof(int);

    if (socket_->getsockopt (ZMQ_THREAD_SAFE, &thread_safe, &thread_safe_size) == 0 && thread_safe)
        socket_->remove_signaler (signaler);

    return 0;
}

int zmq::socket_poller_t::remove_fd (fd_t fd_)
{
    items_t::iterator it;

    for (it = items.begin (); it != items.end (); ++it) {
        if (!it->socket && it->fd == fd_)
            break;
    }

    if (it == items.end()) {
        errno = EINVAL;
        return -1;
    }

    items.erase (it);
    need_rebuild = true;

    return 0;
}

void zmq::socket_poller_t::rebuild ()
{
#if defined ZMQ_POLL_BASED_ON_POLL

    if (pollfds) {
        xfree (pollfds);
        pollfds = NULL;
    }

    use_signaler = false;

    poll_size = 0;

    for (items_t::iterator it = items.begin (); it != items.end (); ++it) {
        if (it->events) {
            if (it->socket) {
                int thread_safe;
                size_t thread_safe_size = sizeof(int);

                int rc = it->socket->getsockopt (ZMQ_THREAD_SAFE, &thread_safe,
                                                 &thread_safe_size);
                zmq_assert (rc == 0);

                if (thread_safe) {
                    if (!use_signaler) {
                        use_signaler = true;
                        poll_size++;
                    }
                }
                else
                    poll_size++;
            }
            else
                poll_size++;
        }
    }

    if (poll_size == 0)
        return;

    pollfds = (pollfd*) malloc (poll_size * sizeof (pollfd));
    alloc_assert (pollfds);

    int item_nbr = 0;

    if (use_signaler) {
        item_nbr = 1;
        pollfds[0].fd = signaler->get_fd();
        pollfds[0].events = POLLIN;
    }

    for (items_t::iterator it = items.begin (); it != items.end (); ++it) {
        if (it->events) {
            if (it->socket) {
                int thread_safe;
                size_t thread_safe_size = sizeof(int);

                int rc = it->socket->getsockopt (ZMQ_THREAD_SAFE, &thread_safe,
                                                 &thread_safe_size);
                zmq_assert (rc == 0);

                if (!thread_safe) {
                    size_t fd_size = sizeof (zmq::fd_t);
                    rc = it->socket->getsockopt (ZMQ_FD, &pollfds [item_nbr].fd, &fd_size);
                    zmq_assert (rc == 0);

                    pollfds [item_nbr].events = POLLIN;
                    item_nbr++;
                }
            }
            else {
                pollfds [item_nbr].fd = it->fd;
                pollfds [item_nbr].events =
                    (it->events & ZMQ_POLLIN ? POLLIN : 0) |
                    (it->events & ZMQ_POLLOUT ? POLLOUT : 0) |
                    (it->events & ZMQ_POLLPRI ? POLLPRI : 0);
                it->pollfd_index = item_nbr;
                item_nbr++;
            }
        }
    }

 #elif defined ZMQ_POLL_BASED_ON_SELECT

    FD_ZERO (&pollset_in);
    FD_ZERO (&pollset_out);
    FD_ZERO (&pollset_err);

    //  Ensure we do not attempt to select () on more than FD_SETSIZE
    //  file descriptors.
    zmq_assert (items.size () <= FD_SETSIZE);

    poll_size = 0;

    use_signaler = false;

    for (items_t::iterator it = items.begin (); it != items.end (); ++it) {
        if (it->socket) {
            int thread_safe;
            size_t thread_safe_size = sizeof(int);

            int rc = it->socket->getsockopt (ZMQ_THREAD_SAFE, &thread_safe,
                                             &thread_safe_size);
            zmq_assert (rc == 0);

            if (thread_safe && it->events) {
                use_signaler = true;
                FD_SET (signaler->get_fd (), &pollset_in);
                poll_size = 1;
                break;
            }
        }
    }

    maxfd = 0;

    //  Build the fd_sets for passing to select ().
    for (items_t::iterator it = items.begin (); it != items.end (); ++it) {
        if (it->events) {
            //  If the poll item is a 0MQ socket we are interested in input on the
            //  notification file descriptor retrieved by the ZMQ_FD socket option.
            if (it->socket) {
                int thread_safe;
                size_t thread_safe_size = sizeof(int);

                int rc = it->socket->getsockopt (ZMQ_THREAD_SAFE, &thread_safe,
                                                 &thread_safe_size);
                zmq_assert (rc == 0);

                if (!thread_safe) {
                    zmq::fd_t notify_fd;
                    size_t fd_size = sizeof (zmq::fd_t);
                    rc = it->socket->getsockopt (ZMQ_FD, &notify_fd, &fd_size);
                    zmq_assert (rc == 0);

                    FD_SET (notify_fd, &pollset_in);
                    if (maxfd < notify_fd)
                        maxfd = notify_fd;

                    poll_size++;
                }
            }
            //  Else, the poll item is a raw file descriptor. Convert the poll item
            //  events to the appropriate fd_sets.
            else {
                if (it->events & ZMQ_POLLIN)
                    FD_SET (it->fd, &pollset_in);
                if (it->events & ZMQ_POLLOUT)
                    FD_SET (it->fd, &pollset_out);
                if (it->events & ZMQ_POLLERR)
                    FD_SET (it->fd, &pollset_err);
                if (maxfd < it->fd)
                    maxfd = it->fd;

                poll_size++;
            }
        }
    }

#endif

    need_rebuild = false;
}

void zmq::socket_poller_t::zero_trail_events (
                                         zmq::socket_poller_t::event_t *events_,
                                         int n_events_,
                                         int found)
{
    for (int i = found; i < n_events_; ++i) {
        events_[i].socket = NULL;
        events_[i].fd = 0;
        events_[i].user_data = NULL;
        events_[i].events = 0;
    }
}

#if defined ZMQ_POLL_BASED_ON_POLL
int zmq::socket_poller_t::check_events (zmq::socket_poller_t::event_t *events_,
                                                              int n_events_)
#elif defined ZMQ_POLL_BASED_ON_SELECT
int zmq::socket_poller_t::check_events (zmq::socket_poller_t::event_t *events_,
                                                              int n_events_,
                                                              fd_set& inset,
                                                              fd_set& outset,
                                                              fd_set& errset)
#endif
{
    int found = 0;
    for (items_t::iterator it = items.begin (); it != items.end () &&
                                                  found < n_events_; ++it) {

        //  The poll item is a 0MQ socket. Retrieve pending events
        //  using the ZMQ_EVENTS socket option.
        if (it->socket) {
            size_t events_size = sizeof (uint32_t);
            uint32_t events;
            if (it->socket->getsockopt (ZMQ_EVENTS, &events, &events_size)
                                                                        == -1) {
                return -1;
            }

            if (it->events & events) {
                events_[found].socket = it->socket;
                events_[found].user_data = it->user_data;
                events_[found].events = it->events & events;
                ++found;
            }
        }
        //  Else, the poll item is a raw file descriptor, simply convert
        //  the events to zmq_pollitem_t-style format.
        else {

#if defined ZMQ_POLL_BASED_ON_POLL

            short revents = pollfds [it->pollfd_index].revents;
            short events = 0;

            if (revents & POLLIN)
                events |= ZMQ_POLLIN;
            if (revents & POLLOUT)
                events |= ZMQ_POLLOUT;
            if (revents & POLLPRI)
                events |= ZMQ_POLLPRI;
            if (revents & ~(POLLIN | POLLOUT | POLLPRI))
                events |= ZMQ_POLLERR;

#elif defined ZMQ_POLL_BASED_ON_SELECT

            short events = 0;

            if (FD_ISSET (it->fd, &inset))
                events |= ZMQ_POLLIN;
            if (FD_ISSET (it->fd, &outset))
                events |= ZMQ_POLLOUT;
            if (FD_ISSET (it->fd, &errset))
                events |= ZMQ_POLLERR;
#endif //POLL_SELECT

            if (events) {
                events_[found].socket = NULL;
                events_[found].user_data = it->user_data;
                events_[found].fd = it->fd;
                events_[found].events = events;
                ++found;
            }
        }
    }

    return found;
}

//Return 0 if timeout is expired otherwise 1
int zmq::socket_poller_t::adjust_timeout (zmq::clock_t& clock, long timeout_,
                                                              uint64_t& now,
                                                              uint64_t& end,
                                                              bool& first_pass)
{

    //  If socket_poller_t::timeout is zero, exit immediately whether there
    //  are events or not.
    if (timeout_ == 0)
        return 0;

    //  At this point we are meant to wait for events but there are none.
    //  If timeout is infinite we can just loop until we get some events.
    if (timeout_ < 0) {
        if (first_pass)
            first_pass = false;
        return 1;
    }

    //  The timeout is finite and there are no events. In the first pass
    //  we get a timestamp of when the polling have begun. (We assume that
    //  first pass have taken negligible time). We also compute the time
    //  when the polling should time out.
    now = clock.now_ms ();
    if (first_pass) {
        end = now + timeout_;
        first_pass = false;
        return 1;
    }

    //  Find out whether timeout have expired.
    if (now >= end)
        return 0;

    return 1;
}

int zmq::socket_poller_t::wait (zmq::socket_poller_t::event_t *events_,
                                                         int n_events_,
                                                         long timeout_)
{
    if (items.empty () && timeout_ < 0) {
        errno = EFAULT;
        return -1;
    }

    if (need_rebuild)
        rebuild ();

    if (unlikely (poll_size == 0)) {
        // We'll report an error (timed out) as if the list was non-empty and
        // no event occurred within the specified timeout. Otherwise the caller
        // needs to check the return value AND the event to avoid using the
        // nullified event data.
        errno = EAGAIN;
        if (timeout_ == 0)
            return -1;
#if defined ZMQ_HAVE_WINDOWS
        Sleep (timeout_ > 0 ? timeout_ : INFINITE);
        return -1;
#elif defined ZMQ_HAVE_ANDROID
        usleep (timeout_ * 1000);
        return -1;
#elif defined ZMQ_HAVE_OSX
        usleep (timeout_ * 1000);
        errno = EAGAIN;
        return -1;
#else
        usleep (timeout_ * 1000);
        return -1;
#endif
    }

#if defined ZMQ_POLL_BASED_ON_POLL
    zmq::clock_t clock;
    uint64_t now = 0;
    uint64_t end = 0;

    bool first_pass = true;

    while (true) {
        //  Compute the timeout for the subsequent poll.
        int timeout;
        if (first_pass)
            timeout = 0;
        else
        if (timeout_ < 0)
            timeout = -1;
        else
            timeout = end - now;

        //  Wait for events.
        while (true) {
            int rc = poll (pollfds, poll_size, timeout);
            if (rc == -1 && errno == EINTR) {
                return -1;
            }
            errno_assert (rc >= 0);
            break;
        }

        //  Receive the signal from pollfd
        if (use_signaler && pollfds[0].revents & POLLIN)
            signaler->recv ();

        //  Check for the events.
        int found = check_events (events_, n_events_);
        if (found) {
            if (found > 0)
                zero_trail_events (events_, n_events_, found);
            return found;
        }

        //  Adjust timeout or break
        if (adjust_timeout (clock, timeout_, now, end, first_pass) == 0)
            break;
    }
    errno = EAGAIN;
    return -1;

#elif defined ZMQ_POLL_BASED_ON_SELECT

    zmq::clock_t clock;
    uint64_t now = 0;
    uint64_t end = 0;

    bool first_pass = true;

    fd_set inset, outset, errset;

    while (true) {

        //  Compute the timeout for the subsequent poll.
        timeval timeout;
        timeval *ptimeout;
        if (first_pass) {
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            ptimeout = &timeout;
        }
        else
        if (timeout_ < 0)
            ptimeout = NULL;
        else {
            timeout.tv_sec = (long) ((end - now) / 1000);
            timeout.tv_usec = (long) ((end - now) % 1000 * 1000);
            ptimeout = &timeout;
        }

        //  Wait for events. Ignore interrupts if there's infinite timeout.
        while (true) {
#if defined ZMQ_HAVE_WINDOWS
            // On Windows we don't need to copy the whole fd_set.
            // SOCKETS are continuous from the beginning of fd_array in fd_set.
            // We just need to copy fd_count elements of fd_array.
            // We gain huge memcpy() improvement if number of used SOCKETs is much lower than FD_SETSIZE.
            memcpy (&inset,  &pollset_in,  (char *) (pollset_in.fd_array  + pollset_in.fd_count ) - (char *) &pollset_in );
            memcpy (&outset, &pollset_out, (char *) (pollset_out.fd_array + pollset_out.fd_count) - (char *) &pollset_out);
            memcpy (&errset, &pollset_err, (char *) (pollset_err.fd_array + pollset_err.fd_count) - (char *) &pollset_err);
            int rc = select (0, &inset, &outset, &errset, ptimeout);
            if (unlikely (rc == SOCKET_ERROR)) {
                errno = zmq::wsa_error_to_errno (WSAGetLastError ());
                wsa_assert (errno == ENOTSOCK);
                return -1;
            }
#else
            memcpy (&inset, &pollset_in, sizeof (fd_set));
            memcpy (&outset, &pollset_out, sizeof (fd_set));
            memcpy (&errset, &pollset_err, sizeof (fd_set));
            int rc = select (maxfd + 1, &inset, &outset, &errset, ptimeout);
            if (unlikely (rc == -1)) {
                errno_assert (errno == EINTR || errno == EBADF);
                return -1;
            }
#endif
            break;
        }

        if (use_signaler && FD_ISSET (signaler->get_fd (), &inset))
            signaler->recv ();

        //  Check for the events.
        int found = check_events(events_, n_events_, inset, outset, errset);
        if (found) {
            if (found > 0)
                zero_trail_events (events_, n_events_, found);
            return found;
        }

        //  Adjust timeout or break
        if (adjust_timeout (clock, timeout_, now, end, first_pass) == 0)
            break;
    }

    errno = EAGAIN;
    return -1;

#else

    //  Exotic platforms that support neither poll() nor select().
    errno = ENOTSUP;
    return -1;

#endif
}
