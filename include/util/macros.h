#define fn auto
#define DEFINE_GETTER(class_name, type, name) \
  fn class_name::get##name() const->type { return m_##name; }
