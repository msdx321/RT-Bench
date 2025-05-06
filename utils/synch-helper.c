/** @file synch-helper.c
 * @ingroup utils
 * @brief Helper program to synchonise benchamrks release
 * @author Mattia Nicolella
 * @details
 * This program Will create a group of synchonised benchmarks
 * and act as the unblocker, once the specified number
 * of benchmarks are waiting to bo synchonised.
 *
 * @copyright (C) 2021 - 2022, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */

#include <argp.h>
#include <fcntl.h> /* For O_* constants */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>

#include "signal_utils.h"
#include "synch_release_data.h"

/// Options for this program.
struct options {
  long size;  ///< group size.
  char *name; ///< group name.
};

static int parse_opt(int key, char *arg, struct argp_state *state) {
  struct options *parsed_args = state->input;
  char *endptr;
  switch (key) {
  case 'g':
    parsed_args->name = arg;
    break;
  case 'n':
    parsed_args->size = strtol(arg, &endptr, 0);
    if (endptr[0] != '\0' || endptr[0] == arg[0]) {
      argp_error(state,
                 "Error while parsing group size: %s in an invalid number\n",
                 arg);
    }
    break;
  case 'h':
    argp_state_help(state, stdout, ARGP_HELP_USAGE | ARGP_HELP_LONG);
    exit(EXIT_SUCCESS);
    break;
  case ARGP_KEY_END:
    if (parsed_args->name == NULL) {
      parsed_args->name = SYNCH_GRP_DEFAULT_NAME;
    }
    if (parsed_args->size <= 0) {
      argp_error(state,
                 "Error while parsing group size: %lu in an invalid benchmark number\n",
                 parsed_args->size);
    }
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

int main(int argc, char **argv) {
  struct options parsed_args = {0};
  int fd = 0, res = 0, ret = 0;
  char *shm_name;
  struct synch_shm *shm;
  // argp variables
  const char *argp_doc = "Synchronise a group of benchmarks.";
  const char *argp_args_doc = "";
  struct argp_option argp_options[] = {
      {"group-name", 'g', "name", OPTION_ARG_OPTIONAL,
       "Name of the benchmark group to synchonise. Default name for the group "
       "is "
       "'" SYNCH_GRP_DEFAULT_NAME "'."
       "Benchmarks in the group need the '-s' option with the same name "
       "specified here."
       "The user need to put extra care in choosing a unique name "
       "for each experiment group when there are more than one."},
      {"num-bench", 'n', "integer>=0", 0,
       "Number of benchmarks to synchonise, excluding this program."},
      {NULL, 'h', NULL, 0, NULL},
      {0}};
  struct argp argp = {0};
  argp.args_doc = argp_args_doc;
  argp.doc = argp_doc;
  argp.parser = parse_opt;
  argp.options = argp_options;
  // parsing parameters
  res = argp_parse(&argp, argc, argv, 0, 0, &parsed_args);
  if (res != 0) {
    perror("Error during argument parsing");
    return EXIT_FAILURE;
  }

  shm_name = malloc(sizeof(char) *
                    (strlen("/rtbench.shm_") + strlen(parsed_args.name) + 1));
  if (shm_name == NULL) {
    perror("Error during shm name allocation");
    return -1;
  }
  res = sprintf(shm_name, "/rtbench.shm_%s", parsed_args.name);
  if (res < 0) {
    perror("Errod during creation of synchronisation shm name");
    return res;
  }

  printf("Waiting for shared memory %s to appear\n", shm_name);
  // now we wait for the master benchmark to initialise the shared memory
  //@todo: find a better way to wait for shm to be initialized
  while (fd <= 0) {
    fd = shm_open(shm_name, O_RDONLY, 0640);
    sleep(1);
  };
  free(shm_name);
  shm = mmap(NULL, sizeof(struct synch_shm), PROT_READ, MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED) {
    perror("Error during shared memory mapping for synchronised start");
    close(fd);
    return -1;
  }
  printf("Waiting for shm init.\n");
  while (shm->status != SYNCH_ENABLED) {
    sleep(1);
  };
  printf("Waiting for benchmarks in the group.\n");
  while (parsed_args.size != shm->waiting_bmarks) {
    sleep(1);
  };
  printf("Unlocking group!\n");
  res = kill(shm->master_pid, SIGNAL_SYNCH_RELEASE);
  if (res < 0) {
    perror("Cannot signal group master");
  }
  ret = res;
  res = munmap(shm, sizeof(struct synch_shm));
  if (res < 0) {
    perror("Cannot unmap shm");
  }
  ret = (ret < 0) ? ret : res;
  res = close(fd);
  if (res < 0) {
    perror("Cannot close shm file descriptor");
  }
  ret = (ret = 0) ? ret : res;
  return res;
}