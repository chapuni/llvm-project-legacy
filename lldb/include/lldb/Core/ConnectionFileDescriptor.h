//===-- ConnectionFileDescriptor.h ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ConnectionFileDescriptor_h_
#define liblldb_ConnectionFileDescriptor_h_

// C Includes
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/Connection.h"

namespace lldb_private {

class ConnectionFileDescriptor :
    public Connection
{
public:

    ConnectionFileDescriptor ();

    ConnectionFileDescriptor (int fd, bool owns_fd);

    virtual
    ~ConnectionFileDescriptor ();

    virtual bool
    IsConnected () const;

    virtual lldb::ConnectionStatus
    Connect (const char *s, Error *error_ptr);

    virtual lldb::ConnectionStatus
    Disconnect (Error *error_ptr);

    virtual size_t
    Read (void *dst, 
          size_t dst_len, 
          uint32_t timeout_usec,
          lldb::ConnectionStatus &status, 
          Error *error_ptr);

    virtual size_t
    Write (const void *src, 
           size_t src_len, 
           lldb::ConnectionStatus &status, 
           Error *error_ptr);

protected:
    
    lldb::ConnectionStatus
    BytesAvailable (uint32_t timeout_usec, Error *error_ptr);
    
    lldb::ConnectionStatus
    SocketListen (uint16_t listen_port_num, Error *error_ptr);

    lldb::ConnectionStatus
    ConnectTCP (const char *host_and_port, Error *error_ptr);
    
    lldb::ConnectionStatus
    ConnectUDP (const char *host_and_port, Error *error_ptr);
    
    lldb::ConnectionStatus
    NamedSocketAccept (const char *socket_name, Error *error_ptr);

    lldb::ConnectionStatus
    NamedSocketConnect (const char *socket_name, Error *error_ptr);
    
    lldb::ConnectionStatus
    Close (int& fd, Error *error);

    typedef enum
    {
        eFDTypeFile,        // Other FD requireing read/write
        eFDTypeSocket,      // Socket requiring send/recv
        eFDTypeSocketUDP    // Unconnected UDP socket requiring sendto/recvfrom
    } FDType;
    int m_fd;    // Socket we use to communicate once conn established
    FDType m_fd_type;
    struct sockaddr_storage m_udp_sockaddr;
    socklen_t m_udp_sockaddr_len;
    bool m_should_close_fd; // True if this class should close the file descriptor when it goes away.
    uint32_t m_socket_timeout_usec;
    
    static int
    GetSocketOption(int fd, int level, int option_name, int &option_value);

    static int
    SetSocketOption(int fd, int level, int option_name, int option_value);

    bool
    SetSocketReceiveTimeout (uint32_t timeout_usec);

private:
    DISALLOW_COPY_AND_ASSIGN (ConnectionFileDescriptor);
};

} // namespace lldb_private

#endif  // liblldb_ConnectionFileDescriptor_h_
