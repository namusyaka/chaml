#include "./chaml.h"

#define DEFINE_GC(ns, t)                                                              \
ns::t* GCNEW_NAME(ns, t)(gc* pool) {                                                  \
  if (pool->MEMBER_NAME(ns, t) == NULL) {                                             \
    pool->MEMBER_NAME(ns, t) = alloc<MEMBER_TYPE(ns, t)>();                           \
  } else if (pool->MEMBER_NAME(ns, t)->max_using_heap_index == GC_POOL_SIZE(ns, t)) { \
    auto new_pool  = alloc<MEMBER_TYPE(ns, t)>();                                     \
    new_pool->next = pool->MEMBER_NAME(ns, t);                                        \
    pool->MEMBER_NAME(ns, t) = new_pool;                                              \
  }                                                                                   \
                                                                                      \
  auto index = ++pool->MEMBER_NAME(ns, t)->max_using_heap_index;                      \
  return &pool->MEMBER_NAME(ns, t)->pool[index];                                      \
}

#define READY(ns, t) ret->MEMBER_NAME(ns, t) = NULL
#define FINAL(ns, t) final(gc_pool->MEMBER_NAME(ns, t))

namespace CHaml {
  namespace GC {

    template <typename T>
    T* alloc() {
      auto ret = ALLOC(T);
      ret->max_using_heap_index = -1;
      ret->next = NULL;
      return ret;
    }

    template <typename T>
    void final(T* pool) {
      while (pool) {
        auto next = pool->next;
        xfree(pool);
        pool = next;
      }
      return;
    }

    DEFINE_GC(String, string);
    DEFINE_GC(Converter, string_chain);
    DEFINE_GC(Converter, line);
    DEFINE_GC(Converter, lines);
    DEFINE_GC(Converter, tree);

    void gc_register_value(const VALUE& value, gc* pool) {
      if (pool->value == NULL) {
        pool->value = alloc<VALUE_t>();
      } else if (pool->value->max_using_heap_index == VALUE_pool_size) {
        auto new_pool  = alloc<VALUE_t>();
        new_pool->next = pool->value;
        pool->value    = new_pool;
      }

      auto index = ++pool->value->max_using_heap_index;
      pool->value->pool[index] = value;
      return;
    }

    char* gc_alloc_n_char(long length, gc* pool) {
      auto new_pool = static_cast<char_t*>(xmalloc(sizeof(char_t) + sizeof(char) * static_cast<size_t>(length)));
      new_pool->next = pool->ch;
      pool->ch = new_pool;
      return pool->ch->pool;
    }

    gc* init() {
      auto ret = ALLOC(gc);
      READY(String, string);
      READY(Converter, string_chain);
      READY(Converter, line);
      READY(Converter, lines);
      READY(Converter, tree);
      ret->value = NULL;
      ret->ch    = NULL;
      return ret;
    }

    void final(gc* gc_pool) {
      FINAL(String, string);
      FINAL(Converter, string_chain);
      FINAL(Converter, line);
      FINAL(Converter, lines);
      FINAL(Converter, tree);
      final(gc_pool->value);
      final(gc_pool->ch);
      xfree(gc_pool);
      return;
    }

  }
}
