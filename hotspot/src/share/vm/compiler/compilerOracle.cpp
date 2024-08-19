/*
 * Copyright (c) 1998, 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "compiler/compilerOracle.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/oopFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/klass.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/jniHandles.hpp"

class MethodMatcher : public CHeapObj<mtCompiler> {
 public:
  enum Mode {
    Exact,
    Prefix = 1,
    Suffix = 2,
    Substring = Prefix | Suffix,
    Any,
    Unknown = -1
  };

 protected:
  Symbol*        _class_name;
  Symbol*        _method_name;
  Symbol*        _signature;
  Mode           _class_mode;
  Mode           _method_mode;
  MethodMatcher* _next;

  static bool match(Symbol* candidate, Symbol* match, Mode match_mode);

  Symbol* class_name() const { return _class_name; }
  Symbol* method_name() const { return _method_name; }
  Symbol* signature() const { return _signature; }

 public:
  MethodMatcher(Symbol* class_name, Mode class_mode,
                Symbol* method_name, Mode method_mode,
                Symbol* signature, MethodMatcher* next);
  MethodMatcher(Symbol* class_name, Symbol* method_name, MethodMatcher* next);

  // utility method
  MethodMatcher* find(methodHandle method) {
    Symbol* class_name  = method->method_holder()->name();
    Symbol* method_name = method->name();
    for (MethodMatcher* current = this; current != NULL; current = current->_next) {
      if (match(class_name, current->class_name(), current->_class_mode) &&
          match(method_name, current->method_name(), current->_method_mode) &&
          (current->signature() == NULL || current->signature() == method->signature())) {
        return current;
      }
    }
    return NULL;
  }

  bool match(methodHandle method) {
    return find(method) != NULL;
  }

  MethodMatcher* next() const { return _next; }

  static void print_symbol(Symbol* h, Mode mode) {
    ResourceMark rm;

    if (mode == Suffix || mode == Substring || mode == Any) {
      tty->print("*");
    }
    if (mode != Any) {
      h->print_symbol_on(tty);
    }
    if (mode == Prefix || mode == Substring) {
      tty->print("*");
    }
  }

  void print_base() {
    print_symbol(class_name(), _class_mode);
    tty->print(".");
    print_symbol(method_name(), _method_mode);
    if (signature() != NULL) {
      tty->print(" ");
      signature()->print_symbol_on(tty);
    }
  }

  virtual void print() {
    print_base();
    tty->cr();
  }
};

MethodMatcher::MethodMatcher(Symbol* class_name, Symbol* method_name, MethodMatcher* next) {
  _class_name  = class_name;
  _method_name = method_name;
  _next        = next;
  _class_mode  = MethodMatcher::Exact;
  _method_mode = MethodMatcher::Exact;
  _signature   = NULL;
}


MethodMatcher::MethodMatcher(Symbol* class_name, Mode class_mode,
                             Symbol* method_name, Mode method_mode,
                             Symbol* signature, MethodMatcher* next):
    _class_mode(class_mode)
  , _method_mode(method_mode)
  , _next(next)
  , _class_name(class_name)
  , _method_name(method_name)
  , _signature(signature) {
}

bool MethodMatcher::match(Symbol* candidate, Symbol* match, Mode match_mode) {
  if (match_mode == Any) {
    return true;
  }

  if (match_mode == Exact) {
    return candidate == match;
  }

  ResourceMark rm;
  const char * candidate_string = candidate->as_C_string();
  const char * match_string = match->as_C_string();

  switch (match_mode) {
  case Prefix:
    return strstr(candidate_string, match_string) == candidate_string;

  case Suffix: {
    size_t clen = strlen(candidate_string);
    size_t mlen = strlen(match_string);
    return clen >= mlen && strcmp(candidate_string + clen - mlen, match_string) == 0;
  }

  case Substring:
    return strstr(candidate_string, match_string) != NULL;

  default:
    return false;
  }
}

enum OptionType {
  IntxType,
  UintxType,
  BoolType,
  CcstrType,
  UnknownType
};

/* Methods to map real type names to OptionType */
template<typename T>
static OptionType get_type_for() {
  return UnknownType;
};

template<> OptionType get_type_for<intx>() {
  return IntxType;
}

template<> OptionType get_type_for<uintx>() {
  return UintxType;
}

template<> OptionType get_type_for<bool>() {
  return BoolType;
}

template<> OptionType get_type_for<ccstr>() {
  return CcstrType;
}

template<typename T>
static const T copy_value(const T value) {
  return value;
}

template<> const ccstr copy_value<ccstr>(const ccstr value) {
  return (const ccstr)strdup(value);
}

template <typename T>
class TypedMethodOptionMatcher : public MethodMatcher {
  const char* _option;
  OptionType _type;
  const T _value;

public:
  TypedMethodOptionMatcher(Symbol* class_name, Mode class_mode,
                           Symbol* method_name, Mode method_mode,
                           Symbol* signature, const char* opt,
                           const T value,  MethodMatcher* next) :
    MethodMatcher(class_name, class_mode, method_name, method_mode, signature, next),
                  _type(get_type_for<T>()), _value(copy_value<T>(value)) {
    _option = strdup(opt);
  }

  ~TypedMethodOptionMatcher() {
    free((void*)_option);
  }

  TypedMethodOptionMatcher* match(methodHandle method, const char* opt) {
    TypedMethodOptionMatcher* current = this;
    while (current != NULL) {
      current = (TypedMethodOptionMatcher*)current->find(method);
      if (current == NULL) {
        return NULL;
      }
      if (strcmp(current->_option, opt) == 0) {
        return current;
      }
      current = current->next();
    }
    return NULL;
  }

  TypedMethodOptionMatcher* next() {
    return (TypedMethodOptionMatcher*)_next;
  }

  OptionType get_type(void) {
      return _type;
  };

  T value() { return _value; }

  void print() {
    ttyLocker ttyl;
    print_base();
    tty->print(" %s", _option);
    tty->print(" <unknown option type>");
    tty->cr();
  }
};

template<>
void TypedMethodOptionMatcher<intx>::print() {
  ttyLocker ttyl;
  print_base();
  tty->print(" intx %s", _option);
  tty->print(" = " INTX_FORMAT, _value);
  tty->cr();
};

template<>
void TypedMethodOptionMatcher<uintx>::print() {
  ttyLocker ttyl;
  print_base();
  tty->print(" uintx %s", _option);
  tty->print(" = " UINTX_FORMAT, _value);
  tty->cr();
};

template<>
void TypedMethodOptionMatcher<bool>::print() {
  ttyLocker ttyl;
  print_base();
  tty->print(" bool %s", _option);
  tty->print(" = %s", _value ? "true" : "false");
  tty->cr();
};

template<>
void TypedMethodOptionMatcher<ccstr>::print() {
  ttyLocker ttyl;
  print_base();
  tty->print(" const char* %s", _option);
  tty->print(" = '%s'", _value);
  tty->cr();
};

// this must parallel the command_names below
enum OracleCommand {
  UnknownCommand = -1,
  OracleFirstCommand = 0,
  BreakCommand = OracleFirstCommand,
  PrintCommand,
  ExcludeCommand,
  InlineCommand,
  DontInlineCommand,
  CompileOnlyCommand,
  LogCommand,
  OptionCommand,
  QuietCommand,
  HelpCommand,
  OracleCommandCount
};

// this must parallel the enum OracleCommand
static const char * command_names[] = {
  "break",
  "print",
  "exclude",
  "inline",
  "dontinline",
  "compileonly",
  "log",
  "option",
  "quiet",
  "help"
};

class MethodMatcher;
static MethodMatcher* lists[OracleCommandCount] = { 0, };


static bool check_predicate(OracleCommand command, methodHandle method) {
  return ((lists[command] != NULL) &&
          !method.is_null() &&
          lists[command]->match(method));
}


static MethodMatcher* add_predicate(OracleCommand command,
                                    Symbol* class_name, MethodMatcher::Mode c_mode,
                                    Symbol* method_name, MethodMatcher::Mode m_mode,
                                    Symbol* signature) {
  assert(command != OptionCommand, "must use add_option_string");
  if (command == LogCommand && !LogCompilation && lists[LogCommand] == NULL)
    tty->print_cr("Warning:  +LogCompilation must be enabled in order for individual methods to be logged.");
  lists[command] = new MethodMatcher(class_name, c_mode, method_name, m_mode, signature, lists[command]);
  return lists[command];
}

template<typename T>
static MethodMatcher* add_option_string(Symbol* class_name, MethodMatcher::Mode c_mode,
                                        Symbol* method_name, MethodMatcher::Mode m_mode,
                                        Symbol* signature,
                                        const char* option,
                                        T value) {
  lists[OptionCommand] = new TypedMethodOptionMatcher<T>(class_name, c_mode, method_name, m_mode,
                                                         signature, option, value, lists[OptionCommand]);
  return lists[OptionCommand];
}

template<typename T>
static bool get_option_value(methodHandle method, const char* option, T& value) {
   TypedMethodOptionMatcher<T>* m;
   if (lists[OptionCommand] != NULL
       && (m = ((TypedMethodOptionMatcher<T>*)lists[OptionCommand])->match(method, option)) != NULL
       && m->get_type() == get_type_for<T>()) {
       value = m->value();
       return true;
   } else {
     return false;
   }
}

bool CompilerOracle::has_option_string(methodHandle method, const char* option) {
  bool value = false;
  get_option_value(method, option, value);
  return value;
}

template<typename T>
bool CompilerOracle::has_option_value(methodHandle method, const char* option, T& value) {
  return ::get_option_value(method, option, value);
}

// Explicit instantiation for all OptionTypes supported.
template bool CompilerOracle::has_option_value<intx>(methodHandle method, const char* option, intx& value);
template bool CompilerOracle::has_option_value<uintx>(methodHandle method, const char* option, uintx& value);
template bool CompilerOracle::has_option_value<bool>(methodHandle method, const char* option, bool& value);
template bool CompilerOracle::has_option_value<ccstr>(methodHandle method, const char* option, ccstr& value);

bool CompilerOracle::should_exclude(methodHandle method, bool& quietly) {
  quietly = true;
  if (lists[ExcludeCommand] != NULL) {
    if (lists[ExcludeCommand]->match(method)) {
      quietly = _quiet;
      return true;
    }
  }

  if (lists[CompileOnlyCommand] != NULL) {
    return !lists[CompileOnlyCommand]->match(method);
  }
  return false;
}


bool CompilerOracle::should_inline(methodHandle method) {
  return (check_predicate(InlineCommand, method));
}


bool CompilerOracle::should_not_inline(methodHandle method) {
  return (check_predicate(DontInlineCommand, method));
}


bool CompilerOracle::should_print(methodHandle method) {
  return (check_predicate(PrintCommand, method));
}


bool CompilerOracle::should_log(methodHandle method) {
  if (!LogCompilation)            return false;
  if (lists[LogCommand] == NULL)  return true;  // by default, log all
  return (check_predicate(LogCommand, method));
}


bool CompilerOracle::should_break_at(methodHandle method) {
  return check_predicate(BreakCommand, method);
}


static OracleCommand parse_command_name(const char * line, int* bytes_read) {
  assert(ARRAY_SIZE(command_names) == OracleCommandCount,
         "command_names size mismatch");

  *bytes_read = 0;
  char command[33];
  int result = sscanf(line, "%32[a-z]%n", command, bytes_read);
  for (uint i = 0; i < ARRAY_SIZE(command_names); i++) {
    if (strcmp(command, command_names[i]) == 0) {
      return (OracleCommand)i;
    }
  }
  return UnknownCommand;
}


static void usage() {
  tty->print_cr("  CompileCommand and the CompilerOracle allows simple control over");
  tty->print_cr("  what's allowed to be compiled.  The standard supported directives");
  tty->print_cr("  are exclude and compileonly.  The exclude directive stops a method");
  tty->print_cr("  from being compiled and compileonly excludes all methods except for");
  tty->print_cr("  the ones mentioned by compileonly directives.  The basic form of");
  tty->print_cr("  all commands is a command name followed by the name of the method");
  tty->print_cr("  in one of two forms: the standard class file format as in");
  tty->print_cr("  class/name.methodName or the PrintCompilation format");
  tty->print_cr("  class.name::methodName.  The method name can optionally be followed");
  tty->print_cr("  by a space then the signature of the method in the class file");
  tty->print_cr("  format.  Otherwise the directive applies to all methods with the");
  tty->print_cr("  same name and class regardless of signature.  Leading and trailing");
  tty->print_cr("  *'s in the class and/or method name allows a small amount of");
  tty->print_cr("  wildcarding.  ");
  tty->cr();
  tty->print_cr("  Examples:");
  tty->cr();
  tty->print_cr("  exclude java/lang/StringBuffer.append");
  tty->print_cr("  compileonly java/lang/StringBuffer.toString ()Ljava/lang/String;");
  tty->print_cr("  exclude java/lang/String*.*");
  tty->print_cr("  exclude *.toString");
}


// The characters allowed in a class or method name.  All characters > 0x7f
// are allowed in order to handle obfuscated class files (e.g. Volano)
#define RANGEBASE "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$_<>" \
        "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f" \
        "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f" \
        "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf" \
        "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf" \
        "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf" \
        "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf" \
        "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef" \
        "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"

#define RANGE0 "[*" RANGEBASE "]"
#define RANGESLASH "[*" RANGEBASE "/]"

static MethodMatcher::Mode check_mode(char name[], const char*& error_msg) {
  int match = MethodMatcher::Exact;
  while (name[0] == '*') {
    match |= MethodMatcher::Suffix;
    // Copy remaining string plus NUL to the beginning
    memmove(name, name + 1, strlen(name + 1) + 1);
  }

  if (strcmp(name, "*") == 0) return MethodMatcher::Any;

  size_t len = strlen(name);
  while (len > 0 && name[len - 1] == '*') {
    match |= MethodMatcher::Prefix;
    name[--len] = '\0';
  }

  if (strstr(name, "*") != NULL) {
    error_msg = "  Embedded * not allowed";
    return MethodMatcher::Unknown;
  }
  return (MethodMatcher::Mode)match;
}

static bool scan_line(const char * line,
                      char class_name[],  MethodMatcher::Mode* c_mode,
                      char method_name[], MethodMatcher::Mode* m_mode,
                      int* bytes_read, const char*& error_msg) {
  *bytes_read = 0;
  error_msg = NULL;
  if (2 == sscanf(line, "%*[ \t]%255" RANGESLASH "%*[ ]" "%255"  RANGE0 "%n", class_name, method_name, bytes_read)) {
    *c_mode = check_mode(class_name, error_msg);
    *m_mode = check_mode(method_name, error_msg);
    return *c_mode != MethodMatcher::Unknown && *m_mode != MethodMatcher::Unknown;
  }
  return false;
}



// Scan next flag and value in line, return MethodMatcher object on success, NULL on failure.
// On failure, error_msg contains description for the first error.
// For future extensions: set error_msg on first error.
static MethodMatcher* scan_flag_and_value(const char* type, const char* line, int& total_bytes_read,
                                          Symbol* c_name, MethodMatcher::Mode c_match,
                                          Symbol* m_name, MethodMatcher::Mode m_match,
                                          Symbol* signature,
                                          char* errorbuf, const int buf_size) {
  total_bytes_read = 0;
  int bytes_read = 0;
  char flag[256];

  // Read flag name.
  if (sscanf(line, "%*[ \t]%255[a-zA-Z0-9]%n", flag, &bytes_read) == 1) {
    line += bytes_read;
    total_bytes_read += bytes_read;

    // Read value.
    if (strcmp(type, "intx") == 0) {
      intx value;
      if (sscanf(line, "%*[ \t]" INTX_FORMAT "%n", &value, &bytes_read) == 1) {
        total_bytes_read += bytes_read;
        return add_option_string(c_name, c_match, m_name, m_match, signature, flag, value);
      } else {
        jio_snprintf(errorbuf, buf_size, "  Value cannot be read for flag %s of type %s ", flag, type);
      }
    } else if (strcmp(type, "uintx") == 0) {
      uintx value;
      if (sscanf(line, "%*[ \t]" UINTX_FORMAT "%n", &value, &bytes_read) == 1) {
        total_bytes_read += bytes_read;
        return add_option_string(c_name, c_match, m_name, m_match, signature, flag, value);
      } else {
        jio_snprintf(errorbuf, buf_size, "  Value cannot be read for flag %s of type %s", flag, type);
      }
    } else if (strcmp(type, "ccstr") == 0) {
      ResourceMark rm;
      char* value = NEW_RESOURCE_ARRAY(char, strlen(line) + 1);
      if (sscanf(line, "%*[ \t]%255[_a-zA-Z0-9]%n", value, &bytes_read) == 1) {
        total_bytes_read += bytes_read;
        return add_option_string(c_name, c_match, m_name, m_match, signature, flag, (ccstr)value);
      } else {
        jio_snprintf(errorbuf, buf_size, "  Value cannot be read for flag %s of type %s", flag, type);
      }
    } else if (strcmp(type, "ccstrlist") == 0) {
      // Accumulates several strings into one. The internal type is ccstr.
      ResourceMark rm;
      char* value = NEW_RESOURCE_ARRAY(char, strlen(line) + 1);
      char* next_value = value;
      if (sscanf(line, "%*[ \t]%255[_a-zA-Z0-9]%n", next_value, &bytes_read) == 1) {
        total_bytes_read += bytes_read;
        line += bytes_read;
        next_value += bytes_read;
        char* end_value = next_value-1;
        while (sscanf(line, "%*[ \t]%255[_a-zA-Z0-9]%n", next_value, &bytes_read) == 1) {
          total_bytes_read += bytes_read;
          line += bytes_read;
          *end_value = ' '; // override '\0'
          next_value += bytes_read;
          end_value = next_value-1;
        }
        return add_option_string(c_name, c_match, m_name, m_match, signature, flag, (ccstr)value);
      } else {
        jio_snprintf(errorbuf, buf_size, "  Value cannot be read for flag %s of type %s", flag, type);
      }
    } else if (strcmp(type, "bool") == 0) {
      char value[256];
      if (sscanf(line, "%*[ \t]%255[a-zA-Z]%n", value, &bytes_read) == 1) {
        if (strcmp(value, "true") == 0) {
          total_bytes_read += bytes_read;
          return add_option_string(c_name, c_match, m_name, m_match, signature, flag, true);
        } else if (strcmp(value, "false") == 0) {
          total_bytes_read += bytes_read;
          return add_option_string(c_name, c_match, m_name, m_match, signature, flag, false);
        } else {
          jio_snprintf(errorbuf, buf_size, "  Value cannot be read for flag %s of type %s", flag, type);
        }
      } else {
        jio_snprintf(errorbuf, sizeof(errorbuf), "  Value cannot be read for flag %s of type %s", flag, type);
      }
    } else {
      jio_snprintf(errorbuf, sizeof(errorbuf), "  Type %s not supported ", type);
    }
  } else {
    jio_snprintf(errorbuf, sizeof(errorbuf), "  Flag name for type %s should be alphanumeric ", type);
  }
  return NULL;
}

void CompilerOracle::parse_from_line(char* line) {
  if (line[0] == '\0') return;
  if (line[0] == '#')  return;

  bool have_colon = (strstr(line, "::") != NULL);
  for (char* lp = line; *lp != '\0'; lp++) {
    // Allow '.' to separate the class name from the method name.
    // This is the preferred spelling of methods:
    //      exclude java/lang/String.indexOf(I)I
    // Allow ',' for spaces (eases command line quoting).
    //      exclude,java/lang/String.indexOf
    // For backward compatibility, allow space as separator also.
    //      exclude java/lang/String indexOf
    //      exclude,java/lang/String,indexOf
    // For easy cut-and-paste of method names, allow VM output format
    // as produced by Method::print_short_name:
    //      exclude java.lang.String::indexOf
    // For simple implementation convenience here, convert them all to space.
    if (have_colon) {
      if (*lp == '.')  *lp = '/';   // dots build the package prefix
      if (*lp == ':')  *lp = ' ';
    }
    if (*lp == ',' || *lp == '.')  *lp = ' ';
  }

  char* original_line = line;
  int bytes_read;
  OracleCommand command = parse_command_name(line, &bytes_read);
  line += bytes_read;
  ResourceMark rm;

  if (command == UnknownCommand) {
    ttyLocker ttyl;
    tty->print_cr("CompilerOracle: unrecognized line");
    tty->print_cr("  \"%s\"", original_line);
    return;
  }

  if (command == QuietCommand) {
    _quiet = true;
    return;
  }

  if (command == HelpCommand) {
    usage();
    return;
  }

  MethodMatcher::Mode c_match = MethodMatcher::Exact;
  MethodMatcher::Mode m_match = MethodMatcher::Exact;
  char class_name[256];
  char method_name[256];
  char sig[1024];
  char errorbuf[1024];
  const char* error_msg = NULL; // description of first error that appears
  MethodMatcher* match = NULL;

  if (scan_line(line, class_name, &c_match, method_name, &m_match, &bytes_read, error_msg)) {
    EXCEPTION_MARK;
    Symbol* c_name = SymbolTable::new_symbol(class_name, CHECK);
    Symbol* m_name = SymbolTable::new_symbol(method_name, CHECK);
    Symbol* signature = NULL;

    line += bytes_read;
    // there might be a signature following the method.
    // signatures always begin with ( so match that by hand
    if (1 == sscanf(line, "%*[ \t](%254[[);/" RANGEBASE "]%n", sig + 1, &bytes_read)) {
      sig[0] = '(';
      line += bytes_read;
      signature = SymbolTable::new_symbol(sig, CHECK);
    }

    if (command == OptionCommand) {
      // Look for trailing options.
      //
      // Two types of trailing options are
      // supported:
      //
      // (1) CompileCommand=option,Klass::method,flag
      // (2) CompileCommand=option,Klass::method,type,flag,value
      //
      // Type (1) is used to support ciMethod::has_option("someflag")
      // (i.e., to check if a flag "someflag" is enabled for a method).
      //
      // Type (2) is used to support options with a value. Values can have the
      // the following types: intx, uintx, bool, ccstr, and ccstrlist.
      //
      // For future extensions: extend scan_flag_and_value()
      char option[256]; // stores flag for Type (1) and type of Type (2)
      while (sscanf(line, "%*[ \t]%255[a-zA-Z0-9]%n", option, &bytes_read) == 1) {
        if (match != NULL && !_quiet) {
          // Print out the last match added
          ttyLocker ttyl;
          tty->print("CompilerOracle: %s ", command_names[command]);
          match->print();
        }
        line += bytes_read;

        if (strcmp(option, "intx") == 0
            || strcmp(option, "uintx") == 0
            || strcmp(option, "bool") == 0
            || strcmp(option, "ccstr") == 0
            || strcmp(option, "ccstrlist") == 0
            ) {

          // Type (2) option: parse flag name and value.
          match = scan_flag_and_value(option, line, bytes_read,
                                      c_name, c_match, m_name, m_match, signature,
                                      errorbuf, sizeof(errorbuf));
          if (match == NULL) {
            error_msg = errorbuf;
            break;
          }
          line += bytes_read;
        } else {
          // Type (1) option
          match = add_option_string(c_name, c_match, m_name, m_match, signature, option, true);
        }
      } // while(
    } else {
      match = add_predicate(command, c_name, c_match, m_name, m_match, signature);
    }
  }

  ttyLocker ttyl;
  if (error_msg != NULL) {
    // an error has happened
    tty->print_cr("CompilerOracle: unrecognized line");
    tty->print_cr("  \"%s\"", original_line);
    if (error_msg != NULL) {
      tty->print_cr("%s", error_msg);
    }
  } else {
    // check for remaining characters
    bytes_read = 0;
    sscanf(line, "%*[ \t]%n", &bytes_read);
    if (line[bytes_read] != '\0') {
      tty->print_cr("CompilerOracle: unrecognized line");
      tty->print_cr("  \"%s\"", original_line);
      tty->print_cr("  Unrecognized text %s after command ", line);
    } else if (match != NULL && !_quiet) {
      tty->print("CompilerOracle: %s ", command_names[command]);
      match->print();
    }
  }
}

static const char* default_cc_file = ".hotspot_compiler";

static const char* cc_file() {
#ifdef ASSERT
  if (CompileCommandFile == NULL)
    return default_cc_file;
#endif
  return CompileCommandFile;
}

bool CompilerOracle::has_command_file() {
  return cc_file() != NULL;
}

bool CompilerOracle::_quiet = false;

void CompilerOracle::parse_from_file() {
  assert(has_command_file(), "command file must be specified");
  FILE* stream = fopen(cc_file(), "rt");
  if (stream == NULL) return;

  char token[1024];
  int  pos = 0;
  int  c = getc(stream);
  while(c != EOF && pos < (int)(sizeof(token)-1)) {
    if (c == '\n') {
      token[pos++] = '\0';
      parse_from_line(token);
      pos = 0;
    } else {
      token[pos++] = c;
    }
    c = getc(stream);
  }
  token[pos++] = '\0';
  parse_from_line(token);

  fclose(stream);
}

void CompilerOracle::parse_from_string(const char* str, void (*parse_line)(char*)) {
  char token[1024];
  int  pos = 0;
  const char* sp = str;
  int  c = *sp++;
  while (c != '\0' && pos < (int)(sizeof(token)-1)) {
    if (c == '\n') {
      token[pos++] = '\0';
      parse_line(token);
      pos = 0;
    } else {
      token[pos++] = c;
    }
    c = *sp++;
  }
  token[pos++] = '\0';
  parse_line(token);
}

void CompilerOracle::append_comment_to_file(const char* message) {
  assert(has_command_file(), "command file must be specified");
  fileStream stream(fopen(cc_file(), "at"));
  stream.print("# ");
  for (int index = 0; message[index] != '\0'; index++) {
    stream.put(message[index]);
    if (message[index] == '\n') stream.print("# ");
  }
  stream.cr();
}

void CompilerOracle::append_exclude_to_file(methodHandle method) {
  assert(has_command_file(), "command file must be specified");
  fileStream stream(fopen(cc_file(), "at"));
  stream.print("exclude ");
  method->method_holder()->name()->print_symbol_on(&stream);
  stream.print(".");
  method->name()->print_symbol_on(&stream);
  method->signature()->print_symbol_on(&stream);
  stream.cr();
  stream.cr();
}


void compilerOracle_init() {
  CompilerOracle::parse_from_string(CompileCommand, CompilerOracle::parse_from_line);
  CompilerOracle::parse_from_string(CompileOnly, CompilerOracle::parse_compile_only);
  if (CompilerOracle::has_command_file()) {
    CompilerOracle::parse_from_file();
  } else {
    struct stat buf;
    if (os::stat(default_cc_file, &buf) == 0) {
      warning("%s file is present but has been ignored.  "
              "Run with -XX:CompileCommandFile=%s to load the file.",
              default_cc_file, default_cc_file);
    }
  }
  if (lists[PrintCommand] != NULL) {
    if (PrintAssembly) {
      warning("CompileCommand and/or %s file contains 'print' commands, but PrintAssembly is also enabled", default_cc_file);
    } else if (FLAG_IS_DEFAULT(DebugNonSafepoints)) {
      warning("printing of assembly code is enabled; turning on DebugNonSafepoints to gain additional output");
      DebugNonSafepoints = true;
    }
  }
}


void CompilerOracle::parse_compile_only(char * line) {
  int i;
  char name[1024];
  const char* className = NULL;
  const char* methodName = NULL;

  bool have_colon = (strstr(line, "::") != NULL);
  char method_sep = have_colon ? ':' : '.';

  if (Verbose) {
    tty->print_cr("%s", line);
  }

  ResourceMark rm;
  while (*line != '\0') {
    MethodMatcher::Mode c_match = MethodMatcher::Exact;
    MethodMatcher::Mode m_match = MethodMatcher::Exact;

    for (i = 0;
         i < 1024 && *line != '\0' && *line != method_sep && *line != ',' && !isspace(*line);
         line++, i++) {
      name[i] = *line;
      if (name[i] == '.')  name[i] = '/';  // package prefix uses '/'
    }

    if (i > 0) {
      char* newName = NEW_RESOURCE_ARRAY( char, i + 1);
      if (newName == NULL)
        return;
      strncpy(newName, name, i);
      newName[i] = '\0';

      if (className == NULL) {
        className = newName;
        c_match = MethodMatcher::Prefix;
      } else {
        methodName = newName;
      }
    }

    if (*line == method_sep) {
      if (className == NULL) {
        className = "";
        c_match = MethodMatcher::Any;
      } else {
        // foo/bar.blah is an exact match on foo/bar, bar.blah is a suffix match on bar
        if (strchr(className, '/') != NULL) {
          c_match = MethodMatcher::Exact;
        } else {
          c_match = MethodMatcher::Suffix;
        }
      }
    } else {
      // got foo or foo/bar
      if (className == NULL) {
        ShouldNotReachHere();
      } else {
        // got foo or foo/bar
        if (strchr(className, '/') != NULL) {
          c_match = MethodMatcher::Prefix;
        } else if (className[0] == '\0') {
          c_match = MethodMatcher::Any;
        } else {
          c_match = MethodMatcher::Substring;
        }
      }
    }

    // each directive is terminated by , or NUL or . followed by NUL
    if (*line == ',' || *line == '\0' || (line[0] == '.' && line[1] == '\0')) {
      if (methodName == NULL) {
        methodName = "";
        if (*line != method_sep) {
          m_match = MethodMatcher::Any;
        }
      }

      EXCEPTION_MARK;
      Symbol* c_name = SymbolTable::new_symbol(className, CHECK);
      Symbol* m_name = SymbolTable::new_symbol(methodName, CHECK);
      Symbol* signature = NULL;

      add_predicate(CompileOnlyCommand, c_name, c_match, m_name, m_match, signature);
      if (PrintVMOptions) {
        tty->print("CompileOnly: compileonly ");
        lists[CompileOnlyCommand]->print();
      }

      className = NULL;
      methodName = NULL;
    }

    line = *line == '\0' ? line : line + 1;
  }
}
