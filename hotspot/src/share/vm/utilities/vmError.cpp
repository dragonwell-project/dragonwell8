/*
 * Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include <fcntl.h>
#include "precompiled.hpp"
#include "compiler/compileBroker.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "jfr/jfrEvents.hpp"
#include "prims/whitebox.hpp"
#include "runtime/arguments.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/init.hpp"
#include "runtime/os.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/vmThread.hpp"
#include "runtime/vm_operations.hpp"
#include "services/memTracker.hpp"
#include "utilities/debug.hpp"
#include "utilities/decoder.hpp"
#include "utilities/defaultStream.hpp"
#include "utilities/errorReporter.hpp"
#include "utilities/events.hpp"
#include "utilities/top.hpp"
#include "utilities/vmError.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_JFR
#include "jfr/jfr.hpp"
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// List of environment variables that should be reported in error log file.
const char *env_list[] = {
  // All platforms
  "JAVA_HOME", "JRE_HOME", "JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "CLASSPATH",
  "JAVA_COMPILER", "PATH", "USERNAME", "DRAGONWELL_JAVA_TOOL_OPTIONS", "DRAGONWELL_JAVA_TOOL_OPTIONS_JDK_ONLY",

  // Env variables that are defined on Solaris/Linux/BSD
  "LD_LIBRARY_PATH", "LD_PRELOAD", "SHELL", "DISPLAY",
  "HOSTTYPE", "OSTYPE", "ARCH", "MACHTYPE",

  // defined on Linux
  "LD_ASSUME_KERNEL", "_JAVA_SR_SIGNUM",

  // defined on Darwin
  "DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH",
  "DYLD_FRAMEWORK_PATH", "DYLD_FALLBACK_FRAMEWORK_PATH",
  "DYLD_INSERT_LIBRARIES",

  // defined on Windows
  "OS", "PROCESSOR_IDENTIFIER", "_ALT_JAVA_HOME_DIR",

  (const char *)0
};

// Fatal error handler for internal errors and crashes.
//
// The default behavior of fatal error handler is to print a brief message
// to standard out (defaultStream::output_fd()), then save detailed information
// into an error report file (hs_err_pid<pid>.log) and abort VM. If multiple
// threads are having troubles at the same time, only one error is reported.
// The thread that is reporting error will abort VM when it is done, all other
// threads are blocked forever inside report_and_die().

// Constructor for crashes
VMError::VMError(Thread* thread, unsigned int sig, address pc, void* siginfo, void* context) {
    _thread = thread;
    _id = sig;
    _pc   = pc;
    _siginfo = siginfo;
    _context = context;

    _verbose = false;
    _current_step = 0;
    _current_step_info = NULL;

    _message = NULL;
    _detail_msg = NULL;
    _filename = NULL;
    _lineno = 0;

    _size = 0;
}

// Constructor for internal errors
VMError::VMError(Thread* thread, const char* filename, int lineno,
                 const char* message, const char * detail_msg)
{
  _thread = thread;
  _id = INTERNAL_ERROR;     // Value that's not an OS exception/signal
  _filename = filename;
  _lineno = lineno;
  _message = message;
  _detail_msg = detail_msg;

  _verbose = false;
  _current_step = 0;
  _current_step_info = NULL;

  _pc = NULL;
  _siginfo = NULL;
  _context = NULL;

  _size = 0;
}

// Constructor for OOM errors
VMError::VMError(Thread* thread, const char* filename, int lineno, size_t size,
                 VMErrorType vm_err_type, const char* message) {
    _thread = thread;
    _id = vm_err_type; // Value that's not an OS exception/signal
    _filename = filename;
    _lineno = lineno;
    _message = message;
    _detail_msg = NULL;

    _verbose = false;
    _current_step = 0;
    _current_step_info = NULL;

    _pc = NULL;
    _siginfo = NULL;
    _context = NULL;

    _size = size;
}


// Constructor for non-fatal errors
VMError::VMError(const char* message) {
    _thread = NULL;
    _id = INTERNAL_ERROR;     // Value that's not an OS exception/signal
    _filename = NULL;
    _lineno = 0;
    _message = message;
    _detail_msg = NULL;

    _verbose = false;
    _current_step = 0;
    _current_step_info = NULL;

    _pc = NULL;
    _siginfo = NULL;
    _context = NULL;

    _size = 0;
}

// -XX:OnError=<string>, where <string> can be a list of commands, separated
// by ';'. "%p" is replaced by current process id (pid); "%%" is replaced by
// a single "%". Some examples:
//
// -XX:OnError="pmap %p"                // show memory map
// -XX:OnError="gcore %p; dbx - %p"     // dump core and launch debugger
// -XX:OnError="cat hs_err_pid%p.log | mail my_email@sun.com"
// -XX:OnError="kill -9 %p"             // ?#!@#

// A simple parser for -XX:OnError, usage:
//  ptr = OnError;
//  while ((cmd = next_OnError_command(buffer, sizeof(buffer), &ptr) != NULL)
//     ... ...
static char* next_OnError_command(char* buf, int buflen, const char** ptr) {
  if (ptr == NULL || *ptr == NULL) return NULL;

  const char* cmd = *ptr;

  // skip leading blanks or ';'
  while (*cmd == ' ' || *cmd == ';') cmd++;

  if (*cmd == '\0') return NULL;

  const char * cmdend = cmd;
  while (*cmdend != '\0' && *cmdend != ';') cmdend++;

  Arguments::copy_expand_pid(cmd, cmdend - cmd, buf, buflen);

  *ptr = (*cmdend == '\0' ? cmdend : cmdend + 1);
  return buf;
}


static void print_bug_submit_message(outputStream *out, Thread *thread) {
  if (out == NULL) return;
  out->print_raw_cr("# If you would like to submit a bug report, please visit:");
  out->print_raw   ("#   ");
  out->print_raw_cr(Arguments::java_vendor_url_bug());
  // If the crash is in native code, encourage user to submit a bug to the
  // provider of that code.
  if (thread && thread->is_Java_thread() &&
      !thread->is_hidden_from_external_view()) {
    JavaThread* jt = (JavaThread*)thread;
    if (jt->thread_state() == _thread_in_native) {
      out->print_cr("# The crash happened outside the Java Virtual Machine in native code.\n# See problematic frame for where to report the bug.");
    }
  }
  out->print_raw_cr("#");
}

bool VMError::coredump_status;
char VMError::coredump_message[O_BUFLEN];

void VMError::report_coredump_status(const char* message, bool status) {
  coredump_status = status;
  strncpy(coredump_message, message, sizeof(coredump_message));
  coredump_message[sizeof(coredump_message)-1] = 0;
}


// Return a string to describe the error
char* VMError::error_string(char* buf, int buflen) {
  char signame_buf[64];
  const char *signame = os::exception_name(_id, signame_buf, sizeof(signame_buf));

  if (signame) {
    jio_snprintf(buf, buflen,
                 "%s (0x%x) at pc=" PTR_FORMAT ", pid=%d, tid=" INTPTR_FORMAT,
                 signame, _id, _pc,
                 os::current_process_id(), os::current_thread_id());
  } else if (_filename != NULL && _lineno > 0) {
    // skip directory names
    char separator = os::file_separator()[0];
    const char *p = strrchr(_filename, separator);
    int n = jio_snprintf(buf, buflen,
                         "Internal Error at %s:%d, pid=%d, tid=" INTPTR_FORMAT,
                         p ? p + 1 : _filename, _lineno,
                         os::current_process_id(), os::current_thread_id());
    if (n >= 0 && n < buflen && _message) {
      if (_detail_msg) {
        jio_snprintf(buf + n, buflen - n, "%s%s: %s",
                     os::line_separator(), _message, _detail_msg);
      } else {
        jio_snprintf(buf + n, buflen - n, "%sError: %s",
                     os::line_separator(), _message);
      }
    }
  } else {
    jio_snprintf(buf, buflen,
                 "Internal Error (0x%x), pid=%d, tid=" INTPTR_FORMAT,
                 _id, os::current_process_id(), os::current_thread_id());
  }

  return buf;
}

void VMError::print_stack_trace(outputStream* st, JavaThread* jt,
                                char* buf, int buflen, bool verbose) {
#ifdef ZERO
  if (jt->zero_stack()->sp() && jt->top_zero_frame()) {
    // StackFrameStream uses the frame anchor, which may not have
    // been set up.  This can be done at any time in Zero, however,
    // so if it hasn't been set up then we just set it up now and
    // clear it again when we're done.
    bool has_last_Java_frame = jt->has_last_Java_frame();
    if (!has_last_Java_frame)
      jt->set_last_Java_frame();
    st->print("Java frames:");

    // If the top frame is a Shark frame and the frame anchor isn't
    // set up then it's possible that the information in the frame
    // is garbage: it could be from a previous decache, or it could
    // simply have never been written.  So we print a warning...
    StackFrameStream sfs(jt);
    if (!has_last_Java_frame && !sfs.is_done()) {
      if (sfs.current()->zeroframe()->is_shark_frame()) {
        st->print(" (TOP FRAME MAY BE JUNK)");
      }
    }
    st->cr();

    // Print the frames
    for(int i = 0; !sfs.is_done(); sfs.next(), i++) {
      sfs.current()->zero_print_on_error(i, st, buf, buflen);
      st->cr();
    }

    // Reset the frame anchor if necessary
    if (!has_last_Java_frame)
      jt->reset_last_Java_frame();
  }
#else
  if (jt->has_last_Java_frame()) {
    st->print_cr("Java frames: (J=compiled Java code, j=interpreted, Vv=VM code)");
    for(StackFrameStream sfs(jt); !sfs.is_done(); sfs.next()) {
      sfs.current()->print_on_error(st, buf, buflen, verbose);
      st->cr();
    }
  }
#endif // ZERO
}

static void print_oom_reasons(outputStream* st) {
  st->print_cr("# Possible reasons:");
  st->print_cr("#   The system is out of physical RAM or swap space");
  if (UseCompressedOops) {
    st->print_cr("#   The process is running with CompressedOops enabled, and the Java Heap may be blocking the growth of the native heap");
  }
  if (LogBytesPerWord == 2) {
    st->print_cr("#   In 32 bit mode, the process size limit was hit");
  }
  st->print_cr("# Possible solutions:");
  st->print_cr("#   Reduce memory load on the system");
  st->print_cr("#   Increase physical memory or swap space");
  st->print_cr("#   Check if swap backing store is full");
  if (LogBytesPerWord == 2) {
    st->print_cr("#   Use 64 bit Java on a 64 bit OS");
  }
  st->print_cr("#   Decrease Java heap size (-Xmx/-Xms)");
  st->print_cr("#   Decrease number of Java threads");
  st->print_cr("#   Decrease Java thread stack sizes (-Xss)");
  st->print_cr("#   Set larger code cache with -XX:ReservedCodeCacheSize=");
  if (UseCompressedOops) {
    switch (Universe::narrow_oop_mode()) {
      case Universe::UnscaledNarrowOop:
        st->print_cr("#   JVM is running with Unscaled Compressed Oops mode in which the Java heap is");
        st->print_cr("#     placed in the first 4GB address space. The Java Heap base address is the");
        st->print_cr("#     maximum limit for the native heap growth. Please use -XX:HeapBaseMinAddress");
        st->print_cr("#     to set the Java Heap base and to place the Java Heap above 4GB virtual address.");
        break;
      case Universe::ZeroBasedNarrowOop:
        st->print_cr("#   JVM is running with Zero Based Compressed Oops mode in which the Java heap is");
        st->print_cr("#     placed in the first 32GB address space. The Java Heap base address is the");
        st->print_cr("#     maximum limit for the native heap growth. Please use -XX:HeapBaseMinAddress");
        st->print_cr("#     to set the Java Heap base and to place the Java Heap above 32GB virtual address.");
        break;
      default:
        break;
    }
  }
  st->print_cr("# This output file may be truncated or incomplete.");
}

// This is the main function to report a fatal error. Only one thread can
// call this function, so we don't need to worry about MT-safety. But it's
// possible that the error handler itself may crash or die on an internal
// error, for example, when the stack/heap is badly damaged. We must be
// able to handle recursive errors that happen inside error handler.
//
// Error reporting is done in several steps. If a crash or internal error
// occurred when reporting an error, the nested signal/exception handler
// can skip steps that are already (or partially) done. Error reporting will
// continue from the next step. This allows us to retrieve and print
// information that may be unsafe to get after a fatal error. If it happens,
// you may find nested report_and_die() frames when you look at the stack
// in a debugger.
//
// In general, a hang in error handler is much worse than a crash or internal
// error, as it's harder to recover from a hang. Deadlock can happen if we
// try to grab a lock that is already owned by current thread, or if the
// owner is blocked forever (e.g. in os::infinite_sleep()). If possible, the
// error handler and all the functions it called should avoid grabbing any
// lock. An important thing to notice is that memory allocation needs a lock.
//
// We should avoid using large stack allocated buffers. Many errors happen
// when stack space is already low. Making things even worse is that there
// could be nested report_and_die() calls on stack (see above). Only one
// thread can report error, so large buffers are statically allocated in data
// segment.

void VMError::report(outputStream* st) {
# define BEGIN if (_current_step == 0) { _current_step = 1;
# define STEP(n, s) } if (_current_step < n) { _current_step = n; _current_step_info = s;
# define END }

  // don't allocate large buffer on stack
  static char buf[O_BUFLEN];

  BEGIN

  STEP(10, "(printing fatal error message)")

    st->print_cr("#");
    if (should_report_bug(_id)) {
      st->print_cr("# A fatal error has been detected by the Java Runtime Environment:");
    } else {
      st->print_cr("# There is insufficient memory for the Java "
                   "Runtime Environment to continue.");
    }

  STEP(15, "(printing type of error)")

     switch(_id) {
       case OOM_MALLOC_ERROR:
       case OOM_MMAP_ERROR:
         if (_size) {
           st->print("# Native memory allocation ");
           st->print((_id == (int)OOM_MALLOC_ERROR) ? "(malloc) failed to allocate " :
                                                 "(mmap) failed to map ");
           jio_snprintf(buf, sizeof(buf), SIZE_FORMAT, _size);
           st->print("%s", buf);
           st->print(" bytes");
           if (_message != NULL) {
             st->print(" for ");
             st->print("%s", _message);
           }
           st->cr();
         } else {
           if (_message != NULL)
             st->print("# ");
             st->print_cr("%s", _message);
         }
         // In error file give some solutions
         if (_verbose) {
           print_oom_reasons(st);
         } else {
           return;  // that's enough for the screen
         }
         break;
       case INTERNAL_ERROR:
       default:
         break;
     }

  STEP(20, "(printing exception/signal name)")

     st->print_cr("#");
     st->print("#  ");
     // Is it an OS exception/signal?
     if (os::exception_name(_id, buf, sizeof(buf))) {
       st->print("%s", buf);
       st->print(" (0x%x)", _id);                // signal number
       st->print(" at pc=" PTR_FORMAT, _pc);
     } else {
       if (should_report_bug(_id)) {
         st->print("Internal Error");
       } else {
         st->print("Out of Memory Error");
       }
       if (_filename != NULL && _lineno > 0) {
#ifdef PRODUCT
         // In product mode chop off pathname?
         char separator = os::file_separator()[0];
         const char *p = strrchr(_filename, separator);
         const char *file = p ? p+1 : _filename;
#else
         const char *file = _filename;
#endif
         st->print(" (%s:%d)", file, _lineno);
       } else {
         st->print(" (0x%x)", _id);
       }
     }

  STEP(30, "(printing current thread and pid)")

     // process id, thread id
     st->print(", pid=%d", os::current_process_id());
     st->print(", tid=" INTPTR_FORMAT, os::current_thread_id());
     st->cr();

  STEP(40, "(printing error message)")

     if (should_report_bug(_id)) {  // already printed the message.
       // error message
       if (_detail_msg) {
         st->print_cr("#  %s: %s", _message ? _message : "Error", _detail_msg);
       } else if (_message) {
         st->print_cr("#  Error: %s", _message);
       }
    }

  STEP(50, "(printing Java version string)")

     // VM version
     st->print_cr("#");
     JDK_Version::current().to_string(buf, sizeof(buf));
     const char* runtime_name = JDK_Version::runtime_name() != NULL ?
                                  JDK_Version::runtime_name() : "";
     const char* runtime_version = JDK_Version::runtime_version() != NULL ?
                                  JDK_Version::runtime_version() : "";
     st->print_cr("# JRE version: %s (%s) (build %s)", runtime_name, buf, runtime_version);
     st->print_cr("# Java VM: %s (%s %s %s %s)",
                   Abstract_VM_Version::vm_name(),
                   Abstract_VM_Version::vm_release(),
                   Abstract_VM_Version::vm_info_string(),
                   Abstract_VM_Version::vm_platform_string(),
                   UseCompressedOops ? "compressed oops" : ""
                 );

  STEP(60, "(printing problematic frame)")

     // Print current frame if we have a context (i.e. it's a crash)
     if (_context) {
       st->print_cr("# Problematic frame:");
       st->print("# ");
       frame fr = os::fetch_frame_from_context(_context);
       fr.print_on_error(st, buf, sizeof(buf));
       st->cr();
       st->print_cr("#");
     }
  STEP(63, "(printing core file information)")
    st->print("# ");
    if (coredump_status) {
      st->print("Core dump written. Default location: %s", coredump_message);
    } else {
      st->print("Failed to write core dump. %s", coredump_message);
    }
    st->cr();
    st->print_cr("#");

  STEP(65, "(printing bug submit message)")

     if (should_report_bug(_id) && _verbose) {
       print_bug_submit_message(st, _thread);
     }

  STEP(70, "(printing thread)" )

     if (_verbose) {
       st->cr();
       st->print_cr("---------------  T H R E A D  ---------------");
       st->cr();
     }

  STEP(80, "(printing current thread)" )

     // current thread
     if (_verbose) {
       if (_thread) {
         st->print("Current thread (" PTR_FORMAT "):  ", _thread);
         _thread->print_on_error(st, buf, sizeof(buf));
         st->cr();
       } else {
         st->print_cr("Current thread is native thread");
       }
       st->cr();
     }

  STEP(90, "(printing siginfo)" )

     // signal no, signal code, address that caused the fault
     if (_verbose && _siginfo) {
       os::print_siginfo(st, _siginfo);
       st->cr();
     }

  STEP(100, "(printing registers, top of stack, instructions near pc)")

     // registers, top of stack, instructions near pc
     if (_verbose && _context) {
       os::print_context(st, _context);
       st->cr();
     }

  STEP(105, "(printing register info)")

     // decode register contents if possible
     if (_verbose && _context && Universe::is_fully_initialized()) {
       os::print_register_info(st, _context);
       st->cr();
     }

  STEP(110, "(printing stack bounds)" )

     if (_verbose) {
       st->print("Stack: ");

       address stack_top;
       size_t stack_size;

       if (_thread) {
          stack_top = _thread->stack_base();
          stack_size = _thread->stack_size();
       } else {
          stack_top = os::current_stack_base();
          stack_size = os::current_stack_size();
       }

       address stack_bottom = stack_top - stack_size;
       st->print("[" PTR_FORMAT "," PTR_FORMAT "]", stack_bottom, stack_top);

       frame fr = _context ? os::fetch_frame_from_context(_context)
                           : os::current_frame();

       if (fr.sp()) {
         st->print(",  sp=" PTR_FORMAT, fr.sp());
         size_t free_stack_size = pointer_delta(fr.sp(), stack_bottom, 1024);
         st->print(",  free space=" SIZE_FORMAT "k", free_stack_size);
       }

       st->cr();
     }

  STEP(120, "(printing native stack)" )

   if (_verbose) {
     if (os::platform_print_native_stack(st, _context, buf, sizeof(buf))) {
       // We have printed the native stack in platform-specific code
       // Windows/x64 needs special handling.
     } else {
       frame fr = _context ? os::fetch_frame_from_context(_context)
                           : os::current_frame();

       print_native_stack(st, fr, _thread, buf, sizeof(buf));
     }
   }

  STEP(130, "(printing Java stack)" )

     if (_verbose && _thread && _thread->is_Java_thread()) {
       print_stack_trace(st, (JavaThread*)_thread, buf, sizeof(buf));
     }

  STEP(135, "(printing target Java thread stack)" )

     // printing Java thread stack trace if it is involved in GC crash
     if (_verbose && _thread && (_thread->is_Named_thread())) {
       JavaThread*  jt = ((NamedThread *)_thread)->processed_thread();
       if (jt != NULL) {
         st->print_cr("JavaThread " PTR_FORMAT " (nid = " UINTX_FORMAT ") was being processed", jt, jt->osthread()->thread_id());
         print_stack_trace(st, jt, buf, sizeof(buf), true);
       }
     }

  STEP(140, "(printing VM operation)" )

     if (_verbose && _thread && _thread->is_VM_thread()) {
        VMThread* t = (VMThread*)_thread;
        VM_Operation* op = t->vm_operation();
        if (op) {
          op->print_on_error(st);
          st->cr();
          st->cr();
        }
     }

  STEP(150, "(printing current compile task)" )

     if (_verbose && _thread && _thread->is_Compiler_thread()) {
        CompilerThread* t = (CompilerThread*)_thread;
        if (t->task()) {
           st->cr();
           st->print_cr("Current CompileTask:");
           t->task()->print_line_on_error(st, buf, sizeof(buf));
           st->cr();
        }
     }

  STEP(160, "(printing process)" )

     if (_verbose) {
       st->cr();
       st->print_cr("---------------  P R O C E S S  ---------------");
       st->cr();
     }

  STEP(170, "(printing all threads)" )

     // all threads
     if (_verbose && _thread) {
       Threads::print_on_error(st, _thread, buf, sizeof(buf));
       st->cr();
     }

  STEP(175, "(printing VM state)" )

     if (_verbose) {
       // Safepoint state
       st->print("VM state:");

       if (SafepointSynchronize::is_synchronizing()) st->print("synchronizing");
       else if (SafepointSynchronize::is_at_safepoint()) st->print("at safepoint");
       else st->print("not at safepoint");

       // Also see if error occurred during initialization or shutdown
       if (!Universe::is_fully_initialized()) {
         st->print(" (not fully initialized)");
       } else if (VM_Exit::vm_exited()) {
         st->print(" (shutting down)");
       } else {
         st->print(" (normal execution)");
       }
       st->cr();
       st->cr();
     }

  STEP(180, "(printing owned locks on error)" )

     // mutexes/monitors that currently have an owner
     if (_verbose) {
       print_owned_locks_on_error(st);
       st->cr();
     }

  STEP(182, "(printing number of OutOfMemoryError and StackOverflow exceptions)")

     if (_verbose && Exceptions::has_exception_counts()) {
       st->print_cr("OutOfMemory and StackOverflow Exception counts:");
       Exceptions::print_exception_counts_on_error(st);
       st->cr();
     }

  STEP(185, "(printing compressed oops mode")

     if (_verbose && UseCompressedOops) {
       Universe::print_compressed_oops_mode(st);
       if (UseCompressedClassPointers) {
         Metaspace::print_compressed_class_space(st);
       }
       st->cr();
     }

  STEP(190, "(printing heap information)" )

     if (_verbose && Universe::is_fully_initialized()) {
       Universe::heap()->print_on_error(st);
       st->cr();

       st->print_cr("Polling page: " INTPTR_FORMAT, os::get_polling_page());
       st->cr();
     }

  STEP(195, "(printing code cache information)" )

     if (_verbose && Universe::is_fully_initialized()) {
       // print code cache information before vm abort
       CodeCache::print_summary(st);
       st->cr();
     }

  STEP(200, "(printing ring buffers)" )

     if (_verbose) {
       Events::print_all(st);
       st->cr();
     }

  STEP(205, "(printing dynamic libraries)" )

     if (_verbose) {
       // dynamic libraries, or memory map
       os::print_dll_info(st);
       st->cr();
     }

  STEP(210, "(printing VM options)" )

     if (_verbose) {
       // VM options
       Arguments::print_on(st);
       st->cr();
     }

  STEP(215, "(printing warning if internal testing API used)" )

     if (WhiteBox::used()) {
       st->print_cr("Unsupported internal testing APIs have been used.");
       st->cr();
     }

  STEP(220, "(printing environment variables)" )

     if (_verbose) {
       os::print_environment_variables(st, env_list, buf, sizeof(buf));
       st->cr();
     }

  STEP(225, "(printing signal handlers)" )

     if (_verbose) {
       os::print_signal_handlers(st, buf, sizeof(buf));
       st->cr();
     }

  STEP(228, "(Native Memory Tracking)" )
     if (_verbose) {
       MemTracker::error_report(st);
     }

  STEP(230, "" )

     if (_verbose) {
       st->cr();
       st->print_cr("---------------  S Y S T E M  ---------------");
       st->cr();
     }

  STEP(240, "(printing OS information)" )

     if (_verbose) {
       os::print_os_info(st);
       st->cr();
     }

  STEP(250, "(printing CPU info)" )
     if (_verbose) {
       os::print_cpu_info(st);
       st->cr();
     }

  STEP(260, "(printing memory info)" )

     if (_verbose) {
       os::print_memory_info(st);
       st->cr();
     }

  STEP(270, "(printing internal vm info)" )

     if (_verbose) {
       st->print_cr("vm_info: %s", Abstract_VM_Version::internal_vm_info_string());
       st->cr();
     }

  STEP(280, "(printing date and time)" )

     if (_verbose) {
       os::print_date_and_time(st, buf, sizeof(buf));
       st->cr();
     }

  END

# undef BEGIN
# undef STEP
# undef END
}

VMError* volatile VMError::first_error = NULL;
volatile jlong VMError::first_error_tid = -1;

// An error could happen before tty is initialized or after it has been
// destroyed. Here we use a very simple unbuffered fdStream for printing.
// Only out.print_raw() and out.print_raw_cr() should be used, as other
// printing methods need to allocate large buffer on stack. To format a
// string, use jio_snprintf() with a static buffer or use staticBufferStream.
fdStream VMError::out(defaultStream::output_fd());
fdStream VMError::log; // error log used by VMError::report_and_die()

/** Expand a pattern into a buffer starting at pos and open a file using constructed path */
static int expand_and_open(const char* pattern, char* buf, size_t buflen, size_t pos) {
  int fd = -1;
  if (Arguments::copy_expand_pid(pattern, strlen(pattern), &buf[pos], buflen - pos)) {
    // the O_EXCL flag will cause the open to fail if the file exists
    fd = open(buf, O_RDWR | O_CREAT | O_EXCL, 0666);
  }
  return fd;
}

/**
 * Construct file name for a log file and return it's file descriptor.
 * Name and location depends on pattern, default_pattern params and access
 * permissions.
 */
static int prepare_log_file(const char* pattern, const char* default_pattern, char* buf, size_t buflen) {
  int fd = -1;

  // If possible, use specified pattern to construct log file name
  if (pattern != NULL) {
    fd = expand_and_open(pattern, buf, buflen, 0);
  }

  // Either user didn't specify, or the user's location failed,
  // so use the default name in the current directory
  if (fd == -1) {
    const char* cwd = os::get_current_directory(buf, buflen);
    if (cwd != NULL) {
      size_t pos = strlen(cwd);
      int fsep_len = jio_snprintf(&buf[pos], buflen-pos, "%s", os::file_separator());
      pos += fsep_len;
      if (fsep_len > 0) {
        fd = expand_and_open(default_pattern, buf, buflen, pos);
      }
    }
  }

   // try temp directory if it exists.
   if (fd == -1) {
     const char* tmpdir = os::get_temp_directory();
     if (tmpdir != NULL && strlen(tmpdir) > 0) {
       int pos = jio_snprintf(buf, buflen, "%s%s", tmpdir, os::file_separator());
       if (pos > 0) {
         fd = expand_and_open(default_pattern, buf, buflen, pos);
       }
     }
   }

  return fd;
}

void VMError::report_and_die() {
  // Don't allocate large buffer on stack
  static char buffer[O_BUFLEN];

  // How many errors occurred in error handler when reporting first_error.
  static int recursive_error_count;

  // We will first print a brief message to standard out (verbose = false),
  // then save detailed information in log file (verbose = true).
  static bool out_done = false;         // done printing to standard out
  static bool log_done = false;         // done saving error log
  static bool transmit_report_done = false; // done error reporting

  if (SuppressFatalErrorMessage) {
      os::abort();
  }
  jlong mytid = os::current_thread_id();
  if (first_error == NULL &&
      Atomic::cmpxchg_ptr(this, &first_error, NULL) == NULL) {

    // first time
    first_error_tid = mytid;
    set_error_reported();

    if (ShowMessageBoxOnError || PauseAtExit) {
      show_message_box(buffer, sizeof(buffer));

      // User has asked JVM to abort. Reset ShowMessageBoxOnError so the
      // WatcherThread can kill JVM if the error handler hangs.
      ShowMessageBoxOnError = false;
    }

    // Write a minidump on Windows, check core dump limits on Linux/Solaris
    os::check_or_create_dump(_siginfo, _context, buffer, sizeof(buffer));

    // reset signal handlers or exception filter; make sure recursive crashes
    // are handled properly.
    reset_signal_handlers();

    EventShutdown e;
    if (e.should_commit()) {
      e.set_reason("VM Error");
      e.commit();
    }

    JFR_ONLY(Jfr::on_vm_shutdown(true);)
  } else {
    // If UseOsErrorReporting we call this for each level of the call stack
    // while searching for the exception handler.  Only the first level needs
    // to be reported.
    if (UseOSErrorReporting && log_done) return;

    // This is not the first error, see if it happened in a different thread
    // or in the same thread during error reporting.
    if (first_error_tid != mytid) {
      char msgbuf[64];
      jio_snprintf(msgbuf, sizeof(msgbuf),
                   "[thread " INT64_FORMAT " also had an error]",
                   mytid);
      out.print_raw_cr(msgbuf);

      // error reporting is not MT-safe, block current thread
      os::infinite_sleep();

    } else {
      if (recursive_error_count++ > 30) {
        out.print_raw_cr("[Too many errors, abort]");
        os::die();
      }

      jio_snprintf(buffer, sizeof(buffer),
                   "[error occurred during error reporting %s, id 0x%x]",
                   first_error ? first_error->_current_step_info : "",
                   _id);
      if (log.is_open()) {
        log.cr();
        log.print_raw_cr(buffer);
        log.cr();
      } else {
        out.cr();
        out.print_raw_cr(buffer);
        out.cr();
      }
    }
  }

  // Part 1: print an abbreviated version (the '#' section) to stdout.
  if (!out_done) {
    first_error->_verbose = false;

    // Suppress this output if we plan to print Part 2 to stdout too.
    // No need to have the "#" section twice.
    if (!(ErrorFileToStdout && out.fd() == 1)) {
      staticBufferStream sbs(buffer, sizeof(buffer), &out);
      first_error->report(&sbs);
    }

    out_done = true;

    first_error->_current_step = 0;         // reset current_step
    first_error->_current_step_info = "";   // reset current_step string
  }

  // Part 2: print a full error log file (optionally to stdout or stderr).
  // print to error log file
  if (!log_done) {
    first_error->_verbose = true;

    // see if log file is already open
    if (!log.is_open()) {
      // open log file
      int fd;
      if (ErrorFileToStdout) {
        fd = 1;
      } else if (ErrorFileToStderr) {
        fd = 2;
      } else {
        fd = prepare_log_file(ErrorFile, "hs_err_pid%p.log", buffer, sizeof(buffer));
        if (fd != -1) {
          out.print_raw("# An error report file with more information is saved as:\n# ");
          out.print_raw_cr(buffer);

        } else {
          out.print_raw_cr("# Can not save log file, dump to screen..");
          fd = defaultStream::output_fd();
          /* Error reporting currently needs dumpfile.
           * Maybe implement direct streaming in the future.*/
          transmit_report_done = true;
        }
      }
      log.set_fd(fd);
    }

    staticBufferStream sbs(buffer, O_BUFLEN, &log);
    first_error->report(&sbs);
    first_error->_current_step = 0;         // reset current_step
    first_error->_current_step_info = "";   // reset current_step string

    // Run error reporting to determine whether or not to report the crash.
    if (!transmit_report_done && should_report_bug(first_error->_id)) {
      transmit_report_done = true;
      FILE* hs_err = os::open(log.fd(), "r");
      if (NULL != hs_err) {
        ErrorReporter er;
        er.call(hs_err, buffer, O_BUFLEN);
      }
    }

    if (log.fd() > 3) {
      close(log.fd());
    }

    log.set_fd(-1);
    log_done = true;
  }


  static bool skip_OnError = false;
  if (!skip_OnError && OnError && OnError[0]) {
    skip_OnError = true;

    out.print_raw_cr("#");
    out.print_raw   ("# -XX:OnError=\"");
    out.print_raw   (OnError);
    out.print_raw_cr("\"");

    char* cmd;
    const char* ptr = OnError;
    while ((cmd = next_OnError_command(buffer, sizeof(buffer), &ptr)) != NULL){
      out.print_raw   ("#   Executing ");
#if defined(LINUX) || defined(_ALLBSD_SOURCE)
      out.print_raw   ("/bin/sh -c ");
#elif defined(SOLARIS)
      out.print_raw   ("/usr/bin/sh -c ");
#endif
      out.print_raw   ("\"");
      out.print_raw   (cmd);
      out.print_raw_cr("\" ...");

      if (os::fork_and_exec(cmd) < 0) {
        out.print_cr("os::fork_and_exec failed: %s (%d)", strerror(errno), errno);
      }
    }

    // done with OnError
    OnError = NULL;
  }

  static bool skip_replay = ReplayCompiles; // Do not overwrite file during replay
  if (DumpReplayDataOnError && _thread && _thread->is_Compiler_thread() && !skip_replay) {
    skip_replay = true;
    ciEnv* env = ciEnv::current();
    if (env != NULL) {
      int fd = prepare_log_file(ReplayDataFile, "replay_pid%p.log", buffer, sizeof(buffer));
      if (fd != -1) {
        FILE* replay_data_file = os::open(fd, "w");
        if (replay_data_file != NULL) {
          fileStream replay_data_stream(replay_data_file, /*need_close=*/true);
          env->dump_replay_data_unsafe(&replay_data_stream);
          out.print_raw("#\n# Compiler replay data is saved as:\n# ");
          out.print_raw_cr(buffer);
        } else {
          out.print_raw("#\n# Can't open file to dump replay data. Error: ");
          out.print_raw_cr(strerror(os::get_last_error()));
        }
      }
    }
  }

  static bool skip_bug_url = !should_report_bug(first_error->_id);
  if (!skip_bug_url) {
    skip_bug_url = true;

    out.print_raw_cr("#");
    print_bug_submit_message(&out, _thread);
  }

  if (!UseOSErrorReporting) {
    // os::abort() will call abort hooks, try it first.
    static bool skip_os_abort = false;
    if (!skip_os_abort) {
      skip_os_abort = true;
      bool dump_core = should_report_bug(first_error->_id);
      os::abort(dump_core);
    }

    // if os::abort() doesn't abort, try os::die();
    os::die();
  }
}

/*
 * OnOutOfMemoryError scripts/commands executed while VM is a safepoint - this
 * ensures utilities such as jmap can observe the process is a consistent state.
 */
class VM_ReportJavaOutOfMemory : public VM_Operation {
 private:
  VMError *_err;
 public:
  VM_ReportJavaOutOfMemory(VMError *err) { _err = err; }
  VMOp_Type type() const                 { return VMOp_ReportJavaOutOfMemory; }
  void doit();
};

void VM_ReportJavaOutOfMemory::doit() {
  // Don't allocate large buffer on stack
  static char buffer[O_BUFLEN];

  tty->print_cr("#");
  tty->print_cr("# java.lang.OutOfMemoryError: %s", _err->message());
  tty->print_cr("# -XX:OnOutOfMemoryError=\"%s\"", OnOutOfMemoryError);

  // make heap parsability
  Universe::heap()->ensure_parsability(false);  // no need to retire TLABs

  char* cmd;
  const char* ptr = OnOutOfMemoryError;
  while ((cmd = next_OnError_command(buffer, sizeof(buffer), &ptr)) != NULL){
    tty->print("#   Executing ");
#if defined(LINUX)
    tty->print  ("/bin/sh -c ");
#elif defined(SOLARIS)
    tty->print  ("/usr/bin/sh -c ");
#endif
    tty->print_cr("\"%s\"...", cmd);

    if (os::fork_and_exec(cmd, true) < 0) {
      tty->print_cr("os::fork_and_exec failed: %s (%d)", strerror(errno), errno);
    }
  }
}

void VMError::report_java_out_of_memory() {
  if (OnOutOfMemoryError && OnOutOfMemoryError[0]) {
    MutexLocker ml(Heap_lock);
    VM_ReportJavaOutOfMemory op(this);
    VMThread::execute(&op);
  }
}
