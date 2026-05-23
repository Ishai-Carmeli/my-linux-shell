# my-linux-shell

A minimal Linux shell implemented in C as an academic project for the **Extended Systems Programming Lab** course.

The goal of the project is to learn how a Unix shell works under the hood, and to get hands-on experience with core Linux system calls and process management:

- `fork`, `execvp`, `waitpid` — creating and managing child processes
- `pipe` and `dup2` — wiring stdin/stdout between processes
- Signals and job control — handling background processes (`&`), suspend/resume
- File descriptors and I/O redirection

## Features

- Run external commands with arguments
- Pipes (`cmd1 | cmd2`)
- Background execution (`&`)
- Built-in history and basic process control

## Build & Run

```sh
make
./build/myshell
```

Additional helper programs (`looper`, `Printers`, `mypipe`) are built alongside and used to demonstrate process and pipe behavior.
