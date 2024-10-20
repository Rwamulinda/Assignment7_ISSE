/*
 * kitty.c
 * 
 * A somewhat contrived replacement for 'cat', for use with ISSE Assignment 7.
 * 
 * kitty will copy its stdin to stdout, just like cat.
 * 
 * In addition, upon startup kitty will check its open file
 * descriptors. Kitty expects to have been started with only stdin,
 * stdout, and stderr open. If other file descriptors are open, kitty
 * will print a diagnostic to stderr.
 * 
 * Finally, kitty expects to receive a single command line argument in
 * the form -n, with n as a small integer. See the usage message for
 * details. Kitty expects the environment to match that specified in
 * the -n argument; if it detects an error, it will print a diagnostic
 * to stderr.
 *
 * Author: Howdy Pierce <howdy@sleepymoose.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <assert.h>

#define EXE_NAME "kitty"
#define CATFOOD "CATFOOD"
#define CATFOOD_EXP_VAL "yummy"
#define KITTYLITTER "KITTYLITTER"

#define EXPECTED_HOME_FILE "./.kitty.expected_home"
#define EXPECTED_PATH_FILE "./.kitty.expected_path"

#define KITTY_WAS_HERE "kitty -%d was here!\n"


/*
 * Prints a usage message to stderr, and exits
 *
 * Does not return
 */
void usage()
{
  fprintf(stderr, "Usage: " EXE_NAME " -n\n\n");
  fprintf(stderr, "where n is one of the following integers:\n");
  fprintf(stderr, "0  Perform no environment or file descriptor checks\n");
  fprintf(stderr, "1  Perform no environment checks\n");
  fprintf(stderr, "2  Ensure that the env variable " CATFOOD " is present and set to 'yummy'\n");
  fprintf(stderr, "3  Ensure that the env variable " KITTYLITTER " is _not_ set\n");
  fprintf(stderr, "4  Ensure that the environment contains _only_ the variables PATH, \n");
  fprintf(stderr, "   HOME and " CATFOOD "\n");
  fprintf(stderr, "5  Force a nonzero error exit, without copying anything\n\n");
  fprintf(stderr, "To prove that it has run, " EXE_NAME " will create the file " EXE_NAME".n\n");
  fprintf(stderr, "in the current working directory.\n");

  exit(1);
}


/*
 * 'touches' the file ./.EXENAME.pid.suffix, creating a zero-length
 * file with that name
 *
 * Parameters:
 *   suffix   the suffix to append to the filename
 * 
 * Returns: If the touch was successful, this function
 * returns. Otherwise it prints an error message and exits.
 */
void touch(const char *suffix)
{
  char fname[32];

  snprintf(fname, sizeof(fname), ".%s.%d.%s", EXE_NAME, getpid(), suffix);

  const int create_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
  int fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, create_mode);
  if (fd == -1) {
    perror("open");
    exit(1);
  }
  close(fd);
}


/*
 * Return true if the file ./.EXENAME.suffix is present
 *
 * Parameters:
 *   suffix   the suffix to append to the filename
 * 
 * Returns: true/false based on presence of the file
 */
bool is_file(const char *suffix)
{
  struct stat s;
  char fname[32];
  snprintf(fname, sizeof(fname), ".%s.%s", EXE_NAME, suffix);

  return (stat(fname, &s) == 0);
}

/*
 * Run through all possible file descriptors, and ensure that none are
 * open except stdin, stdout, and stderr.  
 * 
 * If other file descriptors are open, print a diagnostic to
 * stderr. In this case, exit(1) unless runlevel = 0, in which case
 * proceed.
 *
 * Parameters:
 *   runlevel   The kitty runlevel
 * 
 * Returns: none
 */
void check_fds(int runlevel)
{
  bool error = false;

  for (int i=STDERR_FILENO+1; i < 1024; i++) {
    if (fcntl(i, F_GETFD, 0) != -1) {
      fprintf(stderr, EXE_NAME " -%d error: File descriptor %d is open\n", runlevel, i);
      error = true;
    }
  }

  if (!error)
    touch("fd_ok");
}


/*
 * Ensure that the $PATH found in the environment is correct. We have
 * two approaches for this.
 * 
 * If EXPECTED_PATH_FILE is present, then $PATH should exactly equal
 * the contents of this file.
 *
 * Otherwise, if EXPECTED_PATH_FILE is not present, then $PATH should
 * contain $HOME (which will be true if the caller's $PATH contains
 * $HOME/bin, which is usually but not always the case).
 *
 * Parameters: 
 *   runlevel   The kitty runlevel
 * 
 * Returns: true if the test described above passes, false otherwise.
 *          Also prints a diagnostic to stderr if the test fails.
 */
bool check_path(int runlevel)
{
  struct stat s;

  if (stat(EXPECTED_PATH_FILE, &s) == 0) {     

    // file exists: open, read, close, then compare contents
    int fd = open(EXPECTED_PATH_FILE, O_RDONLY);
    if (fd == -1) {
      perror("open");
      exit(1);
    }
    char expected_path[1024];
    ssize_t bytes_read = read(fd, expected_path, sizeof(expected_path));
    expected_path[bytes_read] = '\0';
    close(fd);

    if (bytes_read == 0)
      return false;

    if (strncmp(expected_path, getenv("PATH"), bytes_read) != 0) {
      fprintf(stderr, EXE_NAME " -%d error: Incorrect value for the "
          "environment variable PATH; expected %s\n", runlevel, expected_path);
      return false;
    }

    return true;
  } 

  // EXPECTED_PATH_FILE does not exist, so compare with HOME
  if (strstr(getenv("PATH"), getenv("HOME")) == NULL) {
    fprintf(stderr, EXE_NAME " -%d error: Incorrect value for the "
        "environment variable PATH; expected it to contain the string '%s'\n",
        runlevel, getenv("HOME"));
    return false;
  }

  return true;
}


/*
 * Ensure that the $HOME found in the environment is correct. We have
 * two approaches for this.
 * 
 * If EXPECTED_HOME_FILE is present, then $HOME should exactly equal
 * the contents of this file.
 *
 * If EXPECTED_HOME_FILE is not present, then $HOME should be
 * constructed based on the username of the current euid. This will
 * fail when running under sudo however.
 *
 * Parameters: 
 *   runlevel   The kitty runlevel
 * 
 * Returns: true if the test described above passes, false otherwise.
 *          Also prints a diagnostic to stderr if the test fails.
 */
bool check_home(int runlevel)
{
  struct stat s;
  char exp_home[256];

  if (stat(EXPECTED_HOME_FILE, &s) == 0) {     

    // file exists: open, read, close, then compare contents
    int fd = open(EXPECTED_HOME_FILE, O_RDONLY);
    if (fd == -1) {
      perror("open");
      exit(1);
    }
    ssize_t bytes_read = read(fd, exp_home, sizeof(exp_home));
    exp_home[bytes_read] = '\0';
    close(fd);

    if (bytes_read == 0)
      return false;

    if (strncmp(exp_home, getenv("HOME"), bytes_read) != 0) {
      fprintf(stderr, EXE_NAME " -%d error: Incorrect value for the "
          "environment variable HOME; expected %s\n", runlevel, exp_home);
      return false;
    }

    return true;
  } 

  // EXPECTED_HOME_FILE does not exist, so construct using euid
  struct passwd *pw;
  pw = getpwuid(getuid());

  if (!pw)     // this shouldn't happen, but if it does we can't check
    return true;

  snprintf(exp_home, sizeof(exp_home), "/home/%s", pw->pw_name);
  if (strcmp(exp_home, getenv("HOME")) != 0) {
    fprintf(stderr, EXE_NAME " -%d error: Expected to find the environment variable "
        "HOME set to %s\n", runlevel, exp_home);
    return false;
  }

  return true;
}


/*
 * Ensure that the environment variable CATFOOD is present in the
 * environment, and set to CATFOOD_EXP_VAL
 *
 * Parameters: 
 *   runlevel   The kitty runlevel
 * 
 * Returns: true if the test described above passes, false otherwise.
 *          Also prints a diagnostic to stderr if the test fails.
 */
bool check_catfood(int runlevel)
{
  const char *val = NULL;

  val = getenv(CATFOOD);
  if (!val) {
    fprintf(stderr, EXE_NAME " -%d error: Expected to find the environment variable "
        CATFOOD "\n", runlevel);
    return false;
  }

  if (strcmp(val, CATFOOD_EXP_VAL) != 0) {
    fprintf(stderr, EXE_NAME " -%d error: Expected to find the environment variable "
        CATFOOD " set to " CATFOOD_EXP_VAL "\n", runlevel);
    return false;
  }

  return true;
}


/*
 * Performs environment validation according to the runlevel:
 * 
 * 0,1  No checks
 * 2    Ensure the environment variable CATFOOD is present and set to 'yummy'
 * 3    Ensure that the environment variable KITTYLITTER is _not_ set
 * 4    Ensure that the environment contains _only_ the variables PATH,
 *       HOME and CATFOOD
 * 5    Force a nonzero error exit, without copying anything
 *
 * Parameters:
 *   runlevel   the kitty runlevel
 *   envp       pointer to all the environment variables
 * 
 * Returns: If the environment validation was successful, this
 * function returns. Otherwise it prints an error message and exits.
 */
void check_env(int runlevel, char *envp[])
{
  bool error = false;
  const char *val = NULL;
  int env_size = 0;

  if (runlevel == 0 || runlevel == 1)
    return;

  while(envp[env_size]) 
    env_size++;

  // all levels: Sanity check PATH and HOME, since these should always exist
  if (!getenv("PATH") || !getenv("HOME")) {
    fprintf(stderr, EXE_NAME " -%d error: Expected to find the environment variables "
        "PATH and HOME\n", runlevel);
    error = true;
  }

  // check that HOME is set correctly
  if (!error)
    error = !check_home(runlevel);

  // check that PATH is set correctly
  if (!error)
    error = !check_path(runlevel);


  switch (runlevel) {
  case 0:
  case 1:
    assert(0);    // should not be here; these cases handled above
    break;

  case 2:
    if (env_size < 10) {
      fprintf(stderr, EXE_NAME " -%d error: Only found %d environment variables\n",
          runlevel, env_size);
      error = true;
    }

    if (!error)
      error = !check_catfood(runlevel);

    break;

  case 3:
    if (env_size < 10) {
      fprintf(stderr, EXE_NAME " -%d error: Only found %d environment variables\n",
          runlevel, env_size);
      error = true;
    }

    val = getenv(KITTYLITTER);
    if (val) {
      fprintf(stderr, EXE_NAME " -%d error: Did NOT expect to find the environment variable "
          KITTYLITTER "\n", runlevel);
      error = true;
    }
    break;

  case 4:
    if (env_size != 3) {
      fprintf(stderr, EXE_NAME " -%d error: Expected only 3 environment variables,"
          " found %d\n", runlevel, env_size);
      error = true;
    }

    if (!error)
      error = !check_catfood(runlevel);

    break;
  }

  if (!error)
    touch("env_ok");

  return;
}


int main(int argc, char *argv[], char *envp[])
{
  int runlevel;
  int c;

  if (argc != 2)
    usage();

  runlevel = argv[1][1] - '0';
  if (runlevel < 0 || runlevel > 5)
    usage();
  
  if (runlevel == 5 || (runlevel == 3 && is_file("force_exit"))) {
    fprintf(stderr, EXE_NAME " -%d error: Forcing an error exit\n", runlevel);
    exit(1);
  }

  touch("launch");

  check_fds(runlevel);

  check_env(runlevel, envp);

  if (runlevel == 2 || runlevel == 3)
    printf(KITTY_WAS_HERE, runlevel);

  // at runlevels 3 and 4, check that the upstream kitty inserted the
  // KITTY_WAS_HERE message, and strip it off
  if (runlevel == 3 || runlevel == 4) {
    char expected[128];
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      fprintf(stderr, EXE_NAME " -%d error: No input\n", runlevel);
      exit(1);
    }

    snprintf(expected, sizeof(expected), KITTY_WAS_HERE, runlevel-1);
    if (strcmp(expected, buffer) != 0) {
      fprintf(stderr, EXE_NAME " -%d error: Upstream is not kitty -%d\n",
          runlevel, runlevel-1);
      fputs(buffer, stdout);
    } else {
      touch("pipe_ok");
    }
  }

  while ( (c = fgetc(stdin)) != EOF)
    if (fputc(c, stdout) == EOF) {
      perror("fputc");
      exit(1);
    }

  touch("eof_ok");
  
  return 0;
}
