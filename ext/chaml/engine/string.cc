#include "./chaml.h"

#define GCNEW(t, pool) GCNEW_NAME(String, t)(pool)

namespace CHaml {
  namespace String {

    static string empty = {
#ifdef __CLANG__
      .buffer = const_cast<char*>(""),
      .length = 0,
#else
      buffer: const_cast<char*>(""),
      length: 0,
#endif
    };

    string* gcnew(const char* buffer, long length, GC::gc* gc_pool) {
      if (length == 0) {
        return &empty;
      }
      auto ret = GCNEW(string, gc_pool);
      ret->buffer = const_cast<char*>(buffer);
      ret->length = length;
      return ret;
    }

    string* gcnew(const char* buffer, GC::gc* gc_pool) {
      return gcnew(buffer, static_cast<long>(strlen(buffer)), gc_pool);
    }

    bool eq(string* s1, string* s2) {
      if (!(s1 && s2)) {
        return false;
      }
      if (s1->length != s2->length) {
        return false;
      }
      for (auto i = 0; i < s1->length; i++) {
        if (s1->buffer[i] != s2->buffer[i]) {
          return false;
        }
      }
      return true;
    }

    bool eq(string* s1, const char* s2) {
      if (!s1) {
        return false;
      }
      for (auto i = 0; i < s1->length; i++) {
        if (s1->buffer[i] != s2[i]) {
          return false;
        }
      }
      return s2[s1->length] == '\0';
    }

    bool find(string* s, long* index, char ch) {
      auto p = s->buffer;
      for (auto i = *index; i < s->length; i++) {
        if (p[i] == '\\') {
          i++;
        } else if (p[i] == ch) {
          *index = i;
          return true;
        }
      }
      *index = s->length;
      return false;
    }

    bool find_first_valid_index(string* s, long* index) {
      auto p = s->buffer;
      for (auto i = *index; i < s->length; i++) {
        if (!(p[i] == ' ' || p[i] == '\t' || p[i] == '\n')) {
          *index = i;
          return true;
        }
      }
      *index = s->length;
      return false;
    }

    static void find_end_of_doctype(string* s, long* index) {
      auto p = s->buffer;
      for (auto i = *index; i < s->length; i++) {
        switch (p[i]) {
          case ' ':
          case '\t':
          case '\n':
            *index = i;
            return;
        }
      }
      *index = s->length;
      return;
    }

    bool find_first_invalid_index(string* s, long* index) {
      auto p = s->buffer;
      for (auto i = *index; i < s->length; i++) {
        switch (p[i]) {
          case ' ':
          case '\t':
          case '\n':
          case '.':
          case '#':
          case '{':
          case '}':
          case '(':
          case ')':
          case '=':
          case '~':
          case '<':
          case '>':
          case '/':
            *index = i;
            return true;
        }
      }
      *index = s->length;
      return false;
    }

    bool find_last_valid_index(string* s, long* index) {
      auto p = s->buffer;
      for (auto i = *index; i >= 0; i--) {
        switch (p[i]) {
          case ' ':
          case '\t':
          case '\n':
            break;
          default:
            *index = i;
            return true;
        }
      }
      *index = -1;
      return false;
    }

    long find_last_valid_index(string* s) {
      auto index = s->length - 1;
      find_last_valid_index(s, &index);
      return index;
    }

    long skip_tag_options(string* s, long* index) {
      auto p = s->buffer;
      long ret = -1;
      for (auto i = *index; i < s->length; i++) {
        switch (p[i]) {
          case '=':
          case '~':
            ret = i;
            break;
          case '<':
          case '>':
          case '/':
            break;
          default:
            *index = i;
            return ret;
        }
      }
      *index = s->length;
      return ret;
    }

    string* tag_options(string* s, long* index, GC::gc* gc_pool) {
      auto old_index = *index;
      skip_tag_options(s, index);
      return gcnew(s->buffer + old_index, *index - old_index, gc_pool);
    }

    string* tok(string* s, long* index, GC::gc* gc_pool) {
      if (!find_first_valid_index(s, index)) {
        return gcnew("", gc_pool);
      }
      auto first_valid_index = *index;

      ++*index;
      find_first_invalid_index(s, index);
      return gcnew(s->buffer + first_valid_index, *index - first_valid_index, gc_pool);
    }

    string* lcase(string* s) {
      auto p = s->buffer;
      for (auto i = 0; i < s->length; i++) {
        if ('A' <= p[i] && p[i] <= 'Z') {
          p[i] += 'a' - 'A';
        }
      }
      return s;
    }

    string* tok_lcase(string* s, long* index, GC::gc* gc_pool) {
      return lcase(tok(s, index, gc_pool));
    }

    string* doctype(string* s, long* index, GC::gc* gc_pool) {
      if (!find_first_valid_index(s, index)) {
        return gcnew("", gc_pool);
      }
      auto doctype_start = *index;

      ++*index;
      find_end_of_doctype(s, index);
      return lcase(gcnew(s->buffer + doctype_start, *index - doctype_start, gc_pool));
    }

    string* rest(string* s, long* index, GC::gc* gc_pool) {
      if (!find_first_valid_index(s, index)) {
        return gcnew("", gc_pool);
      }
      auto first_valid_index = *index;
      return gcnew(s->buffer + first_valid_index, s->length - first_valid_index, gc_pool);
    }

    string* rest(string* s, long index, GC::gc* gc_pool) {
      return rest(s, &index, gc_pool);
    }

    void chomp(string* s) {
      if (s == NULL) {
        return;
      }
      while (s->length != 0 && s->buffer[s->length - 1] == '\n') {
        s->length--;
      }
      return;
    }

    void print(string* s) {
      for (auto i = 0; i < s->length; i++) {
        putchar(s->buffer[i]);
      }
    }
  }
}
