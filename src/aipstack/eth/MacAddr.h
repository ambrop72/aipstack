/*
 * Copyright (c) 2018 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AIPSTACK_MAC_ADDR_H
#define AIPSTACK_MAC_ADDR_H

#include <cstdint>
#include <cstddef>
#include <array>

#include <aipstack/infra/Struct.h>

namespace AIpStack {

/**
 * @addtogroup eth
 * @{
 */

/**
 * Represents an Ethernet MAC address.
 * 
 * A @ref MacAddr object is defined by its value which is an array of 6 octets.
 * The @ref ValueArray type alias defines the `std::array` type used.
 * 
 * A @ref MacAddr object with a specific value can be constructed using the
 * constructor @ref MacAddr(ValueArray), and the value of an object can be obtained
 * using @ref value().
 * 
 * @ref MacAddr may be used as a field type in header definitions based on the
 * @ref struct system; `get` and `set` operations on such a field use the @ref
 * MacAddr type directly.
 */
class MacAddr {
public:
    /**
     * Size of a MAC address in bytes.
     */
    inline static constexpr std::size_t Size = 6;

    /**
     * The `std::array` type used to represent the value of a MAC address.
     */
    using ValueArray = std::array<std::uint8_t, Size>;
    
private:
    ValueArray m_value;

public:
    /**
     * Default constructor, constructs an address with all octets zero.
     */
    inline constexpr MacAddr () :
        m_value{{0, 0, 0, 0, 0, 0}}
    {}

    /**
     * Constructor from a specific value.
     * 
     * @param value Value of the address.
     */
    inline explicit constexpr MacAddr (ValueArray value) :
        m_value(value)
    {}

    /**
     * Constructor from six octet values.
     * 
     * @param b1 First octet.
     * @param b2 Second octet.
     * @param b3 Third octet.
     * @param b4 Fourth octet.
     * @param b5 Fifth octet.
     * @param b6 Sixth octet.
     */
    inline explicit constexpr MacAddr (
        std::uint8_t b1, std::uint8_t b2, std::uint8_t b3,
        std::uint8_t b4, std::uint8_t b5, std::uint8_t b6)
    :
        m_value{{b1, b2, b3, b4, b5, b6}}
    {}

    /**
     * Get the value of the address as an `std::array`.
     * 
     * @return The value of the address.
     */
    inline constexpr ValueArray value () const {
        return m_value;
    }

    /**
     * Get a pointer to the bytes of the stored MAC address.
     * 
     * @return Pointer to MAC address bytes within this object. It is
     *         valid until this object is destructed or reassigned.
     */
    inline std::uint8_t const * dataPtr () const {
        return m_value.data();
    }

    /**
     * Equal-to operator.
     * 
     * @param other The other address to compare.
     * @return `value() == other.value()`
     */
    inline bool operator== (MacAddr other) const {
        return value() == other.value();
    }
    
    /**
     * Not-equal-to operator.
     * 
     * @param other The other address to compare.
     * @return `value() != other.value()`
     */
    inline bool operator!= (MacAddr other) const {
        return value() != other.value();
    }
    
    /**
     * Less-than operator.
     * 
     * @param other The other address to compare.
     * @return `value() < other.value()`
     */
    inline bool operator< (MacAddr other) const {
        return value() < other.value();
    }
    
    /**
     * Less-than-or-equal operator.
     * 
     * @param other The other address to compare.
     * @return `value() <= other.value()`
     */
    inline bool operator<= (MacAddr other) const {
        return value() <= other.value();
    }
    
    /**
     * Greater-than operator.
     * 
     * @param other The other address to compare.
     * @return `value() > other.value()`
     */
    inline bool operator> (MacAddr other) const {
        return value() > other.value();
    }
    
    /**
     * Greater-than-or-equal operator.
     * 
     * @param other The other address to compare.
     * @return `value() >= other.value()`
     */
    inline bool operator>= (MacAddr other) const {
        return value() >= other.value();
    }
    
    /**
     * Return an address with all octets zero.
     * 
     * @return Address with all octets zero.
     */
    inline static constexpr MacAddr ZeroAddr () {
        return MacAddr(0, 0, 0, 0, 0, 0);
    }
    
    /**
     * Return the broadcast address, with all octets 0xFF.
     * 
     * @return Address with all octets 0xFF.
     */
    inline static constexpr MacAddr BroadcastAddr () {
        return MacAddr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    
    /**
     * Decode an address from a byte representation.
     *
     * @param src Memory location to read from; @ref Size (6) bytes must be available.
     * @return Decoded address.
     */
    inline static MacAddr readBinary (char const *src) {
        return MacAddr(ReadSingleField<ValueArray>(src));
    }

    /**
     * Encode the address to a byte representation.
     * 
     * @param dst Memory location to write to; @ref Size (6) bytes will be written.
     */
    inline void writeBinary (char *dst) const {
        WriteSingleField<ValueArray>(dst, value());
    }
};

#ifndef IN_DOXYGEN
template<>
struct StructTypeHandler<MacAddr, void> {
    using Handler = StructConventionalTypeHandler<MacAddr>;
};
#endif

/** @} */

}

#endif
