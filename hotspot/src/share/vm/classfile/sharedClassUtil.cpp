/*
 * Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/classLoader.hpp"
#include "classfile/classLoaderExt.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/sharedClassUtil.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "memory/filemap.hpp"
#include "memory/metadataFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "runtime/arguments.hpp"
#include "runtime/java.hpp"
#include "runtime/os.hpp"

AgentEntry* SharedClassUtil::_agentlib_head = NULL;
AgentEntry* SharedClassUtil::_agentlib_tail = NULL;

AgentEntry::AgentEntry(const char* name) {
  char* copy = NEW_C_HEAP_ARRAY(char, strlen(name)+1, mtInternal);
  strcpy(copy, name);
  _name = copy;
  _next = NULL;
}

AgentEntry::~AgentEntry() {
  FREE_C_HEAP_ARRAY(char, _name, mtInternal);
}

class ManifestStream: public ResourceObj {
  private:
  u1*   _buffer_start; // Buffer bottom
  u1*   _buffer_end;   // Buffer top (one past last element)
  u1*   _current;      // Current buffer position

 public:
  // Constructor
  ManifestStream(u1* buffer, int length) : _buffer_start(buffer),
                                           _current(buffer) {
    _buffer_end = buffer + length;
  }

  static bool is_attr(u1* attr, const char* name) {
    return strncmp((const char*)attr, name, strlen(name)) == 0;
  }

  static char* copy_attr(u1* value, size_t len) {
    char* buf = NEW_RESOURCE_ARRAY(char, len + 1);
    strncpy(buf, (char*)value, len);
    buf[len] = 0;
    return buf;
  }

  // The return value indicates if the JAR is signed or not
  bool check_is_signed() {
    u1* attr = _current;
    bool isSigned = false;
    while (_current < _buffer_end) {
      if (*_current == '\n') {
        *_current = '\0';
        u1* value = (u1*)strchr((char*)attr, ':');
        if (value != NULL) {
          assert(*(value+1) == ' ', "Unrecognized format" );
          if (strstr((char*)attr, "-Digest") != NULL) {
            isSigned = true;
            break;
          }
        }
        *_current = '\n'; // restore
        attr = _current + 1;
      }
      _current ++;
    }
    return isSigned;
  }
};

/*  to slient compiler unused warnings
static int get_num_files_in_dir(const char* path) {
  DIR* dir = os::opendir(path);
  int count = 0;

  if (dir != NULL) {
    struct dirent *entry;
    while ((entry = os::readdir(dir)) != NULL) {
      const char *name = entry->d_name;
      if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
        count++;
      }
    }
    os::closedir(dir);
  }
}
 */

void SharedPathsMiscInfoExt::print_path(outputStream* out, int type, const char* path) {
  switch(type) {
  case APP:
    ClassLoader::trace_class_path(tty, "Expecting -Djava.class.path=", path);
    break;
  default:
    SharedPathsMiscInfo::print_path(out, type, path);
  }
}

int compare_classpath_segment (const void *s1, const void *s2){
  ClassPathSegment* cp1 = (ClassPathSegment*)s1;
  ClassPathSegment* cp2 = (ClassPathSegment*)s2;

  int n = strncmp(cp1->_start, cp2->_start, MIN2(cp1->_len, cp2->_len));
  if (n == 0) {
    return cp1->_len - cp2->_len;
  } else {
    return n;
  }
}


ClassPathSegment* SharedPathsMiscInfoExt::to_sorted_segments(const char* path, int &seg_num) {
  int max_seg_num = 0;
  for (const char *c = path; *c; c++) {
    if (*c == os::path_separator()[0]) {
      max_seg_num++;
    }
  }
  ClassPathSegment *cps = NEW_RESOURCE_ARRAY(ClassPathSegment, max_seg_num + 1);

  int len = strlen(path);
  //not use i < len,but use i<= len for processing the last segment conveniently
  for (int i = 0, j = 0; i <= len; i++) {
    if (i == len || path[i] == os::path_separator()[0]) {
      //i == j mean a empty string
      if (i != j) {
        cps[seg_num]._start = path + j;
        cps[seg_num++]._len = i - j;
      }
      j = i + 1;
    }
  }
  if (seg_num != 0) {
    qsort(cps, seg_num, sizeof(cps[0]), compare_classpath_segment);
  }
  return cps;
}


bool SharedPathsMiscInfoExt::check(jint type, const char* path) {

  switch (type) {
  case APP:
    {
      size_t len = strlen(path);
      const char *appcp = Arguments::get_appclasspath();
      assert(appcp != NULL, "NULL app classpath");
      size_t appcp_len = strlen(appcp);
      if (appcp_len < len) {
        return fail("Run time APP classpath is shorter than the one at dump time: ", appcp);
      }
      // Prefix is OK: E.g., dump with -cp foo.jar, but run with -cp foo.jar:bar.jar
      if (AppCDSVerifyClassPathOrder) {
        if (strncmp(path, appcp, len) != 0) {
          return fail("[APP classpath mismatch, actual: -Djava.class.path=", appcp);
        }
        if (appcp[len] != '\0' && appcp[len] != os::path_separator()[0]) {
          return fail("Dump time APP classpath is not a proper prefix of run time APP classpath: ", appcp);
        }
      } else{
        ResourceMark rm;
        int app_seg_num = 0;
        int path_seg_num = 0;
        ClassPathSegment* app_cps = to_sorted_segments(appcp, app_seg_num);
        ClassPathSegment* path_cps = to_sorted_segments(path, path_seg_num);
        if (app_seg_num < path_seg_num) {
          return fail("[APP classpath mismatch, actual: -Djava.class.path=", appcp);
        } else {
          int i = 0, j = 0;
          while(i < path_seg_num && j < app_seg_num){
            if (app_cps[j]._len != path_cps[i]._len){
              j++;
            } else {
              int c = strncmp(app_cps[j]._start, path_cps[i]._start, app_cps[j]._len);
              if (c == 0) {
                i++;
                j++;
              } else if (c < 0) {
                j++;
              } else {
                break;
              }
            }
          }
          if (i != path_seg_num) {
            return fail("[APP classpath mismatch, actual: -Djava.class.path=", appcp);
          }
        }
      }
    }
    break;
  default:
    return SharedPathsMiscInfo::check(type, path);
  }

  return true;
}

void SharedClassUtil::append_lib_in_bootclasspath(const char* path) {
  if(*path == '\0') return;
  AgentEntry* agent_entry = new AgentEntry(path);
  if (_agentlib_head == NULL) {
    _agentlib_head = agent_entry;
    _agentlib_tail = agent_entry;
  } else {
    _agentlib_tail->set_next(agent_entry);
    _agentlib_tail = agent_entry;
  }
}

bool SharedClassUtil::is_appended_boot_cp(const char* path, int len) {
  AgentEntry* e = _agentlib_head;
  while(e != NULL) {
    if (strncmp(e->name(), path, len) == 0) {
      return true;
    }
    e = e->next();
  }
  return false;
}

void SharedClassUtil::update_shared_classpath(ClassPathEntry *cpe, SharedClassPathEntry* e,
                                              time_t timestamp, long filesize, TRAPS) {
  e->_timestamp = timestamp;
  e->_filesize  = filesize;
  if (UseAppCDS) {
    ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
    SharedClassPathEntryExt* ent = (SharedClassPathEntryExt*)e;
    ResourceMark rm(THREAD);
    jint manifest_size;
    bool isSigned;
    char* manifest = ClassLoaderExt::read_manifest(cpe, &manifest_size, CHECK);
    if (manifest != NULL) {
       ManifestStream* stream = new ManifestStream((u1*)manifest,
                                                  manifest_size);
      isSigned = stream->check_is_signed();
      if (isSigned) {
        ent->_is_signed = true;
      } else {
        // Copy the manifest into the shared archive
        manifest = ClassLoaderExt::read_raw_manifest(cpe, &manifest_size, CHECK);
        Array<u1>* buf = MetadataFactory::new_array<u1>(loader_data,
                                                        manifest_size,
                                                        THREAD);
        char* p = (char*)(buf->data());
        memcpy(p, manifest, manifest_size);
        ent->set_manifest(buf);
        ent->_is_signed = false;
      }
    }
  }
}

void SharedClassUtil::initialize(TRAPS) {
  if (UseSharedSpaces) {
    int size = FileMapInfo::get_number_of_share_classpaths();
    if (size > 0) {
      SystemDictionaryShared::allocate_shared_data_arrays(size, THREAD);
      if (!DumpSharedSpaces) {
        FileMapHeaderExt* header = (FileMapHeaderExt*)FileMapInfo::current_info()->header();
        ClassLoaderExt::init_paths_start_index(header->_app_paths_start_index);
      }
    }
  }

}

bool SharedClassUtil::is_classpath_entry_signed(int classpath_index) {
  assert(classpath_index >= 0, "Sanity");
  SharedClassPathEntryExt* ent = (SharedClassPathEntryExt*)
    FileMapInfo::shared_classpath(classpath_index);
  return ent->_is_signed;
}

void FileMapHeaderExt::populate(FileMapInfo* mapinfo, size_t alignment) {
  FileMapInfo::FileMapHeader::populate(mapinfo, alignment);

  ClassLoaderExt::finalize_shared_paths_misc_info();
  _app_paths_start_index = ClassLoaderExt::app_paths_start_index();

  _verify_local = BytecodeVerificationLocal;
  _verify_remote = BytecodeVerificationRemote;
  _has_ext_or_app_or_cus_classes = ClassLoaderExt::has_ext_or_app_or_cus_classes();
}

bool FileMapHeaderExt::validate() {
  if (UseAppCDS) {
    const char* prop = Arguments::get_property("java.system.class.loader");
    if (prop != NULL) {
      warning("UseAppCDS is disabled because the java.system.class.loader property is specified (value = \"%s\"). "
              "To enable UseAppCDS, this property must be not be set", prop);
      UseAppCDS = false;
    }
  }

  if (!FileMapInfo::FileMapHeader::validate()) {
    return false;
  }

  // For backwards compatibility, we don't check the verification setting
  // if the archive only contains system classes.
  if (_has_ext_or_app_or_cus_classes &&
      ((!_verify_local && BytecodeVerificationLocal) ||
       (!_verify_remote && BytecodeVerificationRemote))) {
    FileMapInfo::fail_continue("The shared archive file was created with less restrictive "
                  "verification setting than the current setting.");
    return false;
  }

  return true;
}
