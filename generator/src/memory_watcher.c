/** @file memory_watcher.c
 * @ingroup generator
 * @brief Implementation of a memory watcher, which will crash the program if it
 * detects and heap extension.
 * @author Mattia Nicolella
 *
 * **Dependencies**:
 * - Glibc.
 *
 * @copyright (C) 2021 - 2022, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */
#include "optional_features.h"
#define _FILE_OFFSET_BITS 64
#include "logging.h"
#include "memory_watcher.h"
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/// The mask used to extract the present bit from a page table entry.
#define PRES_MASK (1LL << 63)
/// The mask used to extract the page frame number from a page table entry.
#define PFN_MASK ((1LL << 55) - 1)

typedef long long unsigned int u64;

/** @brief Enum used to determine the memory watcher states
 *
 * @bug When state is `::MEMORY_WATCHER_FIXED_HEAP` log levels higher than
 * `::LOG_LEVEL_FILE` might lead to a bus error due to unaligned accesses
 * not being supported by `/dev/mem`.
 */
enum memory_watcher_states {
  MEMORY_WATCHER_DISABLED = 0, ///< The memory watcher is not enabled.
  MEMORY_WATCHER_ENABLED,      ///< The memory watcher is enabled.
  MEMORY_WATCHER_FIXED_HEAP,   ///< The memory watcher is enabled and the heap
                               ///< location is fixed (heap is backed by
                               ///< /dev/mem).
  MEMORY_WATCHER_FILE_HEAP, ///< The memory watcher is enabled and the heap is
                            ///< backed by a file (not /dev/mem).
};

///@brief The current configuration of the memory watcher.
static struct memory_watcher_config {

  /** The status of the memory watcher.
   * Possible values are defined by `::memory_watcher_states`.
   */
  enum memory_watcher_states status;
  mem_watcher_address_t
      /** @brief The initial value of the program break.
       * This value is used to determine if the program heap was expanded
       * after an allocation.
       */
      *initial_program_break,
      /** The current program break when the memory
       * watcher is enabled and the heap location
       *is fixed. */
      *fixed_heap_program_break,
      /// The configured heap start. `NULL` if not configured.
      *heap_start,
      /** The address that is the result of mmapping
       * the file which will back the heap with the `heap_start` offset and
       * `heap_size` length. This will be the address used
       * for allocations. */
      *mapping;
  int heap_fd,      ///< The file pointer that will back the heap.
      pagemap_fd;   ///< The file pointer to the pagemap file.
  size_t heap_size; ///< The configured heap size. `0` if not configured.
} memory_watcher_config = {
    .status = MEMORY_WATCHER_DISABLED,
    .initial_program_break = NULL,
    .fixed_heap_program_break = NULL,
    .heap_start = NULL,
    .heap_size = 0,
    .mapping = NULL,
    .heap_fd = -1,
    .pagemap_fd = -1,
};

/** @brief Print the content of a page table entry.
 * @param[in] map The page table entry to be printed.
 * @details This function will print the raw value of the page table entry, the
 * page frame number and the present bit.
 */
static void print_map(u64 map) {
  elogf(LOG_LEVEL_DEBUG, "\t Raw: 0x%016llx\n", map);
  elogf(LOG_LEVEL_DEBUG, "\t PFN: 0x%016llx\n", map & PFN_MASK);
  elogf(LOG_LEVEL_DEBUG, "\t Present: %d\n", !!(map & PRES_MASK));
}

/**
 * @brief Translate a virtual address to a physical address.
 * @param[in] vaddr The virtual address to be translated.
 * @return 0 on success, -1 on failure.
 * @details This function will use the pagemap file to translate a virtual
 * address to a physical address.
 * The offset in the pagemap file is calculated as `(vaddr >> 12) << 3`, since
 * the page size is 4KB and each entry in the pagemap file is 8 bytes long.
 * The page frame number is extracted from the pagemap entry and printed.
 * The present bit is also printed.
 * If the verbosity level is not `LOG_LEVEL_DEBUG` or the pagemap file has not
 * been initialized, the function will return immediately.
 */
static int translate_va(u64 vaddr) {
  u64 map;

  if (benchmark_verbosity < LOG_LEVEL_DEBUG ||
      memory_watcher_config.pagemap_fd <= 0) {
    return 0;
  }

  if (lseek(memory_watcher_config.pagemap_fd, (vaddr >> 12) << 3, SEEK_SET) <
      0) {
    perror("Unable to lseek in pagemap file");
    return -1;
  }

  if (read(memory_watcher_config.pagemap_fd, &map, 8) < 0) {
    perror("Unable to read pagemap file");
    return -1;
  }

  elogf(LOG_LEVEL_DEBUG, "pointer info:\n");
  print_map(map);

  return 0;
}

/**
 * Memory preallocation is done via `mallopt()`, using `M_TOP_PAD`.
 * In addition, we need to avoid having `malloc()` use `mmap()`, so `mallopt()`
 * is used to set `M_MMAP_MAX` to `0`. Then, a dummy allocation (a `malloc()`
 * and a `free()`) is performed, to have the requested memory preallocated.
 *
 * To enable the memory watcher, `::memory_watcher_config->status` is set to
 * `::MEMORY_WATCHER_ENABLED` and the initial value of the program break is
 * stored in `::memory_watcher_config->initial_program_break` via `sbrk(0)`. As
 * a side effect from the memory watcher start, `mmap()` will be disabled.
 *
 * If `heap_size` is not `NULL`, the memory watcher will also use a cutsom
 * `sbrk()` and `malloc()` perfom allocations starting from that address. In
 * this configuration the default preallocation strategy of the `dlmalloc`
 * implementation is used.
 *
 * `heap_file` can be used to customize the heap location to be a file (e.g.
 * /dev/mem). For this purpose, `heap_start` can be used to specify the offset
 * of the heap in the file.
 *
 * When fixing the heap location make sure to have some extra space available
 * for our `malloc` implementation to use.
 * As an example, consider running the @ref latency benchmark with a fixed heap
 * location and 2MB size. The `dlmalloc()` that we are using will need ~150
 * bytes to setup its internal datastructures, so the maximum amount of
 * memory that latency can use would be ~1900KB.
 */
void start_memory_watcher(size_t heap_size, void *heap_start,
                          const char *heap_file) {
  int res;
  size_t page_size = sysconf(_SC_PAGESIZE);
  void *dummy_alloc = NULL;
  // sanity check on the heap size
  if (heap_size > 0) {
    // we don't want to enable the memory watcher twice!
    if (memory_watcher_config.status == MEMORY_WATCHER_DISABLED) {
      // disable mmap usage
      res = mallopt(M_MMAP_MAX, 0);
      if (res == 0) {
        elogf(LOG_LEVEL_ERR, "Cannot disable mmap based allocation.\n");
        exit(-1);
      }
      // We need to figure out if we just want to limit the heap size or also
      // having it a specific location.
      if (heap_file == NULL) {
        elogf(LOG_LEVEL_TRACE,
              "Starting  memory watcher, bytes to preallocate: %zu.\n",
              heap_size);
        // preallocate the requested memory
        res = mallopt(M_TOP_PAD, heap_size);
        if (res == 0) {
          elogf(LOG_LEVEL_ERR, "Cannot preallocate %zu bytes.\n", heap_size);
          exit(-1);
        }
        // a dummy allocation to have malloc preallocate the requested amount of
        // memory.
        dummy_alloc = malloc(heap_size);
        if (dummy_alloc == NULL) {
          elogf(LOG_LEVEL_ERR, "Cannot allocate dynamic memory, aborting.\n");
          exit(-1);
        }
        free(dummy_alloc);
        memory_watcher_config.status = MEMORY_WATCHER_ENABLED;
      } else {
        // open the file that will back the heap
        memory_watcher_config.heap_fd = open(heap_file, O_RDWR);
        if (memory_watcher_config.heap_fd < 0) {
          perror("Cannot open heap file to fix heap location, aborting.\n");
          exit(-1);
        }

        // map a region of `/dev/dem`, starting from `heap_start` and of size
        // `heap_size`

        // @todo `heap_size` is a void*, is it ok to directly cast to `off_t`?
        // Considering we are addressing `/dev/mem` which has a view of all the
        // physical memory this is conceptually sound. This could create
        // problems with fixed heap sizes and memory that is not 1 byte
        // addressable

        // Make the mapping aligned to the page size (just drop last 12
        // bits of heap_start and readd them mmap has returned).
        memory_watcher_config.mapping = (mem_watcher_address_t *)mmap(
            NULL, heap_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
            memory_watcher_config.heap_fd,
            ((off_t)heap_start & ~(page_size - 1)));
        if (memory_watcher_config.mapping == MAP_FAILED) {
          perror("Cannot mmap heap file to fix heap location, aborting.\n");
          close(memory_watcher_config.heap_fd);
          close(memory_watcher_config.pagemap_fd);
          exit(-1);
        }
        // add the 12 bits that we masked to make the mapping page-aligned.
        memory_watcher_config.mapping += ((off_t)heap_start & (page_size - 1));
        memory_watcher_config.heap_start = (mem_watcher_address_t *)heap_start;
        memory_watcher_config.fixed_heap_program_break =
            memory_watcher_config.mapping;
        if (strncmp(heap_file, "/dev/mem", 8) == 0) {
          memory_watcher_config.status = MEMORY_WATCHER_FIXED_HEAP;
          elogf(LOG_LEVEL_TRACE, "Fixed heap enabled.\n");
        } else {
          memory_watcher_config.status = MEMORY_WATCHER_FILE_HEAP;
          elogf(LOG_LEVEL_TRACE, "File backed heap enabled.\n");
        }
      }
      memory_watcher_config.heap_size = heap_size;
      memory_watcher_config.initial_program_break =
          (mem_watcher_address_t *)sbrk(0);
      elogf(LOG_LEVEL_TRACE,
            "Memory watcher enabled, initial program "
            "break:%p.\n",
            memory_watcher_config.initial_program_break);
      // Initialize the memory watcher configuration struct
      // we get the value of the program break after the
      // preallocation.
      if (memory_watcher_config.initial_program_break ==
          (mem_watcher_address_t *)-1) {
        perror("Cannot find the program break during memory "
               "watcher setup.");
        exit(-1);
      }
    } else {
      elogf(LOG_LEVEL_ERR, "Attempt to configure the memory watcher when it's "
                           "already started.\n");
      exit(-1);
    }
  } else {
    elogf(LOG_LEVEL_ERR,
          "Attempt to configure the memory watcher with an invalid heap size "
          "(%lu).\n",
          heap_size);
    exit(-1);
  }
  // open the pagemap file to translate virtual addresses to physical
  // ones
  if (benchmark_verbosity >= LOG_LEVEL_DEBUG) {
    elogf(LOG_LEVEL_DEBUG, "Opening pagemap file.\n");
    memory_watcher_config.pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
    if (memory_watcher_config.pagemap_fd < 0) {
      perror("Unable to open pagemap file");
      exit(-1);
    }
  }
}

/**
 * To disable the memory watcher is we set `::memory_watcher_config->status`
 * to
 * `::MEMORY_WATCHER_DISABLED`, to re-enable the use of `mmap()` in
 * `malloc()`, by setting `M_MMAP_MAX` to its default value (`65536`), via
 * `mallopt()` and to reset `M_TOP_PAD` to its default value (`128*1024`) via
 * `mallopt()`.
 */
void stop_memory_watcher() {
  int res;
  if (memory_watcher_config.pagemap_fd != -1 &&
      memory_watcher_config.status >= MEMORY_WATCHER_ENABLED) {
    res = close(memory_watcher_config.pagemap_fd);
    memory_watcher_config.pagemap_fd = -1;
    if (res < 0) {
      perror("Cannot close pagemap file descriptor");
    }
  }
  if (memory_watcher_config.status >= MEMORY_WATCHER_ENABLED) {
    elogf(LOG_LEVEL_TRACE, "Stopping memory watcher.\n");
    if (memory_watcher_config.status >= MEMORY_WATCHER_FIXED_HEAP) {
      res = munmap(memory_watcher_config.mapping,
                   memory_watcher_config.heap_size);
      if (res < 0) {
        perror("Cannot unmap heap file");
      }
      res = close(memory_watcher_config.heap_fd);
      if (res < 0) {
        perror("Cannot close heap file descriptor");
      }
    }
    // stop the memory watcher
    memory_watcher_config.status = MEMORY_WATCHER_DISABLED;
    // reset M_TOP_PAD
    res = mallopt(M_TOP_PAD, 128 * 1024);
    if (res == 0) {
      elogf(LOG_LEVEL_ERR,
            "Cannot reset M_TOP_PAD, after stopping memory watcher.\n");
      exit(-1);
    }
    // enable mmap usage
    res = mallopt(M_MMAP_MAX, 65536);
    if (res == 0) {
      elogf(LOG_LEVEL_ERR, "Cannot enable mmap based allocation after stopping "
                           "memory watcher.\n");
      exit(-1);
    }
  }
}

/// The symbol that corresponds to the glibc `malloc()`, after the linker has
/// made the wrapping.
extern void *__real_malloc(size_t size);

/// The symbol that corresponds to the armMbed version of `malloc()`, which we
/// are using when the user requests as fixed size heap.
extern void *dlmalloc(size_t size);

/** @brief The wrapped `malloc()` function, where the memory watcher is
 * implemented.
 * @details Every time `malloc()` is invoked, we let the original
 * implementation allocate memory via `__real_malloc()`, then we check, via
 * `sbrk(0)`, if the current program break is different from the value in
 * `::memory_watcher_config->initial_program_break`. When these values differ
 * we free the memory that was allocated, give the user an error message and
 * call `exit(-1)`.
 */
void *__wrap_malloc(size_t size) {
  elogf(LOG_LEVEL_DEBUG, "wapper malloc with size %zu\n", size);
  void *pointer = NULL, *current_program_break = NULL;
  void *(*malloc)(size_t) =
      (memory_watcher_config.status >= MEMORY_WATCHER_FIXED_HEAP)
          ? dlmalloc
          : __real_malloc;

  elogf(LOG_LEVEL_DEBUG, "wrapped malloc mem watcher config status:%d \n",
        memory_watcher_config.status);
  elogf(LOG_LEVEL_DEBUG,
        "wrapped malloc \t real_malloc address: %p, dlmalloc %p, selected:%p\n",
        __real_malloc, dlmalloc, malloc);
  pointer = (malloc)(size);
  if (memory_watcher_config.status == MEMORY_WATCHER_ENABLED) {
    current_program_break = sbrk(0);
    if (current_program_break == (void *)-1) {
      perror("Cannot find the current program break.");
      exit(-1);
    }

    if (current_program_break != memory_watcher_config.initial_program_break) {
      free(pointer);
      elogf(LOG_LEVEL_ERR,
            "Memory allocation of %zu bytes has caused an heap "
            "extension.\ninitial program break: %p\ncurrent program "
            "break:%p.\nExecution will be aborted.\n",
            size, memory_watcher_config.initial_program_break,
            current_program_break);
      exit(-1);
    }
  }
  elogf(LOG_LEVEL_DEBUG, "wapper malloc done with pointer %p\n", pointer);
  if (translate_va((u64)pointer) < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot translate pointer %p\n", pointer);
    exit(-1);
  }
  return pointer;
}

/// The symbol that corresponds to the real `mmap()`, after the linker has
/// done the wrapping.
extern void *__real_mmap(void *addr, size_t len, int prot, int flags,
                         int fildes, off_t off);

/** @brief Wrapper of `mmap()` which disables the function if the memory
 * watcher is enabled.
 * @details If `mmap()` is called when the memory watcher is enabled, the
 * program will crash using `exit(-1)`.
 */
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes,
                  off_t off) {
  if (memory_watcher_config.status >= MEMORY_WATCHER_ENABLED) {
    elogf(LOG_LEVEL_ERR, "Use of mmap() after enabling the memory watcher is "
                         "not allowed, aborting.\n");
    exit(-1);
  } else {
    return __real_mmap(addr, len, prot, flags, fildes, off);
  }
}

/** @brief Custom `sbrk()`, which will use the user defined heap.
 * @param[in] offset sbrk's offset.
 */
void *rtbench_sbrk(intptr_t offset) {
  elogf(LOG_LEVEL_DEBUG,
        "wrapped sbrk with offset %ld, memory watcher status: %d\n", offset,
        memory_watcher_config.status);
  void *pointer = NULL;
  switch (memory_watcher_config.status) {
  case MEMORY_WATCHER_FILE_HEAP:
  case MEMORY_WATCHER_FIXED_HEAP:
    // with a positive increment we need to check if the new program break is
    // within the heap maximum size
    if (memory_watcher_config.fixed_heap_program_break + offset <
            memory_watcher_config.mapping ||
        memory_watcher_config.fixed_heap_program_break + offset >
            memory_watcher_config.mapping + memory_watcher_config.heap_size) {
      elogf(LOG_LEVEL_ERR,
            "sbrk heap modification (%ld bytes) would result in a wrong heap "
            "size (%lu bytes, limit is %lu bytes), aborting.\n",
            offset,
            memory_watcher_config.fixed_heap_program_break + offset -
                memory_watcher_config.mapping,
            memory_watcher_config.heap_size);
      errno = ENOMEM;
      pointer = (void *)-1;
      break;
    }
    pointer = memory_watcher_config.fixed_heap_program_break;
    memory_watcher_config.fixed_heap_program_break +=
        offset; // modify the current program break
    elogf(LOG_LEVEL_DEBUG, "wrapped sbrk new program break %p\n",
          memory_watcher_config.fixed_heap_program_break);
    break;
  case MEMORY_WATCHER_DISABLED:
    elogf(LOG_LEVEL_ERR,
          "Use of rtbench_sbrk() after with the memory watcher disabled is "
          "not allowed, aborting.\n");
    errno = ENOMEM;
    pointer = (void *)-1;
    break;
  case MEMORY_WATCHER_ENABLED:
    if (offset == 0) {
      pointer = sbrk(offset);
    } else {
      elogf(LOG_LEVEL_ERR, "Use of rtbench_sbrk() with offset != 0 after "
                           "enabling the memory watcher is "
                           "not allowed, aborting.\n");
      errno = ENOMEM;
      pointer = (void *)-1;
    }
    break;
  default:
    elogf(LOG_LEVEL_ERR, "Invalid memory watcher status: %d.\n",
          memory_watcher_config.status);
    errno = ENOMEM;
    pointer = (void *)-1;
    break;
  }

  elogf(LOG_LEVEL_DEBUG, "wrapped sbrk done pointer:%p\n", pointer);
  if (translate_va((u64)memory_watcher_config.fixed_heap_program_break) < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot translate pointer %p\n", pointer);
    exit(-1);
  }
  return pointer;
}

/// The symbol that corresponds to the real `free()`, after the linker has
/// done the wrapping.
extern void __real_free(void *ptr);

/// The symbol that corresponds to the armMbed `free()`, implementation, used
/// when we want a fixed heap.
extern void dlfree(void *ptr);

/* @brief `free()` wrapper.
 * @param[in] ptr The pointer to the memory to be freed.
 * @details We wrap `free()` because we need to use `dlfree()` if the heap
 * location is fixed.
 */
void __wrap_free(void *ptr) {
  elogf(LOG_LEVEL_DEBUG, "wrapped free for %p\n", ptr);
  if (translate_va((u64)ptr) < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot translate pointer %p\n", ptr);
    exit(-1);
  }
  if (memory_watcher_config.status >= MEMORY_WATCHER_FIXED_HEAP) {
    dlfree(ptr);
  } else {
    __real_free(ptr);
  }
  elogf(LOG_LEVEL_DEBUG, "wrapped free done\n");
}
