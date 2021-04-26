# <ins>Project 1: Simple Shell</ins>

### <ins>Project Description:</ins>

The goal of this project is to implement a basic shell which is able to execute commands, redirect the standard input/output (​stdin/stdout​) of commands to files, pipe the output of commands to other commands, and carry out commands in the background. In the optional previous assignment, you developed a shell parser that implements the same input requirements as this assignment. If you started or completed challenge 0, be sure to use that as your starting point for this assignment!

Your shell must implement a simple REPL (read – eval – print – loop) paradigm. Your shell must use “​my_shell$​” (without the quotes) as prompt. At each prompt, the user must be able to type commands (e.g., ​ls​, ​ps​, ​cat​) which shall be executed by the shell. You can access these binaries by searching directories determined by the PATH environment variable that is passed to your shell (__HINT__: read the man pages for the various execv wrappers. One of them performs the search for you).

Commands can have arguments that are separated by whitespace (one or more space characters). For example, if the user types ​cat x​, your shell will need to invoke the cat​ binary and pass ​x​ as an argument. When the shell has received a line of input, it typically waits until all commands have finished. Only then, a new prompt is displayed (however, this behavior can be altered – see below for details).

Your shell must also be able to interpret and execute the following meta-characters: ‘<’, ‘>’, ‘|’, and ‘&’:

1. *command​ < ​filename​*: In this case, a command takes its input from the file (not stdin). Note that spacing is irrelevant around the < operator. For example, ​*cat<file*​ and *​cat <file​* are valid inputs. Also, only one input redirection is allowed for a single command. (​*cat<<file* ​is invalid)

2. *command​ > ​filename​*: An input following this template indicates that a command writes its output to the specified file (not stdout). Again, spacing is irrelevant (see case a) and only one input redirection is allowed for a single command.

3. *command1 | command2*: The pipe character allows several commands to be connected, forming a pipeline. The output of the command before “​|​” is piped to the input of the command following “​|​”. A series of multiple piped commands is allowed on the command line. Spacing is irrelevant (described above). Example: *​cat a| sort | wc*​ indicates that the output of the ​cat​ command is channeled to the ​sort​ and ​sort​ sends its output to the input of the ​wc​ program.

4. *command &* The ampersand character '​&​’ should allow the user to execute a command (or pipeline of commands) in the background. In this case, the shell <ins>immediately</ins> displays a prompt for the next line regardless of whether the commands on the previous line have finished or are still in progress.

For simplification purposes, you should assume that only one ‘​&​’ character is allowed and can only appear at the end of the line. Also, if the input line consists of multiple commands, only the first command on the input line can have its input redirected, and only the last can have its output redirected. In case of a single command with no pipes, consider the command as both the first and last of a pipeline. E.g., ​​*cat < x > y​​* is valid, while ​​*cat f | cat < g​​* is not.

During regular execution (i.e., with no errors), your shell must ​only​ print the prompt (“my_shell$”
, or an empty prompt when run as ​./myshell -n​). Extra output (e.g., from debugging printf calls)
will result in test failures.

In case of errors (e.g., if the input does not follow the rules/assumptions described above, a command fails to execute, etc.), your shell should display an error message (cannot exceed a single line), then print the prompt, and wait for the next input. The error message should follow the template ​​*ERROR:* + your_error_message. To facilitate automated grading, when you start your simple shell program with the argument '​-n​', then your shell must not output any command prompt (no "my_shell$"). Just read and process commands as usual.

When the user types Ctrl-D (pressing the D button while holding control), the shell must cleanly exit. You may assume that the maximum length of individual tokens (commands and filenames) is 32 characters, and that the maximum length of an input line is 512 characters. Your shell is supposed to collect the exit codes of all processes that it spawns. That is, you are not allowed to leave zombie processes of commands that you start. Your shell should use the ​*fork(2)* system call and the *​​execve(2)* system call (​**HINT​**: you can use its *execvp(3)* library wrapper) to execute commands. It should also use *​​waitpid(2)​​* or *wait(2)* to wait for a program to complete execution (unless the program is in the background). You might also find the documentation for signals (and in particular ​​*SIGCHLD*​​) useful to be able to collect the status of processes that exit when running in the background.

### <ins>Example Shell Sessions:</ins>

    root@ea3aca4ec300:/ec440# ./myshell my_shell$whoami
    root
    my_shell$pwd
    /ec440
    my_shell$/bin/pwd
    /ec440
    my_shell$try to run this!
    ERROR: try: No such file or directory my_shell$ls -al
    total 36
    drwx------. 6 1000 1000
    drwxr-xr-x. 1 root root
    -rw-------. 1 1000 1000
    -rw-r--r--. 1 1000 1000
    -rw-r--r--. 1 1000 1000
    -rw-r--r--. 1 1000 1000
    drwx------. 3 1000 1000
    drwxrwxr-x. 3 1000 1000
    drwx------. 2 1000 1000
    drwxrwxr-x. 2 1000 1000
    -rwxr-xr-x. 1 root root
    my_shell$ls -al | wc -l
    12
    my_shell$ls -al |wc -l>lines_in_ls my_shell$cat lines_in_ls
    12 my_shell$root@ea3aca4ec300:/ec440#

**^-- Note: I pressed CTRL-D here**

    root@ea3aca4ec300:/ec440# ./myshell -n whoami
    root
    pwd
    /ec440 root@ea3aca4ec300:/ec440#

**^-- Note: I pressed CTRL-D here**

You may find it easier to automate your own tests with the following pattern:
    
    echo 'ls' | ./myshell

### <ins>Test Cases:</ins>

![Screenshot 2021-04-19 at 20 50 00](https://user-images.githubusercontent.com/60196280/115321848-b4f0c400-a152-11eb-983f-e2754272f478.png)

