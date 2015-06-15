#include "./chaml.h"

#define GCNEW(t, pool) GCNEW_NAME(Converter, t)(pool)

namespace CHaml {
  namespace Converter {

    static string_chain* gcnew(String::string* s, GC::gc* gc_pool) {
      auto ret = GCNEW(string_chain, gc_pool);
      ret->next = NULL;
      ret->s = s;
      return ret;
    }

    static string_chain* gcnew(const char* buffer, GC::gc* gc_pool) {
      return gcnew(String::gcnew(buffer, gc_pool), gc_pool);
    }

    static line* gcnew(int indent_depth, String::string* s, GC::gc* gc_pool) {
      auto ret = GCNEW(line, gc_pool);
      ret->indent_depth = indent_depth;
      ret->first = ret->last = gcnew(s, gc_pool);
      return ret;
    }

    static line* gcnew(int indent_depth, const char* s, GC::gc* gc_pool) {
      auto ret = GCNEW(line, gc_pool);
      ret->indent_depth = indent_depth;
      ret->first = ret->last = gcnew(String::gcnew(s, gc_pool), gc_pool);
      return ret;
    }

    static lines* gcnew(line* l, GC::gc* gc_pool) {
      auto ret = GCNEW(lines, gc_pool);
      ret->next = NULL;
      ret->l = l;
      return ret;
    }

    static tree* gcnew_tree(line* l, GC::gc* gc_pool) {
      auto ret = GCNEW(tree, gc_pool);
      ret->subtree = NULL;
      ret->next    = NULL;
      ret->l       = l;
      return ret;
    }

    static tree* gcnew_tree(int indent_depth, const char* buffer, GC::gc* gc_pool) {
      return gcnew_tree(gcnew(indent_depth, String::gcnew(buffer, gc_pool), gc_pool), gc_pool);
    }

    static String::string* connect_chain(string_chain* sc, GC::gc* gc_pool) {
      // calc sum of length

      long length = 0;
      for (auto p = sc; p != NULL; p = p->next) {
        length += p->s->length;
      }

      // string-chain -> cstr
      auto buffer = GC::gc_alloc_n_char(length, gc_pool);
      long i = 0;
      for (auto p = sc; p != NULL; p = p->next) {
        memcpy(buffer + i, p->s->buffer, static_cast<size_t>(p->s->length));
        i += p->s->length;
      }
      return String::gcnew(buffer, length, gc_pool);
    }

    static lines* sprit_cr(char* buffer, long length, const Option& options, GC::gc* gc_pool) {
      auto *s = buffer, *p = buffer, *e = buffer + length;
      lines *ret = NULL, *ret_last = NULL;
      int indent_depth;

      while (p < e) {
        /// initialize
        indent_depth = 0;

        /// lexical analyze
        // count indents
        for (; p < e; p++) {
          if (*p == '\t') {
            auto default_indent_depth = options.default_indent_depth;
            auto over = indent_depth % default_indent_depth;
            indent_depth += default_indent_depth - over;
          } else if (*p == ' ') {
            indent_depth++;
          } else {
            // end of the indent
            break;
          }
        }
        // continue if line.empty?
        if (*p == '\n') {
          p++;
          continue;
        }
        // find end of line
        for (s = p; p < e && *p != '\n'; p++) {}
        // skip carriage return iff. it was carriage return.
        if (*p == '\n') {
          p++;
        }

        /// final
        auto l = gcnew(indent_depth, String::gcnew(s, p - s, gc_pool), gc_pool);
        if (ret) {
          ret_last->next = gcnew(l, gc_pool);
          ret_last = ret_last->next;
        } else {
          ret = ret_last = gcnew(l, gc_pool);
        }
      }
      return ret;
    }

    static tree* parse(lines* ls, lines** parsing, GC::gc* gc_pool) {
      auto ret = gcnew_tree(ls->l, gc_pool);
      auto p = ret;
      auto indent_depth = ls->l->indent_depth;
      ls = ls->next;

      while (ls) {
        if (indent_depth < ls->l->indent_depth) {
          p->subtree = parse(ls, &ls, gc_pool);
        } else if (indent_depth > ls->l->indent_depth) {
          break;
        } else {
          p = p->next = gcnew_tree(ls->l, gc_pool);
          ls = ls->next;
        }
      }

      *parsing = ls;
      return ret;
    }

    static tree* remove_comments(tree* t) {
      if (t == NULL) {
        return NULL;
      }

      t->subtree = remove_comments(t->subtree);
      t->next    = remove_comments(t->next);

      // delete iff. line start with '-#'
      auto s = t->l->first->s;
      if (s->length >= 2 && s->buffer[0] == '-' && s->buffer[1] == '#') {
        t = t->next;
      }
      return t;
    }

    static lines* solve_multiline(lines* ls, GC::gc* gc_pool) {
      auto ret = ls;
      while (ls != NULL) {
        auto s = ls->l->first->s;
        auto i = find_last_valid_index(s);

        // connect lines iff. there are ~/\|[ \t\n]*$/
        if (s->buffer[i] == '|') {
          i--;
          find_last_valid_index(s, &i);
          s->length = i + 2;
          s->buffer[s->length - 1] = ' ';
          auto l      = gcnew(ls->l->indent_depth, s, gc_pool);
          auto sc     = l->first;
          auto old_ls = ls;
          ls = ls->next;
          while (ls != NULL) {
            s = ls->l->first->s;
            i = find_last_valid_index(s);
            if (s->buffer[i] != '|') {
              break;
            }
            i--;
            find_last_valid_index(s, &i);
            s->length = i + 2;
            s->buffer[s->length - 1] = ' ';
            sc = sc->next = gcnew(s, gc_pool);
            ls = ls->next;
          }
          old_ls->l = gcnew(l->indent_depth, connect_chain(l->first, gc_pool), gc_pool);
          old_ls->next = ls;
          ls = old_ls;
          s = ls->l->first->s;
          s->length--;
        }

        // connect line iff. that will continues attributes
        if (s->buffer[0] == '%') {
          i = 1;
          find_first_invalid_index(s, &i);
          switch (s->buffer[i]) {
            case '.':
            case '#':
            case '(':
            case '{': {
              auto l = gcnew(ls->l->indent_depth, s, gc_pool);
              auto sc = l->first;
              auto old_ls = ls;
              auto parents = 0;
              char string_type = 0;
              auto inside_string = false;
              bool attr_ends = false;
              while (i < s->length && !attr_ends) {
                switch (s->buffer[i]) {
                  case '(':
                  case '{':
                  case '[':
                    if (!inside_string) {
                      parents++;
                    }
                    break;
                  case ')':
                  case '}':
                  case ']':
                    if (!inside_string) {
                      parents--;
                    }
                    break;
                  case '\\':
                    if (inside_string) {
                      i++;
                    }
                    break;
                  case '"':
                  case '\'':
                    if (inside_string) {
                      if (string_type == s->buffer[i]) {
                        inside_string = false;
                      }
                    } else {
                      string_type = s->buffer[i];
                      inside_string = true;
                    }
                    break;
                  case '\n':
                    if (!inside_string && parents == 0) {
                      attr_ends = true;
                      break;
                    }
                    s->buffer[i] = ' ';
                    ls = ls->next;
                    s = ls->l->first->s;
                    sc = sc->next = gcnew(s, gc_pool);
                    i = -1;
                    break;
                  case ' ':
                    if (!inside_string && parents == 0) {
                      attr_ends = true;
                    }
                    break;
                }
                i++;
              }
              if (old_ls != ls) {
                old_ls->l = gcnew(l->indent_depth, connect_chain(l->first, gc_pool), gc_pool);
                old_ls->next = ls->next;
                ls = old_ls;
              }
              break;
            }
          }
        }
        ls = ls->next;
      }
      return ret;
    }

    tree* haml_from_haml_plaintext(char* buffer, long length, const Option& options, GC::gc* gc_pool) {
      auto ls = solve_multiline(sprit_cr(buffer, length, options, gc_pool), gc_pool);
      if (ls == NULL) {
        return NULL;
      }
      return remove_comments(parse(ls, &ls, gc_pool));
    }

    // return s.first == '-'
    static bool silent_script(line* l) {
      auto s = l->first->s;
      long index = 0;
      find_first_valid_index(s, &index);
      switch (s->buffer[index]) {
        case '-':
          return true;
      }
      return false;
    }

    // return s.first == ':'
    static bool is_filter(line* l) {
      auto s = l->first->s;
      long index = 0;
      find_first_valid_index(s, &index);
      switch (s->buffer[index]) {
        case ':':
          return true;
      }
      return false;
    }

    static void increment_indents(tree* t, int indent_depth) {
      if (t == NULL) {
        return;
      }

      increment_indents(t->subtree, indent_depth);
      increment_indents(t->next,    indent_depth);
      t->l->indent_depth += indent_depth;
      return;
    }

    static void decrement_indents(line* l, int indent_depth) {
      if (l == NULL) {
        return;
      }

      l->indent_depth -= indent_depth;
      if (l->indent_depth < 0) {
        l->indent_depth = 0;
      }
      return;
    }

    static void decrement_indents(tree* t, int indent_depth) {
      if (t == NULL) {
        return;
      }

      decrement_indents(t->subtree, indent_depth);
      decrement_indents(t->next,    indent_depth);
      t->l->indent_depth -= indent_depth;
      if (t->l->indent_depth < 0) {
        t->l->indent_depth = 0;
      }
      return;
    }

    // return true iff. s ~ /\\|#{/
    static bool is_dynamic_part(String::string* s, long index) {
      for (; index < s->length; index++) {
        switch (s->buffer[index]) {
          case '\\':
            return true;
          case '#':
            if (index + 1 < s->length && s->buffer[index + 1] == '{') {
              return true;
            }
            break;
        }
      }
      return false;
    }

    // return t.map &:plain
    static void plainize(tree* t, VALUE location, GC::gc* gc_pool) {
      if (t == NULL) {
        return;
      }

      plainize(t->subtree, location, gc_pool);
      plainize(t->next   , location, gc_pool);

      if (is_dynamic_part(t->l->first->s, 0)) {
        auto expr_t = gcnew(0, "\"\\\\ ", gc_pool);
        auto sc     = expr_t->first;
        sc = sc->next = gcnew(t->l->first->s, gc_pool);
        sc = sc->next = gcnew("\"", gc_pool);
        auto expr_s = connect_chain(expr_t->first, gc_pool);
        AT_STACK(expr, rb_str_new(expr_s->buffer, expr_s->length));
        AT_STACK(ret, METHOD_CALL(location, METHOD(instance_eval), expr));

        gc_register_value(ret, gc_pool);
        auto rs = StringValuePtr(ret);
        t->l->first->s = String::gcnew(rs, gc_pool);
      } else {
        auto old_first = t->l->first;
        t->l->first = gcnew("\\ ", gc_pool);
        t->l->first->next = old_first;
      }
      return;
    }

    // return t.map &:escape_html
    static void escapilze(tree* t, GC::gc* gc_pool) {
      if (t == NULL) {
        return;
      }

      escapilze(t->subtree, gc_pool);
      escapilze(t->next   , gc_pool);
      auto old_first = t->l->first;
      t->l->first = gcnew("& ", gc_pool);
      t->l->first->next = old_first;
      t->l->first->s = connect_chain(t->l->first, gc_pool);
      t->l->first->next = NULL;
      return;
    }

    static int calc_max_indent_depth(tree* t) {
      if (t == NULL) {
        return 0;
      }

      auto max     = t->l->indent_depth;
      auto subtree = calc_max_indent_depth(t->subtree);
      auto next    = calc_max_indent_depth(t->next);
      if (max < subtree) {
        max = subtree;
      }
      if (max < next) {
        max = next;
      }
      return max;
    }

    static String::string* flatten_(tree* t, int max_indent_depth, GC::gc* gc_pool);
    // return t.map &:preserve
    static void preservate(tree* t, VALUE location, GC::gc* gc_pool) {
      auto max_indent_depth = calc_max_indent_depth(t);
      auto expr_t = gcnew(0, "\"\\\\ ", gc_pool);
      auto sc     = expr_t->first;
      sc = sc->next = gcnew(0, flatten_(t, max_indent_depth, gc_pool), gc_pool)->first;
      sc = sc->next = gcnew("\".gsub(/\\n/,'&#x000A;')"
                            ".gsub(/\\r/,'')"
                            ".concat(\"\\n\")", gc_pool);
      auto expr_s = connect_chain(expr_t->first, gc_pool);
      AT_STACK(expr, rb_str_new(expr_s->buffer, expr_s->length));
      AT_STACK(ret, METHOD_CALL(location, METHOD(instance_eval), expr));

      gc_register_value(ret, gc_pool);
      auto rs = StringValuePtr(ret);
      t->l->first->s = String::gcnew(rs, gc_pool);
      t->l->last = t->l->first;
      t->l->first->next = NULL;
      t->subtree = NULL;
      t->next    = NULL;
      return;
    }

    static tree* solve_filter(tree* t, VALUE location, const Option& options, GC::gc* gc_pool) {
      auto s = t->l->first->s;
      long index = 1;
      auto filter = String::tok(s, &index, gc_pool);
      switch (filter->length) {
        case 3:
          if (String::eq(filter, "css")) {
            if (options.format == FORMAT_XHTML) {
              t->l->first->s = String::gcnew("<style type='text/css'>\n", gc_pool);
              auto old_subtree = t->subtree;
              t->subtree = gcnew_tree(t->l->indent_depth, "\\/*<![CDATA[*/\n", gc_pool);
              t->subtree->subtree = old_subtree;
              t->subtree->next    = gcnew_tree(t->l->indent_depth, "\\/*]]>*/\n", gc_pool);
              increment_indents(t->subtree, options.default_indent_depth);
            } else {
              t->l->first->s = String::gcnew("<style>\n", gc_pool);
            }
            auto old_next = t->next;
            t = t->next = gcnew_tree(t->l->indent_depth, "</style>\n", gc_pool);
            t->next = old_next;
          }
          break;
        case 5:
          if (String::eq(filter, "cdata")) {
            t->l->first->s = String::gcnew("<![CDATA[\n", gc_pool);
            auto old_next = t->next;
            t = t->next = gcnew_tree(t->l->indent_depth, "]]>\n", gc_pool);
            t->next = old_next;
          } else if (String::eq(filter, "plain")) {
            t->l->first->s = String::gcnew("", gc_pool);
            decrement_indents(t->subtree, options.default_indent_depth);
            plainize(t->subtree, location, gc_pool);
          }
          break;
        case 7:
          if (String::eq(filter, "escaped")) {
            t->l->first->s = String::gcnew("", gc_pool);
            decrement_indents(t->subtree, options.default_indent_depth);
            escapilze(t->subtree, gc_pool);
          }
          break;
        case 8:
          if (String::eq(filter, "preserve")) {
            t->l->first->s = String::gcnew("", gc_pool);
            if (t->subtree != NULL) {
              decrement_indents(t->subtree, t->subtree->l->indent_depth);
              preservate(t->subtree, location, gc_pool);
            }
          }
          break;
        case 10:
          if (String::eq(filter, "javascript")) {
            if (options.format == FORMAT_XHTML) {
              t->l->first->s = String::gcnew("<script type='text/javascript'>\n", gc_pool);
              auto old_subtree = t->subtree;
              t->subtree = gcnew_tree(t->l->indent_depth, "\\//<![CDATA[\n", gc_pool);
              t->subtree->subtree = old_subtree;
              t->subtree->next    = gcnew_tree(t->l->indent_depth, "\\//]]>\n", gc_pool);
              increment_indents(t->subtree, options.default_indent_depth);
            } else {
              t->l->first->s = String::gcnew("<script>\n", gc_pool);
            }
            auto old_next = t->next;
            t = t->next = gcnew_tree(t->l->indent_depth, "</script>\n", gc_pool);
            t->next = old_next;
          }
          break;
      }
      return t;
    }

    // return true iff. l ~ /^[-=~]|[&!]=/
    static bool is_script(line* l) {
      auto s = l->first->s;
      long index = 0;
      find_first_valid_index(s, &index);
      switch (s->buffer[index]) {
        case '-':
        case '=':
        case '~':
          return true;
        case '&':
        case '!':
          if (s->length >= 2 && s->buffer[1] == '=') {
            return true;
          }
      }
      return false;
    }

    // return true iff. s ~ /^[\\-=~:.#]|[&!]=/ or s.has_attribute or s ~ /\\|#{/
    static bool is_dynamic(String::string* s) {
      long index = 0;
      find_first_valid_index(s, &index);
      switch (s->buffer[index]) {
        case '\\':
          return false;
        case '-':
        case '=':
        case '~':
        case ':':
          return true;
        case '%':
          index++;
          find_first_invalid_index(s, &index);
          // s: tag, and it have attributes
          switch (s->buffer[index]) {
            case '.':
            case '#':
            case '(':
            case '{':
              return true;
          }
          // '=' in tag options
          if (skip_tag_options(s, &index) != -1) {
            return true;
          }
          break;
        case '.':
        case '#':
          return true;
        case '&':
        case '!':
          if (s->length >= 2 && s->buffer[1] == '=') {
            return true;
          }
          index++;
      }
      return is_dynamic_part(s, index);
    }

    // haml-formed attributes -> inner-formed attributes
    static line* solve_attr(String::string* s, long* index, const Option& options, GC::gc* gc_pool) {
      auto ret = gcnew(0, "@_h={};@_s={id:[nil],class:[]};", gc_pool);
      auto sc  = ret->first;
      auto i = *index;
      while (i < s->length) {
        auto ch = s->buffer[i];
        if (ch == '{') {
          auto j = i++;
          auto parents = 1;
          char string_type = 0;
          auto inside_string = false;
          while (i < s->length && parents != 0) {
            switch (s->buffer[i]) {
              case '(':
              case '{':
              case '[':
                if (!inside_string) {
                  parents++;
                }
                break;
              case ')':
              case '}':
              case ']':
                if (!inside_string) {
                  parents--;
                }
                break;
              case '\\':
                if (inside_string) {
                  i++;
                }
                break;
              case '"':
              case '\'':
                if (inside_string) {
                  if (string_type == s->buffer[i]) {
                    inside_string = false;
                  }
                } else {
                  string_type = s->buffer[i];
                  inside_string = true;
                }
                break;
            }
            i++;
          }
          s->buffer[j] = '[';
          s->buffer[i - 1] = ']';
          sc = sc->next = gcnew(String::gcnew(s->buffer + j, i - j, gc_pool), gc_pool);
          sc = sc->next = gcnew(".each{|h|h.each{|k,v|k=k.to_sym;"
              "if k==:id||k==:class;"
                "@_s[k]<<v;"
              "elsif v.is_a?Hash;"
                "v.each{|s,t|@_h[[k,s].map{|x|x.to_s}.join('-').gsub('_','-')]=t};"
              "else;"
                "@_h[k]=v;"
              "end}};", gc_pool);
        } else if (ch == '(') {
          i++;
          while (i < s->length && ch != ')') {
            auto j = i;
            find_first_invalid_index(s, &i);
            auto key = String::gcnew(s->buffer + j, i - j, gc_pool);
            j = ++i;
            if (s->buffer[i] == '\'' || s->buffer[i] == '"') {
              i++;
              find(s, &i, s->buffer[i - 1]);
              i++;
            } else {
              find_first_invalid_index(s, &i);
            }
            auto value = String::gcnew(s->buffer + j, i - j, gc_pool);
            if (String::eq(key, "id") || String::eq(key, "class")) {
              sc = sc->next = gcnew("@_s[:", gc_pool);
              sc = sc->next = gcnew(key, gc_pool);
              sc = sc->next = gcnew("]<<((", gc_pool);
              sc = sc->next = gcnew(value, gc_pool);
              sc = sc->next = gcnew(").to_s);", gc_pool);
            } else {
              sc = sc->next = gcnew("@_h[:'", gc_pool);
              sc = sc->next = gcnew(key, gc_pool);
              sc = sc->next = gcnew("']=(", gc_pool);
              sc = sc->next = gcnew(value, gc_pool);
              sc = sc->next = gcnew(");", gc_pool);
            }
            find_first_valid_index(s, &i);
            ch = s->buffer[i];
          }
          i++;
        } else if (ch == '.') {
          auto j = ++i;
          find_first_invalid_index(s, &i);
          auto klass = String::gcnew(s->buffer + j, i - j, gc_pool);
          sc = sc->next = gcnew("@_s[:class]<<'", gc_pool);
          sc = sc->next = gcnew(klass, gc_pool);
          sc = sc->next = gcnew("';", gc_pool);
        } else if (ch == '#') {
          auto j = ++i;
          find_first_invalid_index(s, &i);
          auto id = String::gcnew(s->buffer + j, i - j, gc_pool);
          sc = sc->next = gcnew("@_s[:id][0]='", gc_pool);
          sc = sc->next = gcnew(id, gc_pool);
          sc = sc->next = gcnew("';", gc_pool);
        } else {
          break;
        }
      }
      if (options.format == FORMAT_XHTML) {
        sc = sc->next = gcnew("@_attr=' '<<(("
            "(@_h.to_a.map{|p|p[1]=p[0]if p[1]==true;\"#{p[0]}='#{p[1]}'\"})"
            "<<(@_s[:class].empty??nil:'class=\\''<<(@_s[:class].flatten.sort.join' ')<<'\\'')"
            "<<(@_s[:id].keep_if{|i|i}.empty??nil:'id=\\''<<(@_s[:id].join'_')<<'\\'')"
            ").keep_if{|i|i}.join(' ').gsub('}','\\}'));", gc_pool);
      } else {
        sc = sc->next = gcnew("@_attr=' '<<(("
            "(@_h.to_a.map{|p|if p[1]==true;p[0].to_s;else;\"#{p[0]}='#{p[1]}'\";end})"
            "<<(@_s[:class].empty??nil:'class=\\''<<(@_s[:class].flatten.sort.join' ')<<'\\'')"
            "<<(@_s[:id].keep_if{|i|i}.empty??nil:'id=\\''<<(@_s[:id].join'_')<<'\\'')"
            ").keep_if{|i|i}.join(' ').gsub('}','\\}'));", gc_pool);
      }
      ret->last = sc;
      *index = i;
      return ret;
    }

#define PRESERVE ".gsub(@_r){|s|"                                                                     \
                   "s=~@_r;"                                                                          \
                   "r1,r2,r3=$1,$2,$3;"                                                               \
                   "\"<#{r1}#{r2}>#{r3.chomp(\"\\n\").gsub(/\\n/,'&#x000A;').gsub(/\\r/,'')}</#{r1}>\"" \
                 "}"
    static line* convert_to_ruby_form_support(line* ret, string_chain* sc, String::string* s, long index, GC::gc* gc_pool) {
      // opt
      auto i = index;
      auto j = String::skip_tag_options(s, &index);
      auto opt = String::gcnew(s->buffer + i, index - i, gc_pool);
      bool preserve = false;
      if (j != -1) {  // '=' in tag options
        if (s->buffer[j] == '~') {
          preserve = true;
        }
        opt->buffer[j - i] = opt->buffer[opt->length - 1];
        opt->length--;
      }
      sc = sc->next = gcnew(opt, gc_pool);
      sc = sc->next = gcnew(" ' << ", gc_pool);

      // rest
      auto rest = String::rest(s, &index, gc_pool);
      if (j != -1) {  // '=' in tag options
        sc = sc->next = gcnew("(", gc_pool);
        sc = sc->next = gcnew(rest, gc_pool);
        if (preserve) {
          sc = sc->next = gcnew(").to_s" PRESERVE " << \"\\n\";", gc_pool);
        } else {
          sc = sc->next = gcnew(").to_s << \"\\n\";", gc_pool);
        }
      } else {
        sc = sc->next = gcnew("\"", gc_pool);
        sc = sc->next = gcnew(rest, gc_pool);
        sc = sc->next = gcnew("\\n\";", gc_pool);
      }
      ret->last = sc;
      return ret;
    }

    // haml -> ruby expression
    static line* convert_to_ruby_form(line* l, const Option& options, GC::gc* gc_pool) {
      auto s = l->first->s;
      long index = 0;
      find_first_valid_index(s, &index);
      switch (s->buffer[index]) {
        case '-': {
          // "%buffer"
          auto ret = gcnew(0, String::rest(s, index + 1, gc_pool), gc_pool);
          auto sc  = ret->first;
          sc = sc->next = gcnew(";", gc_pool);
          ret->last = sc;
          return ret;
        }
        case '=': {
          // "@_ << (%buffer).to_s"
          auto ret = gcnew(0, "@_ << ((", gc_pool);
          auto sc  = ret->first;
          sc = sc->next = gcnew(String::rest(s, index + 1, gc_pool), gc_pool);
          sc = sc->next = gcnew(").to_s) << \"\\n\";", gc_pool);
          ret->last = sc;
          return ret;
        }
        case '~': {
          // "@_ << (%buffer).to_s.preserve"
          auto ret = gcnew(0, "@_ << ((", gc_pool);
          auto sc  = ret->first;
          sc = sc->next = gcnew(String::rest(s, index + 1, gc_pool), gc_pool);
          sc = sc->next = gcnew(").to_s" PRESERVE ") << \"\\n\";", gc_pool);
          ret->last = sc;
          return ret;
        }
        case '&': {
          // one of
          // "@_ << '& ' << (%rest).to_s"
          // "@_ << '& ' << \"%rest\""
          auto ret = gcnew(0, "@_ << '& ' << ", gc_pool);
          auto sc  = ret->first;

          index++;
          if (s->buffer[index] == '=') {
            sc = sc->next = gcnew("((", gc_pool);
            sc = sc->next = gcnew(String::rest(s, index + 1, gc_pool), gc_pool);
            sc = sc->next = gcnew(").to_s) << \"\\n\";", gc_pool);
          } else {
            sc = sc->next = gcnew("\"", gc_pool);
            sc = sc->next = gcnew(String::rest(s, index, gc_pool), gc_pool);
            sc = sc->next = gcnew("\\n\";", gc_pool);
          }

          ret->last = sc;
          return ret;
        }
        case '!': {
          // one of
          // "@_ << '! ' << (%rest).to_s"
          // "@_ << '! ' << \"%rest\""
          auto ret = gcnew(0, "@_ << '! ' << ", gc_pool);
          auto sc  = ret->first;

          index++;
          if (s->buffer[index] == '=') {
            sc = sc->next = gcnew("((", gc_pool);
            sc = sc->next = gcnew(String::rest(s, index + 1, gc_pool), gc_pool);
            sc = sc->next = gcnew(").to_s) << \"\\n\";", gc_pool);
          } else {
            sc = sc->next = gcnew("\"", gc_pool);
            sc = sc->next = gcnew(String::rest(s, index, gc_pool), gc_pool);
            sc = sc->next = gcnew("\\n\";", gc_pool);
          }

          ret->last = sc;
          return ret;
        }
        case '.':
        case '#': {
          // "@_ << '%%div{' << %attr << '}' << %opt << ' ' << %rest"

          // 'div' tag
          auto ret = gcnew(0, "@_ << '%div{';", gc_pool);
          auto sc = ret->first;

          // attr
          auto attr = solve_attr(s, &index, options, gc_pool);
          sc->next = attr->first;
          sc = attr->last;
          sc = sc->next = gcnew("@_ << @_attr << '}", gc_pool);

          return convert_to_ruby_form_support(ret, sc, s, index, gc_pool);
        }
        case '%': {
          // one of
          // "@_ << '%tag'                  << %opt << ' ' << %rest"
          // "@_ << '%tag{' << %attr << '}' << %opt << ' ' << %rest"

          // tag
          auto ret = gcnew(0, "@_ << '", gc_pool);
          auto sc  = ret->first;
          sc = sc->next = gcnew(String::tok(s, &index, gc_pool), gc_pool);

          // attr
          switch (s->buffer[index]) {
            case '{':
            case '(':
            case '.':
            case '#': {
              sc = sc->next = gcnew("{';", gc_pool);
              auto attr = solve_attr(s, &index, options, gc_pool);
              sc->next = attr->first;
              sc = attr->last;
              sc = sc->next = gcnew("@_ << @_attr << '}", gc_pool);
              break;
            }
          }

          return convert_to_ruby_form_support(ret, sc, s, index, gc_pool);
        }
        default: {
          // "@_ << \"%buffer\""
          String::chomp(s);
          auto ret = gcnew(0, "@_ << \"", gc_pool);
          auto sc  = ret->first;
          sc = sc->next = gcnew(s, gc_pool);
          sc = sc->next = gcnew("\\n\";", gc_pool);
          ret->last = sc;
          return ret;
        }
      }
    }

    tree* static_haml_from_haml(tree* t, VALUE location, const Option& options, GC::gc* gc_pool) {
      if (t == NULL) {
        return NULL;
      }

      auto old_t = t;

      if (is_dynamic(t->l->first->s)) {
        if (is_filter(t->l)) {
          t = solve_filter(t, location, options, gc_pool);
        } else if (t->subtree != NULL && is_script(t->l)) {
          // TODO: implement
        } else {
          METHOD_READY(instance_eval);
          METHOD_CALL(location, instance_eval, rb_str_new2("@_ = ''"));
          auto expr_s = connect_chain(convert_to_ruby_form(t->l, options, gc_pool)->first, gc_pool);
          AT_STACK(expr, rb_str_new(expr_s->buffer, expr_s->length));
          AT_STACK(ret, METHOD_CALL(location, instance_eval, expr));
          if (silent_script(t->l)) {
            t->l = gcnew(0, "", gc_pool);
          } else {
            gc_register_value(ret, gc_pool);
            auto rs = StringValuePtr(ret);
            t->l->first->s = String::gcnew(rs, gc_pool);
          }
          static_haml_from_haml(t->subtree, location, options, gc_pool);
        }
      } else {
        static_haml_from_haml(t->subtree, location, options, gc_pool);
      }
      static_haml_from_haml(t->next, location, options, gc_pool);
      return old_t;
    }

    static bool is_void_tag(String::string* s) {
      // [meta, img, link, br, hr, input, area, param, col, base, isindex, frame, basefont] tags are empty tags.
      switch (s->length) {
        case 2:
          if (String::eq(s, "br") || String::eq(s, "hr")) {
            return true;
          }
          break;
        case 3:
          if (String::eq(s, "img") || String::eq(s, "col")) {
            return true;
          }
          break;
        case 4:
          if (String::eq(s, "meta") || String::eq(s, "link") || String::eq(s, "area") || String::eq(s, "base")) {
            return true;
          }
          break;
        case 5:
          if (String::eq(s, "input") || String::eq(s, "param") || String::eq(s, "frame")) {
            return true;
          }
          break;
        case 7:
          if (String::eq(s, "isindex")) {
            return true;
          }
          break;
        case 8:
          if (String::eq(s, "basefont")) {
            return true;
          }
          break;
        default:
          return false;
      }
      return false;
    }

    static String::string* convert_escape_html(String::string* s, GC::gc* gc_pool);
    static String::string* escaped_rest(String::string* s, int index, GC::gc* gc_pool) {
      return convert_escape_html(String::rest(s, index, gc_pool), gc_pool);
    }

    static String::string* escape(String::string* s, GC::gc* gc_pool) {
      return convert_escape_html(s, gc_pool);
    }

    /// haml '!!! xxxx' -> html/xml/xhtml doctype
    static string_chain* convert_doctype(string_chain* p, tree* t, const Option& options, GC::gc* gc_pool) {
      long index = 3;
      auto spec = String::doctype(p->s, &index, gc_pool);
      if (String::eq(spec, "xml")) {
        if (options.format == FORMAT_XHTML) {
          auto enc = String::tok_lcase(p->s, &index, gc_pool);
          if (enc->length != 0) {
            p->s = String::gcnew("<?xml version='1.0' encoding='", gc_pool);
            auto p_next_old = p->next;
            p = p->next = gcnew(enc, gc_pool);
            p = p->next = gcnew("' ?>\n", gc_pool);
            p->next = p_next_old;
            if (p_next_old == NULL) {
              t->l->last = p;
            }
          } else {
            p->s = String::gcnew("<?xml version='1.0' encoding='utf-8' ?>\n", gc_pool);
          }
        } else {
          p->s = String::gcnew("", gc_pool);
        }
      } else {
        switch (options.format) {
          case FORMAT_XHTML: {
            if (String::eq(spec, "")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
                  "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "strict")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD XHTML 1.0 Strict//EN\" "
                  "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "frameset")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD XHTML 1.0 Frameset//EN\" "
                  "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "5")) {
              p->s = String::gcnew("<!DOCTYPE html>\n", gc_pool);
            } else if (String::eq(spec, "1.1")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD XHTML 1.1//EN\" "
                  "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "basic")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD XHTML Basic 1.1//EN\" "
                  "\"http://www.w3.org/TR/xhtml-basic/xhtml-basic11.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "mobile")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//WAPFORUM//DTD XHTML Mobile 1.2//EN\" "
                  "\"http://www.openmobilealliance.org/tech/DTD/xhtml-mobile12.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "rdfa")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD XHTML-RDFa 1.0//EN\" "
                  "\"http://www.w3.org/MarkUp/DTD/xhtml-rdfa-1.dtd\">\n", gc_pool);
            } else {  // implementation dependent
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
                  "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n", gc_pool);
            }
            break;
          }
          case FORMAT_HTML4: {
            if (String::eq(spec, "")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD HTML 4.01 Transitional//EN\" "
                  "\"http://www.w3.org/TR/html4/loose.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "strict")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD HTML 4.01//EN\" "
                  "\"http://www.w3.org/TR/html4/strict.dtd\">\n", gc_pool);
            } else if (String::eq(spec, "frameset")) {
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD HTML 4.01 Frameset//EN\" "
                  "\"http://www.w3.org/TR/html4/frameset.dtd\">\n", gc_pool);
            } else {  // implementation dependent
              p->s = String::gcnew(
                  "<!DOCTYPE html PUBLIC "
                  "\"-//W3C//DTD HTML 4.01 Traditional//EN\" "
                  "\"http://www.w3.org/TR/html4/loose.dtd\">\n", gc_pool);
            }
            break;
          }
          case FORMAT_HTML5: {
            // When the :format is set to :html5, !!! is always <!DOCTYPE html>
            p->s = String::gcnew("<!DOCTYPE html>\n", gc_pool);
            break;
          }
        }
      }
      return p;
    }

    const char* preservation_regexp = "@_r = /<(textarea|pre|code)([^>]*)>(.*?)(<\\/\\1>)/im";
    static bool is_preserve_tag(String::string* s) {
      // [textarea, pre, code] tags are preserve tags.
      auto l = s->length;
      switch (l) {
        case 3:
          if (String::eq(s, "pre")) {
            return true;
          }
          break;
        case 4:
          if (String::eq(s, "code")) {
            return true;
          }
          break;
        case 8:
          if (String::eq(s, "textarea")) {
            return true;
          }
          break;
        default:
          return false;
      }
      return false;
    }

    static void remove_indents(tree* t) {
      if (t == NULL) {
        return;
      }

      remove_indents(t->subtree);
      remove_indents(t->next);
      t->l->indent_depth = 0;
      return;
    }

    // haml tags (with optional attributes) -> html tags
    static string_chain* convert_tag(string_chain* p,
                                     tree* t,
                                     bool* opt_gt,
                                     const Option& options,
                                     GC::gc* gc_pool) {
      long index = 1;
      auto tag = String::tok(p->s, &index, gc_pool);

      String::string *attr;
      if (p->s->buffer[index] == '{') {
        auto i = index;
        String::find(p->s, &index, '}');
        attr = String::gcnew(p->s->buffer + i + 1, index - i - 1, gc_pool);
        index++;
      } else {
        attr = String::gcnew("", gc_pool);
      }

      auto opt = String::tag_options(p->s, &index, gc_pool);
      bool void_tag = false, opt_lt = false, preserve = is_preserve_tag(tag);
      for (long i = 0; i < opt->length; i++) {
        switch (opt->buffer[i]) {
          case '/':
            void_tag = true;
            break;
          case '<':
            opt_lt = true;
            break;
          case '>':
            *opt_gt = true;
            break;
        }
      }

      if (!void_tag) {
        void_tag |= is_void_tag(tag);
      }

      if (opt_lt && t->subtree != NULL) {
        decrement_indents(t->subtree, options.default_indent_depth);

        // make lastline.end_with_cr? == false
        auto last = t->subtree;
        while (last->next != NULL) {
          last = last->next;
        }
        auto s = last->l->last->s;
        String::chomp(s);
      }

      if (preserve) {
        remove_indents(t->subtree);

        // make lastline.end_with_cr? == false
        auto last = t->subtree;
        while (last->next != NULL) {
          last = last->next;
        }
        auto s = last->l->last->s;
        String::chomp(s);
      }

      auto rest = String::rest(p->s, &index, gc_pool);
      if (options.escape_html) {
        rest = escape(rest, gc_pool);
      }
      String::chomp(rest);

      // tag open
      auto p_next_old = p->next;
      p->s = String::gcnew("<", gc_pool);
      p = p->next = gcnew(tag, gc_pool);
      p = p->next = gcnew(attr, gc_pool);
      if (options.format == FORMAT_XHTML && void_tag) {
        p = p->next = gcnew(" />", gc_pool);
      } else {
        p = p->next = gcnew(">", gc_pool);
      }
      p = p->next = gcnew(rest, gc_pool);
      // end_with_cr! iff. tag.void? and !opt.gt?
      if (void_tag && !*opt_gt) {
        p = p->next = gcnew("\n", gc_pool);
      }

      // tag close
      if (t->subtree) {
        if (!(opt_lt || preserve)) {
          p = p->next = gcnew("\n", gc_pool);
        }
        auto close_tag = gcnew(t->l->indent_depth, String::gcnew("</", gc_pool), gc_pool);
        close_tag->last = close_tag->last->next = gcnew(tag, gc_pool);
        if (*opt_gt) {
          close_tag->last = close_tag->last->next = gcnew(">", gc_pool);
          decrement_indents(t->l, options.default_indent_depth);
          decrement_indents(close_tag, options.default_indent_depth);
        } else {
          close_tag->last = close_tag->last->next = gcnew(">\n", gc_pool);
        }
        auto t_next_old = t->next;
        t->next = gcnew_tree(close_tag, gc_pool);
        t->next->next = t_next_old;
      } else if (!void_tag) {
        p = p->next = gcnew("</", gc_pool);
        p = p->next = gcnew(tag, gc_pool);
        if (*opt_gt) {
          p = p->next = gcnew(">", gc_pool);
        } else {
          p = p->next = gcnew(">\n", gc_pool);
        }
      }
      p->next = p_next_old;
      if (p->next == NULL) {
        t->l->last = p;
      }
      return p;
    }

    // haml comment -> html comment
    static string_chain* convert_comment(char* s, string_chain* p, tree* t, const Option& options, GC::gc* gc_pool) {
      long index = 1;
      auto conditional = false;
      String::string* ctag = NULL;
      if (s[1] == '[') {
        conditional = true;
        String::find(p->s, &index, ']');
        ctag = String::gcnew(s + 1, index, gc_pool);
      }
      auto rest = String::rest(p->s, &index, gc_pool);
      if (options.escape_html) {
        rest = escape(rest, gc_pool);
      }
      String::chomp(rest);
      auto p_next_old = p->next;
      if (t->subtree) {
        // around subtree by a comment tag
        auto t_next_old = t->next;
        if (conditional) {
          p->s = String::gcnew("<!--", gc_pool);
          p = p->next = gcnew(ctag, gc_pool);
          p = p->next = gcnew(">\n", gc_pool);
          if (p_next_old == NULL) {
            t->l->last = p;
          }
          t = t->next = gcnew_tree(t->l->indent_depth, "<![endif]-->\n", gc_pool);
        } else {
          p->s = String::gcnew("<!--\n", gc_pool);
          t = t->next = gcnew_tree(t->l->indent_depth, "-->\n", gc_pool);
        }
        t->next = t_next_old;
      } else {
        // around liner comment by a comment tag
        p->s = String::gcnew("<!-- ", gc_pool);
        p = p->next = gcnew(rest, gc_pool);
        p = p->next = gcnew(" -->\n", gc_pool);
        if (p_next_old == NULL) {
          t->l->last = p;
        }
      }
      p->next = p_next_old;
      return p;
    }

    static string_chain* sprit_escapes(string_chain* p, GC::gc* gc_pool) {
      auto buffer = p->s->buffer;
      auto length = p->s->length;
      auto next_start_index = 0;
      auto q = gcnew("", gc_pool);
      auto ret = q;
      for (auto i = 0; i < length; i++) {
        switch (buffer[i]) {
          case '&':
          case '>':
          case '<':
          case '"':
            q->s = String::gcnew(buffer + next_start_index, i - next_start_index, gc_pool);
            q = q->next = gcnew(String::gcnew(buffer + i, 1, gc_pool), gc_pool);
            q = q->next = gcnew("", gc_pool);
            next_start_index = i + 1;
            break;
        }
      }
      q->s = String::gcnew(buffer + next_start_index, length - next_start_index, gc_pool);
      return ret;
    }

    static void convert_escape_html(string_chain* p, GC::gc* gc_pool) {
      auto chain = sprit_escapes(p, gc_pool);
      for (auto q = chain; q != NULL; q = q->next) {
        if (q->s->length == 1) {
          if (String::eq(q->s, "&")) {
            q->s = String::gcnew("&amp;",  gc_pool);
          } else if (String::eq(q->s, ">")) {
            q->s = String::gcnew("&gt;",   gc_pool);
          } else if (String::eq(q->s, "<")) {
            q->s = String::gcnew("&lt;",   gc_pool);
          } else if (String::eq(q->s, "\"")) {
            q->s = String::gcnew("&quot;", gc_pool);
          }
        }
      }
      p->s = connect_chain(chain, gc_pool);
      return;
    }

    static String::string* convert_escape_html(String::string* s, GC::gc* gc_pool) {
      auto sc = gcnew(s, gc_pool);
      convert_escape_html(sc, gc_pool);
      return sc->s;
    }

    // haml -> html
    // NOTE: it has destructive modifications ...
    static tree* html_from_static_haml(tree* t, int* max_indent_depth, bool* opt_gt, const Option& options, GC::gc* gc_pool) {
      if (t == NULL) {
        return NULL;
      }

      bool child_opt_gt = false;
      bool next_opt_gt = false;
      t->subtree = html_from_static_haml(t->subtree, max_indent_depth, &child_opt_gt, options, gc_pool);
      t->next    = html_from_static_haml(t->next,    max_indent_depth, &next_opt_gt, options, gc_pool);

      auto p = t->l->first;
      auto s = p->s->buffer;
      auto sl = p->s->length;
      if (sl != 0) {
        if (s[0] == '!') {
          if (sl >= 3 && s[1] == '!' && s[2] == '!') {
            // line starts with '!!!' => Doctype
            p = convert_doctype(p, t, options, gc_pool);
          } else {
            // line starts with '!' => Always Unescaping HTML
            p->s = String::rest(p->s, 1, gc_pool);
          }
        } else if (s[0] == '%') {
          p = convert_tag(p, t, opt_gt, options, gc_pool);
        } else if (s[0] == '/') {
          p = convert_comment(s, p, t, options, gc_pool);
        } else if (s[0] == '\\') {
          p->s = String::rest(p->s, 1, gc_pool);
        } else if (s[0] == '&') {
          p->s = escaped_rest(p->s, 1, gc_pool);
        } else if (options.escape_html) {
          p->s = escape(p->s, gc_pool);
        }
      }

      if (child_opt_gt || next_opt_gt) {
        auto last = t->l->last->s;
        String::chomp(last);
        if (child_opt_gt) {
          decrement_indents(t->subtree, options.default_indent_depth);
        }
      }

      if (*max_indent_depth < t->l->indent_depth) {
        *max_indent_depth = t->l->indent_depth;
      }
      return t;
    }

    tree* html_from_static_haml(tree* t, int* max_indent_depth, const Option& options, GC::gc* gc_pool) {
      bool dummy = false;
      return html_from_static_haml(t, max_indent_depth, &dummy, options, gc_pool);
    }

    static line* flatten_(tree* subtree, tree* next, line* l, String::string* sp, GC::gc* gc_pool);
    static line* flatten_(tree* t, String::string* sp, GC::gc* gc_pool) {
      if (t == NULL) {
        return NULL;
      }
      return flatten_(t->subtree, t->next, t->l, sp, gc_pool);
    }

    // NOTE: it has destructive modifications ...
    static line* flatten_(tree* subtree, tree* next, line* l, String::string* sp, GC::gc* gc_pool) {
      auto ret = GCNEW(line, gc_pool);
      ret->first = ret->last = NULL;
      ret->indent_depth = 0;

      if (l) {
        auto indent = gcnew(String::gcnew(sp->buffer, l->indent_depth, gc_pool), gc_pool);
        ret->first = ret->last = indent;
        ret->last->next = l->first;
        ret->last       = l->last;
      }

      auto ls = flatten_(subtree, sp, gc_pool);
      if (ls) {
        if (l) {
          ret->last->next = ls->first;
        } else {
          ret->first = ls->first;
        }
        ret->last = ls->last;
      }

      auto ln = flatten_(next, sp, gc_pool);
      if (ln) {
        if (l || ls) {
          ret->last->next = ln->first;
        } else {
          ret->first = ln->first;
        }
        ret->last = ln->last;
      }

      if (ret->last) {
        ret->last->next = NULL;
      }
      return ret;
    }

    static String::string* flatten_(tree* t, int max_indent_depth, GC::gc* gc_pool) {
      if (t == NULL) {
        return String::gcnew("", gc_pool);
      }

      // create buffer for the indents that filled by spaces
      auto spaces_ = GC::gc_alloc_n_char(max_indent_depth, gc_pool);
      for (int i = 0; i < max_indent_depth; i++) {
        spaces_[i] = ' ';
      }
      auto spaces = String::gcnew(spaces_, max_indent_depth, gc_pool);

      // tree -> string-chain
      auto sc = flatten_(t->subtree, t->next, t->l, spaces, gc_pool)->first;

      // string-chain -> string
      return connect_chain(sc, gc_pool);
    }

    VALUE flatten(tree* t, int max_indent_depth, GC::gc* gc_pool) {
      auto str = flatten_(t, max_indent_depth, gc_pool);
      AT_STACK(ret, rb_str_new(str->buffer, str->length));
      return ret;
    }

  }
}
