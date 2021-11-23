/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "precompiled.hpp"

#include "classfile/classLoaderData.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "compiler/compileBroker.hpp"
#include "jwarmup/jitWarmUp.hpp"
#include "jwarmup/jitWarmUpThread.hpp"
#include "oops/method.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/arguments.hpp"
#include "runtime/compilationPolicy.hpp"
#include "runtime/fieldType.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"
#include "runtime/thread.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/stack.hpp"
#include "utilities/stack.inline.hpp"
#include "runtime/atomic.hpp"
#include "jwarmup/jitWarmUpLog.hpp"  // must be last one to use customized jwarmup log

#define JITWARMUP_VERSION  0x2

JitWarmUp*                JitWarmUp::_instance         = NULL;

JitWarmUp::JitWarmUp()
  : _state(NOT_INIT),
    _version(JITWARMUP_VERSION),
    _dummy_method(NULL),
    _recorder(NULL),
    _preloader(NULL),
    _excluding_matcher(NULL) {
}

JitWarmUp::~JitWarmUp() {
  delete _recorder;
  delete _preloader;
}

JitWarmUp* JitWarmUp::create_instance() {
  _instance = new JitWarmUp();
  return _instance;
}

// JitWarmUp Init functions
JitWarmUp::JitWarmUpState JitWarmUp::init_for_recording() {
  assert(CompilationWarmUpRecording && !CompilationWarmUp, "JVM option verify failure");
  _recorder = new ProfileRecorder();
  _recorder->set_holder(this);
  _recorder->init();
  if (CompilationWarmUpRecordTime > 0) {
    // use a thread to flush info
    JitWarmUpFlushThread::spawn_wait_for_flush(CompilationWarmUpRecordTime);
  }
  if (_recorder->is_valid()) {
    _state = JitWarmUp::IS_OK;
  } else  {
    _state = JitWarmUp::IS_ERR;
  }
  return _state;
}

JitWarmUp::JitWarmUpState JitWarmUp::init_for_warmup() {
  assert(!CompilationWarmUpRecording && CompilationWarmUp, "JVM option verify");
  if (CompilationWarmUpExclude != NULL) {
    _excluding_matcher = new (ResourceObj::C_HEAP, mtClass) SymbolMatcher<mtClass>(CompilationWarmUpExclude);
  }
  if (CompilationWarmUpExplicitDeopt && CompilationWarmUpDeoptTime > 0) {
    log_warning(warmup)("[JitWarmUp] WARNING : CompilationWarmUpDeoptTime is unused when CompilationWarmUpExplicitDeopt is enable");
  }
  _preloader = new PreloadJitInfo();
  _preloader->set_holder(this);
  _preloader->init();
  if (_preloader->is_valid()) {
    _state = JitWarmUp::IS_OK;
  } else  {
    _state = JitWarmUp::IS_ERR;
  }
  return _state;
}

// init JitWarmUp module
void JitWarmUp::init() {
  if (CompilationWarmUp) {
    init_for_warmup();
  } else if(CompilationWarmUpRecording) {
    init_for_recording();
  }
  // check valid
  if ((CompilationWarmUpRecording || CompilationWarmUp) && !JitWarmUp::is_valid()) {
    log_error(warmup)("[JitWarmUp] ERROR: init error.");
    vm_exit(-1);
  }
}

JitWarmUp::JitWarmUpState JitWarmUp::flush_logfile() {
  // state in error
  if(_state == IS_ERR) {
    return _state;
  }
  _recorder->flush();
  if (_recorder->is_valid()) {
    _state = IS_OK;
  } else {
    _state = IS_ERR;
  }
  return _state;
}

bool JitWarmUp::commit_compilation(methodHandle m, int bci, TRAPS) {
  // use C2 compiler
  int comp_level = CompLevel_full_optimization;
  if (CompilationPolicy::can_be_compiled(m, comp_level)) {
    // Force compilation
    CompileBroker::compile_method(m, bci, comp_level,
                                  methodHandle(), 0,
                                  "JitWarmUp", THREAD);
    return true;
  }
  return false;
}

Symbol* JitWarmUp::get_class_loader_name(ClassLoaderData* cld) {
  Handle class_loader(Thread::current(), cld->class_loader());
  Symbol* loader_name = NULL;
  if (class_loader() != NULL) {
    loader_name = PreloadJitInfo::remove_meaningless_suffix(class_loader()->klass()->name());
  } else {
    loader_name = SymbolTable::new_symbol("NULL", Thread::current());
  }
  return loader_name;
}

ProfileRecorder::ProfileRecorder()
  : _holder(NULL),
    _logfile(NULL),
    _pos(0),
    _state(NOT_INIT),
    _class_init_list(NULL),
    _init_list_tail_node(NULL),
    _dict(NULL),
    _class_init_order_count(-1),
    _flushed(false),
    _logfile_name(NULL),
    _max_symbol_length(0) {
}

ProfileRecorder::~ProfileRecorder() {
  if (!CompilationWarmUpLogfile) {
    os::free((void*)logfile_name());
  }
  delete _class_init_list;
}

#define PROFILE_RECORDER_HT_SIZE  10240

void ProfileRecorder::init() {
  assert(_state == NOT_INIT, "state error");
  if (CompilationWarmUp) {
    log_error(warmup)("[JitWarmUp] ERROR: you can not set both CompilationWarmUp and CompilationWarmUpRecording");
    _state = IS_ERR;
    return;
  }
  if (!ProfileInterpreter) {
    log_error(warmup)("[JitWarmUp] ERROR: flag ProfileInterpreter must be on");
    _state = IS_ERR;
    return;
  }
  // disable class unloading
  if (ClassUnloading) {
    log_error(warmup)("[JitWarmUp] ERROR: flag ClassUnloading must be off");
    _state = IS_ERR;
    return;
  }
  if (UseConcMarkSweepGC && CMSClassUnloadingEnabled) {
    log_error(warmup)("[JitWarmUp] ERROR: if use CMS gc, flag CMSClassUnloadingEnabled must be off");
    _state = IS_ERR;
    return;
  }
  if (UseG1GC && ClassUnloadingWithConcurrentMark) {
    log_error(warmup)("[JitWarmUp] ERROR: if use G1 gc, flag ClassUnloadingWithConcurrentMark must be off");
    _state = IS_ERR;
    return;
  }
  // check class data sharing
  if (UseSharedSpaces) {
    log_error(warmup)("[JitWarmUp] ERROR: flag UseSharedSpaces must be off");
    _state = IS_ERR;
    return;
  }
  // log file name
  if (CompilationWarmUpLogfile == NULL) {
    char* buf = (char*)os::malloc(100, mtInternal);
    char fmt[] = "jwarmup_%p.profile";
    Arguments::copy_expand_pid(fmt, sizeof(fmt), buf, 100);
    _logfile_name = buf;
  } else {
    _logfile_name = CompilationWarmUpLogfile;
  }

  _class_init_list = new (ResourceObj::C_HEAP, mtInternal) LinkedListImpl<ClassSymbolEntry>();
  _dict = new ProfileRecordDictionary(PROFILE_RECORDER_HT_SIZE);
  _state = IS_OK;

  log_debug(warmup)("[JitWarmUp] begin to collect, log file is %s", logfile_name());
}

int ProfileRecorder::assign_class_init_order(InstanceKlass* klass) {
  // ignore anonymous class
  if (klass->is_anonymous()) {
    return -1;
  }
  Symbol* name = klass->name();
  Symbol* path = klass->source_file_path();
  Symbol* loader_name = JitWarmUp::get_class_loader_name(klass->class_loader_data());
  if (name == NULL || name->utf8_length() == 0) {
      return -1;
  }
  MutexLockerEx mu(ProfileRecorder_lock);
  if (_init_list_tail_node == NULL) {
    // add head node
    _class_init_list->add(ClassSymbolEntry(name, loader_name, path));
    _init_list_tail_node = _class_init_list->head();
  } else {
    _class_init_list->insert_after(ClassSymbolEntry(name, loader_name, path),
                                   _init_list_tail_node);
    _init_list_tail_node = _init_list_tail_node->next();
  }
  _class_init_order_count++;
#ifndef PRODUCT
  klass->set_initialize_order(_class_init_order_count);
#endif
  return _class_init_order_count;
}

void ProfileRecorder::add_method(Method* m, int bci) {
  MutexLockerEx mu(ProfileRecorder_lock, Mutex::_no_safepoint_check_flag);
  // if is flushed, stop adding method
  if (flushed()) {
    return;
  }
  // not deal with OSR Compilation
  if (bci != InvocationEntryBci) {
    return;
  }
  assert(is_valid(), "JitWarmUp state must be OK");
  unsigned int hash = compute_hash(m);
  dict()->add_method(hash, m, bci);
}

// NYI
void ProfileRecorder::remove_method(Method* m) {
  ShouldNotCallThis();
}

void ProfileRecorder::update_max_symbol_length(int len) {
  if (len > _max_symbol_length) {
    _max_symbol_length = len;
  }
}

ProfileRecordDictionary::ProfileRecordDictionary(unsigned int size)
  : Hashtable<Method*, mtInternal>(size, sizeof(ProfileRecorderEntry)),
    _count(0) {
  // do nothing
}

ProfileRecordDictionary::~ProfileRecordDictionary() {
  free_buckets();
}

ProfileRecorderEntry* ProfileRecordDictionary::new_entry(unsigned int hash, Method* method) {
  ProfileRecorderEntry* entry = (ProfileRecorderEntry*)Hashtable<Method*, mtInternal>::new_entry(hash, method);
  entry->init();
  return entry;
}

ProfileRecorderEntry* ProfileRecordDictionary::add_method(unsigned int hash, Method* method, int bci) {
  // this method should be called after a compilation task done
  assert_lock_strong(ProfileRecorder_lock);
  int index = hash_to_index(hash);
  ProfileRecorderEntry* entry = find_entry(hash, method);
  if (entry != NULL) {
    return entry;
  }
  // not existed
  entry = new_entry(hash, method);
  entry->set_bci(bci);
  entry->set_order(count());
  add_entry(index, entry);
  _count++;
  return entry;
}

ProfileRecorderEntry* ProfileRecordDictionary::find_entry(unsigned int hash, Method* method) {
  int index = hash_to_index(hash);
  for (ProfileRecorderEntry* p = bucket(index); p != NULL; p = p->next()) {
    if (p->literal() == method) {
      return p;
    }
  }
  return NULL;
}

void ProfileRecordDictionary::free_entry(ProfileRecorderEntry* entry) {
  Hashtable<Method*, mtInternal>::free_entry(entry);
}

// head section macro defines

// offset section
#define VERSION_OFFSET                  0
#define MAGIC_NUMBER_OFFSET             4
#define FILE_SIZE_OFFSET                8
#define CRC32_OFFSET                    12
#define APPID_OFFSET                    16
#define MAX_SYMBOL_LENGTH_OFFSET        20
#define RECORD_COUNT_OFFSET             24
#define TIME_OFFSET                     28

#define HEADER_SIZE                     36

// width section
#define VERSION_WIDTH (MAGIC_NUMBER_OFFSET - VERSION_OFFSET)
#define MAGIC_WIDTH (FILE_SIZE_OFFSET - MAGIC_NUMBER_OFFSET)
#define FILE_SIZE_WIDTH (CRC32_OFFSET - FILE_SIZE_OFFSET)
#define CRC32_WIDTH (APPID_OFFSET - CRC32_OFFSET)
#define APPID_WIDTH (MAX_SYMBOL_LENGTH_OFFSET - APPID_OFFSET)
#define MAX_SYMBOL_LENGTH_WIDTH (RECORD_COUNT_OFFSET - MAX_SYMBOL_LENGTH_OFFSET)
#define RECORD_COUNTS_WIDTH (TIME_OFFSET - RECORD_COUNT_OFFSET)
#define TIME_WIDTH (HEADER_SIZE - TIME_OFFSET)

// default value
#define MAGIC_NUMBER                    0xBABA
#define FILE_DEFAULT_NUMBER             0
#define CRC32_DEFAULT_NUMBER            0


static char record_buf[12];
void ProfileRecorder::write_u1(u1 value) {
  *(u1*)record_buf = value;
  _logfile->write(record_buf, 1);
  _pos += 1;
}

void ProfileRecorder::write_u4(u4 value) {
  *(u4*)record_buf = value;
  _logfile->write(record_buf, 4);
  _pos += 4;
}

void ProfileRecorder::overwrite_u4(u4 value, unsigned int offset) {
  *(u4*)record_buf = value;
  _logfile->write(record_buf, 4, offset);
}

void ProfileRecorder::write_u8(u8 value) {
  *(u8*)record_buf = value;
  _logfile->write(record_buf, 8);
  _pos += 8;
}

void ProfileRecorder::write_string(const char* src, size_t len) {
  assert(src != NULL && len != 0, "empty string is not allowed");
  _logfile->write(src, len);
  _logfile->write("\0", 1);
  _pos += (unsigned int)len + 1;
  update_max_symbol_length((int)len);
}

void ProfileRecorder::write_string(const char* src) {
  write_string(src, ::strlen(src));
}

#define JVM_DEFINE_CLASS_PATH "_JVM_DefineClass_"

#define CRC32_BUF_SIZE   1024
static char crc32_buf[CRC32_BUF_SIZE];

int ProfileRecorder::compute_crc32(randomAccessFileStream* fs) {
  long old_position = (long)fs->ftell();
  fs->fseek(HEADER_SIZE, SEEK_SET);
  int content_size = fs->fileSize() - HEADER_SIZE;
  assert(content_size > 0, "sanity check");
  int loops = content_size / CRC32_BUF_SIZE;
  int rest_size = content_size % CRC32_BUF_SIZE;
  int crc = 0;

  for (int i = 0; i < loops; ++i) {
    fs->read(crc32_buf, CRC32_BUF_SIZE, 1);
    crc = ClassLoader::crc32(crc, crc32_buf, CRC32_BUF_SIZE);
  }
  if (rest_size > 0) {
    fs->read(crc32_buf, rest_size, 1);
    crc = ClassLoader::crc32(crc, crc32_buf, rest_size);
  }
  // reset
  fs->fseek(old_position, SEEK_SET);

  return crc;
}
#undef CRC32_BUF_SIZE


// buffer used in write_header()
static char header_buf[HEADER_SIZE];
void ProfileRecorder::write_header() {
  assert(_logfile->is_open(), "");
  // header info
  size_t offset = 0;
  // version number
  *(unsigned int*)header_buf = holder()->version();
  _pos += VERSION_WIDTH;
  offset += VERSION_WIDTH;
  // magic number
  *(unsigned int*)((char*)header_buf + offset) = MAGIC_NUMBER;
  _pos += MAGIC_WIDTH;
  offset += MAGIC_WIDTH;
  // file size
  *(unsigned int*)((char*)header_buf + offset) = FILE_DEFAULT_NUMBER;
  _pos += CRC32_WIDTH;
  offset += CRC32_WIDTH;
  // crc32
  *(unsigned int*)((char*)header_buf + offset) = CRC32_DEFAULT_NUMBER;
  _pos += CRC32_WIDTH;
  offset += CRC32_WIDTH;

  // App id
  *(unsigned int*)((char*)header_buf + offset) = CompilationWarmUpAppID;
  _pos += APPID_WIDTH;
  offset += APPID_WIDTH;

  // max symbol length
  *(unsigned int*)((char*)header_buf + offset) = 0;
  _pos += MAX_SYMBOL_LENGTH_WIDTH;
  offset += MAX_SYMBOL_LENGTH_WIDTH;

  // record counts
  *(unsigned int*)((char*)header_buf + offset) = recorded_count();
  _pos += RECORD_COUNTS_WIDTH;
  offset += RECORD_COUNTS_WIDTH;
  // record time
  *(jlong*)((char*)header_buf + offset) = os::javaTimeMillis();
  _pos += TIME_WIDTH;
  offset += TIME_WIDTH;
  // write to file
  _logfile->write(header_buf, offset);
}

// write class initialize order section
void ProfileRecorder::write_inited_class() {
  assert(_logfile->is_open(), "log file must be opened");
  ResourceMark rm;
  unsigned int begin_pos = _pos;
  unsigned int size_anchor = begin_pos;
  // size place holder
  write_u4((u4)MAGIC_NUMBER);
  // class init order, beginning from -1
  write_u4((u4)class_init_count());
  int cnt = 0;
  const LinkedListNode<ClassSymbolEntry>* node = class_init_list()->head();
  while (node != NULL) {
    const ClassSymbolEntry* entry = node->peek();
    char* class_name = entry->class_name()->as_C_string();
    const char* class_loader_name = NULL;
    if (entry->class_loader_name() == NULL) {
      class_loader_name = "NULL";
    } else {
      class_loader_name = entry->class_loader_name()->as_C_string();
    }
    const char* path = NULL;
    if (entry->path() == NULL) {
      path = JVM_DEFINE_CLASS_PATH;
    } else {
      path = entry->path()->as_C_string();
    }
    write_string(class_name, strlen(class_name));
    write_string(class_loader_name, strlen(class_loader_name));
    write_string(path, strlen(path));
    node = node->next();
    cnt++;
  }
  assert(cnt == class_init_count(), "error happened in profile info record");
  unsigned int end_pos = _pos;
  unsigned int section_size = end_pos - begin_pos;
  overwrite_u4(section_size, size_anchor);
}

// write profile information
void ProfileRecorder::write_record(Method* method, int bci, int order) {
  ResourceMark rm;
  unsigned int begin_pos = _pos;
  unsigned int total_size = 0;
  ConstMethod* cm = method->constMethod();
  MethodCounters* mc = method->method_counters();
  InstanceKlass* klass = cm->constants()->pool_holder();

  unsigned int size_anchor = begin_pos;
  // size place holder
  write_u4((u4)MAGIC_NUMBER);
  write_u4((u4)order);

  // write compilation type
  u1 compilation_type = bci == -1 ? 0 : 1;
  write_u1(compilation_type);

  // write method info
  char* method_name = method->name()->as_C_string();
  write_string(method_name, strlen(method_name));
  char* method_sig = method->signature()->as_C_string();
  write_string(method_sig, strlen(method_sig));
  // first invoke init order
  write_u4((u4)method->first_invoke_init_order());
  // bytecode size
  write_u4((u4)cm->code_size());
  int method_hash = hashstr2((char *)(cm->code_base()), cm->code_size());
  write_u4((u4)method_hash);
  write_u4((u4)bci);

  // write class info
  char* class_name = klass->name()->as_C_string();
  Symbol* path_sym = klass->source_file_path();
  const char* path = NULL;
  if (path_sym != NULL) {
    path = path_sym->as_C_string();
  } else {
    path = JVM_DEFINE_CLASS_PATH;
  }
  oop class_loader = klass->class_loader();
  const char* loader_name = NULL;
  if (class_loader != NULL) {
    loader_name = class_loader->klass()->name()->as_C_string();
  } else {
    loader_name = "NULL";
  }
  write_string(class_name, strlen(class_name));
  write_string(loader_name, strlen(loader_name));
  write_string(path, strlen(path));
  write_u4((u4)klass->bytes_size());
  write_u4((u4)klass->crc32());
  write_u4((u4)0x00); // class hash field is reserved, not used yet

  // method counters
  if (mc!=NULL) {
    write_u4((u4)mc->interpreter_invocation_count());
    write_u4((u4)mc->interpreter_throwout_count());
    write_u4((u4)mc->invocation_counter()->raw_counter());
    write_u4((u4)mc->backedge_counter()->raw_counter());
  } else {
    log_warning(warmup)("[JitWarmUp] WARNING : method counter is NULL for method %s::%s %s",
                        class_name, method_name, method_sig);
    write_u4((u4)0);
    write_u4((u4)0);
    write_u4((u4)0);
    write_u4((u4)0);
  }

  unsigned int end_pos = _pos;
  unsigned int section_size = end_pos - begin_pos;
  overwrite_u4(section_size, size_anchor);
}

void ProfileRecorder::write_footer() {
}

void ProfileRecorder::flush() {
  MutexLockerEx mu(ProfileRecorder_lock);
  if (!is_valid() || flushed()) {
    return;
  }
  set_flushed(true);

  // open randomAccessFileStream
  _logfile = new (ResourceObj::C_HEAP, mtInternal) randomAccessFileStream(logfile_name(), "wb+");
  if (_logfile == NULL || !_logfile->is_open()) {
    log_error(warmup)("[JitWarmUp] ERROR : open log file error! path is %s", logfile_name());
    _state = IS_ERR;
    return;
  }

  // head section
  write_header();
  // write class init section
  write_inited_class();
  // write method profile info
  for (int index = 0; index < dict()->table_size(); index++) {
    for (ProfileRecorderEntry* entry = dict()->bucket(index);
                               entry != NULL;
                               entry = entry->next()) {
      write_record(entry->literal(), entry->bci(), entry->order());
    }
  }
  // foot section
  write_footer();

  // set file size
  overwrite_u4((u4)_pos, FILE_SIZE_OFFSET);
  // set max symbol length
  overwrite_u4((u4)_max_symbol_length, MAX_SYMBOL_LENGTH_OFFSET);
  // compute and set file's crc32
  int crc32 = ProfileRecorder::compute_crc32(_logfile);
  overwrite_u4((u4)crc32, CRC32_OFFSET);

  _logfile->flush();
  // close fd
  delete _logfile;
  _logfile = NULL;

  log_info(warmup)("[JitWarmUp] output profile info has done, file is %s", logfile_name());
}

// =========================== Preload Class Module ========================== //

PreloadClassDictionary::PreloadClassDictionary(int size)
  : Hashtable<Symbol*, mtInternal>(size, sizeof(PreloadClassEntry)) {
  // do nothing
}

PreloadClassDictionary::~PreloadClassDictionary() {  }

PreloadClassEntry* PreloadClassDictionary::new_entry(Symbol* symbol) {
  unsigned int hash = symbol->identity_hash();
  PreloadClassEntry* entry = (PreloadClassEntry*)Hashtable<Symbol*, mtInternal>::
                             new_entry(hash, symbol);
  entry->init();
  return entry;
}

// NYI
void PreloadClassDictionary::remove_entry(unsigned int hash_value,
                                          unsigned int class_size,
                                          unsigned int crc32,
                                          Symbol* symbol) {
  ShouldNotCallThis();
}

PreloadClassEntry* PreloadClassDictionary::find_entry(InstanceKlass* k) {
  Symbol* name = k->name();
  Symbol* path = k->source_file_path();
  if (path == NULL) {
    path = SymbolTable::new_symbol(JVM_DEFINE_CLASS_PATH, Thread::current());
  }
  Symbol* loader_name = JitWarmUp::get_class_loader_name(k->class_loader_data());
  int hash = name->identity_hash();
  return find_entry(hash, name, loader_name, path);
}

PreloadClassEntry* PreloadClassDictionary::find_entry(unsigned int hash_value,
                                                      Symbol* name,
                                                      Symbol* loader_name,
                                                      Symbol* path) {
  int index = hash_to_index(hash_value);
  for (PreloadClassEntry* p = bucket(index); p != NULL; p = p->next()) {
    if (p->literal()->fast_compare(name) == 0 &&
        p->loader_name()->fast_compare(loader_name) == 0 &&
        p->path()->fast_compare(path) == 0) {
      return p;
    }
  }
  // not found
  return NULL;
}

PreloadClassEntry* PreloadClassDictionary::find_head_entry(unsigned int hash_value,
                                                           Symbol* name) {
  int index = hash_to_index(hash_value);
  for (PreloadClassEntry* p = bucket(index); p != NULL; p = p->next()) {
    if (p->literal()->fast_compare(name) == 0) {
      return p;
    }
  }
  // not found
  return NULL;
}

PreloadClassHolder* PreloadClassDictionary::find_holder(unsigned int hash_value,
                                                        unsigned int class_size,
                                                        unsigned int crc32,
                                                        Symbol* symbol,
                                                        Symbol* loader_name,
                                                        Symbol* path) {
  PreloadClassEntry* entry = find_entry(hash_value, symbol, loader_name, path);
  assert(entry != NULL, "JitWarmUp log file error");
  return entry->find_holder_in_entry(class_size, crc32);
}


PreloadClassHolder* PreloadClassEntry::find_holder_in_entry(unsigned int size,
                                                            unsigned int crc32) {
  for (PreloadClassHolder* p = this->head_holder(); p != NULL; p = p->next()) {
    if (p->crc32() == crc32 && p->size() == size) {
      return p;
    }
  }
  // not found
  return NULL;
}

PreloadMethodHolder* PreloadMethodHolder::clone_and_add() {
  PreloadMethodHolder* clone = new PreloadMethodHolder(*this);
  clone->set_next(_next);
  _next = clone;
  return clone;
}

PreloadClassEntry* PreloadClassDictionary::find_and_add_class_entry(unsigned int hash_value,
                                                                    Symbol* name,
                                                                    Symbol* loader_name,
                                                                    Symbol* path,
                                                                    int index) {
  PreloadClassEntry* p = find_entry(hash_value, name, loader_name, path);
  if (p == NULL) {
    p = new_entry(name);
    p->set_chain_offset(index);
    p->set_loader_name(loader_name);
    p->set_path(path);
    add_entry(hash_to_index(hash_value), p);
  }
  return p;
}

PreloadMethodHolder::PreloadMethodHolder(Symbol* name, Symbol* signature)
  : _name(name),
    _signature(signature),
    _size(0),
    _hash(0),
    _intp_invocation_count(0),
    _intp_throwout_count(0),
    _invocation_count(0),
    _backage_count(0),
    _mounted_offset(-1),
    _owns_md_list(true),
    _is_deopted(false),
    _next(NULL),
    _resolved_method(NULL),
    _md_list(new (ResourceObj::C_HEAP, mtClass)
             GrowableArray<MDRecordInfo*>(16, true, mtClass)) {
  // do nothing
}

PreloadMethodHolder::PreloadMethodHolder(PreloadMethodHolder& rhs)
  : _name(rhs._name),
    _signature(rhs._signature),
    _size(rhs._size),
    _hash(rhs._hash),
    _intp_invocation_count(rhs._intp_invocation_count),
    _intp_throwout_count(rhs._intp_throwout_count),
    _invocation_count(rhs._invocation_count),
    _backage_count(rhs._backage_count),
    _mounted_offset(rhs._mounted_offset),
    _owns_md_list(false),
    _is_deopted(false),
    _next(NULL),
    _resolved_method(NULL),
    _md_list(rhs._md_list) {
}

PreloadMethodHolder::~PreloadMethodHolder() {
  if (_owns_md_list) {
    delete _md_list;
  }
}

bool PreloadMethodHolder::check_matching(Method* method) {
  // NYI size and hash not used yet
  if (name()->fast_compare(method->name()) == 0
    && signature()->fast_compare(method->signature()) == 0) {
    return true;
  } else {
    return false;
  }
}

bool PreloadMethodHolder::is_alive(BoolObjectClosure* is_alive_closure) const {
  if (_resolved_method == NULL) {
    return false;
  }
  ClassLoaderData* data = _resolved_method->method_holder()->class_loader_data();
  return data->is_alive(is_alive_closure);
}

PreloadClassHolder::PreloadClassHolder(Symbol* name, Symbol* loader_name,
                                       Symbol* path, unsigned int size,
                                       unsigned int hash, unsigned int crc32)
  : _size(size),
    _hash(hash),
    _crc32(crc32),
    _class_name(name),
    _class_loader_name(loader_name),
    _path(path),
    _method_list(new (ResourceObj::C_HEAP, mtInternal)
                 GrowableArray<PreloadMethodHolder*>(16, true, mtClass)),
    _resolved(false),
    _next(NULL) {
  // do nothing
}


PreloadClassHolder::~PreloadClassHolder() {
  delete _method_list;
}

bool PreloadClassChain::PreloadClassChainEntry::is_all_initialized() {
  int len = resolved_klasses()->length();
  if (len == 0) {
    // no resolved klasses in this entry
    return false;
  }
  for (int i = 0; i < len; i++) {
    InstanceKlass* k = resolved_klasses()->at(i);
    // Thread which invokes notify_application_startup_is_done might be potentially
    // blocked for a long time if class initialization (<clinit>) gets executed
    // for quite a while, e.g : reading from network.
    // we use is_not_initialized() as condition here instead of !is_initialized()
    // to alleviate this case.
    if (k != NULL && k->is_not_initialized() && !k->is_in_error_state() ) {
      return false;
    }
  }
  return true;
}

// warmup record methods when class is loaded,
// if class is redefined between class load and warmup compilation
// the recorded method oop is invalid, so we need check redefined
// class before warmup compilation
bool PreloadClassChain::PreloadClassChainEntry::has_redefined_class() {
  int len = resolved_klasses()->length();
  for (int i = 0; i < len; i++) {
    InstanceKlass* k = resolved_klasses()->at(i);
    if (k != NULL && k->has_been_redefined()) {
      ResourceMark rm;
      log_warning(warmup)("[JitWarmUp] WARNING: ignore redefined class after API"
                          " notifyApplicationStartUpIsDone : %s:%s@%s.", class_name()->as_C_string(),
                          loader_name()->as_C_string(), path()->as_C_string());
      return true;
    }
  }
  return false;
}

InstanceKlass* PreloadClassChain::PreloadClassChainEntry::get_first_uninitialized_klass() {
  int len = resolved_klasses()->length();
  for (int i = 0; i < len; i++) {
    InstanceKlass* k = resolved_klasses()->at(i);
    if (k != NULL && k->is_not_initialized()) {
      return k;
    }
  }
  return NULL;
}

PreloadMethodHolder* MethodHolderIterator::next() {
  PreloadMethodHolder* next_holder = _cur->next();
  if (next_holder != NULL) {
    _cur = next_holder;
    return _cur;
  }
  while (_index > 0) {
    _index--;
    PreloadClassChain::PreloadClassChainEntry* entry = _chain->at(_index);
    if (entry->method_holder() != NULL) {
      _cur = entry->method_holder();
      return _cur;
    }
  }
  _cur = NULL;
  return _cur;
}

PreloadClassChain::PreloadClassChain(unsigned int size)
  : _inited_index(-1),
    _loaded_index(-1),
    _length(size),
    _state(NOT_INITED),
    _entries(new PreloadClassChainEntry[size]),
    _holder(NULL),
    _init_timestamp(),
    _last_timestamp(),
    _deopt_index(-1),
    _deopt_cur_holder(NULL),
    _has_unmarked_compiling_flag(false) {
  _init_timestamp.update();
  _last_timestamp.update();
  state_trans_to(INITED);
}

PreloadClassChain::~PreloadClassChain() {
  delete[] _entries;
}

bool PreloadClassChain::state_trans_to(ClassChainState new_state) {
  ClassChainState old_state = current_state();
  if (old_state == new_state) {
    log_warning(warmup)("JitWarmUp [WARNING]: warmup state already transfer to %d", new_state);
    return true;
  }
  bool can_transfer = false;
  switch (new_state) {
    case ERROR_STATE:
      if (old_state != WARMUP_DEOPTIMIZED) {
        // almost all states could transfer to error state
        // except jwarmup has done all work
        can_transfer = true;
      }
      break;
    default:
      if (new_state == old_state + 1) {
        can_transfer = true;
      }
      break;
  }
  if (can_transfer) {
    if (Atomic::cmpxchg((jint)new_state, (jint*)&_state, (jint)old_state) == old_state) {
      return true;
    } else {
      log_warning(warmup)("JitWarmUp [WARNING]: failed to transfer warmup state from %d to %d, conflict with other operation", old_state, new_state);
      return false;
    }
  } else {
    log_warning(warmup)("JitWarmUp [WARNING]: can not transfer warmup state from %d to %d", old_state, new_state);
    return false;
  }
}

void PreloadClassChain::record_loaded_class(InstanceKlass* k) {
  Symbol* class_name = k->name();
  unsigned int crc32 = k->crc32();
  unsigned int size = k->bytes_size();

  // state check, if warmup compilation was done, no need to record class
  if (!can_record_class()) {
    return;
  }

  PreloadClassEntry* class_entry = holder()->dict()->find_entry(k);
  if (class_entry == NULL) {
    // this class is NOT in jwarmup profile
    return;
  }
  int chain_index = class_entry->chain_offset();
  PreloadClassHolder* holder = class_entry->find_holder_in_entry(size, crc32);
  if (holder != NULL) {
    if (holder->resolved()) {
      // same class may be loaded by different class loaders, now we can not
      // know which one is recorded in profile and should be warmup, the simple
      // solution here is skip duplicated class
      Thread* t = Thread::current();
      if (!t->in_super_class_resolving()) {
        // just print a warning
        assert(k->is_not_initialized(), "klass state error");
        assert(t->is_Java_thread(), "sanity check");
        ResourceMark rm;
        log_warning(warmup)("[JitWarmUp] WARNING : duplicate load class %s at index %d",
                            k->name()->as_C_string(), chain_index);
      }
      return;
    } else {
      // resolve methods in holder
      MutexLockerEx mu(PreloadClassChain_lock);
      int methods = k->methods()->length();
      for (int index = 0; index < methods; index++) {
        Method* m = k->methods()->at(index);
        resolve_method_info(m, holder);
      }
      {
        ResourceMark rm;
        log_debug(warmup)("[JitWarmUp] DEBUG : class %s at index %d is recorded",
                          k->name()->as_C_string(), chain_index);
      }
      holder->set_resolved();
    }
  } else {
    ResourceMark rm;
    log_debug(warmup)("[JitWarmUp] DEBUG : class %s is not in profile",
                      k->name()->as_C_string());
  }
  //set entry to loaded
  {
    // add loaded class to chain
    MutexLockerEx mu(PreloadClassChain_lock);
    assert(chain_index >= 0 && chain_index <= length(), "index out of bound");
    assert(loaded_index() >= inited_index(), "loaded index must larger than inited index");
    PreloadClassChainEntry* chain_entry = &_entries[chain_index];
    // check state
    if (chain_entry->is_skipped()) {
      // the loaded class is skipped to record
      ResourceMark rm;
      log_warning(warmup)("[JitWarmUp] WARNING : loaded class %s at index %d is ignored",
                          k->name()->as_C_string(), chain_index);
      return;
    } else if (chain_entry->is_inited()) {
      // warmup has compiled methods for this entry
      return;
    }
    chain_entry->resolved_klasses()->append(k);
    chain_entry->set_loaded();

    // increase loaded index by chance
    if (chain_index == loaded_index() + 1) {
      update_loaded_index(chain_index);
    }
  } // end of MutexLocker guard
}

void PreloadClassChain::mount_method_at(PreloadMethodHolder* mh, int index) {
  assert(index >= 0 && index < length(), "out of bound");
  PreloadClassChainEntry* entry = &_entries[index];
  entry->add_method_holder(mh);
}

void PreloadClassChain::update_loaded_index(int index) {
  assert(index >= 0 && index < length(), "out of bound");
  // out of bound or entry is not loaded
  while (index < length() && !_entries[index].is_not_loaded()) {
    index++;
  }
  set_loaded_index(index - 1);
}

void PreloadClassChain::compile_methodholders_queue(Stack<PreloadMethodHolder*, mtInternal>& compile_queue) {
  while (!compile_queue.is_empty()) {
    PreloadMethodHolder* pmh = compile_queue.pop();
    compile_methodholder(pmh);
    Thread* THREAD = Thread::current();
    if (HAS_PENDING_EXCEPTION) {
      ResourceMark rm;
      log_warning(warmup)("[JitWarmUp] WARNING: Exceptions happened in compiling %s",
                          pmh->name()->as_C_string());
      // ignore exception occurs during compilation
      CLEAR_PENDING_EXCEPTION;
      continue;
    }
  }
}

void PreloadClassChain::warmup_impl() {
  Thread* THREAD = Thread::current();
  if (!state_trans_to(WARMUP_COMPILING)) {
    log_warning(warmup)("JitWarmUp [WARNING]: invalid state to begin compiling");
    return;
  }

  /* iterate all PreloadClassChainEntry to submit warmup compilation*/
  bool cancel_warmup = false;
  for ( int index = 0; index < length(); index++ ) {
    if (cancel_warmup) {
      break;
    }
    InstanceKlass* klass = NULL;
    // the stack used for storing methods that need be compiled
    Stack<PreloadMethodHolder*, mtInternal> compile_queue;
    {
      MutexLockerEx mu(PreloadClassChain_lock);
      PreloadClassChainEntry *entry = &_entries[index];
      switch(entry->state()) {
        case PreloadClassChainEntry::_not_loaded:
          // skip non-loaded class
          entry->set_skipped();
          {
            ResourceMark rm;
            log_warning(warmup)("[JitWarmUp] WARNING: ignore class because it is not loaded before warmup"
                                " : %s:%s@%s.", entry->class_name()->as_C_string(),
                                entry->loader_name()->as_C_string(), entry->path()->as_C_string());
          }
        case PreloadClassChainEntry::_is_skipped:
          break;
        case PreloadClassChainEntry::_is_loaded:
          // TODO: now only one klass is recorded in resolved_klasses, we can extend to initialize
          //       all recorded classes
          // PreloadClassChain_lock is _safepoint_check_always, we can not initialize class here
          // so defer initialization when mutex is out of scope
          klass = entry->get_first_uninitialized_klass();
          entry->set_inited();
        case PreloadClassChainEntry::_is_inited:
          if (!entry->has_redefined_class()){
            PreloadMethodHolder* mh = entry->method_holder();
            while (mh != NULL) {
              compile_queue.push(mh);
              mh = mh->next();
            }
          }
          break;
        default:
          {
            ResourceMark rm;
            log_error(warmup)("[JitWarmUp] ERROR: class %s has invalid entry state %d.",
                              entry->class_name()->as_C_string(),
                              entry->state());
            return;
          }
      }
    } // end of mutex guard
    // initialize loaded and uninitilaized klass
    if (klass != NULL) {
      // if exception occurs during initialization, just throws back to java code
      assert(THREAD->is_Java_thread(), "sanity check");
      klass->initialize(THREAD);
      if (HAS_PENDING_EXCEPTION) {
        Symbol *loader = JitWarmUp::get_class_loader_name(klass->class_loader_data());
        ResourceMark rm;
        log_error(warmup)("[JitWarmUp] ERROR: Exceptions happened in initializing %s being loaded by %s",
                          klass->name()->as_C_string(), loader->as_C_string());
        return;
      }
    }
    {
      MutexLockerEx mu(PreloadClassChain_lock);
      // update loaded_index and inited_index
      refresh_indexes();
      if (index > inited_index()) {
        // we go beyond inited_index, can not continue
        cancel_warmup = true;
      }
    }
    // compile methods in compile_queue
    compile_methodholders_queue(compile_queue);
  }
}

bool PreloadClassChain::compile_methodholder(PreloadMethodHolder* mh) {
  Thread* t = Thread::current();
  methodHandle m(t, mh->resolved_method());
  if (m() == NULL || m->compiled_by_jwarmup() || m->has_compiled_code()) {
    return false;
  }
  InstanceKlass* klass = m->constants()->pool_holder();
  // skip uninitialized klass
  if (!klass->is_initialized()) {
    return false;
  }

  m->set_compiled_by_jwarmup(true);
  // not deal with osr compilation
  int bci = InvocationEntryBci;
  bool ret = JitWarmUp::commit_compilation(m, bci, t);
  if (ret) {
    ResourceMark rm;
    log_info(warmup)("[JitWarmUp] preload method %s success compiled",
                     m->name_and_sig_as_C_string());
  }
  return ret;
}

void PreloadClassChain::refresh_indexes() {
  assert_lock_strong(PreloadClassChain_lock);
  int loaded = loaded_index();
  int inited = inited_index();
  for (int i = inited + 1; i < length(); i++) {
    PreloadClassChainEntry* e = &_entries[i];
    int len = e->resolved_klasses()->length();
    if (e->is_not_loaded()) {
      assert(len == 0, "wrong state");
    }
    if (e->is_loaded()) {
      assert(len > 0, "class init chain entry state error");
      if (e->is_all_initialized()) {
        e->set_inited();
      }
    }
    // refresh indexes
    if (e->is_loaded() && i == loaded + 1) {
      loaded = i;
    } else if (e->is_inited() && i == inited + 1) {
      loaded = i;   // ???, loaded index may be reduced
      inited = i;
    } else if (e->is_skipped()) {
      if (i == loaded + 1) {
        loaded = i;
      }
      if (i == inited + 1) {
        inited = i;
      }
    } else {
      // break loop
      break;
    }
  } // end of loop
  assert(loaded >= inited, "loaded index must not less than inited index");
  set_loaded_index(loaded);
  set_inited_index(inited);
}

bool PreloadClassChain::should_deoptimize_methods() {
  assert(CompilationWarmUp, "Sanity check");
  assert(SafepointSynchronize::is_at_safepoint(), "must be in safepoint");
  ClassChainState state = current_state();
  if (state == WARMUP_DEOPTIMIZED || state == ERROR_STATE) {
    return false;
  }
  if (!CompilationWarmUpExplicitDeopt && CompilationWarmUpDeoptTime > 0) {
    if (_init_timestamp.seconds() < CompilationWarmUpDeoptTime) {
      return false;
    } else if (state == WARMUP_DONE) {
      state_trans_to(WARMUP_PRE_DEOPTIMIZE);
    } else {
      // do nothing
    }
  }

  if (current_state() != WARMUP_DEOPTIMIZING
      && current_state() != WARMUP_PRE_DEOPTIMIZE) {
    return false;
  }

  Method* dummy_method = JitWarmUp::instance()->dummy_method();
  if (dummy_method == NULL || dummy_method->code() == NULL) {
    // dummy method is not compiled
    return false;
  }

  // deoptimize interval
  if (_last_timestamp.seconds() < CompilationWarmUpDeoptMinInterval) {
    return false;
  }
  // if this safepoint doesn't allow the nested VMOperation, skip it
  VM_Operation* op = VMThread::vm_operation();
  if (op != NULL && !op->allow_nested_vm_operations()) {
    return false;
  }
  // length is too short, maybe log file corrupted
  if (_length <= 1) {
    return false;
  }
  return true;
}

void PreloadClassChain::deopt_prologue() {
  /* 
   * state transition:
   *   inside deopt_prologue, state should be WARMUP_PRE_DEOPTIMIZE or WARMUP_DEOPTIMIZING.
   *   If state is WARMUP_PRE_DEOPTIMIZE, we are first time to enter prologue,
   *     trans state to WARMUP_DEOPTIMIZING
   *   If state is WARMUP_DEOPTIMIZING, do nothing and return
   */
  if (current_state() == WARMUP_PRE_DEOPTIMIZE) {
    // first time we can start deoptimize warmup methods
    if (state_trans_to(WARMUP_DEOPTIMIZING)) {
      log_info(warmup)("JitWarmUp [INFO]: start deoptimize warmup methods");
      _deopt_index = length() - 1;
      while (_deopt_index > 0 && _deopt_cur_holder == NULL) {
        PreloadClassChain::PreloadClassChainEntry* entry = this->at(_deopt_index);
        _deopt_cur_holder = entry->method_holder();
        _deopt_index--;
      }
    } else {
      // we are in vm thread, no conflict with other threads
      ShouldNotReachHere();
    }
  } else {
    guarantee(current_state() == WARMUP_DEOPTIMIZING, "invalid warmup state");
  }
}

void PreloadClassChain::deopt_epilogue() {
  state_trans_to(WARMUP_DEOPTIMIZED);
  log_info(warmup)("JitWarmUp [INFO]: all warmup methods are deoptimized");
}

void PreloadClassChain::invoke_deoptimize_vmop() {
  VM_Deoptimize op;
  VMThread::execute(&op);
}

void PreloadClassChain::deoptimize_methods() {
  assert(SafepointSynchronize::is_at_safepoint(), "must be in safepoint");
  deopt_prologue();

  Method* dummy_method = JitWarmUp::instance()->dummy_method();
  assert( dummy_method != NULL && dummy_method->code() != NULL, "dummy method must be compiled");
  int dummy_compile_id = dummy_method->code()->compile_id();

  MethodHolderIterator iter(this, _deopt_cur_holder, _deopt_index);
  int num = 0;
  while (*iter != NULL) {
    PreloadMethodHolder* pmh = *iter;
    if (pmh->resolved_method() == NULL) {
      iter.next();
      continue;
    }
    methodHandle m(pmh->resolved_method());
#ifndef PRODUCT
    m->set_deopted_by_jwarmup(true);
#endif
    pmh->set_deopted(true);
    if (m->code() != NULL && m->code()->compile_id() > dummy_compile_id) {
      // the method is compiled after warmup, ignore it
      ResourceMark rm;
      log_warning(warmup)("[JitWarmUp] WARNING : skip deoptimize %s because it is compiled after warmup",
                          m->name_and_sig_as_C_string());
      iter.next();
      continue;
    }
    int result = 0;
    if (m->code() != NULL) {
      m->code()->mark_for_deoptimization();
      result++;
    }
    result += CodeCache::mark_for_deoptimization(m());
    if (result > 0) {
      ResourceMark rm;
      log_warning(warmup)("[JitWarmUp] WARNING : deoptimize warmup method %s",
                          m->name_and_sig_as_C_string());
      num++;
    }
    iter.next();
    if (num == (int)CompilationWarmUpDeoptNumOfMethodsPerIter) {
      break;
    }
  }
  if (num > 0) {
    invoke_deoptimize_vmop();
  }

  _last_timestamp.update();

  // update index and cur_holder in PreloadClassChain
  _deopt_index = iter.index();
  _deopt_cur_holder = *iter;

  if (*iter == NULL) {
    deopt_epilogue();
  }
}

void PreloadClassChain::do_unloading(BoolObjectClosure* is_alive) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be in safepoint");
  if (deopt_has_done()) {
    return;
  }
  for (int i = 0; i < length(); i++) {
    PreloadClassChainEntry* entry = this->at(i);
    GrowableArray<InstanceKlass*>* array = entry->resolved_klasses();
    for (int i = 0; i < array->length(); i++) {
      InstanceKlass* k = array->at(i);
      if (k == NULL) {
        continue;
      }
      // reset InstanceKlass pointer
      ClassLoaderData* data = k->class_loader_data();
      if (!data->is_alive(is_alive)) {
        array->remove_at(i);
      }
    }
    for (PreloadMethodHolder* holder = entry->method_holder(); holder != NULL;
         holder = holder->next()) {
      if (holder->deopted()) {
        continue;
      }
      if (!holder->is_alive(is_alive)) {
        // reset Method pointer to NULL
        holder->set_resolved_method(NULL);
      }
    } // end of method holder loop
  } // end of entry loop
}

PreloadMethodHolder* PreloadClassChain::resolve_method_info(Method* method, PreloadClassHolder* holder) {
  // in runtime triggered by class load event
  // FIX ME! O(n) search, should be binary search
  // search for matched method holder that has minimal mounted_offset
  PreloadMethodHolder* mh = NULL;
  for (int i = 0; i < holder->method_list()->length(); i++) {
    PreloadMethodHolder* current_mh = holder->method_list()->at(i);
    if (current_mh->check_matching(method)) {
      mh = current_mh;
      break;
    }
  } // end of method_list loop
  if (mh == NULL) {
    // not found
    return mh;
  } else if (mh->resolved_method() == NULL) {
    mh->set_resolved_method(method);
    return mh;
  } else {
    // if the method has been resolved, it is likely same class are loaded again
    // by different classloader. we clone a new holder for this method and
    // append it to holder list
    PreloadMethodHolder* new_holder = mh->clone_and_add();
    new_holder->set_resolved_method(method);
    return new_holder;
  }
}

// JitWarmUp log parser
class JitWarmUpLogParser : CHeapObj<mtInternal> {
public:
  JitWarmUpLogParser(randomAccessFileStream* fs, PreloadJitInfo* holder);
  virtual ~JitWarmUpLogParser();

  bool valid();

  bool parse_header();
  bool parse_class_init_section();

  bool should_ignore_this_class(Symbol* s);

  // whether method record section parsing is finished
  bool has_next();
  // parse a record from method record section
  // return NULL means the record should be skipped
  // or error occurred in parsing process
  PreloadMethodHolder* next();

  void inc_parsed_number() { _parsed_methods++; }

  int parsed_methods() { return _parsed_methods; }
  int total_methods()  { return _total_methods; }

  long file_size()              { return _file_size; }
  void set_file_size(long size) { _file_size = size; }

  int max_symbol_length() { return _max_symbol_length; }

  PreloadJitInfo* info_holder() { return _holder; }
  void set_info_holder(PreloadJitInfo* holder) { _holder = holder; }

private:
  // disable default constructor
  JitWarmUpLogParser();

  bool                    _is_valid;
  bool                    _has_parsed_header;
  // current position of log file in parsing process
  int                     _position;
  // method count that successful parsed from log file
  int                     _parsed_methods;
  // method count recorded in log file
  int                     _total_methods;
  long                    _file_size;
  randomAccessFileStream* _fs;

  int                     _max_symbol_length;
  char*                   _parse_str_buf;

  PreloadJitInfo*         _holder;
  Arena*                  _arena;

  u1                      read_u1();
  u4                      read_u4();
  u8                      read_u8();
  const char*             read_string();
};

JitWarmUpLogParser::JitWarmUpLogParser(randomAccessFileStream* fs, PreloadJitInfo* holder)
  : _is_valid(false),
    _has_parsed_header(false),
    _position(0),
    _parsed_methods(0),
    _total_methods(0),
    _file_size(0),
    _fs(fs),
    _max_symbol_length(0),
    _parse_str_buf(NULL),
    _holder(holder),
    _arena(new (mtInternal) Arena(mtInternal, 128)) {
}

JitWarmUpLogParser::~JitWarmUpLogParser() {
  // fileStream lifecycle is not managed by this class
  delete _arena;
}

char parse_int_buf[8];
u1 JitWarmUpLogParser::read_u1() {
  _fs->read(parse_int_buf, 1, 1);
  _position += 1;
  return *(u1*)parse_int_buf;
}

u4 JitWarmUpLogParser::read_u4() {
  _fs->read(parse_int_buf, 4, 1);
  _position += 4;
  return *(u4*)parse_int_buf;
}

u8 JitWarmUpLogParser::read_u8() {
  _fs->read(parse_int_buf, 8, 1);
  _position += 8;
  return *(u8*)parse_int_buf;
}

const char* JitWarmUpLogParser::read_string() {
  int index = 0;
  do {
    _fs->read(_parse_str_buf + index, 1, 1);
    index++;
  } while (*(_parse_str_buf + index - 1) != '\0'
          && index <= _max_symbol_length + 1);

  _position += index;
  int len = index - 1;
  if (len == 0) {
    log_warning(warmup)("[JitWarmUp] WARNING : Parsed empty symbol at position %d\n", _position);
    return "";
  } else if (len > max_symbol_length()) {
    log_error(warmup)("[JitWarmUp] ERROR : Parsed symbol length is longer than %d\n", max_symbol_length());
    return NULL;
  } else {
    char* str = NEW_RESOURCE_ARRAY(char, len + 1);
    ::memcpy(str, _parse_str_buf, len + 1);
    return str;
  }
}

#define MAX_COUNT_VALUE (1024 * 1024 * 128)

#define LOGPARSER_ILLEGAL_STRING_CHECK(s, ret_value)                 \
  if (_position > end_pos) {                                         \
    log_error(warmup)("[JitWarmUp] ERROR : read out of bound, "      \
                      "file format error");                          \
    return ret_value;                                                \
  }                                                                  \
  if (s == NULL) {                                                   \
    _position = end_pos;                                             \
    log_error(warmup)("[JitWarmUp] ERROR : illegal string in log file"); \
    return ret_value;                                                \
  }

#define LOGPARSER_ILLEGAL_COUNT_CHECK(cnt, ret_value)                \
  if (_position > end_pos) {                                         \
    log_error(warmup)("[JitWarmUp] ERROR : read out of bound, "      \
                      "file format error");                          \
    return ret_value;                                                \
  }                                                                  \
  if ((u4)cnt > MAX_COUNT_VALUE) {                                   \
    _position = end_pos;                                             \
    log_error(warmup)("[JitWarmUp] ERROR : illegal count ("          \
                      UINT32_FORMAT ") too big", cnt);               \
    return ret_value;                                                \
  }

bool JitWarmUpLogParser::should_ignore_this_class(Symbol* s) {
  // FIXME deal with spring auto-generated
  ResourceMark rm;
  char* name = s->as_C_string();
  const char* CGLIB_SIG = "CGLIB$$";
  const char* ACCESSER_SUFFIX = "ConstructorAccess";
  if (::strstr(name, CGLIB_SIG) != NULL ||
      ::strstr(name, ACCESSER_SUFFIX) != NULL) {
    return true;
  }
  JitWarmUp* jwp = info_holder()->holder();
  SymbolMatcher<mtClass>* matcher = jwp->excluding_matcher();
  if (matcher == NULL) {
    return false;
  }
  return matcher->match(s);
}

bool JitWarmUpLogParser::parse_header() {
  int begin_pos = _position;
  int end_pos = begin_pos + HEADER_SIZE;
  u4 version_number = read_u4();
  u4 magic_number = read_u4();
  u4 file_size = read_u4();
  int crc32_recorded = (int)read_u4();
  u4 appid = read_u4();
  // valid version & magic number & file size
  unsigned int version = JitWarmUp::instance()->version();
  if (version_number != version) {
    _is_valid = false;
    log_error(warmup)("[JitWarmUp] ERROR : Version not match, expect %d but %d", version, version_number);
    return false;
  }
  if (magic_number != MAGIC_NUMBER
      || (long)file_size != this->file_size()) {
    _is_valid = false;
    log_error(warmup)("[JitWarmUp] ERROR : illegal header");
    return false;
  }
  // valid appid
  if (CompilationWarmUpAppID != 0 && CompilationWarmUpAppID != appid) {
    _is_valid = false;
    log_error(warmup)("[JitWarmUp] ERROR : illegal CompilationWarmUpAppID");
    return false;
  }
  // valid crc32
  int crc32_actual = ProfileRecorder::compute_crc32(_fs);
  if (crc32_recorded != crc32_actual) {
    _is_valid = false;
    log_error(warmup)("[JitWarmUp] ERROR : log file crc32 check failure");
    return false;
  }

  u4 max_symbol_length = read_u4();
  LOGPARSER_ILLEGAL_COUNT_CHECK(max_symbol_length, false);
  _parse_str_buf = (char*)_arena->Amalloc(max_symbol_length + 2);
  _max_symbol_length = (int)max_symbol_length;

  u4 record_count = read_u4();
  LOGPARSER_ILLEGAL_COUNT_CHECK(record_count, false);
  _total_methods = record_count;
  u4 utc_time = read_u8();
  _is_valid = true;
  return true;
}

#define CREATE_SYMBOL(char_name)      \
  SymbolTable::new_symbol(char_name, (int)strlen(char_name), Thread::current())

bool JitWarmUpLogParser::parse_class_init_section() {
  ResourceMark rm;
  int begin_pos = _position;
  u4 section_size = read_u4();
  int end_pos = begin_pos + (int)section_size;
  u4 cnt = read_u4();
  LOGPARSER_ILLEGAL_COUNT_CHECK(cnt, false);

  PreloadClassChain* chain = new PreloadClassChain(cnt);
  info_holder()->set_chain(chain);
  chain->set_holder(this->info_holder());

  for (int i = 0; i < (int)cnt; i++) {
    const char* name_char = read_string();
    LOGPARSER_ILLEGAL_STRING_CHECK(name_char, false);
    const char* loader_char = read_string();
    LOGPARSER_ILLEGAL_STRING_CHECK(loader_char, false);
    const char* path_char = read_string();
    LOGPARSER_ILLEGAL_STRING_CHECK(path_char, false);
    Symbol* name = CREATE_SYMBOL(name_char);
    Symbol* loader_name = CREATE_SYMBOL(loader_char);
    Symbol* path = CREATE_SYMBOL(path_char);
    loader_name = PreloadJitInfo::remove_meaningless_suffix(loader_name);
    chain->at(i)->set_class_name(name);
    chain->at(i)->set_loader_name(loader_name);
    chain->at(i)->set_path(path);
    // add to preload class dictionary
    unsigned int hash_value = name->identity_hash();

    PreloadClassEntry* e = info_holder()->dict()->
                           find_and_add_class_entry(hash_value, name, loader_name, path, i);

    // e->chain_offset() < i : means same class symbol already existed in the chain
    // should_ignore_this_class(name): means this class is in skipped list(build-in or user-defined)
    // so set entry state is skipped, will be ignored in JitWarmUp
    if (e->chain_offset() < i || should_ignore_this_class(name)) {
      chain->at(i)->set_skipped();
    } else {
      Symbol* name_no_suffix = PreloadJitInfo::remove_meaningless_suffix(name);
      if (name_no_suffix->fast_compare(name) != 0) {
        unsigned int hash_no_suffix = name_no_suffix->identity_hash();
        PreloadClassEntry* e_no_suffix = info_holder()->dict()->
                               find_and_add_class_entry(hash_no_suffix, name_no_suffix, loader_name, path, i);
        if (e_no_suffix->chain_offset() < i) {
          chain->at(i)->set_skipped();
        }
      }
    }
  } // end of for loop

  // check section size
  if (_position - begin_pos != (int)section_size) {
    log_error(warmup)("[JitWarmUp] ERROR : log file init section parse error.");
    return false;
  }
  return true;
}

bool JitWarmUpLogParser::valid() {
  if(!_has_parsed_header) {
    parse_header();
  }
  return _is_valid;
}

bool JitWarmUpLogParser::has_next() {
  return _parsed_methods < _total_methods && _position < _file_size;
}

PreloadMethodHolder* JitWarmUpLogParser::next() {
  ResourceMark rm;
  _fs->fseek(_position, SEEK_SET);
  int begin_pos = _position;
  u4 section_size = read_u4();
  int end_pos = begin_pos + section_size;

  u4 comp_order = read_u4();
  u1 compilation_type = read_u1();
  // 0 means standard compile
  // 1 means OSR compile
  if (compilation_type != 0 && compilation_type != 1) {
    log_error(warmup)("[JitWarmUp] ERROR : illegal compilation type in log file");
    _position = end_pos;
    return NULL;
  }
  // method info
  const char* method_name_char = read_string();
  LOGPARSER_ILLEGAL_STRING_CHECK(method_name_char, NULL);
  Symbol* method_name = CREATE_SYMBOL(method_name_char);
  const char* method_sig_char = read_string();
  LOGPARSER_ILLEGAL_STRING_CHECK(method_sig_char, NULL);
  Symbol* method_sig = CREATE_SYMBOL(method_sig_char);
  u4 first_invoke_init_order = read_u4();
  // INVALID_FIRST_INVOKE_INIT_ORDER means no first_invoke_init_order record in log file,
  // so put this method at last entry of class init chain
  if ((int)first_invoke_init_order == INVALID_FIRST_INVOKE_INIT_ORDER) {
    first_invoke_init_order = this->info_holder()->chain()->length() - 1;
  }
  u4 method_size = read_u4();
  u4 method_hash = read_u4();
  int32_t bci = (int32_t)read_u4();
  if (bci != InvocationEntryBci) {
    LOGPARSER_ILLEGAL_COUNT_CHECK(bci, NULL);
  }

  // class info
  const char* class_name_char = read_string();
  LOGPARSER_ILLEGAL_STRING_CHECK(class_name_char, NULL);
  Symbol* class_name = CREATE_SYMBOL(class_name_char);
  // ignore
  if (should_ignore_this_class(class_name)) {
    _position = end_pos;
    return NULL;
  }
  const char* class_loader_char = read_string();
  LOGPARSER_ILLEGAL_STRING_CHECK(class_loader_char, NULL);
  Symbol* class_loader = CREATE_SYMBOL(class_loader_char);
  class_loader = PreloadJitInfo::remove_meaningless_suffix(class_loader);
  const char* path_char = read_string();
  LOGPARSER_ILLEGAL_STRING_CHECK(path_char, NULL);
  Symbol* path = CREATE_SYMBOL(path_char);

  PreloadClassDictionary* dict = this->info_holder()->dict();
  unsigned int dict_hash = class_name->identity_hash();
  PreloadClassEntry* entry = dict->find_head_entry(dict_hash, class_name);
  if (entry == NULL) {
    log_warning(warmup)("[JitWarmUp] WARNING : class %s is missed in init section", class_name_char);
    _position = end_pos;
    return NULL;
  }
  u4 class_size = read_u4();
  u4 class_crc32 = read_u4();
  u4 class_hash = read_u4();

  // method counters info
  u4 intp_invocation_count = read_u4();
  u4 intp_throwout_count = read_u4();
  u4 invocation_count = read_u4();
  u4 backedge_count = read_u4();

  int class_chain_offset = entry->chain_offset();
  PreloadClassHolder* holder = entry->find_holder_in_entry(class_size, class_crc32);
  if (holder == NULL) {
      holder = new PreloadClassHolder(class_name, class_loader, path, class_size, class_hash, class_crc32);
      entry->add_class_holder(holder);
  }
  PreloadMethodHolder* mh = new PreloadMethodHolder(method_name, method_sig);
  mh->set_intp_invocation_count(intp_invocation_count);
  mh->set_intp_throwout_count(intp_throwout_count);
  mh->set_invocation_count(invocation_count);
  mh->set_backage_count(backedge_count);
  mh->set_bci((int)bci);

  mh->set_hash(method_hash);
  mh->set_size(method_size);

  // add class init chain relation
  /*
  int method_chain_offset = static_cast<int>(first_invoke_init_order) >= class_chain_offset ? first_invoke_init_order
                            : _holder->chain()->length() -1;
                            */
  int method_chain_offset = class_chain_offset;
  mh->set_mounted_offset(method_chain_offset);
  this->info_holder()->chain()->mount_method_at(mh, method_chain_offset);
  holder->add_method(mh);
  return mh;
}

#undef MAX_SIZE_VALUE
#undef MAX_COUNT_VALUE
#undef LOGPARSER_ILLEGAL_STRING_CHECK
#undef LOGPARSER_ILLEGAL_COUNT_CHECK

#undef CREATE_SYMBOL

#define INIT_PRECLASS_INIT_SIZE 4*1024*1024
#define PRELOAD_CLASS_HS_SIZE   10240

PreloadJitInfo::PreloadJitInfo()
  : _dict(NULL),
    _chain(NULL),
    _loaded_count(0),
    _state(NOT_INIT),
    _holder(NULL),
    _jvm_booted_is_done(false) {
}

PreloadJitInfo::~PreloadJitInfo() {
  delete _dict;
  delete _chain;
}

Symbol* PreloadJitInfo::remove_meaningless_suffix(Symbol* s) {
  ResourceMark rm;
  Thread* t = Thread::current();
  Symbol* result = s;
  char* s_char = s->as_C_string();
  int len = (int)::strlen(s_char);
  // first, remove $$+suffix
  int i = 0;
  for (i = 0; i < len - 1; i++) {
    if (s_char[i] == '$' && s_char[i+1] == '$') {
      break;
    }
  }
  // has $$+suffix, remove it
  if (i < len - 1) {
    // example: s is $$123, convert to $
    i = i == 0 ? i = 1: i;
    result = SymbolTable::new_symbol(s_char, i, t);
    s_char = result->as_C_string();
  }
  // second, remove number or $+number
  len = (int)::strlen(s_char);
  i = len - 1;
  for (; i >= 0; i--) {
    if (s_char[i] >= '0' && s_char[i] <= '9') {
      continue;
    } else if (s_char[i] == '$') {
      continue;
    } else {
      break;
    }
  }
  if (i != len - 1){
    i = i == -1 ? 0 : i;
    result = SymbolTable::new_symbol(s_char, i + 1, t);
  }
  return result;
}

void PreloadJitInfo::jvm_booted_is_done() {
  _jvm_booted_is_done = true;
  PreloadClassChain* chain = this->chain();
  assert(chain != NULL, "PreloadClassChain is NULL");
}

void PreloadClassChain::eager_load_class_in_constantpool() {
  int index = 0;
  int klass_index = 0;
  while (true) {
    InstanceKlass* current_k = NULL;
    {
      MutexLockerEx mu(PreloadClassChain_lock);
      if (index == length()) {
        break;
      }
      PreloadClassChain::PreloadClassChainEntry* e = this->at(index);
      GrowableArray<InstanceKlass*>* array =  e->resolved_klasses();
      assert(array != NULL, "should not be NULL");
      // skip not loaded entry
      if (e->is_skipped() || e->is_not_loaded() || klass_index >= array->length()) {
        index++;
        // reset index
        klass_index = 0;
        continue;
      }
      current_k = array->at(klass_index);
    } // end of Mutex guard

    if (current_k != NULL) {
      current_k->constants()->preload_jwarmup_classes(Thread::current());
    }
    klass_index++;
  } // end of while
}

void PreloadJitInfo::notify_application_startup_is_done() {
  PreloadClassChain *chain = this->chain();
  assert(chain != NULL, "PreloadClassChain is NULL");
  chain->state_trans_to(PreloadClassChain::PRE_WARMUP);

  // 1st, eager load classes, do eager initialize just once
  log_info(warmup)("JitWarmUp [INFO]: start eager loading classes from constant pool");
  chain->eager_load_class_in_constantpool();

  // 2nd, warmup compilation
  log_info(warmup)("JitWarmUp [INFO]: start warmup compilation");
  chain->warmup_impl();
  Thread *THREAD = Thread::current();
  // if exception occurs in warmup compilation, return and throw
  if (HAS_PENDING_EXCEPTION) {
    return;
  }

  // 3rd, commit dummy method for compilation, check dummy method to know warmup compilation is completed
  JitWarmUp *jwp = this->holder();
  Method *dm = jwp->dummy_method();
  guarantee(dm->code() == NULL, "dummy method has been compiled unexceptedly!");
  methodHandle mh(THREAD, dm);
  JitWarmUp::commit_compilation(mh, InvocationEntryBci, THREAD);
  if (!chain->state_trans_to(PreloadClassChain::WARMUP_DONE)) {
    // warmup has been marked as WARMUP_DONE, but compilation requests are still on
    // going. warmup is really done when dummy method is compiled, check the dummy
    // method to determine warmup is done
    log_error(warmup)("JitWarmUp [ERROR]: can not change state to WARMUP_DONE");
  } else {
    log_info(warmup)("JitWarmUp [INFO]: warmup compilation is done");
  }
}

bool PreloadJitInfo::should_load_class_eagerly(Symbol* s) {
  // check black list
  SymbolMatcher<mtClass>* matcher = holder()->excluding_matcher();
  if (matcher != NULL && matcher->match(s)) {
    return false;
  }
  int hash = s->identity_hash();
  PreloadClassEntry* e = dict()->find_head_entry(hash, s);
  // not in JitWarmUp log file
  if (e == NULL) {
    return false;
  }
  if (!CompilationWarmUpResolveClassEagerly) {
    // check whether has been loaded
    int offset = e->chain_offset();
    PreloadClassChain::PreloadClassChainEntry* entry = chain()->at(offset);
    return entry->is_not_loaded();
  } else {
    return true;
  }
}

bool PreloadJitInfo::resolve_loaded_klass(InstanceKlass* k) {
  if (k == NULL) { return false; }
  // has loaded before
  if (k->is_jwarmup_recorded()) {
    return false;
  }
  {
    MutexLockerEx mu(PreloadClassChain_lock);
    // JitWarmUp compilation is done
    if (!chain()->can_record_class()) {
      return false;
    }
  }
  // record in PreloadClassChain
  // set flag before actually invoking record function due to
  // the record_loaded_class may trigger recursive
  // class recording
  k->set_jwarmup_recorded(true);
  chain()->record_loaded_class(k);
  return true;
}

class RandomFileStreamGuard : StackObj {
public:
  RandomFileStreamGuard(randomAccessFileStream* fs)
    : _fs(fs) {
  }

  ~RandomFileStreamGuard() { delete _fs; }

  randomAccessFileStream* operator ->() const { return _fs; }
  randomAccessFileStream* operator ()() const { return _fs; }

private:
  randomAccessFileStream*  _fs;
};

void PreloadJitInfo::init() {
  if (CompilationWarmUpRecording) {
    log_error(warmup)("[JitWarmUp] ERROR: you can not set both CompilationWarmUp and CompilationWarmUpRecording");
    _state = IS_ERR;
    return;
  }
  // check class data sharing
  if (UseSharedSpaces) {
    log_error(warmup)("[JitWarmUp] ERROR: flag UseSharedSpaces must be off");
    _state = IS_ERR;
    return;
  }
  _dict = new PreloadClassDictionary(PRELOAD_CLASS_HS_SIZE);
  _loaded_count = 0;  // init count
  _state = IS_OK; // init state

  // parse log
  if (CompilationWarmUpLogfile == NULL) {
    _state = IS_ERR;
    return;
  }

  RandomFileStreamGuard fsg(new (ResourceObj::C_HEAP, mtInternal) randomAccessFileStream(
                                 CompilationWarmUpLogfile, "rb+"));
  JitWarmUpLogParser parser(fsg(), this);
  if (!fsg->is_open()) {
    log_error(warmup)("[JitWarmUp] ERROR : log file %s doesn't exist", CompilationWarmUpLogfile);
    _state = IS_ERR;
    return;
  }
  parser.set_file_size(fsg->fileSize());
  // parse header section
  if (!parser.parse_header()) {
    // not valid log file format
    _state = IS_ERR;
    return;
  }
  // parse class init section
  if (!parser.parse_class_init_section()) {
    // invalid log file format
    _state = IS_ERR;
    return;
  }
  while (parser.has_next()) {
    PreloadMethodHolder* holder = parser.next();
    if (holder != NULL) {
      //successfully parsed from log file
      ++_loaded_count;
    }
    parser.inc_parsed_number();
  }
}
