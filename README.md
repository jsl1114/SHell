<p align="center">
<h1 align="center">SHell</h1>

<p align="center">SHell is an <b>simple, lightweight</b> Linux shell program written by <a href="https://jsl1114.github.io"><i>Jason</i></a>.</p>

<p align="center"><b><i>Please not that you should run it in an Linux enviroment.</i></b></p>
</p>

## Features
- Program locaiton in the form of `[SHell <cwd>]:`
- Process termination and suspension
- Signal handling
- I/O redirection
  - Input redirection
  - Output redirection
  - Pipe
- Built in commands
  - `cd <dir>`
  - `jobs`
  - `fg <index>`
  - `exit`

## Some possible use cases
### I/O Redirection
-`cat shell.c | grep main | less`
-`cat < input.txt`
-`cat > output.txt`
-`cat >> output.txt`
-`cat < input.txt > output.txt`
-`cat < input.txt >> output.txt`
-`cat > output.txt < input.txt`
-`cat >> output.txt < input.txt`
-`cat < input.txt | cat > output.txt`
-`cat < input.txt | cat | cat >> output.txt`

### Built-in Commands
- `cd <dir>`
- `jobs`
  - prints all suspended jobs in the format of `[index] command`.
  - For example:
  ```console
  [SHell demo]$ jobs
  [1] ./hello
  [2] /usr/bin/top -c
  [3] cat > output.txt
  [SHell demo]$ █ 
  ```
- `fg <index>`
  - resumes a job in the foreground, takes the job's index as the only argument
  ```console
  [SHell demo]$ jobs
  [1] ./hello
  [2] /usr/bin/top -c
  [3] cat > output.txt
  [SHell demo]$ fg 2
  ```
  - The last command will resume `/usr/bin/top -c`
- `exit`
  - The command terminates the shell. If there are ongoing jobs, the user will be warned and exit will fail