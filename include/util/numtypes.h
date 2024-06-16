#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @typedef u8
 * @brief Represents an 8-bit unsigned integer.
 *
 * This type alias is used for 8-bit unsigned integers, ranging from 0 to 255.
 * It is based on the std::uint8_t type.
 */
using u8 = std::uint8_t;

/**
 * @typedef u16
 * @brief Represents a 16-bit unsigned integer.
 *
 * This type alias is used for 16-bit unsigned integers, ranging from 0 to 65,535.
 * It is based on the std::uint16_t type.
 */
using u16 = std::uint16_t;

/**
 * @typedef u32
 * @brief Represents a 32-bit unsigned integer.
 *
 * This type alias is used for 32-bit unsigned integers, ranging from 0 to 4,294,967,295.
 * It is based on the std::uint32_t type.
 */
using u32 = std::uint32_t;

/**
 * @typedef u64
 * @brief Represents a 64-bit unsigned integer.
 *
 * This type alias is used for 64-bit unsigned integers, ranging from 0 to
 * 18,446,744,073,709,551,615. It is based on the std::uint64_t type.
 */
using u64 = std::uint64_t;

// Type Aliases for Signed Integers

/**
 * @typedef i8
 * @brief Represents an 8-bit signed integer.
 *
 * This type alias is used for 8-bit signed integers, ranging from -128 to 127.
 * It is based on the std::int8_t type.
 */
using i8 = std::int8_t;

/**
 * @typedef i16
 * @brief Represents a 16-bit signed integer.
 *
 * This type alias is used for 16-bit signed integers, ranging from -32,768 to 32,767.
 * It is based on the std::int16_t type.
 */
using i16 = std::int16_t;

/**
 * @typedef i32
 * @brief Represents a 32-bit signed integer.
 *
 * This type alias is used for 32-bit signed integers, ranging from -2,147,483,648 to 2,147,483,647.
 * It is based on the std::int32_t type.
 */
using i32 = std::int32_t;

/**
 * @typedef i64
 * @brief Represents a 64-bit signed integer.
 *
 * This type alias is used for 64-bit signed integers, ranging from -9,223,372,036,854,775,808 to
 * 9,223,372,036,854,775,807. It is based on the std::int64_t type.
 */
using i64 = std::int64_t;

// Type Aliases for Floating-Point Numbers

/**
 * @typedef f32
 * @brief Represents a 32-bit floating-point number.
 *
 * This type alias is used for 32-bit floating-point numbers, which follow the IEEE 754 standard.
 * It is based on the float type.
 */
using f32 = float;

/**
 * @typedef f64
 * @brief Represents a 64-bit floating-point number.
 *
 * This type alias is used for 64-bit floating-point numbers, which follow the IEEE 754 standard.
 * It is based on the double type.
 */
using f64 = double;

// Type Aliases for Size Types

/**
 * @typedef usize
 * @brief Represents an unsigned size type.
 *
 * This type alias is used for representing the size of objects in bytes.
 * It is based on the std::size_t type, which is the result type of the sizeof operator.
 */
using usize = std::size_t;

/**
 * @typedef isize
 * @brief Represents a signed size type.
 *
 * This type alias is used for representing pointer differences.
 * It is based on the std::ptrdiff_t type, which is the signed integer type returned when
 * subtracting two pointers.
 */
using isize = std::ptrdiff_t;
