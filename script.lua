function hook_sendto(socket, ip, port, packet, length)
    local str = string.format("%s:%s SEND", ip, port)

    for i=1, length do
        str = string.format("%s %02x", str, packet:get(i-1))
    end

    print(str)
    return 0
end

function hook_recvfrom(socket, ip, port, packet, length)
    local str = string.format("%s:%s RECV", ip, port)

    for i=1, length do
        str = string.format("%s %02x", str, packet:get(i-1))
    end

    print(str)
    return 0
end

print("udp_hack script loaded!")
