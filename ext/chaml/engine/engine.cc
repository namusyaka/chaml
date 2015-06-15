#include "./chaml.h"

/* # abstruct
 * module CHaml
 *   class Engine
 *     def initialize(option = {})
 *       # do something ...
 *     end
 *
 *     def open(file_name)
 *       # do something ...
 *     end
 *
 *     def concat(templ)
 *       # do something ...
 *     end
 *
 *     def append_option(option)
 *       # do something ...
 *     end
 *
 *     def render(location = self)
 *       # do something ...
 *     end
 *
 *     class UnknownOptionError < StandardError
 *     end
 *
 *     class UnknownParameterError < StandardError
 *     end
 *   end
 * end
 */

static VALUE chaml, engine;
static VALUE err_unknown_option, err_unknown_param;

static VALUE sym_format, sym_escape_html, sym_raise_unknown_option, sym_default_indent_depth;

namespace CHaml {
  namespace Engine {
    static VALUE alloc(VALUE klass);
  }
}

#define DEFINE_METHOD(klass, name, argc)          \
  rb_define_method(klass, #name,                  \
      RUBY_METHOD_FUNC(CHaml::Engine::name), argc)

#define DEFINE_PRIVATE_METHOD(klass, name, argc)  \
  rb_define_private_method(klass, #name,          \
      RUBY_METHOD_FUNC(CHaml::Engine::name), argc)

#define DECLARE_ERROR_CLASS_UNDER(value, name, super)                 \
  rb_gc_register_address(&err_##value);                               \
  err_##value = rb_define_class_under(super, name, rb_eStandardError)

#define PRELOAD_SYMBOL(sym)           \
  rb_gc_register_address(&sym_##sym); \
  sym_##sym = SYMBOL(sym)

extern "C" void Init_engine(void) {
  rb_gc_register_address(&chaml);
  rb_gc_register_address(&engine);

  chaml  = rb_define_module("CHaml");

  engine = rb_define_class_under(chaml, "Engine", rb_cObject);
  rb_define_alloc_func(engine, CHaml::Engine::alloc);

  DEFINE_PRIVATE_METHOD(engine, initialize, -1);
  DEFINE_METHOD(engine, open, 1);
  DEFINE_METHOD(engine, concat, 1);
  DEFINE_METHOD(engine, append_option, 1);
  DEFINE_METHOD(engine, render, -1);

  DECLARE_ERROR_CLASS_UNDER(unknown_option, "UnknownOptionError",    chaml);
  DECLARE_ERROR_CLASS_UNDER(unknown_param,  "UnknownParameterError", chaml);

  PRELOAD_SYMBOL(format);
  PRELOAD_SYMBOL(escape_html);
  PRELOAD_SYMBOL(raise_unknown_option);
  PRELOAD_SYMBOL(default_indent_depth);
  return;
}

namespace CHaml {
  namespace Engine {

    static void mark(GC::VALUE_t* gc_value_pool) {
      while (gc_value_pool != NULL) {
        for (auto i = 0; i <= gc_value_pool->max_using_heap_index; i++) {
          rb_gc_mark(gc_value_pool->pool[i]);
        }
        gc_value_pool = gc_value_pool->next;
      }
      return;
    }

    static void mark(GC::gc* gc_pool) {
      if (gc_pool != NULL) {
        mark(gc_pool->value);
      }
      return;
    }

    static void mark(engine* e) {
      rb_gc_mark(e->templ);
      mark(e->gc_pool);
      return;
    }

    static void mark(void* e) {
      mark(static_cast<engine*>(e));
      return;
    }

    static VALUE alloc(VALUE klass) {
      return Data_Wrap_Struct(klass, mark, -1, ALLOC(engine));
    }

    static int merge_option_body(VALUE key, VALUE value, VALUE self) {
      METHOD_READY(to_sym);
      DATA_READY(engine, e, self);

      if (!SYMBOL_P(key)) {
        key = METHOD_CALL(key, to_sym);
      }
      // SPECIAL_CONST_P => true iff. value in [NilClass, TrueClass, FalseClass, Fixnum, Symbol]
      if (!SPECIAL_CONST_P(value)) {
        value = METHOD_CALL(value, to_sym);
      }

      if (key == sym_format) {
        if (value == SYMBOL(html4)) {
          e->options.format = FORMAT_HTML4;
        } else if (value == SYMBOL(html5)) {
          e->options.format = FORMAT_HTML5;
        } else if (value == SYMBOL(xhtml)) {
          e->options.format = FORMAT_XHTML;
        } else {
          AT_STACK(rs, METHOD_CALL(value, METHOD(to_s)));
          const char* s = StringValuePtr(rs);
          rb_raise(err_unknown_param, "unknown parameter `%s' for `format' detecetd.", s);
        }
      } else if (key == sym_escape_html) {
        if (value == Qnil || value == Qfalse) {
          e->options.escape_html = false;
        } else {
          e->options.escape_html = true;
        }
      } else if (key == sym_raise_unknown_option) {
        if (value == Qnil || value == Qfalse) {
          e->options.raise_unknown_option = false;
        } else if (value == Qtrue) {
          e->options.raise_unknown_option = true;
        } else {
          AT_STACK(rs, METHOD_CALL(value, METHOD(to_s)));
          const char* s = StringValuePtr(rs);
          rb_raise(err_unknown_param, "unknown parameter `%s' for `raise_unknown_option' detected.", s);
        }
      } else if (key == sym_default_indent_depth) {
        e->options.default_indent_depth = FIX2INT(value);
      } else {
        if (e->options.raise_unknown_option) {
          AT_STACK(rs, METHOD_CALL(key, METHOD(to_s)));
          const char* s = StringValuePtr(rs);
          rb_raise(err_unknown_option, "unknwon option `%s' detected.", s);
        }
      }

      return ST_CONTINUE;
    }

    // def append_option(options) # options: Hash
    VALUE append_option(VALUE self, VALUE options) {
      Check_Type(options, T_HASH);

      /*
       * @options.merge! options
       */

      rb_hash_foreach(options, RUBY_EACH_FUNC(merge_option_body), self);

      return self;
    }

    static const engine::option_t default_options = {
#ifdef __CLANG__
      .format               = default_format,
      .escape_html          = false,
      .raise_unknown_option = true,
      .default_indent_depth = 2,
#else
      format              : default_format,
      escape_html         : false,
      raise_unknown_option: true,
      default_indent_depth: 2,
#endif
    };

    // def initialize(template, options = {})
    VALUE initialize(int argc, VALUE* argv, VALUE self) {
      volatile VALUE options_;
      volatile VALUE templ_;
      rb_scan_args(argc, argv, "11", &templ_, &options_);

      if(TYPE(templ_) != T_STRING)
        rb_raise(rb_eTypeError, "type mismatch: %s given", rb_obj_classname(templ_));

      register auto options = options_;
      DATA_READY(engine, e, self);

      e->options = default_options;
      e->templ   = templ_;
      e->gc_pool = NULL;

      if (!NIL_P(options)) {
        append_option(self, options);
      }

      return Qnil;
    }

    // def open(file_name) # file_name: String
    VALUE open(VALUE self, VALUE file_name) {
      Check_Type(file_name, T_STRING);
      DATA_READY(engine, e, self);

      /*
       * file   = File.open(file_name)
       * @templ = file.read
       * file.close
       */

      AT_STACK(file, METHOD_CALL(CLASS(File), METHOD(open), file_name));
      e->templ = METHOD_CALL(file, METHOD(read));
      METHOD_CALL(file, METHOD(close));

      return self;
    }

    // def concat(templ) # templ: String
    VALUE concat(VALUE self, VALUE templ) {
      Check_Type(templ, T_STRING);
      DATA_READY(engine, e, self);

      /*
       * @templ.concat(templ)
       */
      METHOD_CALL(e->templ, METHOD(concat), templ);

      return self;
    }

    // def render(location = self)
    VALUE render(int argc, VALUE* argv, VALUE self) {
      // location ||= self
      volatile VALUE location_;
      rb_scan_args(argc, argv, "01", &location_);
      register auto location = location_;
      if (NIL_P(location)) {
        location = self;
      }

      // preload regexp for preservation
      METHOD_CALL(location, METHOD(instance_eval), rb_str_new2(Converter::preservation_regexp));

      DATA_READY(engine, e, self);
      auto gc_pool = GC::init();
      e->gc_pool = gc_pool;

      // templ.end_with_cr!
      METHOD_CALL(e->templ, METHOD(concat), rb_str_new2("\n"));

      auto templ = StringValuePtr(e->templ);
      auto len = NUM2INT(METHOD_CALL(e->templ, METHOD(bytesize)));

      int max_indent_depth = 0;
      auto haml        = Converter::haml_from_haml_plaintext(templ, len, e->options, gc_pool);
      auto static_haml = Converter::static_haml_from_haml(haml, location, e->options, gc_pool);
      auto html        = Converter::html_from_static_haml(static_haml, &max_indent_depth, e->options, gc_pool);
      auto ret         = Converter::flatten(html, max_indent_depth, gc_pool);

      e->gc_pool = NULL;
      GC::final(gc_pool);
      return ret;
    }

  }
}
