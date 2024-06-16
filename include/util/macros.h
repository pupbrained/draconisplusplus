#pragma once

/**
 * @brief Allows for rust-style function definitions
 */
#define fn auto

/**
 * @brief Allows for easy getter creation
 *
 * @param class_name The class to use
 * @param type       Type of the getter
 * @param name       Name of the getter
 */
#define DEFINE_GETTER(class_name, type, name) \
  fn class_name::get##name() const->type { return m_##name; }

/**
 * @brief Helper for making reflect-cpp impls
 *
 * @param struct_name The struct name
 * @param lower_name  The arg name
 * @param ...         Values of the class to convert
 */
#define DEF_IMPL(struct_name, ...)                                                   \
  struct struct_name##Impl {                                                         \
    __VA_ARGS__;                                                                     \
                                                                                     \
    static fn from_class(const struct_name& instance) noexcept -> struct_name##Impl; \
                                                                                     \
    [[nodiscard]] fn to_class() const -> struct_name;                                \
  };
