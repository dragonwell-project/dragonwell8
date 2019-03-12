/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include <string.h>
#include <math.h>
#include <errno.h>
#include "utilities/globalDefinitions.hpp"
#include "memory/allocation.hpp"
#include "runtime/os.hpp"
#include "osContainer_linux.hpp"

#define PER_CPU_SHARES 1024

bool  OSContainer::_is_initialized   = false;
bool  OSContainer::_is_containerized = false;
int   OSContainer::_active_processor_count = 1;
julong _unlimited_memory;

class CgroupSubsystem: CHeapObj<mtInternal> {
 friend class OSContainer;

 private:
    volatile jlong _next_check_counter;

    /* mountinfo contents */
    char *_root;
    char *_mount_point;

    /* Constructed subsystem directory */
    char *_path;

 public:
    CgroupSubsystem(char *root, char *mountpoint) {
      _root = os::strdup(root);
      _mount_point = os::strdup(mountpoint);
      _path = NULL;
      _next_check_counter = min_jlong;
    }

    /*
     * Set directory to subsystem specific files based
     * on the contents of the mountinfo and cgroup files.
     */
    void set_subsystem_path(char *cgroup_path) {
      char buf[MAXPATHLEN+1];
      if (_root != NULL && cgroup_path != NULL) {
        if (strcmp(_root, "/") == 0) {
          int buflen;
          strncpy(buf, _mount_point, MAXPATHLEN);
          buf[MAXPATHLEN-1] = '\0';
          if (strcmp(cgroup_path,"/") != 0) {
            buflen = strlen(buf);
            if ((buflen + strlen(cgroup_path)) > (MAXPATHLEN-1)) {
              return;
            }
            strncat(buf, cgroup_path, MAXPATHLEN-buflen);
            buf[MAXPATHLEN-1] = '\0';
          }
          _path = os::strdup(buf);
        } else {
          if (strcmp(_root, cgroup_path) == 0) {
            strncpy(buf, _mount_point, MAXPATHLEN);
            buf[MAXPATHLEN-1] = '\0';
            _path = os::strdup(buf);
          } else {
            char *p = strstr(cgroup_path, _root);
            if (p != NULL && p == _root) {
              if (strlen(cgroup_path) > strlen(_root)) {
                int buflen;
                strncpy(buf, _mount_point, MAXPATHLEN);
                buf[MAXPATHLEN-1] = '\0';
                buflen = strlen(buf);
                if ((buflen + strlen(cgroup_path) - strlen(_root)) > (MAXPATHLEN-1)) {
                  return;
                }
                strncat(buf, cgroup_path + strlen(_root), MAXPATHLEN-buflen);
                buf[MAXPATHLEN-1] = '\0';
                _path = os::strdup(buf);
              }
            }
          }
        }
      }
    }

    char *subsystem_path() { return _path; }

    bool cache_has_expired() {
      return os::elapsed_counter() > _next_check_counter;
    }

    void set_cache_expiry_time(jlong timeout) {
      _next_check_counter = os::elapsed_counter() + timeout;
    }
};

class CgroupMemorySubsystem: CgroupSubsystem {
 friend class OSContainer;

 private:
    /* Some container runtimes set limits via cgroup
     * hierarchy. If set to true consider also memory.stat
     * file if everything else seems unlimited */
    bool _uses_mem_hierarchy;

 public:
    CgroupMemorySubsystem(char *root, char *mountpoint) : CgroupSubsystem::CgroupSubsystem(root, mountpoint) {
      _uses_mem_hierarchy = false;
    }

    bool is_hierarchical() { return _uses_mem_hierarchy; }
    void set_hierarchical(bool value) { _uses_mem_hierarchy = value; }
};

CgroupMemorySubsystem* memory = NULL;
CgroupSubsystem* cpuset = NULL;
CgroupSubsystem* cpu = NULL;
CgroupSubsystem* cpuacct = NULL;

typedef char * cptr;

PRAGMA_DIAG_PUSH
PRAGMA_FORMAT_NONLITERAL_IGNORED
template <typename T> int subsystem_file_line_contents(CgroupSubsystem* c,
                                              const char *filename,
                                              const char *matchline,
                                              const char *scan_fmt,
                                              T returnval) {
  FILE *fp = NULL;
  char *p;
  char file[MAXPATHLEN+1];
  char buf[MAXPATHLEN+1];
  char discard[MAXPATHLEN+1];
  bool found_match = false;

  if (c == NULL) {
    if (PrintContainerInfo) {
      tty->print_cr("subsystem_file_line_contents: CgroupSubsytem* is NULL");
    }
    return OSCONTAINER_ERROR;
  }
  if (c->subsystem_path() == NULL) {
    if (PrintContainerInfo) {
      tty->print_cr("subsystem_file_line_contents: subsystem path is NULL");
    }
    return OSCONTAINER_ERROR;
  }

  strncpy(file, c->subsystem_path(), MAXPATHLEN);
  file[MAXPATHLEN-1] = '\0';
  int filelen = strlen(file);
  if ((filelen + strlen(filename)) > (MAXPATHLEN-1)) {
    if (PrintContainerInfo) {
      tty->print_cr("File path too long %s, %s", file, filename);
    }
    return OSCONTAINER_ERROR;
  }
  strncat(file, filename, MAXPATHLEN-filelen);
  if (PrintContainerInfo) {
    tty->print_cr("Path to %s is %s", filename, file);
  }
  fp = fopen(file, "r");
  if (fp != NULL) {
    int err = 0;
    while ((p = fgets(buf, MAXPATHLEN, fp)) != NULL) {
      found_match = false;
      if (matchline == NULL) {
        // single-line file case
        int matched = sscanf(p, scan_fmt, returnval);
        found_match = (matched == 1);
      } else {
        // multi-line file case
        if (strstr(p, matchline) != NULL) {
          // discard matchline string prefix
          int matched = sscanf(p, scan_fmt, discard, returnval);
          found_match = (matched == 2);
        } else {
          continue; // substring not found
        }
      }
      if (found_match) {
        fclose(fp);
        return 0;
      } else {
        err = 1;
        if (PrintContainerInfo) {
          tty->print_cr("Type %s not found in file %s", scan_fmt, file);
        }
      }
      if (err == 0 && PrintContainerInfo) {
        tty->print_cr("Empty file %s", file);
      }
    }
  } else {
    if (PrintContainerInfo) {
      tty->print_cr("Open of file %s failed, %s", file, strerror(errno));
    }
  }
  if (fp != NULL)
    fclose(fp);
  return OSCONTAINER_ERROR;
}
PRAGMA_DIAG_POP

#define GET_CONTAINER_INFO(return_type, subsystem, filename,              \
                           logstring, scan_fmt, variable)                 \
  return_type variable;                                                   \
{                                                                         \
  int err;                                                                \
  err = subsystem_file_line_contents(subsystem,                           \
                                     filename,                            \
                                     NULL,                                \
                                     scan_fmt,                            \
                                     &variable);                          \
  if (err != 0)                                                           \
    return (return_type) OSCONTAINER_ERROR;                               \
                                                                          \
  if (PrintContainerInfo)                                                 \
    tty->print_cr(logstring, variable);                                   \
}

#define GET_CONTAINER_INFO_CPTR(return_type, subsystem, filename,         \
                               logstring, scan_fmt, variable, bufsize)    \
  char variable[bufsize];                                                 \
{                                                                         \
  int err;                                                                \
  err = subsystem_file_line_contents(subsystem,                           \
                                     filename,                            \
                                     NULL,                                \
                                     scan_fmt,                            \
                                     variable);                           \
  if (err != 0)                                                           \
    return (return_type) NULL;                                            \
                                                                          \
  if (PrintContainerInfo)                                                 \
    tty->print_cr(logstring, variable);                                   \
}

#define GET_CONTAINER_INFO_LINE(return_type, subsystem, filename,         \
                           matchline, logstring, scan_fmt, variable)      \
  return_type variable;                                                   \
{                                                                         \
  int err;                                                                \
  err = subsystem_file_line_contents(subsystem,                           \
                                filename,                                 \
                                matchline,                                \
                                scan_fmt,                                 \
                                &variable);                               \
  if (err != 0)                                                           \
    return (return_type) OSCONTAINER_ERROR;                               \
                                                                          \
  if (PrintContainerInfo)                                                 \
    tty->print_cr(logstring, variable);                                   \
}


/* init
 *
 * Initialize the container support and determine if
 * we are running under cgroup control.
 */
void OSContainer::init() {
  FILE *mntinfo = NULL;
  FILE *cgroup = NULL;
  char buf[MAXPATHLEN+1];
  char tmproot[MAXPATHLEN+1];
  char tmpmount[MAXPATHLEN+1];
  char *p;
  jlong mem_limit;

  assert(!_is_initialized, "Initializing OSContainer more than once");

  _is_initialized = true;
  _is_containerized = false;

  _unlimited_memory = (LONG_MAX / os::vm_page_size()) * os::vm_page_size();

  if (PrintContainerInfo) {
    tty->print_cr("OSContainer::init: Initializing Container Support");
  }
  if (!UseContainerSupport) {
    if (PrintContainerInfo) {
      tty->print_cr("Container Support not enabled");
    }
    return;
  }

  /*
   * Find the cgroup mount point for memory and cpuset
   * by reading /proc/self/mountinfo
   *
   * Example for docker:
   * 219 214 0:29 /docker/7208cebd00fa5f2e342b1094f7bed87fa25661471a4637118e65f1c995be8a34 /sys/fs/cgroup/memory ro,nosuid,nodev,noexec,relatime - cgroup cgroup rw,memory
   *
   * Example for host:
   * 34 28 0:29 / /sys/fs/cgroup/memory rw,nosuid,nodev,noexec,relatime shared:16 - cgroup cgroup rw,memory
   */
  mntinfo = fopen("/proc/self/mountinfo", "r");
  if (mntinfo == NULL) {
      if (PrintContainerInfo) {
        tty->print_cr("Can't open /proc/self/mountinfo, %s",
                       strerror(errno));
      }
      return;
  }

  while ((p = fgets(buf, MAXPATHLEN, mntinfo)) != NULL) {
    char tmpcgroups[MAXPATHLEN+1];
    char *cptr = tmpcgroups;
    char *token;

    // mountinfo format is documented at https://www.kernel.org/doc/Documentation/filesystems/proc.txt
    if (sscanf(p, "%*d %*d %*d:%*d %s %s %*[^-]- cgroup %*s %s", tmproot, tmpmount, tmpcgroups) != 3) {
      continue;
    }
    while ((token = strsep(&cptr, ",")) != NULL) {
      if (strcmp(token, "memory") == 0) {
        memory = new CgroupMemorySubsystem(tmproot, tmpmount);
      } else if (strcmp(token, "cpuset") == 0) {
        cpuset = new CgroupSubsystem(tmproot, tmpmount);
      } else if (strcmp(token, "cpu") == 0) {
        cpu = new CgroupSubsystem(tmproot, tmpmount);
      } else if (strcmp(token, "cpuacct") == 0) {
        cpuacct= new CgroupSubsystem(tmproot, tmpmount);
      }
    }
  }
  fclose(mntinfo);

  if (memory == NULL) {
    if (PrintContainerInfo) {
      tty->print_cr("Required cgroup memory subsystem not found");
    }
    return;
  }
  if (cpuset == NULL) {
    if (PrintContainerInfo) {
      tty->print_cr("Required cgroup cpuset subsystem not found");
    }
    return;
  }
  if (cpu == NULL) {
    if (PrintContainerInfo) {
      tty->print_cr("Required cgroup cpu subsystem not found");
    }
    return;
  }
  if (cpuacct == NULL) {
    if (PrintContainerInfo) {
      tty->print_cr("Required cgroup cpuacct subsystem not found");
    }
    return;
  }

  /*
   * Read /proc/self/cgroup and map host mount point to
   * local one via /proc/self/mountinfo content above
   *
   * Docker example:
   * 5:memory:/docker/6558aed8fc662b194323ceab5b964f69cf36b3e8af877a14b80256e93aecb044
   *
   * Host example:
   * 5:memory:/user.slice
   *
   * Construct a path to the process specific memory and cpuset
   * cgroup directory.
   *
   * For a container running under Docker from memory example above
   * the paths would be:
   *
   * /sys/fs/cgroup/memory
   *
   * For a Host from memory example above the path would be:
   *
   * /sys/fs/cgroup/memory/user.slice
   *
   */
  cgroup = fopen("/proc/self/cgroup", "r");
  if (cgroup == NULL) {
    if (PrintContainerInfo) {
      tty->print_cr("Can't open /proc/self/cgroup, %s",
                     strerror(errno));
      }
    return;
  }

  while ((p = fgets(buf, MAXPATHLEN, cgroup)) != NULL) {
    char *controllers;
    char *token;
    char *base;

    /* Skip cgroup number */
    strsep(&p, ":");
    /* Get controllers and base */
    controllers = strsep(&p, ":");
    base = strsep(&p, "\n");

    if (controllers == NULL) {
      continue;
    }

    while ((token = strsep(&controllers, ",")) != NULL) {
      if (strcmp(token, "memory") == 0) {
        memory->set_subsystem_path(base);
        jlong hierarchy = uses_mem_hierarchy();
        if (hierarchy > 0) {
          memory->set_hierarchical(true);
        }
      } else if (strcmp(token, "cpuset") == 0) {
        cpuset->set_subsystem_path(base);
      } else if (strcmp(token, "cpu") == 0) {
        cpu->set_subsystem_path(base);
      } else if (strcmp(token, "cpuacct") == 0) {
        cpuacct->set_subsystem_path(base);
      }
    }
  }

  fclose(cgroup);

  // We need to update the amount of physical memory now that
  // command line arguments have been processed.
  if ((mem_limit = memory_limit_in_bytes()) > 0) {
    os::Linux::set_physical_memory(mem_limit);
    if (PrintContainerInfo) {
      tty->print_cr("Memory Limit is: " JLONG_FORMAT, mem_limit);
    }
  }

  _is_containerized = true;

}

const char * OSContainer::container_type() {
  if (is_containerized()) {
    return "cgroupv1";
  } else {
    return NULL;
  }
}

/* uses_mem_hierarchy
 *
 * Return whether or not hierarchical cgroup accounting is being
 * done.
 *
 * return:
 *    A number > 0 if true, or
 *    OSCONTAINER_ERROR for not supported
 */
jlong OSContainer::uses_mem_hierarchy() {
  GET_CONTAINER_INFO(jlong, memory, "/memory.use_hierarchy",
                    "Use Hierarchy is: " JLONG_FORMAT, JLONG_FORMAT, use_hierarchy);
  return use_hierarchy;
}


/* memory_limit_in_bytes
 *
 * Return the limit of available memory for this process.
 *
 * return:
 *    memory limit in bytes or
 *    -1 for unlimited
 *    OSCONTAINER_ERROR for not supported
 */
jlong OSContainer::memory_limit_in_bytes() {
  GET_CONTAINER_INFO(julong, memory, "/memory.limit_in_bytes",
                     "Memory Limit is: " JULONG_FORMAT, JULONG_FORMAT, memlimit);

  if (memlimit >= _unlimited_memory) {
    if (PrintContainerInfo) {
      tty->print_cr("Non-Hierarchical Memory Limit is: Unlimited");
    }
    if (memory->is_hierarchical()) {
      const char* matchline = "hierarchical_memory_limit";
      char* format = "%s " JULONG_FORMAT;
      GET_CONTAINER_INFO_LINE(julong, memory, "/memory.stat", matchline,
                             "Hierarchical Memory Limit is: " JULONG_FORMAT, format, hier_memlimit)
      if (hier_memlimit >= _unlimited_memory) {
        if (PrintContainerInfo) {
          tty->print_cr("Hierarchical Memory Limit is: Unlimited");
        }
      } else {
        return (jlong)hier_memlimit;
      }
    }
    return (jlong)-1;
  }
  else {
    return (jlong)memlimit;
  }
}

jlong OSContainer::memory_and_swap_limit_in_bytes() {
  GET_CONTAINER_INFO(julong, memory, "/memory.memsw.limit_in_bytes",
                     "Memory and Swap Limit is: " JULONG_FORMAT, JULONG_FORMAT, memswlimit);
  if (memswlimit >= _unlimited_memory) {
    if (PrintContainerInfo) {
      tty->print_cr("Non-Hierarchical Memory and Swap Limit is: Unlimited");
    }
    if (memory->is_hierarchical()) {
      const char* matchline = "hierarchical_memsw_limit";
      char* format = "%s " JULONG_FORMAT;
      GET_CONTAINER_INFO_LINE(julong, memory, "/memory.stat", matchline,
                             "Hierarchical Memory and Swap Limit is : " JULONG_FORMAT, format, hier_memlimit)
      if (hier_memlimit >= _unlimited_memory) {
        if (PrintContainerInfo) {
          tty->print_cr("Hierarchical Memory and Swap Limit is: Unlimited");
        }
      } else {
        return (jlong)hier_memlimit;
      }
    }
    return (jlong)-1;
  } else {
    return (jlong)memswlimit;
  }
}

jlong OSContainer::memory_soft_limit_in_bytes() {
  GET_CONTAINER_INFO(julong, memory, "/memory.soft_limit_in_bytes",
                     "Memory Soft Limit is: " JULONG_FORMAT, JULONG_FORMAT, memsoftlimit);
  if (memsoftlimit >= _unlimited_memory) {
    if (PrintContainerInfo) {
      tty->print_cr("Memory Soft Limit is: Unlimited");
    }
    return (jlong)-1;
  } else {
    return (jlong)memsoftlimit;
  }
}

/* memory_usage_in_bytes
 *
 * Return the amount of used memory for this process.
 *
 * return:
 *    memory usage in bytes or
 *    -1 for unlimited
 *    OSCONTAINER_ERROR for not supported
 */
jlong OSContainer::memory_usage_in_bytes() {
  GET_CONTAINER_INFO(jlong, memory, "/memory.usage_in_bytes",
                     "Memory Usage is: " JLONG_FORMAT, JLONG_FORMAT, memusage);
  return memusage;
}

/* memory_max_usage_in_bytes
 *
 * Return the maximum amount of used memory for this process.
 *
 * return:
 *    max memory usage in bytes or
 *    OSCONTAINER_ERROR for not supported
 */
jlong OSContainer::memory_max_usage_in_bytes() {
  GET_CONTAINER_INFO(jlong, memory, "/memory.max_usage_in_bytes",
                     "Maximum Memory Usage is: " JLONG_FORMAT, JLONG_FORMAT, memmaxusage);
  return memmaxusage;
}

/* active_processor_count
 *
 * Calculate an appropriate number of active processors for the
 * VM to use based on these three inputs.
 *
 * cpu affinity
 * cgroup cpu quota & cpu period
 * cgroup cpu shares
 *
 * Algorithm:
 *
 * Determine the number of available CPUs from sched_getaffinity
 *
 * If user specified a quota (quota != -1), calculate the number of
 * required CPUs by dividing quota by period.
 *
 * If shares are in effect (shares != -1), calculate the number
 * of CPUs required for the shares by dividing the share value
 * by PER_CPU_SHARES.
 *
 * All results of division are rounded up to the next whole number.
 *
 * If neither shares or quotas have been specified, return the
 * number of active processors in the system.
 *
 * If both shares and quotas have been specified, the results are
 * based on the flag PreferContainerQuotaForCPUCount.  If true,
 * return the quota value.  If false return the smallest value
 * between shares or quotas.
 *
 * If shares and/or quotas have been specified, the resulting number
 * returned will never exceed the number of active processors.
 *
 * return:
 *    number of CPUs
 */
int OSContainer::active_processor_count() {
  int quota_count = 0, share_count = 0;
  int cpu_count, limit_count;
  int result;

  // We use a cache with a timeout to avoid performing expensive
  // computations in the event this function is called frequently.
  // [See 8227006].
  if (!cpu->cache_has_expired()) {
    if (PrintContainerInfo) {
      tty->print_cr("OSContainer::active_processor_count (cached): %d", OSContainer::_active_processor_count);
    }

    return OSContainer::_active_processor_count;
  }

  cpu_count = limit_count = os::Linux::active_processor_count();
  int quota  = cpu_quota();
  int period = cpu_period();
  int share  = cpu_shares();

  if (quota > -1 && period > 0) {
    quota_count = ceilf((float)quota / (float)period);
    if (PrintContainerInfo) {
      tty->print_cr("CPU Quota count based on quota/period: %d", quota_count);
    }
  }
  if (share > -1) {
    share_count = ceilf((float)share / (float)PER_CPU_SHARES);
    if (PrintContainerInfo) {
      tty->print_cr("CPU Share count based on shares: %d", share_count);
    }
  }

  // If both shares and quotas are setup results depend
  // on flag PreferContainerQuotaForCPUCount.
  // If true, limit CPU count to quota
  // If false, use minimum of shares and quotas
  if (quota_count !=0 && share_count != 0) {
    if (PreferContainerQuotaForCPUCount) {
      limit_count = quota_count;
    } else {
      limit_count = MIN2(quota_count, share_count);
    }
  } else if (quota_count != 0) {
    limit_count = quota_count;
  } else if (share_count != 0) {
    limit_count = share_count;
  }

  result = MIN2(cpu_count, limit_count);
  if (PrintContainerInfo) {
    tty->print_cr("OSContainer::active_processor_count: %d", result);
  }

  // Update the value and reset the cache timeout
  OSContainer::_active_processor_count = result;
  cpu->set_cache_expiry_time(OSCONTAINER_CACHE_TIMEOUT);

  return result;
}

char * OSContainer::cpu_cpuset_cpus() {
  GET_CONTAINER_INFO_CPTR(cptr, cpuset, "/cpuset.cpus",
                     "cpuset.cpus is: %s", "%1023s", cpus, 1024);
  return os::strdup(cpus);
}

char * OSContainer::cpu_cpuset_memory_nodes() {
  GET_CONTAINER_INFO_CPTR(cptr, cpuset, "/cpuset.mems",
                     "cpuset.mems is: %s", "%1023s", mems, 1024);
  return os::strdup(mems);
}

/* cpu_quota
 *
 * Return the number of milliseconds per period
 * process is guaranteed to run.
 *
 * return:
 *    quota time in milliseconds
 *    -1 for no quota
 *    OSCONTAINER_ERROR for not supported
 */
int OSContainer::cpu_quota() {
  GET_CONTAINER_INFO(int, cpu, "/cpu.cfs_quota_us",
                     "CPU Quota is: %d", "%d", quota);
  return quota;
}

int OSContainer::cpu_period() {
  GET_CONTAINER_INFO(int, cpu, "/cpu.cfs_period_us",
                     "CPU Period is: %d", "%d", period);
  return period;
}

/* cpu_shares
 *
 * Return the amount of cpu shares available to the process
 *
 * return:
 *    Share number (typically a number relative to 1024)
 *                 (2048 typically expresses 2 CPUs worth of processing)
 *    -1 for no share setup
 *    OSCONTAINER_ERROR for not supported
 */
int OSContainer::cpu_shares() {
  GET_CONTAINER_INFO(int, cpu, "/cpu.shares",
                     "CPU Shares is: %d", "%d", shares);
  // Convert 1024 to no shares setup
  if (shares == 1024) return -1;

  return shares;
}

