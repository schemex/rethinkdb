
#ifndef __IO_CALLS_HPP__
#define __IO_CALLS_HPP__

#include <libaio.h>
#include <vector>
#include "utils2.hpp"
#include "config/args.hpp"
#include "arch/linux/event_queue.hpp"
#include "event.hpp"
#include "corefwd.hpp"

struct linux_iocallback_t {
    virtual ~linux_iocallback_t() {}
    virtual void on_io_complete(event_t *event) = 0;
};

struct linux_net_conn_callback_t {
    virtual void on_net_conn_readable() = 0;
    virtual void on_net_conn_writable() = 0;
    virtual void on_net_conn_close() = 0;
};

class linux_net_conn_t :
    public linux_event_callback_t
{
public:
    void set_callback(linux_net_conn_callback_t *cb);
    ssize_t read_nonblocking(void *buf, size_t count);
    ssize_t write_nonblocking(const void *buf, size_t count);
    ~linux_net_conn_t();
    
private:
    friend class linux_io_calls_t;
    friend class linux_net_listener_t;
    
    fd_t sock;
    linux_net_conn_callback_t *callback;
    bool *set_me_true_on_delete;   // So we can tell if a callback deletes the conn_fsm_t

    // We are implementing this for level-triggered mechanisms such as
    // poll, that will keep bugging us about the write when we don't
    // need it, and use up 100% of cpu
    bool registered_for_write_notifications;
    
    linux_net_conn_t(fd_t);
    void on_event(int events);
};

struct linux_net_listener_callback_t {
    virtual void on_net_listener_accept(linux_net_conn_t *conn) = 0;
};

class linux_net_listener_t :
    public linux_event_callback_t
{

public:
    linux_net_listener_t(int port);
    void set_callback(linux_net_listener_callback_t *cb);
    ~linux_net_listener_t();

private:
    fd_t sock;
    linux_net_listener_callback_t *callback;
    
    void on_event(int events);
};

/* The "direct" in linux_direct_file_t refers to the fact that the file is opened in
O_DIRECT mode, and there are restrictions on the alignment of the chunks being written
and read to and from the file. */

class linux_direct_file_t
{

public:
    enum mode_t {
        mode_read = 1 << 0,
        mode_write = 1 << 1
    };
    
    linux_direct_file_t(const char *path, int mode);
    
    bool is_block_device();
    uint64_t get_size();
    void set_size(size_t size);
    void set_size_at_least(size_t size);
    
    /* These always return 'false'; the reason they return bool instead of void
    is for consistency with other asynchronous-callback methods */
    bool read_async(size_t offset, size_t length, void *buf, linux_iocallback_t *cb);
    bool write_async(size_t offset, size_t length, void *buf, linux_iocallback_t *cb);
    
    void read_blocking(size_t offset, size_t length, void *buf);
    void write_blocking(size_t offset, size_t length, void *buf);
    
    ~linux_direct_file_t();
    
private:
    fd_t fd;
    bool is_block;
    uint64_t file_size;
    void verify(size_t offset, size_t length, void* buf);

    DISABLE_COPYING(linux_direct_file_t);
};

class linux_io_calls_t :
    public linux_event_callback_t
{

public:
    linux_io_calls_t(linux_event_queue_t *queue);
    ~linux_io_calls_t();

    void process_requests();

    linux_event_queue_t *queue;
    io_context_t aio_context;
    fd_t aio_notify_fd;
    
    int n_pending;
    
    struct queue_t {
        
        linux_io_calls_t *parent;
        typedef std::vector<iocb*> request_vector_t;
        request_vector_t queue;
        
        queue_t(linux_io_calls_t *parent);
        int process_request_batch();
        ~queue_t();
        
    } r_requests, w_requests;

public:
    void on_event(int events);
    void aio_notify(iocb *event, int result);
};

#endif // __IO_CALLS_HPP__

