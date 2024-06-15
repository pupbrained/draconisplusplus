#pragma once

#define fn auto

#define DEFINE_GETTER(class_name, type, name) \
  fn class_name::get##name() const -> type { return m_##name; }

#define DEF_IMPL(struct_name, lower_name, ...)                                \
  struct struct_name##Impl {                                                  \
    __VA_ARGS__;                                                              \
    static fn from_class(const struct_name& lower_name) -> struct_name##Impl; \
    [[nodiscard]] fn to_class() const -> struct_name;                         \
  };
