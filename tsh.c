/*
 * tsh - A tiny shell program with job control
 *
 * name   = Andrew Chang-DeWitt
 * emails = achangdewitt@hawk.iit.edu
 * github = andrew-chang-dewitt
 */
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* Logger helpers */
#define PREF_ERR "[ERROR] "
#define PREF_WARN "[WARN] "
#define PREF_INFO "[INFO] "

#define LOGWLOC(stream, prefix, msg)                                           \
  {                                                                            \
    fprintf(stream, "[pid:%d] %s[%s:%d] %s\n", getpid(), prefix, __FILE__,     \
            __LINE__, msg);                                                    \
  }

#define LOGWLOCF(stream, prefix, fmt, ...)                                     \
  {                                                                            \
    char loc[] = "[%s:%d] ";                                                   \
    char *res;                                                                 \
    if (asprintf(&res, "[pid:%d] %s%s%s\n", getpid(), prefix, loc, fmt) < 0)   \
      fprintf(stream, fmt, __VA_ARGS__);                                       \
    else                                                                       \
      fprintf(stream, res, __FILE__, __LINE__, __VA_ARGS__);                   \
    free(res);                                                                 \
  }

#define LOGERR(msg)                                                            \
  if (verbose)                                                                 \
  LOGWLOC(stderr, PREF_ERR, msg)
#define FLOGERR(fmt, ...)                                                      \
  if (verbose)                                                                 \
  LOGWLOCF(stderr, PREF_ERR, fmt, __VA_ARGS__)
#define LOGWARN(msg)                                                           \
  if (verbose)                                                                 \
  LOGWLOC(stdout, PREF_WARN, msg)
#define FLOGWARN(fmt, ...)                                                     \
  if (verbose)                                                                 \
  LOGWLOCF(stdout, PREF_WARN, fmt, __VA_ARGS__)
#define LOGINFO(msg)                                                           \
  if (verbose)                                                                 \
  LOGWLOC(stdout, PREF_INFO, msg)
#define FLOGINFO(fmt, ...)                                                     \
  if (verbose)                                                                 \
  LOGWLOCF(stdout, PREF_INFO, fmt, __VA_ARGS__)

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t {           /* The job struct */
  pid_t pid;             /* job PID */
  int jid;               /* job ID [1, 2, ...] */
  int state;             /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(int argc, char **argv);
void do_bgfg(int argc, char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, int *argc_dest, char **argv);
void sigquit_handler(int sig);

void initjobs(struct job_t *jobs);
void clearjob(struct job_t *job);
void listjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv) {
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h': /* print help message */
      usage();
      break;
    case 'v': /* emit additional diagnostic info */
      verbose = 1;
      break;
    case 'p':          /* don't print a prompt */
      emit_prompt = 0; /* handy for automatic testing */
      break;
    default:
      usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT, sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler);

  /* Initialize the job list */
  initjobs(jobs);

  /* Execute the shell's read/eval loop */
  while (1) {

    /* Read command line */
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  }

  exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
void eval(char *cmdline) {
  LOGINFO("begin eval");

  int argc;
  char *argv[MAXARGS]; // parse commandline into args
  int bg = parseline(
      cmdline, &argc,
      argv); // parseline returns truthy iff command is to be run in background

  FLOGINFO("%s: checking if builtin command...", argv[0]);
  int is_builtin =
      builtin_cmd(argc, argv); // run as builtin command, if builtin

  if (!is_builtin) { // otherwise, handle command
    FLOGINFO(
        "%s: %s",
        "not a builtin command, attempting to exec command in child process",
        argv[0]);
    // setup signal masks
    sigset_t mask_sigall, // for masking all signals
        mask_sigchld,     // for child signals only
        prev_sigset;      // for temporarily
                          // storing previous
                          // masks to be restored
                          // after some op

    sigfillset(&mask_sigall); // then construct actual sets for all & child
    sigemptyset(&mask_sigchld);
    sigaddset(&mask_sigchld, SIGCHLD);

    // block sigchld while creating new child to prevent race before ready to
    // handle child signals
    LOGINFO("blocking SIGCHLD");
    if (sigprocmask(SIG_BLOCK, &mask_sigchld, &prev_sigset) != 0)
      fprintf(stderr, "WARNING: failed to block SIGCHLD");

    LOGINFO("attempting to create child process");
    pid_t pid = fork(); // fork & exec program in child process
    if (pid == -1) {    // handle fork error
      fprintf(stderr, "Unable to fork child process for: %s",
              cmdline);                             // warn user
      sigprocmask(SIG_SETMASK, &prev_sigset, NULL); // restore sig mask
      return;                                       // quit eval
    }

    if (pid == 0) { // BEGIN CHILD PROC
      setpgrp();    // ensure all children are in own process group
      if (sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL) !=
          0) // allow child to handle sigchld
        fprintf(stderr, "WARNING: failed to unblock SIGCHLD");

      FLOGINFO("%s: executing command in child process...", argv[0]);
      int result = execv(argv[0], argv); // exec command program
      if (result < 0) { // execv returns negative if command isn't found
        printf("%s: Command not found\n", argv[0]);
        exit(1);
      }

      LOGINFO("...done. exiting child process");
      exit(0); // "return early" by exiting child process w/ success state
    } // END CHILD PROC

    // PARENT PROC (TSH) RESUMES HERE
    int state = bg ? BG : FG;                          // determine job state
    int job_added = addjob(jobs, pid, state, cmdline); // add job to jobs list

    if (!job_added) { // handle error adding job
      fprintf(stderr, "Failed to create job for %s", cmdline); // alert user
      return;                                                  // quit eval
    }

    sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL); // ready to handle sigchld

    if (bg) // show pid and jid then return control immediately
      printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
    else // wait for job to term or stop before returning control to user
      waitfg(pid);
  }
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, int *argc_dest, char **argv) {
  static char array[MAXLINE]; /* holds local copy of command line */
  char *buf = array;          /* ptr that traverses command line */
  char *delim;                /* points to first space delimiter */
  int argc;
  int bg; /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  } else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    } else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc - 1] == '&')) != 0) {
    argv[--argc] = NULL;
  }

  // update argc pointer arg
  *argc_dest = argc;

  return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(int argc, char **argv) {
  // quit command exits tsh
  if (strcmp("quit", argv[0]) == 0) {
    LOGINFO("quit received, exiting tsh");
    exit(0);
  }

  // jobs command shows jobs list
  if (strcmp("jobs", argv[0]) == 0) {
    LOGINFO("jobs builtin received, printing jobs list");
    listjobs(jobs);
    return 1;
  }

  // bg & fg commands
  if (strcmp("bg", argv[0]) == 0 || strcmp("fg", argv[0]) == 0) {
    FLOGINFO("%s builtin received, forwarding command to handler", argv[0]);
    do_bgfg(argc, argv);
    return 1;
  }

  return 0; // not a builtin command
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(int argc, char **argv) {
  FLOGINFO("%s command received with arg %s, handling...", argv[0], argv[1]);

  // ensure command syntax is correct
  // check arg length correct
  if (argc != 2) {
    fprintf(stderr, "%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }

  // parse arg
  char *id_str = argv[1];
  int is_jid = 0;
  if (id_str[0] == '%') {
    is_jid = 1;
    id_str++;
  }

  // id str -> num conversion
  char *endptr; // stores ptr first invalid char in num conversion
  long id_lng = strtol(id_str, &endptr, 10);
  // check for conversion error
  if (*endptr != '\0') { // num converted successfully if endptr is null char
    fprintf(stderr, "%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }
  int id = id_lng; // coerce long to int

  struct job_t *job = is_jid                     // get requested job data
                          ? getjobjid(jobs, id)  // by jid if %
                          : getjobpid(jobs, id); // else by pid
  if (!job) {                                    // alert user if bad job id/pid
    char *fmt = is_jid ? "%%%d: No such job\n" : "(%d): No such process\n";
    fprintf(stderr, fmt, id);
    return;
  }

  kill(-job->pid, SIGCONT); // resume job process

  if (strcmp("bg", argv[0]) == 0) {                           // handle bg
    printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline); // update user
    job->state = BG;  // update job status to background
  } else {            // handle fg
    job->state = FG;  // update job status to foreground
    waitfg(job->pid); // wait for job to complete
  }
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
  sigset_t mask_none, prev_mask; // allow receiving of all signals while waiting
  sigemptyset(&mask_none);
  sigprocmask(SIG_BLOCK, &mask_none, &prev_mask);

  int waiting = 1; // init waiting to True

  while (waiting) {
    struct job_t *job = getjobpid(jobs, pid); // check if still waiting
    waiting = (job && job->state == FG) ? 1 : 0;

    if (waiting) // pause until a signal is received
      pause();
    // NOTE: all actual signal handling will be done in sig handlers,
    //       including updating job status on appropriate signals
  }
  sigprocmask(SIG_BLOCK, &prev_mask,
              NULL); // when done waiting, restore previous mask
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int _) {
  LOGINFO("SIGCHLD caught, handling...");
  int pid;                           // to store pid of changed child proc
  int status;                        // to store child proc status
  sigset_t mask_sigall, prev_sigset; // signal set masks

  // block all signals while handline sigchld
  sigfillset(&mask_sigall);
  sigprocmask(SIG_BLOCK, &mask_sigall, &prev_sigset);

  // reap and update ALL children necessary
  while ((pid = waitpid(-1,           // wait for ANY child to term/stop
                        &status,      // save status here
                        WNOHANG |     // poll instead of blocking
                            WUNTRACED // get stopped jobs too, not just termed
                        )) > 0) {
    FLOGINFO("Child proc (%d) changed, checking status...", pid);

    // handle the following cases:
    // 1. child termed due to exit
    if (WIFEXITED(status)) {
      LOGINFO("process exited, requesting deletion...");
      deletejob(jobs, pid); // then remove from jobs list

      // handler done, cleanup
      sigprocmask(SIG_SETMASK, &prev_sigset, NULL); // restore signal mask
      return; // return early to end processing
    }

    // 2. child termed due to signal
    if (WIFSIGNALED(status)) {
      int sig = WTERMSIG(status);
      FLOGINFO("process  termed due to signal %d", sig);
      deletejob(jobs, pid); // then remove from jobs list

      // handler done, cleanup
      sigprocmask(SIG_SETMASK, &prev_sigset, NULL); // restore signal mask
      return; // return early to end processing
    }

    // 3. child stopped due to signal
    if (WIFSTOPPED(status)) {
      int sig = WSTOPSIG(status);
      FLOGINFO("process  stopped due to signal %d", sig);
      struct job_t *job = getjobpid(jobs, pid); // get job data
      job->state = ST;                          // update state to Stopped
      printf("Job [%d] (%d) stopped by signal %d\n", job->jid, pid,
             sig); // & print confirmation

      // handler done, cleanup
      sigprocmask(SIG_SETMASK, &prev_sigset, NULL); // restore signal mask
      return; // return early to end processing
    }

    // ERROR -- if this point is reached, then something has gone wrong
    sigprocmask(SIG_SETMASK, &prev_sigset, NULL); // restore signal mask
    unix_error("Unhandled SIGCHLD received, unable to continue."); // exit
  }
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *    user types ctrl-c at the keyboard. Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig) {
  // NOTE: may need to ensure signals are captured by tsh instead of
  // host shell...
  // get pid of current fg job
  pid_t pid = fgpid(jobs);
  // if no such job, silently return early
  if (!pid) {
    printf("no foreground job exists\n");
    return;
  }

  // else get job info
  struct job_t *job = getjobpid(jobs, pid);
  // and send kill signal to it
  if (kill(-pid, sig) < 0)
    printf("Interrupt error: failed to kill %d\n", pid);
  else
    // then print confirmation job was killed
    printf("Job [%d] (%d) terminated by signal %d\n", job->jid, pid, sig);
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig) {
  FLOGINFO("handling signal %d", sig);
  // get current fg job pid
  pid_t pid = fgpid(jobs);
  // if no such job, silently return early
  if (!pid) {
    printf("no foreground job exists\n");
    return;
  }

  // else stop job by forwarding the signal
  struct job_t *job = getjobpid(jobs, pid);
  FLOGINFO("fg job [%d] (%d) found, forwarding signal to child group...",
           job->jid, pid);
  if (kill(-pid, sig) < 0)
    printf("Stop error: failed to stop %d\n", pid);
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) {
  int i, max = 0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max)
      max = jobs[i].jid;
  return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) {
  int i;

  if (pid < 1) {
    fprintf(stderr, "Unable to create job for pid %d", pid);
    return 0;
  }

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == 0) {
      jobs[i].pid = pid;
      jobs[i].state = state;
      jobs[i].jid = nextjid++;
      if (nextjid > MAXJOBS)
        nextjid = 1;
      strcpy(jobs[i].cmdline, cmdline);
      if (verbose) {
        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid,
               jobs[i].cmdline);
      }
      return 1;
    }
  }

  fprintf(stderr, "Tried to create too many jobs\n");
  return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) {
  FLOGINFO("delete requested for job(%d)", pid);
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      struct job_t job = jobs[i];
      FLOGINFO("[%d] (%d) %s: job found, deleting", job.jid, job.pid,
               job.cmdline);
      clearjob(&jobs[i]);
      nextjid = maxjid(jobs) + 1;
      LOGINFO("job deleted");
      return 1;
    }
  }
  return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
      return jobs[i].pid;
  return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) {
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) {
  int i;

  if (pid < 1)
    return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid) {
      return jobs[i].jid;
    }
  return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state) {
      case BG:
        printf("Running ");
        break;
      case FG:
        printf("Foreground ");
        break;
      case ST:
        printf("Stopped ");
        break;
      default:
        printf("listjobs: Internal error: job[%d].state=%d ", i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) {
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg) {
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {
  printf("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}
