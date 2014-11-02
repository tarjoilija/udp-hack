// g++ -std=c++11 -fPIC -m32 -shared -o main.so main.cpp

#include <iostream>
#include <sstream>
#include <iomanip>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dlfcn.h>

typedef ssize_t (*sendto_ptr)(int, const void*, size_t, int, const struct sockaddr*, socklen_t);
typedef ssize_t (*recvfrom_ptr)(int, void*, size_t, int, struct sockaddr*, socklen_t*);

static void init() __attribute__((constructor));
static sendto_ptr orig_sendto = nullptr;
static recvfrom_ptr orig_recvfrom = nullptr;

void locate_functions()
{
    char *error;

    dlerror();

    orig_sendto = (sendto_ptr)dlsym(RTLD_NEXT, "sendto");
    if ((error = dlerror()) != NULL)
    {
        std::cout << error << std::endl;
    }

    orig_recvfrom = (recvfrom_ptr)dlsym(RTLD_NEXT, "recvfrom");
    if ((error = dlerror()) != NULL)
    {
        std::cout << error << std::endl;
    }
}

void init()
{
    locate_functions();
}

std::string bytes_to_hex(const unsigned char *bytes, size_t len)
{
    std::ostringstream os;
    os << std::hex << std::setfill('0');

    for (size_t i = 0; i < len; ++i)
    {
        os << std::setw(2) << (int)bytes[i] << " ";
    }

    return os.str();
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
           const struct sockaddr *dest_addr, socklen_t addrlen)
{
    const struct sockaddr_in *addr4 = (struct sockaddr_in*) dest_addr;
    const unsigned char      *cbuf  = (const unsigned char *) buf;
    char                     *ip    = inet_ntoa(addr4->sin_addr);
    const unsigned short     &port  = addr4->sin_port;

    std::cout << "send " << ip << ":" << port << std::endl;
    std::cout << "    " << bytes_to_hex(cbuf, len) << std::endl;

    return orig_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *from_addr, socklen_t *addrlen)
{
    ssize_t bytes_read = orig_recvfrom(sockfd, buf, len, flags, from_addr, addrlen);

    const struct sockaddr_in *addr4 = (struct sockaddr_in*) from_addr;
    unsigned char            *cbuf  = (unsigned char *) buf;
    char                     *ip    = inet_ntoa(addr4->sin_addr);
    const unsigned short     &port  = addr4->sin_port;

    std::cout << "recv " << ip << ":" << port << std::endl;
    std::cout << "    " << bytes_to_hex(cbuf, len) << std::endl;

    return bytes_read;
}
