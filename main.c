#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <libgen.h>

struct command
{
  char **cmd;
};

struct Job
{
  pid_t pid;
  char *name;
} suspended_jobs[100];

// global variables
int num_of_commands;
int num_of_suspended_jobs = 0;
int err = 0;
int in, out;

int shell_exit_with_error_msg(char *msg)
{
  fprintf(stderr, "Error: %s\n", msg);
  exit(EXIT_FAILURE);
}

// Reads line from stdin and returns it
char *read_line(void)
{
  int buffsize = 1000;
  int position = 0;
  char *buffer = malloc(sizeof(char) * buffsize);
  int c;

  while (1)
  {
    c = getchar();
    if (c == EOF || c == '\n')
    {
      buffer[position] = '\0';
      return buffer;
    }
    else
    {
      buffer[position] = c;
    }
    position++;
  }
}

struct command *split_line_into_commands(char *line)
{
  err = 0;
  num_of_commands = 0;
  int buff_size = 1000, pos = 0, cmd_pos = 0, pipe_count = 0;

  char **tokens = malloc(buff_size * sizeof(char *));
  char *token;
  struct command *commands = malloc(buff_size * sizeof(struct command));

  token = strtok(line, " ");

  if (!token)
  {
    err = 2;
  }

  // illegal command starting with |
  if (err != 2 && !strcmp(token, "|"))
  {
    err = 1;
  }

  while (token)
  {
    if (!strcmp(token, "|"))
    {
      tokens[pos] = NULL;
      commands[cmd_pos] = (struct command){.cmd = malloc((pos + 1) * sizeof(char *))};

      for (int i = 0; i < pos; i++)
      {
        commands[cmd_pos].cmd[i] = tokens[i];
      }
      commands[cmd_pos].cmd[pos] = NULL; // Null terminate the command array

      // Move to the next command
      cmd_pos++;
      num_of_commands++;
      pipe_count++;

      // Reset tokens for the next command
      pos = 0;
      token = strtok(NULL, " ");
    }
    else
    {
      tokens[pos++] = token;
      token = strtok(NULL, " ");
    }
  }

  // Handle the last command (if any)
  if (pos > 0)
  {
    tokens[pos] = NULL;
    commands[cmd_pos] = (struct command){.cmd = malloc((pos + 1) * sizeof(char *))};

    for (int i = 0; i < pos; i++)
    {
      commands[cmd_pos].cmd[i] = tokens[i];
    }
    commands[cmd_pos].cmd[pos] = NULL;

    num_of_commands++;
    cmd_pos++;
  }

  if (pipe_count >= num_of_commands && num_of_commands)
  { // | terminated, which is invalid
    printf("pipe_c(%d) >= num_of_cmds(%d) \n", pipe_count, num_of_commands);
    err = 1;
  }

  // for (int i = 0; i < num_of_commands; i++)
  // {
  //   for (int j = 0; commands[i].cmd[j] != NULL; j++)
  //   {
  //     printf("commands[%d].cmd[%d] = %s\n", i, j, commands[i].cmd[j]);
  //   }
  // }

  return commands;
}

int shell_exit(char **args)
{
  if (num_of_suspended_jobs != 0)
  {
    fprintf(stderr, "Error: there are suspended jobs\n");
    return 0;
  }
  if (args[1] != NULL)
  {
    fprintf(stderr, "Error: invalid command\n");
    return 0;
  }
  return 1;
}

int shell_cd(char **args)
{
  if (args[1] == NULL || args[2] != NULL)
  {
    fprintf(stderr, "Error: invalid command\n");
  }
  else
  {
    if (chdir(args[1]) != 0)
    {
      fprintf(stderr, "Error: invalid directory\n");
    }
  }
  return 0;
}

int shell_jobs(char **args)
{
  if (args[1])
  {
    fprintf(stderr, "Error: invalid command\n");
    return 0;
  }

  for (int i = 0; i < num_of_suspended_jobs; i++)
  {
    printf("[%d] %s\n", i + 1, suspended_jobs[i].name);
  }

  return 0;
}

int shell_fg(char **args)
{
  if (!args[1] || args[2])
  {
    fprintf(stderr, "Error: invalid command\n");
    return 0;
  }

  int j_index = atoi(args[1]);

  if (j_index <= 0 || j_index > num_of_suspended_jobs)
  {
    fprintf(stderr, "Error: invalid job\n");
    return 0;
  }

  pid_t fg_pid = suspended_jobs[j_index - 1].pid;
  char *fg_name = suspended_jobs[j_index - 1].name;

  kill(fg_pid, SIGCONT);

  // shift the jobs after left by 1
  for (int i = j_index - 1; i < num_of_suspended_jobs; i++)
  {
    suspended_jobs[i] = suspended_jobs[i + 1];
  }
  num_of_suspended_jobs--;

  // in case if it's stopped again
  int status;
  waitpid(fg_pid, &status, WUNTRACED);

  if (WIFSTOPPED(status))
  {
    suspended_jobs[num_of_suspended_jobs].pid = fg_pid;
    suspended_jobs[num_of_suspended_jobs].name = malloc(sizeof(char) * (strlen(fg_name + 1)));
    strcpy(suspended_jobs[num_of_suspended_jobs++].name, fg_name);
  }

  return 0;
}

// input redirect
void shell_input_redirect(char *target)
{
  if (!target)
  {
    shell_exit_with_error_msg("invalid command");
  }
  int redirect_fd = open(target, O_RDONLY);
  if (redirect_fd == -1)
  {
    shell_exit_with_error_msg("invalid file");
  }
  dup2(redirect_fd, STDIN_FILENO);
  close(redirect_fd);
}

// output redirect
void shell_output_redirect(char *target, int append)
{
  int redirect_fd;

  if (append == 1)
  {
    redirect_fd = open(target, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
  }
  else
  {
    redirect_fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  }

  dup2(redirect_fd, STDOUT_FILENO);
  close(redirect_fd);
}

void shell_redirect(char *pipe, char *target)
{
  if (!strcmp(pipe, ">"))
  {
    shell_output_redirect(target, 0);
    out++;
  }
  else if (!strcmp(pipe, ">>"))
  {
    shell_output_redirect(target, 1);
    out++;
  }
  else if (!strcmp(pipe, "<"))
  {
    shell_input_redirect(target);
    in++;
  }
  else
  {
    shell_exit_with_error_msg("invalid command");
  }
}

void shell_io_redirect_execute(char **args)
{
  int i = 0;
  char **new_args = malloc(50 * sizeof(char *));

  while (args[i])
  {
    if ((strcmp(args[i], ">>")) && (strcmp(args[i], ">")) && (strcmp(args[i], "<")))
    {
      if (i == 0 && !strstr(args[i], "/") && strcmp(args[i], "time"))
      {
        char cmd[100];
        strcpy(cmd, "/usr/bin/");
        strcat(cmd, args[i]);
        new_args[i] = cmd;
        i++;
      }
      else
      {
        new_args[i] = args[i];
        i++;
      }
    }
    else
    {
      char *pipe, *fn;
      pipe = args[i];
      fn = args[i + 1];
      shell_redirect(pipe, fn);

      // check for second redirect
      if (args[i + 2] != NULL)
      {
        if (args[i + 3] == NULL || args[i + 4] != NULL)
        {
          shell_exit_with_error_msg("invalid command");
        }

        // 2 ins
        if (!strcmp(pipe, "<") && !strcmp(pipe, args[i + 2]))
        {
          shell_exit_with_error_msg("invalid command");
        }

        // 2 outs
        if ((!strcmp(pipe, ">") && !strcmp(pipe, args[i + 2])) || (!strcmp(pipe, ">>") && !strcmp(pipe, args[i + 2])))
        {
          shell_exit_with_error_msg("invalid command");
        }

        shell_redirect(args[i + 2], args[i + 3]);

        // exit when two ins or outs after redirect
        if (in > 1 || out > 1)
        {
          shell_exit_with_error_msg("invalid command");
        }
      }
      // execute
      execvp(new_args[0], new_args);
      shell_exit_with_error_msg("invalid program");
    }
  }

  // execute
  execvp(new_args[0], new_args);
  shell_exit_with_error_msg("invalid program");
}

int execute_all(struct command *commands, char *line)
{
  int status;
  int pipefd[num_of_commands - 1][2];
  pid_t pids[num_of_commands];

  // create pipes for |
  for (int i = 0; i < num_of_commands - 1; i++)
  {
    if (pipe(pipefd[i]) < 0)
    {
      perror("pipe");
      exit(EXIT_FAILURE);
    }
  }

  // execut all commands
  for (int i = 0; i < num_of_commands; i++)
  {
    char **args = commands[i].cmd;
    in = 0, out = 0;

    // case exit, exit the process
    if (!strcmp(args[0], "exit"))
    {
      return shell_exit(args);
    }

    // case cd, happens on parent process
    if (!strcmp(args[0], "cd"))
    {
      return shell_cd(args);
    }

    // print jobs
    if (!strcmp(args[0], "jobs"))
    {
      return shell_jobs(args);
    }

    // fg
    if (!strcmp(args[0], "fg"))
    {
      return shell_fg(args);
    }

    pids[i] = fork();

    if (pids[i] < 0)
    { // error pid
      perror("fork_error");
      return 1;
    }
    else if (pids[i] == 0)
    { // child
      if (num_of_commands > 1)
      {
        // if not first, redirect input from the previous pipe's read end (pipefd[i-1][0])
        if (i > 0)
        {
          // printf("input redirected from pipefd[%d][0]\n", i - 1);
          dup2(pipefd[i - 1][0], STDIN_FILENO);
          in++;
        }

        // if not last, redirect output to this pipe's write end (pipefd[i][1])
        if (i < (num_of_commands - 1))
        {
          // printf("output redirected to pipefd[%d][1]\n", i);
          dup2(pipefd[i][1], STDOUT_FILENO);
          out++;
        }
        // close all unused pipes
        for (int j = 0; j < num_of_commands - 1; j++)
        {
          close(pipefd[j][0]);
          close(pipefd[j][1]);
        }
      }
      shell_io_redirect_execute(args);
    }
  }

  // close unused pipes in parent
  for (int i = 0; i < num_of_commands - 1; i++)
  {
    close(pipefd[i][0]);
    close(pipefd[i][1]);
  }

  // parent waits for all children
  for (int i = 0; i < num_of_commands; i++)
  {
    waitpid(pids[i], &status, WUNTRACED);
    // if the child process has been stopped, add to suspended_jobs
    if (pids[i] > 0 && WIFSTOPPED(status))
    {
      suspended_jobs[num_of_suspended_jobs].pid = pids[i];
      suspended_jobs[num_of_suspended_jobs].name = malloc(sizeof(char) * (strlen(line)));
      strcpy(suspended_jobs[num_of_suspended_jobs++].name, line);
    }
  }

  return 0;
}

void shell_sig_ign_handler(int sig)
{
}

int main(int argc, char **argv)
{
  int status = 0;
  char *line = malloc(1000 * sizeof(char *));
  struct command *args = malloc(1000 * sizeof(struct command));

  char cwd[1024];
  char *cwd_base;

  signal(SIGINT, shell_sig_ign_handler);
  signal(SIGQUIT, shell_sig_ign_handler);
  signal(SIGTSTP, shell_sig_ign_handler);

  while (!status)
  {
    cwd_base = basename(getcwd(cwd, sizeof(cwd)));
    printf("[SHell %s]$ ", cwd_base);
    fflush(stdout);

    // read and parse line
    line = read_line();
    char *linecpy = malloc(1000 * sizeof(char *));
    strcpy(linecpy, line);

    args = split_line_into_commands(linecpy);

    if (num_of_commands == 0)
    {
      break;
    }

    // invalid command check
    if (err == 1)
    {
      fprintf(stderr, "Error: invalid command\n");
      continue;
    }

    // execute command
    status = execute_all(args, line);
  }

  return 0;
}
