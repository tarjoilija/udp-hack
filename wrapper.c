#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dlfcn.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#define PACKET "Packet"
#define PACKET_SIZE 2048
typedef unsigned char * Packet;

static void con() __attribute__((constructor));
static lua_State *L;
static ssize_t (*oldsendto)(int sockfd, const void *buf, size_t len, int flags,
                            const struct sockaddr *dest_addr, socklen_t addrlen) = NULL;
static ssize_t (*oldrecvfrom)(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *from_addr, socklen_t *addrlen) = NULL;

Packet check_Packet(lua_State *L, int index)
{
    Packet p = (Packet) luaL_checkudata(L, index, PACKET);

    if (p == NULL)
        luaL_error(L, "failed to get userdata: wrong type");

    return p;
}

int Hack_NewPacket(lua_State *L)
{
    lua_newuserdata(L, PACKET_SIZE);
    luaL_setmetatable(L, PACKET);
    return 1;
}

int Packet__gc(lua_State *L)
{
    return 0;
}

int Packet_readByte(lua_State *L)
{
    Packet p = check_Packet(L, 1);
    int i = luaL_checkint(L, 2);

    if (i < 0 || i+1 > PACKET_SIZE)
        luaL_error(L, "index out of range");

    lua_pushnumber(L, p[i]);
    return 1;
}

int Packet_readShort(lua_State *L)
{
    Packet p = check_Packet(L, 1);
    int i = luaL_checkint(L, 2);

    if (i < 0 || i+2 > PACKET_SIZE)
        luaL_error(L, "index out of range");

    lua_pushnumber(L, p[i] | (p[i+1] << 8));
    return 1;
}

int Packet_readString(lua_State *L)
{
    Packet p = check_Packet(L, 1);
    int i = luaL_checkint(L, 2);
    int len = luaL_checkint(L, 3);

    if (i < 0 || len < 1 || i+len > PACKET_SIZE)
    {
        luaL_error(L, "index out of range");
    }

    char* str = malloc(len+1);
    memcpy(str, &p[i], len);
    str[len] = 0x00;

    lua_pushstring(L, str);
    free(str);
    return 1;
}

int Packet_writeByte(lua_State *L)
{
    Packet p = check_Packet(L, 1);
    int i = luaL_checkint(L, 2);
    char value = luaL_checkint(L, 3);

    if (i < 0 || i+1 > PACKET_SIZE)
    {
        luaL_error(L, "index out of range");
    }

    p[i] = value;
    return 0;
}

int Packet_writeShort(lua_State *L)
{
    Packet p = check_Packet(L, 1);
    int i = luaL_checkint(L, 2);
    unsigned short value = luaL_checkint(L, 3);

    if (i < 0 || i+2 > PACKET_SIZE)
    {
        luaL_error(L, "index out of range");
    }

    p[i] = (char) value;
    p[i+1] = (char) (value >> 8);

    return 0;
}

int Packet_writeString(lua_State *L)
{
    Packet p = check_Packet(L, 1);
    int i = luaL_checkint(L, 2);
    char *str = luaL_checkstring(L, 3);
    int length = strlen(str);

    if (i < 0 || i > PACKET_SIZE || i+length > PACKET_SIZE)
    {
        luaL_error(L, "index out of range");
    }

    strcpy((char *) &p[i], str);

    return 0;
}

int Packet_sendto(lua_State *L)
{
    Packet p = check_Packet(L, 1);
    int sockfd = luaL_checkint(L, 2);
    const char *ip = luaL_checkstring(L, 3);
    int port = luaL_checkint(L, 4);
    int len = luaL_checkint(L, 5);

    struct sockaddr_in addr4;
    inet_pton(AF_INET, ip, &(addr4.sin_addr));
    addr4.sin_family = AF_INET;
    addr4.sin_port = port;

    oldsendto(sockfd, p, len, 0, (const struct sockaddr *) &addr4, 16);
    return 0;
}

int luaopen_Hack(lua_State *L)
{
    static const luaL_Reg Packet_lib[] = {
        {"get",         Packet_readByte},
        {"getShort",    Packet_readShort},
        {"getString",   Packet_readString},
        {"set",         Packet_writeByte},
        {"setShort",    Packet_writeShort},
        {"setString",   Packet_writeString},
        {"sendto",      Packet_sendto},
        {NULL,          NULL}
    };
    static const luaL_Reg Hack_lib[] = {
        {"NewPacket",  Hack_NewPacket},
        {NULL,          NULL}
    };

    luaL_newlib(L, Hack_lib);

    luaL_newmetatable(L, PACKET);
    luaL_newlib(L, Packet_lib);
    lua_setfield(L, -2, "__index");

    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, Packet__gc);
    lua_settable(L, -3);
    lua_pop(L, 1);

    return 1;
}

void con(void)
{
    char *error;

    dlerror();

    oldsendto = dlsym(RTLD_NEXT, "sendto");
    if ((error = dlerror()) != NULL)
    {
        fprintf(stderr, "%s\n", error);
        exit(EXIT_FAILURE);
    }

    oldrecvfrom = dlsym(RTLD_NEXT, "recvfrom");
    if ((error = dlerror()) != NULL)
    {
        fprintf(stderr, "%s\n", error);
        exit(EXIT_FAILURE);
    }

    L = luaL_newstate();
    luaL_openlibs(L);

    luaL_requiref(L, "Hack", &luaopen_Hack, 1);
    lua_pop(L, 1);

    int result = luaL_dofile(L, "script.lua");
    if (result)
    {
        fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
        exit(1);
    }
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
           const struct sockaddr *dest_addr, socklen_t addrlen)
{
    const struct sockaddr_in *addr4 = (struct sockaddr_in*) dest_addr;
    const unsigned char * cbuf = (const unsigned char *) buf;

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr4->sin_addr, ipstr, INET_ADDRSTRLEN);

    lua_getglobal(L, "hook_sendto");
    lua_pushnumber(L, sockfd);
    lua_pushstring(L, ipstr);
    lua_pushnumber(L, addr4->sin_port);

    Packet *p = lua_newuserdata(L, PACKET_SIZE);
    memcpy(p, cbuf, len);
    luaL_setmetatable(L, PACKET);

    lua_pushnumber(L, len);

    if (lua_pcall(L, 5, 1, 0) != 0)
    {
        fprintf(stderr, "lua error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    else
    {
        int result = lua_tonumber(L, -1);
        lua_pop(L, 1);

        if (result != 0) {
            return len;
        }
    }

    return oldsendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *from_addr, socklen_t *addrlen)
{
    ssize_t read = oldrecvfrom(sockfd, buf, len, flags, from_addr, addrlen);

    const struct sockaddr_in *addr4 = (struct sockaddr_in*) from_addr;
    unsigned char * cbuf = (unsigned char *) buf;

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr4->sin_addr, ipstr, INET_ADDRSTRLEN);

    lua_getglobal(L, "hook_recvfrom");
    lua_pushnumber(L, sockfd);
    lua_pushstring(L, ipstr);
    lua_pushnumber(L, addr4->sin_port);

    Packet *p = lua_newuserdata(L, PACKET_SIZE);
    memcpy(p, cbuf, len);
    luaL_setmetatable(L, PACKET);

    lua_pushnumber(L, len);

    if (lua_pcall(L, 5, 1, 0) != 0)
    {
        fprintf(stderr, "lua error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    else
    {
        int result = lua_tonumber(L, -1);
        lua_pop(L, 1);

        if (result != 0) {
            return len;
        }
    }

    return read;
}
