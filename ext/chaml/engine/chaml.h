#ifndef CHAML_H_
#define CHAML_H_

#include <stdlib.h>
#include <stdint.h>
#ifdef __CLANG__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#include <ruby.h>
#ifdef __CLANG__
#pragma clang diagnostic pop
#endif

#define AT_STACK(var, val)    \
  volatile auto var##_ = val; \
  register auto var = var##_

#define DATA_READY(type, var, arg)  \
  type* var;                        \
  Data_Get_Struct(arg, type, var)

#define CLASS(klass) rb_path2class(#klass)
#define CLASS_READY(klass) AT_STACK(klass, CLASS(klass))

#define METHOD(method) rb_intern(#method)
#define METHOD_READY(method) ID method = METHOD(method)

#define SYMBOL(sym) ID2SYM(rb_intern(#sym))
#ifndef SYMBOL_P
#define SYMBOL_P(value) (TYPE(value) == T_SYMBOL)
#endif

#define RUBY_EACH_FUNC(f) reinterpret_cast<int(*)(...)>(f)

#define SIZE_OF(x) static_cast<long>(sizeof(x))

template <typename... Args>
VALUE METHOD_CALL(const VALUE& recv, const ID& method, const Args&... args) {
  return rb_funcall(recv, method, sizeof...(args), args...);
}

namespace CHaml {
  namespace GC {
    struct gc;
  }

  namespace Converter {
    extern const char* preservation_regexp;
  }

  namespace Engine {
#define FORMAT_HTML5 0
#define FORMAT_HTML4 1
#define FORMAT_XHTML 2

    const int default_format = FORMAT_HTML5;
    struct engine {
      struct option_t {
        int format;
        bool escape_html;
        bool raise_unknown_option;
        int default_indent_depth;
      } options;
      VALUE templ;
      GC::gc* gc_pool;
    };

    VALUE initialize(int argc, VALUE* argv, VALUE self);

    VALUE render(int argc, VALUE* argv, VALUE self);
    VALUE open(VALUE self, VALUE file_name);
    VALUE append_option(VALUE self, VALUE options);
    VALUE concat(VALUE self, VALUE templ);
  }

  namespace String {
    struct string {
      char* buffer;
      long length;
    };

    string* gcnew(const char* buffer, long length, GC::gc* gc_pool);
    string* gcnew(const char* buffer, GC::gc* gc_pool);

    bool eq(string* s1, const char* s2);
    bool eq(string* s1, string* s2);

    bool find(string* s, long* index, char ch);
    bool find_first_valid_index(string* s, long* index);
    bool find_first_invalid_index(string* s, long* index);
    bool find_last_valid_index(string* s, long* index);
    long find_last_valid_index(string* s);
    long skip_tag_options(string* s, long* index);

    string* tag_options(string* s, long* index, GC::gc* gc_pool);
    string* tok(string* s, long* index, GC::gc* gc_pool);
    string* tok_lcase(string* s, long* index, GC::gc* gc_pool);
    string* doctype(string* s, long* index, GC::gc* gc_pool);
    string* rest(string* s, long index, GC::gc* gc_pool);
    string* rest(string* s, long* index, GC::gc* gc_pool);

    void chomp(string* s);

    void print(string* s);
  }

  namespace Converter {
    struct string_chain {
      string_chain* next;
      String::string* s;
    };

    struct line {
      int indent_depth;
      string_chain *first, *last;
    };

    struct lines {
      lines* next;
      line* l;
    };

    struct tree {
      tree* subtree;
      tree* next;
      line* l;
    };

    typedef Engine::engine::option_t Option;

    tree* haml_from_haml_plaintext(char* buffer, long length, const Option& options, GC::gc* gc_pool);
    tree* static_haml_from_haml(tree* t, VALUE location, const Option& options, GC::gc* gc_pool);
    tree* html_from_static_haml(tree* t, int* max_indent_depth, const Option& options, GC::gc* gc_pool);
    VALUE flatten(tree* t, int max_indent_depth, GC::gc* gc_pool);
  }

#define MEMBER_NAME(ns, t)        ns##_##t##_pool
#define MEMBER_TYPE(ns, t)        ns##_##t##_pool_t
#define GC_POOL_SIZE(ns, t)       ns##_##t##_pool_size
#define GCNEW_NAME(ns, t) gcnew_##ns##_##t

#define DECLARE_GC(ns, t)               \
  const int GC_POOL_SIZE(ns, t) = 1024; \
  struct MEMBER_TYPE(ns, t) {           \
    MEMBER_TYPE(ns, t)* next;           \
    int max_using_heap_index;           \
    ns::t pool[GC_POOL_SIZE(ns, t)];    \
  };                                    \
  ns::t* GCNEW_NAME(ns, t)(struct gc*)

#define DECLARE_MEMBER(ns, t) MEMBER_TYPE(ns, t)* MEMBER_NAME(ns, t)

  namespace GC {
    DECLARE_GC(String, string);
    DECLARE_GC(Converter, string_chain);
    DECLARE_GC(Converter, line);
    DECLARE_GC(Converter, lines);
    DECLARE_GC(Converter, tree);

    const int VALUE_pool_size = 1024;
    struct VALUE_t {
      VALUE_t* next;
      int max_using_heap_index;
      VALUE pool[VALUE_pool_size];
    };
    void gc_register_value(const VALUE& value, gc* pool);

    struct char_t {
      char_t* next;
      char pool[];
    };
    char* gc_alloc_n_char(long length, gc* pool);

    struct gc {
      DECLARE_MEMBER(String, string);
      DECLARE_MEMBER(Converter, string_chain);
      DECLARE_MEMBER(Converter, line);
      DECLARE_MEMBER(Converter, lines);
      DECLARE_MEMBER(Converter, tree);
      VALUE_t* value;
      char_t* ch;
    };

    gc* init();
    void final(gc* pool);
  }

}

#endif  // CHAML_H_
