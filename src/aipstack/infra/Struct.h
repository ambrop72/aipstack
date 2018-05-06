/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef AIPSTACK_STRUCT_H
#define AIPSTACK_STRUCT_H

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <type_traits>

#include <aipstack/meta/TypeListUtils.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/BinaryTools.h>
#include <aipstack/misc/EnumUtils.h>

namespace AIpStack {

/**
 * @ingroup infra
 * @defgroup struct Fixed-layout Structures
 * @brief Definition and access of fixed-layout structures for protocol headers.
 * 
 * Notable features of the system are:
 * - Concise structure definition using a single macro.
 * - Automatic endianness handling (big endian encoding is used). The user always interacts
 *   with logical values.
 * - Can reference structures in existing memory (no pointer casts violating strict
 *   aliasing).
 * - Support for nested structures.
 * - Ability to add support for additional field types.
 * 
 * Structures (typically protocol headers) are defined using the @ref AIPSTACK_DEFINE_STRUCT
 * macro which defines a class deriving from @ref StructBase. Here is an example.
 * 
 * ```
 * AIPSTACK_DEFINE_STRUCT(MyHeader,
 *     (FieldA, std::uint32_t)
 *     (FieldB, std::uint64_t)
 * )
 * ```
 * 
 * This expands to the following:
 * 
 * ```
 * struct MyHeader : public AIpStack::StructBase<MyHeader> {
 *      struct FieldA : public AIpStack::StructField<std::uint32_t> {};
 *      struct FieldB : public AIpStack::StructField<std::uint64_t> {};
 *      using StructFields = AIpStack::MakeTypeList<FieldA, FieldB>;
 *      static std::size_t const Size = MyHeader::GetStructSize();
 * };
 * ```
 * 
 * Each structure field specified will result in a type within this structure that
 * is used as an identifier for the field (e.g. `MyHeader::FieldA`). There will also be
 * a `Size` constant (e.g. `MyHeader::Size`) which represents the size of the structure,
 * which is the sum of field sizes. The `StructFields` type alias is a list of all fields
 * and is used internally in the implementation.
 * 
 * There are different ways to use the structure definition, but the most common is via
 * the @ref StructBase::Ref class, which wraps a pointer to raw structure data. The
 * @ref StructBase::MakeRef helper function can be used to create a @ref StructBase::Ref.
 * For example:
 * 
 * ```
 * char data[MyHeader::Size];
 * 
 * {
 *     MyHeader::Ref ref = MyHeader::MakeRef(data);
 *     // Each set call directly writes to 'data'.
 *     ref.set(MyHeader::FieldA(), 5);
 *     ref.set(MyHeader::FieldB(), 123);
 * }
 * 
 * {
 *     MyHeader::Ref ref = MyHeader::MakeRef(data);
 *     // Each get call directly reads from 'data'.
 *     std::uint32_t a = ref.get(MyHeader::FieldA());
 *     std::uint64_t b = ref.get(MyHeader::FieldB());
 * }
 * ```
 * 
 * As can be seen, the @ref StructBase::Ref::set and @ref StructBase::Ref::get functions
 * are used to set and get field values. These directly read/write from/to the pointed-to
 * memory. For certain field types (but not integers), the @ref StructBase::Ref::ref
 * function returns some kind of reference to the field; see the list of supported field
 * types below.
 * 
 * The @ref StructBase::Val class is similar to @ref StructBase::Ref, but it contains a
 * character array (@ref StructBase::Val::data) instead of referencing external data. The
 * @ref StructBase::MakeVal helper function can be used to create a @ref StructBase::Val
 * from existing data.
 * 
 * The system directly supports the following field types:
 * - Fixed-width integer types (`intN_t` and `uintN_t` for N=8,16,32,64) and enum types
 *   based on these types. Big-endian byte order is used. The `get` and `set` operations
 *   use the same type as the field is defined as. The `ref` operation is not available.
 * - Structures types defined though this same system (nested structures). The `get` and
 *   `set` operations use the type @ref StructBase::Val corresponding to the field type.
 *   The `ref` operation is available and returns a @ref StructBase::Ref referencing the
 *   nested structure.
 * - Fixed-length arrays of fixed-width integers, using field type
 *   @ref StructIntArray\<T, Length\>. Big-endian byte order is used. The `get` and `set`
 *   operations use this @ref StructIntArray type, which contains the
 *   @ref StructIntArray::data array. The `ref` operation is not available, except when
 *   T is `std::uint8_t` (see below).
 * - Fixed-length byte arrays, using field type @ref StructByteArray\<Length\>. This is
 *   actually a type alias for @ref StructIntArray\<std::uint8_t, Length\>, but the `ref`
 *   operation is also available, which returns a `char *` pointing to the field contents.
 * - Any trivial type T (as defined by `std::is_trivial<T>`) using its native
 *   representation, when using field type @ref StructRawField\<T\>. The `get` and `set`
 *   operations use the type T. The `ref` operation is not available.
 * 
 * Support for additional types can be added by adding additional specializations of
 * @ref StructTypeHandler (see the documentation of that).
 * 
 * @{
 */

/**
 * Specializations of this provide support for different field types.
 * 
 * Note that specializations for fields types directly supported by the system are not
 * included in the documentation (see the @ref struct module description). When adding
 * custom specializations it is important that there are no conflicts with the predefined
 * ones.
 * 
 * Each specialization of @ref StructTypeHandler must provide a type alias `Handler` which
 * defines the handler class for the field type(s).
 * 
 * The handler class must have the following definitions:
 * - `static std::size_t const FieldSize`: Number of bytes in the byte representation.
 * - `using ValType`: Type alias defining the value type for the `get` and `set` operations.
 * - `static ValType get (char const *data)`: Function which decodes a byte representation
 *   into a value.
 * - `static void set (char *data, ValType value)`: Function which encodes a value into
 *   a byte representation.
 * 
 * If and only if the `ref` operation is supported, the handler class must have the
 * following definitions:
 * - `using RefType`: Type alias defining the reference type for the `ref` operation
 *   (not a C++ reference).
 * - `static RefType ref (char *data)`: Function which returns a reference to the specified
 *   field data, for the `ref` operation.
 * 
 * @tparam Type Field type (as declared via @ref AIPSTACK_DEFINE_STRUCT).
 * @tparam Dummy Dummy template parameter to assist SFINAE, will be `void`.
 */
template <typename Type, typename Dummy=void>
struct StructTypeHandler;

#ifndef IN_DOXYGEN

template <typename TType>
struct StructField {
    using StructFieldType = TType;
};

template <typename FieldType>
using StructFieldHandler = typename StructTypeHandler<FieldType>::Handler;

#endif

/**
 * Get the value type for the given structure field type.
 * 
 * The value type is the one used for the `get` and `set` operations using this field type.
 * 
 * @tparam FieldType Field type, as used in the structure definition.
 */
template <typename FieldType>
using StructFieldValType =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename StructFieldHandler<FieldType>::ValType;
#endif

/**
 * Get the reference type for the given structure field type.
 * 
 * The reference type is the one used for the `ref` operation using this field type.
 * This is only defined when the field type supports the `ref` operation.
 * 
 * @tparam FieldType Field type, as used in the structure definition.
 */
template <typename FieldType>
using StructFieldRefType =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename StructFieldHandler<FieldType>::RefType;
#endif

/**
 * Base class for protocol structure definitions.
 * 
 * See the @ref struct module description. This class should only be used via derived
 * classes and those should only be defined using the @ref AIPSTACK_DEFINE_STRUCT macro.
 * 
 * There are two nested classes providing different ways to work with structure data:
 * - @ref Ref references structure data using `char *` (@ref Ref::data).
 * - @ref Val contains structure data as `char[Size]` (@ref Val::data).
 * 
 * Note that there is no use for actual @ref StructBase instances (and instances
 * of derived types).
 * 
 * @tparam StructType_ Structure type (derived from this class).
 */
template <typename StructType_>
class StructBase {
    using StructType = StructType_;
    
    template <typename This=StructBase>
    using Fields = typename This::StructType::StructFields;
    
    template <int FieldIndex, typename Dummy=void>
    struct FieldInfo;
    
    template <typename Dummy>
    struct FieldInfo<-1, Dummy> {
        static std::size_t const PartialStructSize = 0;
    };
    
    template <int FieldIndex, typename Dummy>
    struct FieldInfo {
        using PrevFieldInfo = FieldInfo<FieldIndex-1, void>;
        using Field = TypeListGet<Fields<>, FieldIndex>;
        
        using Handler = StructFieldHandler<typename Field::StructFieldType>;
        using ValType = typename Handler::ValType;
        static std::size_t const FieldOffset = PrevFieldInfo::PartialStructSize;
        static std::size_t const PartialStructSize = FieldOffset + Handler::FieldSize;
    };
    
    template <typename Field, typename This=StructBase>
    using GetFieldInfo = FieldInfo<TypeListIndex<Fields<This>, Field>::Value, void>;
    
    template <typename This=StructBase>
    using LastFieldInfo = FieldInfo<TypeListLength<Fields<This>>::Value-1, void>;
    
public:
    class Ref;
    class Val;
    
    /**
     * Get the value type of a specific field.
     * 
     * @tparam Field Field identifier.
     */
    template <typename Field>
    using ValType = StructFieldValType<typename Field::StructFieldType>;
    
    /**
     * Get the reference type of a specific field.
     * 
     * This is only defined when the field type supports the `ref` operation.
     * 
     * @tparam Field Field identifier.
     */
    template <typename Field>
    using RefType = StructFieldRefType<typename Field::StructFieldType>;
    
    /**
     * Return the size of the structure.
     * 
     * This is a function because of issues with eager resolution. The
     * @ref AIPSTACK_DEFINE_STRUCT macro defines a static `Size` member which should be used
     * instead of this.
     * 
     * @return Structure size.
     */
    inline static constexpr std::size_t GetStructSize () 
    {
        return LastFieldInfo<>::PartialStructSize;
    }
    
    /**
     * Get the byte offset of a field.
     * 
     * @tparam Field Field identifier.
     * @return Field offset from the start of the structure in bytes.
     */
    template <typename Field>
    inline static std::size_t getOffset (Field)
    {
        using Info = GetFieldInfo<Field>;
        return Info::FieldOffset;
    }
    
    /**
     * Read a field.
     * 
     * @tparam Field Field identifier.
     * @param data Pointer to the start of the structure.
     * @return Field value that was read.
     */
    template <typename Field>
    inline static ValType<Field> get (char const *data, Field)
    {
        using Info = GetFieldInfo<Field>;
        return Info::Handler::get(data + Info::FieldOffset);
    }
    
    /**
     * Write a field.
     * 
     * @tparam Field Field identifier.
     * @param data Pointer to the start of the structure.
     * @param value Field value to write.
     */
    template <typename Field>
    inline static void set (char *data, Field, ValType<Field> value)
    {
        using Info = GetFieldInfo<Field>;
        Info::Handler::set(data + Info::FieldOffset, value);
    }
    
    /**
     * Return a reference to a field.
     * 
     * Support for this depends on the type handler.
     * 
     * @tparam Field Field identifier.
     * @param data Pointer to the start of the structure.
     * @return Reference to the field.
     */
    template <typename Field>
    inline static RefType<Field> ref (char *data, Field)
    {
        using Info = GetFieldInfo<Field>;
        return Info::Handler::ref(data + Info::FieldOffset);
    }
    
    /**
     * Return a @ref Ref object referencing the specified memory.
     * 
     * @param data Pointer to the start of the structure.
     * @return `Ref(data)`
     */
    inline static Ref MakeRef (char *data)
    {
        return Ref(data);
    }
    
    /**
     * Read a structure from the specified memory location and return a @ref Val object
     * containing the structure data.
     * 
     * @param data Pointer to the start of the structure.
     * @return A @ref Val object initialized with a copy of the data.
     */
    inline static Val MakeVal (char const *data)
    {
        Val val;
        std::memcpy(val.data, data, GetStructSize());
        return val;
    }
    
    /**
     * Base class with definitions common to @ref Val and @ref Ref.
     */
    class ValRefBase {
    public:
        /**
         * The structure type.
         * 
         * This is the corresponding type defined by the @ref AIPSTACK_DEFINE_STRUCT macro,
         * and is also the `StructType_` template parameter of @ref StructBase.
         */
        using Struct = StructType_;
        
        /**
         * The size of the structure.
         */
        static constexpr std::size_t Size () { return GetStructSize(); }
    };
    
    /**
     * Represents a structure as a value.
     * 
     * This class contains structure data as a character array (@ref data). In addition to
     * default construction, @ref Val objects can be created using @ref StructBase::MakeVal
     * and from the conversion operators in @ref Ref.
     */
    class Val : public ValRefBase {
    public:
        /**
         * Default constructor, leaves the @ref data array uninitialized.
         */
        Val () = default;
        
        /**
         * Read a field.
         * 
         * @see StructBase::get
         * 
         * @tparam Field Field identifier.
         * @return Field value that was read.
         */
        template <typename Field>
        inline ValType<Field> get (Field) const
        {
            return StructBase::get(data, Field());
        }
        
        /**
         * Write a field.
         * 
         * @see StructBase::set
         * 
         * @tparam Field Field identifier.
         * @param value Field value to write.
         */
        template <typename Field>
        inline void set (Field, ValType<Field> value)
        {
            StructBase::set(data, Field(), value);
        }
        
        /**
         * Return a reference to a field.
         * 
         * @see StructBase::ref
         * 
         * @tparam Field Field identifier.
         * @return Reference to the field.
         */
        template <typename Field>
        inline RefType<Field> ref (Field)
        {
            return StructBase::ref(data, Field());
        }
        
        /**
         * Return a @ref Ref referencing data in this @ref Val.
         * 
         * @return `Ref(data)`
         */
        inline operator Ref ()
        {
            return Ref(data);
        }
        
    public:
        /**
         * The data array.
         */
        char data[GetStructSize()];
    };
    
    /**
     * Represents a reference to a structure stored elsewhere.
     * 
     * This class contains a pointer to the start of a structure (@ref data). In addition
     * to the constructor, @ref Ref objects can be created using @ref StructBase::MakeRef.
     */
    class Ref : public ValRefBase {
    public:
        /**
         * Default constructor, leaves the @ref data pointer uninitialized.
         */
        Ref () = default;
        
        /**
         * Construct referencing the specified memory.
         * 
         * @param data_ Pointer to the start of the structure.
         */
        inline Ref (char *data_)
        : data(data_)
        {}
        
        /**
         * Read a field.
         * 
         * @see StructBase::get
         * 
         * @tparam Field Field identifier.
         * @return Field value that was read.
         */
        template <typename Field>
        inline ValType<Field> get (Field) const
        {
            return StructBase::get(data, Field());
        }
        
        /**
         * Write a field.
         * 
         * @see StructBase::set
         * 
         * @tparam Field Field identifier.
         * @param value Field value to write.
         */
        template <typename Field>
        inline void set (Field, ValType<Field> value) const
        {
            StructBase::set(data, Field(), value);
        }
        
        /**
         * Return a reference to a field.
         * 
         * @see StructBase::ref
         * 
         * @tparam Field Field identifier.
         * @return Reference to the field.
         */
        template <typename Field>
        inline RefType<Field> ref (Field) const
        {
            return StructBase::ref(data, Field());
        }
        
        /**
         * Read and return the current structure data as a @ref Val.
         */
        inline operator Val () const
        {
            return MakeVal(data);
        }
        
        /**
         * Copy the structure referenced by another @ref Ref over the structure referenced
         * by this @ref Ref.
         * 
         * Both @ref Ref must not point to the same or overlapping memory (`memcpy` is
         * used).
         * 
         * @param src Structure reference to copy from.
         */
        inline void load (Ref src) const
        {
            std::memcpy(data, src.data, GetStructSize());
        }
        
    public:
        /**
         * The data pointer.
         */
        char *data;
    };
};

/**
 * Read (decode) the value of a single struct field of specified type.
 * 
 * This allows decoding a field without requiring any structure definition.
 * 
 * @tparam FieldType Field type.
 * @param ptr Pointer to field data to decode.
 * @return Decoded field value.
 */
template <typename FieldType>
inline StructFieldValType<FieldType> ReadSingleField (char const *ptr)
{
    return StructFieldHandler<FieldType>::get(ptr);
}

/**
 * Write (encode) the value of a single struct field of specified type.
 * 
 * This allows encoding a field without requiring any structure definition.
 * 
 * @tparam FieldType Field type.
 * @param ptr Pointer to where field data will be written.
 * @param value Value to encode.
 */
template <typename FieldType>
inline void WriteSingleField (char *ptr, StructFieldValType<FieldType> value)
{
    StructFieldHandler<FieldType>::set(ptr, value);
}

/**
 * Define a structure type.
 * 
 * See the @ref struct module description.
 * 
 * @param StructName Name of the class to define which will represent the structure.
 * @param Fields List of fields as a sequence of `(FieldName, FieldType)`.
 */
#ifdef IN_DOXYGEN
#define AIPSTACK_DEFINE_STRUCT(StructName, Fields) implementation_hidden;
#else
#define AIPSTACK_DEFINE_STRUCT(StructName, Fields) \
struct StructName : public AIpStack::StructBase<StructName> { \
    AIPSTACK_DEFINE_STRUCT_ADD_END(AIPSTACK_DEFINE_STRUCT_FIELD_1 Fields) \
    using StructFields = AIpStack::MakeTypeList< \
        AIPSTACK_DEFINE_STRUCT_ADD_END(AIPSTACK_DEFINE_STRUCT_LIST_0 Fields) \
    >; \
    static std::size_t const Size = StructName::GetStructSize(); \
};
#endif

#ifndef IN_DOXYGEN

#define AIPSTACK_DEFINE_STRUCT_ADD_END(...) AIPSTACK_DEFINE_STRUCT_ADD_END_2(__VA_ARGS__)
#define AIPSTACK_DEFINE_STRUCT_ADD_END_2(...) __VA_ARGS__ ## _END

#define AIPSTACK_DEFINE_STRUCT_FIELD_1(FieldName, FieldType) \
struct FieldName : public AIpStack::StructField<FieldType> {}; \
AIPSTACK_DEFINE_STRUCT_FIELD_2

#define AIPSTACK_DEFINE_STRUCT_FIELD_2(FieldName, FieldType) \
struct FieldName : public AIpStack::StructField<FieldType> {}; \
AIPSTACK_DEFINE_STRUCT_FIELD_1

#define AIPSTACK_DEFINE_STRUCT_FIELD_1_END
#define AIPSTACK_DEFINE_STRUCT_FIELD_2_END

#define AIPSTACK_DEFINE_STRUCT_LIST_0(FieldName, FieldType) FieldName AIPSTACK_DEFINE_STRUCT_LIST_1
#define AIPSTACK_DEFINE_STRUCT_LIST_1(FieldName, FieldType) , FieldName AIPSTACK_DEFINE_STRUCT_LIST_2
#define AIPSTACK_DEFINE_STRUCT_LIST_2(FieldName, FieldType) , FieldName AIPSTACK_DEFINE_STRUCT_LIST_1

#define AIPSTACK_DEFINE_STRUCT_LIST_1_END
#define AIPSTACK_DEFINE_STRUCT_LIST_2_END

template <typename Type>
class StructBinaryTypeHandler {
    using IntType = GetSameOrEnumBaseType<Type>;
    using Endian = BinaryBigEndian;
    
public:
    static std::size_t const FieldSize = sizeof(IntType);
    
    using ValType = Type;
    
    inline static ValType get (char const *data)
    {
        return ValType(ReadBinaryInt<IntType, Endian>(data));
    }
    
    inline static void set (char *data, ValType value)
    {
        WriteBinaryInt<IntType, Endian>(IntType(value), data);
    }
};

#define AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(IntType) \
template <typename Type> \
struct StructTypeHandler<Type, std::enable_if_t<AIpStack::IsSameOrEnumWithBaseType<Type, IntType>::Value>> { \
    using Handler = AIpStack::StructBinaryTypeHandler<Type>; \
};

AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::uint8_t)
AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::uint16_t)
AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::uint32_t)
AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::uint64_t)
AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::int8_t)
AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::int16_t)
AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::int32_t)
AIPSTACK_STRUCT_REGISTER_BINARY_TYPE(std::int64_t)

template <typename StructType>
class StructNestedTypeHandler {
public:
    static std::size_t const FieldSize = StructType::GetStructSize();
    
    using ValType = typename StructType::Val;
    using RefType = typename StructType::Ref;
    
    inline static ValType get (char const *data)
    {
        return StructType::MakeVal(data);
    }
    
    inline static void set (char *data, ValType value)
    {
        std::memcpy(data, value.data, sizeof(value.data));
    }
    
    inline static RefType ref (char *data)
    {
        return RefType{data};
    }
};

template <typename Type>
struct StructTypeHandler<Type, std::enable_if_t<std::is_base_of<StructBase<Type>, Type>::value>> {
    using Handler = StructNestedTypeHandler<Type>;
};

#endif

/**
 * Contains a fixed-length array of integers.
 * 
 * Elements are stored in the @ref data array which can be accessed directly.
 * 
 * @tparam ElemType_ Type of elements. Must be one of the fixed-width integer types, or
 *         more specifically must be supported by @ref ReadBinaryInt and
 *         @ref WriteBinaryInt.
 * @tparam Length_ Number of elements in the array.
 */
template <typename ElemType_, std::size_t Length_>
class StructIntArray {
public:
    /**
     * Element type.
     */
    using ElemType = ElemType_;
    
    /**
     * Size of an encoded element in bytes.
     */
    static std::size_t const ElemSize = sizeof(ElemType);
    
    /**
     * Number of elements in the array.
     */
    static std::size_t const Length = Length_;
    
    /**
     * Size of an encoded array in bytes.
     */
    static std::size_t const Size = Length * ElemSize;
    
    /**
     * Decode a sequence of bytes into a @ref AIpStack::StructIntArray "StructIntArray" or
     * derived type.
     * 
     * Decoding is done using big-endian byte order. A `ResType` object is
     * default-constructed, its @ref data elements are filled in and the object
     * is returned.
     * 
     * @tparam ResType Result type. Must be @ref StructIntArray or a type derived from that.
     * @param bytes Pointer to start of data. There must be at least @ref Size bytes
     *        available which will be read.
     * @return Decoded integer array.
     */
    template <typename ResType>
    inline static ResType decodeTo (char const *bytes)
    {
        static_assert(std::is_base_of<StructIntArray, ResType>::value, "");
        
        ResType result;
        for (std::size_t i = 0; i < Length; i++) {
            result.StructIntArray::data[i] = ReadBinaryInt<ElemType, BinaryBigEndian>(bytes + i * ElemSize);
        }
        return result;
    }
    
    /**
     * Decode a sequence of bytes into a @ref AIpStack::StructIntArray "StructIntArray".
     * 
     * This is equivalent to @ref decodeTo\<StructIntArray\>.
     * 
     * @param bytes Pointer to start of data. There must be at least @ref Size bytes
     *        available which will be decoded.
     * @return Decoded integer array.
     */
    inline static StructIntArray decode (char const *bytes)
    {
        return decodeTo<StructIntArray>(bytes);
    }
    
    /**
     * Encode into a sequence of bytes.
     * 
     * Encoding is done using big-endian byte order.
     * 
     * @param bytes Pointer to where the encoded data will be written. There must be at
     *        least @ref Size bytes available which will be written.
     */
    inline void encode (char *bytes) const
    {
        for (std::size_t i = 0; i < Length; i++) {
            WriteBinaryInt<ElemType, BinaryBigEndian>(data[i], bytes + i * ElemSize);
        }
    }
    
    /**
     * Equality operator.
     * 
     * @param other The other integer array.
     * @return Whether the arrays are equal.
     */
    inline constexpr bool operator== (StructIntArray const &other) const
    {
        for (std::size_t i = 0; i < Length; i++) {
            if (data[i] != other.data[i]) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * Inequality operator.
     * 
     * @param other The other integer array.
     * @return Whether the arrays are not equal.
     */
    inline constexpr bool operator!= (StructIntArray const &other) const
    {
        return !operator==(other);
    }
    
    /**
     * Less-than operator using lexicographical order.
     * 
     * @param other The other integer array.
     * @return Whether this array is less than `other`.
     */
    inline constexpr bool operator< (StructIntArray const &other) const
    {
        for (std::size_t i = 0; i < Length; i++) {
            if (data[i] < other.data[i]) {
                return true;
            }
            if (data[i] != other.data[i]) {
                return false;
            }
        }
        return false;
    }
    
    /**
     * Greater-than operator using lexicographical order.
     * 
     * @param other The other integer array.
     * @return Whether this array is greater than `other`.
     */
    inline constexpr bool operator> (StructIntArray const &other) const
    {
        return other.operator<(*this);
    }
    
    /**
     * Less-than-or-equal operator using lexicographical order.
     * 
     * @param other The other integer array.
     * @return Whether this array is less than or equal to `other`.
     */
    inline constexpr bool operator<= (StructIntArray const &other) const
    {
        return !other.operator<(*this);
    }
    
    /**
     * Greater-than-or-equal operator using lexicographical order.
     * 
     * @param other The other integer array.
     * @return Whether this array is greater than or equal to `other`.
     */
    inline constexpr bool operator>= (StructIntArray const &other) const
    {
        return !operator<(other);
    }
    
public:
    /**
     * The array of integers stored in this object.
     */
    ElemType data[Length];
};

/**
 * A type containing an array of bytes (std::uint8_t).
 * 
 * This is just an alias for @ref StructIntArray\<std::uint8_t, Length\>;
 */
template <std::size_t Length>
using StructByteArray = StructIntArray<std::uint8_t, Length>;

#ifndef IN_DOXYGEN

template <typename TValType>
class StructIntArrayTypeHandler {
public:
    static std::size_t const FieldSize = TValType::Size;
    
    using ValType = TValType;
    
    inline static ValType get (char const *data)
    {
        return ValType::template decodeTo<ValType>(data);
    }
    
    inline static void set (char *data, ValType value)
    {
        value.encode(data);
    }
};

template <typename TValType>
class StructByteArrayTypeHandler {
public:
    static std::size_t const FieldSize = TValType::Size;
    
    using ValType = TValType;
    using RefType = char *;
    
    inline static ValType get (char const *data)
    {
        ValType value;
        std::memcpy(value.data, data, ValType::Size);
        return value;
    }
    
    inline static void set (char *data, ValType value)
    {
        std::memcpy(data, value.data, ValType::Size);
    }
    
    inline static RefType ref (char *data)
    {
        return data;
    }
};

template <typename Type>
struct StructTypeHandler<Type, std::enable_if_t<std::is_base_of<StructIntArray<typename Type::ElemType, Type::Length>, Type>::value>> {
    using Handler = std::conditional_t<
        std::is_base_of<StructByteArray<Type::Length>, Type>::value,
        StructByteArrayTypeHandler<Type>,
        StructIntArrayTypeHandler<Type>
    >;
};

template <typename Type>
class StructRawTypeHandler {
public:
    static_assert(std::is_trivial<Type>::value, "");
    
    static std::size_t const FieldSize = sizeof(Type);
    
    using ValType = Type;
    
    inline static ValType get (char const *data)
    {
        Type value;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }
    
    inline static void set (char *data, ValType value)
    {
        std::memcpy(data, &value, sizeof(value));
    }
};

#endif

/**
 * Field type of a raw field using native representation.
 * 
 * See the @ref struct module description.
 * 
 * @tparam Type Value type. Must be a trivial type (`std::is_trivial<T>`).
 */
template <typename Type>
struct StructRawField {};

#ifndef IN_DOXYGEN

template <typename Type>
struct StructTypeHandler<StructRawField<Type>, void> {
    using Handler = StructRawTypeHandler<Type>;
};

#endif

/** @} */

}

#endif
