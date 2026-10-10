# MyShell — Interactive Linux Learning Shell

A command-line shell developed in C as part of my Operating Systems (OSSP) project. The goal is to understand Linux shell concepts through practical implementation.

## Features

- User input handling
- Command parsing
- External command execution
- Built-in commands (`cd`, `pwd`, `history`, `export`, `info`, `exit`)
- Environment variable expansion
- Command validation and history search
- Persistent command history
- Pipeline support (`|`)
- Input redirection (`<`)
- Output redirection (`>`)

## Weekly Progress

- **Week 1:** Project setup and repository initialization
- **Week 2:** Input handling
- **Week 3:** Command parsing
- **Week 4:** Process execution
- **Week 5:** Built-in commands and environment variables
- **Week 6:** Command validation and history handling
- **Week 7:** Persistent command history
- **Week 8:** Pipeline support
- **Week 9:** Input and output redirection
- **Week 10:** Command chaining — planned (`;`, `&&`, `||`)

## Technologies Used

- C
- Linux / Ubuntu on WSL2
- GCC
- Git and GitHub

## Compile and Run

```bash
gcc -Wall -Wextra -std=c11 shell.c -o myshell
./myshell
```

Type `info` inside MyShell to see the available features. Type `exit` to close the shell.

## Project Status

Weeks 1–9 are committed to the repository. Week 10 command chaining is planned and has not yet been completed.
