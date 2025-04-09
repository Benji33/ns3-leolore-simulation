#ifndef IPV4_ADDRESS_HASH_H
#define IPV4_ADDRESS_HASH_H

#include "ns3/ipv4-address.h"
#include <functional>

namespace std {
    template <>
    struct hash<ns3::Ipv4Address> {
        std::size_t operator()(const ns3::Ipv4Address& addr) const noexcept {
            return std::hash<uint32_t>()(addr.Get()); // Use the uint32_t representation of the address
        }
    };
}

#endif // IPV4_ADDRESS_HASH_H