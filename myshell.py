import os
import subprocess
import json
import shlex
from datetime import date


# ============================================================
# STUDENT DATA
# ============================================================

DATA_FILE = "student_data.json"


def load_data():
    if os.path.exists(DATA_FILE):
        try:
            with open(DATA_FILE, "r") as file:
                return json.load(file)
        except:
            pass

    return {
        "score": 0,
        "questions_answered": 0,
        "correct_answers": 0,
        "streak": 0,
        "last_active": "",
        "learned_commands": [],
        "completed_lessons": []
    }


student = load_data()


def save_data():
    with open(DATA_FILE, "w") as file:
        json.dump(student, file, indent=4)


# ============================================================
# STREAK
# ============================================================

def update_streak():

    today = str(date.today())

    if student["last_active"] == "":
        student["streak"] = 1

    elif student["last_active"] == today:
        return

    else:
        previous = date.fromisoformat(student["last_active"])
        difference = (date.today() - previous).days

        if difference == 1:
            student["streak"] += 1
        else:
            student["streak"] = 1

    student["last_active"] = today
    save_data()


# ============================================================
# LESSON DATA
# ============================================================

lessons = {

    "pwd": {
        "title": "pwd - Print Working Directory",

        "meaning": """
pwd means "Print Working Directory".

It tells you where you currently are
inside the Linux file system.
""",

        "when": """
Use pwd when you are not sure which directory
you are currently inside.

For example, if you have moved through several
folders and want to know your current location,
use pwd.
""",

        "example": """
MyShell> pwd

Example output:

/home/student/LinuxProject
""",

        "inside": """
The shell and processes have a current working
directory.

When pwd runs, it obtains that location and
prints it.

Think of Linux as a building:

    Building  = computer
    Rooms     = directories
    pwd       = "Which room am I in?"
""",

        "question": "What does pwd show?",

        "options": [
            "1. The current directory",
            "2. Running processes",
            "3. Files inside a directory",
            "4. The current user"
        ],

        "answer": "1"
    },


    "ls": {
        "title": "ls - List Files",

        "meaning": """
ls is used to list files and directories.

It lets you see what is inside a directory.
""",

        "when": """
Use ls when you want to see the files and
folders available in your current location.
""",

        "example": """
MyShell> ls

Example output:

Documents
Downloads
project.c
myshell.py
""",

        "inside": """
When you run ls, Linux provides information
about the contents of the directory.

The ls program receives that information and
prints the names of files and directories.

Think of ls as:

    "Show me what is inside this room."
""",

        "question": "What does ls normally do?",

        "options": [
            "1. Change directory",
            "2. List files and directories",
            "3. Delete files",
            "4. Show the current user"
        ],

        "answer": "2"
    },


    "cd": {
        "title": "cd - Change Directory",

        "meaning": """
cd means "Change Directory".

It is used to move from one directory
to another.
""",

        "when": """
Use cd when you want to move into another
folder.

For example:

cd Documents

moves you into the Documents directory.
""",

        "example": """
MyShell> cd Documents

After this, your shell is working inside
the Documents directory.
""",

        "inside": """
cd is special.

It needs to change the current directory
of the shell itself.

If the shell created another process just
to change its directory, the parent shell
would remain in the old directory.

This is one reason cd is normally implemented
as a shell built-in command.
""",

        "question": "Why must cd change the shell's own directory?",

        "options": [
            "1. So the shell remains in the new directory",
            "2. To create a new file",
            "3. To create a pipe",
            "4. To start a background process"
        ],

        "answer": "1"
    },


    "ps": {
        "title": "ps - Processes",

        "meaning": """
ps is used to display information about
running processes.

A process is a program that is currently
being executed.
""",

        "when": """
Use ps when you want to see processes running
on Linux.

It is useful when learning how Linux manages
programs.
""",

        "example": """
MyShell> ps

Example output:

PID     TTY       TIME     CMD
1201    pts/0     00:00    bash
1450    pts/0     00:00    ps
""",

        "inside": """
Every running process has a Process ID,
usually called a PID.

Linux uses these IDs to keep track of
different running processes.

This becomes very important when we later
learn fork(), exec(), wait(), and signals.
""",

        "question": "What does PID stand for?",

        "options": [
            "1. Process Identifier",
            "2. Program Input Data",
            "3. Pipe Identifier",
            "4. Permission ID"
        ],

        "answer": "1"
    },


    "pipe": {
        "title": "| - Pipe",

        "meaning": """
The | symbol is called a pipe.

It connects the output of one command to
the input of another command.
""",

        "when": """
Use a pipe when you want to combine commands.

For example:

ls | grep txt

The first command produces output.
The second command receives that output.
""",

        "example": """
MyShell> ls | grep txt

This means:

    ls
      ↓
    output
      ↓
      |
      ↓
    grep txt
""",

        "inside": """
The shell creates a pipe between processes.

The first process writes its output into
the pipe.

The second process reads that data from
the pipe as its input.

In a C shell, this involves concepts such as:

    pipe()
    fork()
    dup2()
    exec()

You will learn these later in the roadmap.
""",

        "question": "What does the | symbol do?",

        "options": [
            "1. Delete output",
            "2. Connect one command's output to another's input",
            "3. Create a directory",
            "4. Stop a process"
        ],

        "answer": "2"
    }
}


# ============================================================
# ROADMAP
# ============================================================

roadmap = [
    ("LEVEL 1", "Linux Basics",
     ["pwd", "ls", "cd"]),

    ("LEVEL 2", "Files and Directories",
     ["mkdir", "touch", "cp", "mv", "rm", "cat"]),

    ("LEVEL 3", "Processes",
     ["ps", "PID", "fork()", "exec()", "wait()"]),

    ("LEVEL 4", "Input and Output",
     ["stdin", "stdout", "stderr", ">", "<"]),

    ("LEVEL 5", "Pipes and IPC",
     ["|", "pipe()", "dup2()"]),

    ("LEVEL 6", "Permissions",
     ["chmod", "users", "groups"]),

    ("LEVEL 7", "Process Control",
     ["signals", "background processes"]),

    ("LEVEL 8", "Shell Development",
     ["system calls", "shell architecture", "job control"])
]


# ============================================================
# WELCOME
# ============================================================

def welcome():

    print("""
╔════════════════════════════════════════════════════╗
║                                                    ║
║              WELCOME TO MYSHELL 🎓                ║
║                                                    ║
╚════════════════════════════════════════════════════╝

MyShell is a Linux learning environment.

You do NOT need previous Linux knowledge.

This shell will teach you Linux step by step.

For every command you will learn:

    📖 What it means
    🎯 When to use it
    💻 Example
    ⚙️ What happens inside Linux
    🧪 Practice
    🧠 Quiz
    ⭐ Score

Your goal is not just to memorize commands.

Your goal is to understand how Linux works.

────────────────────────────────────────────────────

👉 To begin learning:

    Type 1

👉 To see the roadmap:

    Type 2

👉 To see your progress:

    Type 3

👉 For help:

    Type help

👉 To exit:

    Type exit
""")


# ============================================================
# MAIN MENU
# ============================================================

def main_menu():

    while True:

        print("""
╔════════════════════════════════════════════════════╗
║                    MAIN MENU                      ║
╚════════════════════════════════════════════════════╝

    1 → Start / continue learning
    2 → View learning roadmap
    3 → View my progress
    4 → Learn a specific command
    5 → Practice a command

    help → Show help
    exit → Exit MyShell

👉 Type one of the options above.
""")

        choice = input("MyShell> ").strip()

        if choice == "1":
            learning_menu()

        elif choice == "2":
            show_roadmap()

        elif choice == "3":
            show_progress()

        elif choice == "4":
            command_selection()

        elif choice == "5":
            practice_selection()

        elif choice == "help":
            show_help()

        elif choice == "exit":
            return False

        else:
            print("""
❌ I don't recognize that option.

👉 Type 1, 2, 3, 4, 5, help, or exit.
""")


# ============================================================
# LEARNING MENU
# ============================================================

def learning_menu():

    while True:

        print("""
╔════════════════════════════════════════════════════╗
║                 CHOOSE A LESSON 📚                ║
╚════════════════════════════════════════════════════╝

Choose what you want to learn.

    1 → pwd  - Find your current location
    2 → ls   - List files and folders
    3 → cd   - Move between folders
    4 → ps   - Understand processes
    5 → |    - Understand pipes

    6 → View roadmap
    7 → View progress
    0 → Return to main menu
    exit → Exit MyShell

👉 Type the NUMBER of your choice.
""")

        choice = input("Your choice> ").strip()

        if choice == "exit":
            return "exit"

        commands = {
            "1": "pwd",
            "2": "ls",
            "3": "cd",
            "4": "ps",
            "5": "pipe"
        }

        if choice in commands:

            result = teach_command(commands[choice])

            if result == "exit":
                return "exit"

        elif choice == "6":

            result = show_roadmap()

            if result == "exit":
                return "exit"

        elif choice == "7":

            result = show_progress()

            if result == "exit":
                return "exit"

        elif choice == "0":

            return

        else:

            print("""
❌ Invalid choice.

👉 Type a number from 0 to 7.
""")


# ============================================================
# COMMAND SELECTION
# ============================================================

def command_selection():

    print("""
╔════════════════════════════════════════════════════╗
║              WHAT DO YOU WANT TO LEARN?           ║
╚════════════════════════════════════════════════════╝

    1 → pwd
    2 → ls
    3 → cd
    4 → ps
    5 → |

👉 Type the number.

👉 Or type exit to leave MyShell.
""")

    choice = input("Your choice> ").strip()

    if choice == "exit":
        return "exit"

    commands = {
        "1": "pwd",
        "2": "ls",
        "3": "cd",
        "4": "ps",
        "5": "pipe"
    }

    if choice in commands:

        return teach_command(commands[choice])

    print("❌ Invalid choice.")


# ============================================================
# COMPLETE LESSON
# ============================================================

def teach_command(command):

    lesson = lessons[command]

    print("\n")
    print("╔════════════════════════════════════════════════════╗")
    print("║                 LESSON 📖                         ║")
    print("╚════════════════════════════════════════════════════╝")

    print("\n" + lesson["title"])

    print("""
────────────────────────────────────────────────────
📖 WHAT DOES IT MEAN?
────────────────────────────────────────────────────
""")

    print(lesson["meaning"])

    print("""
────────────────────────────────────────────────────
🎯 WHEN SHOULD I USE IT?
────────────────────────────────────────────────────
""")

    print(lesson["when"])

    print("""
────────────────────────────────────────────────────
💻 EXAMPLE
────────────────────────────────────────────────────
""")

    print(lesson["example"])

    print("""
────────────────────────────────────────────────────
⚙️ WHAT HAPPENS INSIDE LINUX?
────────────────────────────────────────────────────
""")

    print(lesson["inside"])

    print("""
╔════════════════════════════════════════════════════╗
║              END OF LESSON EXPLANATION            ║
╚════════════════════════════════════════════════════╝

You have now learned the basic idea.

👉 Next, you will practice the command.

Type:

    1

to start practice.

Or type:

    exit

to leave MyShell.
""")

    while True:

        choice = input("Your choice> ").strip()

        if choice == "1":

            return practice_command(command)

        elif choice == "exit":

            return "exit"

        else:

            print("👉 Type 1 to start practice, or exit.")


# ============================================================
# PRACTICE
# ============================================================

def practice_command(command):

    print("""
╔════════════════════════════════════════════════════╗
║                  PRACTICE 🧪                      ║
╚════════════════════════════════════════════════════╝
""")

    if command == "pwd":

        print("""
You learned:

    pwd → shows your current directory.

👉 Type exactly:

    pwd

and press ENTER.

👉 If you want to leave MyShell, type:

    exit
""")

        expected = "pwd"

    elif command == "ls":

        print("""
You learned:

    ls → lists files and directories.

👉 Type exactly:

    ls

and press ENTER.

👉 If you want to leave MyShell, type:

    exit
""")

        expected = "ls"

    elif command == "cd":

        print("""
You learned:

    cd → changes directory.

For this practice, move to the parent directory.

👉 Type exactly:

    cd ..

and press ENTER.

👉 If you want to leave MyShell, type:

    exit
""")

        expected = "cd .."

    elif command == "ps":

        print("""
You learned:

    ps → displays process information.

👉 Type exactly:

    ps

and press ENTER.

👉 If you want to leave MyShell, type:

    exit
""")

        expected = "ps"

    else:

        print("""
You learned:

    | → connects one command's output
        to another command's input.

👉 Type exactly:

    ls | grep c

and press ENTER.

👉 If you want to leave MyShell, type:

    exit
""")

        expected = "ls | grep c"

    while True:

        user_input = input("Practice> ").strip()

        if user_input == "exit":
            return "exit"

        if user_input == expected:

            print("""
✓ Correct! 🎉

You successfully practiced the command.
""")

            execute_command(user_input)
            break

        else:

            print("""
❌ Not quite.

Look at the command shown above and try again.

👉 Remember: you can type exit at any time.
""")

    if command not in student["learned_commands"]:

        student["learned_commands"].append(command)

    save_data()

    print("""
────────────────────────────────────────────────────

🧠 Now let's check your understanding.

👉 Type:

    1

to take the quiz.

👉 Type:

    2

to practice again.

👉 Type:

    3

to learn another command.

👉 Type:

    4

to view your progress.

👉 Type:

    exit

to leave MyShell.
""")

    while True:

        choice = input("Choose> ").strip()

        if choice == "1":
            return run_quiz(command)

        elif choice == "2":
            return practice_command(command)

        elif choice == "3":
            return learning_menu()

        elif choice == "4":
            return show_progress()

        elif choice == "exit":
            return "exit"

        else:
            print("👉 Please type 1, 2, 3, 4, or exit.")


# ============================================================
# QUIZ
# ============================================================

def run_quiz(command):

    lesson = lessons[command]

    print("""
╔════════════════════════════════════════════════════╗
║                    QUIZ 🧠                        ║
╚════════════════════════════════════════════════════╝
""")

    print(lesson["question"])
    print()

    for option in lesson["options"]:
        print("   " + option)

    print("""
👉 Type the NUMBER of your answer.

👉 Or type exit to leave MyShell.
""")

    while True:

        answer = input("Quiz> ").strip()

        if answer == "exit":
            return "exit"

        if answer in ["1", "2", "3", "4"]:
            break

        print("👉 Please type 1, 2, 3, or 4.")

    student["questions_answered"] += 1

    if answer == lesson["answer"]:

        print("""
╔════════════════════════════════════════════════════╗
║                 ✓ CORRECT! 🎉                    ║
╚════════════════════════════════════════════════════╝

Excellent!

You understood the concept.

+10 POINTS ⭐
""")

        student["correct_answers"] += 1
        student["score"] += 10

        if command not in student["completed_lessons"]:
            student["completed_lessons"].append(command)

    else:

        print("""
╔════════════════════════════════════════════════════╗
║                ✗ NOT QUITE RIGHT                 ║
╚════════════════════════════════════════════════════╝
""")

        print("The correct answer was:", lesson["answer"])

        print("""
Don't worry.

Mistakes are part of learning.

You can practice this command again.
""")

    update_streak()
    save_data()

    print("""
────────────────────────────────────────────────────

📊 YOUR CURRENT RESULT

⭐ Score:""", student["score"])

    print("🔥 Learning streak:", student["streak"], "day(s)")

    print("""
────────────────────────────────────────────────────

What would you like to do?

    1 → Learn the next command
    2 → Practice this command again
    3 → View my progress
    4 → View roadmap
    5 → Main menu
    exit → Exit MyShell
""")

    while True:

        choice = input("Choose> ").strip()

        if choice == "1":
            return next_lesson(command)

        elif choice == "2":
            return practice_command(command)

        elif choice == "3":
            return show_progress()

        elif choice == "4":
            return show_roadmap()

        elif choice == "5":
            return

        elif choice == "exit":
            return "exit"

        else:
            print("👉 Please type 1, 2, 3, 4, 5, or exit.")


# ============================================================
# NEXT LESSON
# ============================================================

def next_lesson(current):

    order = ["pwd", "ls", "cd", "ps", "pipe"]

    try:

        index = order.index(current)

        if index + 1 < len(order):

            next_command = order[index + 1]

            print("""
╔════════════════════════════════════════════════════╗
║                 NEXT LESSON 🚀                    ║
╚════════════════════════════════════════════════════╝
""")

            print("Your next lesson is:")
            print()
            print("    " + lessons[next_command]["title"])

            print("""
👉 Type:

    1

to start the lesson.

👉 Or type exit to leave MyShell.
""")

            while True:

                choice = input("Choose> ").strip()

                if choice == "1":
                    return teach_command(next_command)

                elif choice == "exit":
                    return "exit"

                else:
                    print("👉 Type 1 or exit.")

        else:

            print("""
╔════════════════════════════════════════════════════╗
║              LEVEL 1 COMPLETE! 🎉                ║
╚════════════════════════════════════════════════════╝

You completed the basic command lessons!

You now understand:

    ✓ pwd
    ✓ ls
    ✓ cd
    ✓ ps
    ✓ pipes

This is only the beginning.

Next we can build the deeper Linux concepts.
""")

            return show_progress()

    except ValueError:
        return learning_menu()


# ============================================================
# PRACTICE SELECTION
# ============================================================

def practice_selection():

    print("""
╔════════════════════════════════════════════════════╗
║                  PRACTICE 🧪                      ║
╚════════════════════════════════════════════════════╝

    1 → pwd
    2 → ls
    3 → cd
    4 → ps
    5 → |

👉 Type the number.

👉 Or type exit.
""")

    choice = input("Your choice> ").strip()

    if choice == "exit":
        return "exit"

    commands = {
        "1": "pwd",
        "2": "ls",
        "3": "cd",
        "4": "ps",
        "5": "pipe"
    }

    if choice in commands:
        return practice_command(commands[choice])

    print("❌ Invalid choice.")


# ============================================================
# PROGRESS
# ============================================================

def show_progress():

    print("""
╔════════════════════════════════════════════════════╗
║                  MY PROGRESS 📊                   ║
╚════════════════════════════════════════════════════╝
""")

    print("⭐ Total score:", student["score"])
    print("🧠 Questions answered:", student["questions_answered"])
    print("✓ Correct answers:", student["correct_answers"])
    print("🔥 Learning streak:", student["streak"], "day(s)")

    if student["questions_answered"] > 0:

        accuracy = (
            student["correct_answers"]
            / student["questions_answered"]
        ) * 100

        print("📈 Quiz accuracy:", round(accuracy, 1), "%")

    print("\n📚 Completed lessons:")

    if student["completed_lessons"]:

        for command in student["completed_lessons"]:
            print("   ✓", command)

    else:
        print("   No lessons completed yet.")

    print("\n📖 Commands practiced:")

    if student["learned_commands"]:

        for command in student["learned_commands"]:
            print("   ✓", command)

    else:
        print("   None yet.")

    print("""
────────────────────────────────────────────────────

👉 Type:

    1

to return to the main menu.

👉 Or type:

    exit

to leave MyShell.
""")

    while True:

        choice = input("Choose> ").strip()

        if choice == "1":
            return

        elif choice == "exit":
            return "exit"

        else:
            print("👉 Type 1 or exit.")


# ============================================================
# ROADMAP
# ============================================================

def show_roadmap():

    print("""
╔════════════════════════════════════════════════════╗
║              LINUX LEARNING ROADMAP 🗺️           ║
╚════════════════════════════════════════════════════╝
""")

    for level, title, topics in roadmap:

        print(level + " — " + title)

        print("   " + " → ".join(topics))

        print()

    print("""
Your learning path goes from:

Linux basics
      ↓
Files and directories
      ↓
Processes
      ↓
Input/output
      ↓
Pipes
      ↓
Permissions
      ↓
Process control
      ↓
Shell development

👉 Type 1 to return.

👉 Or type exit to leave MyShell.
""")

    while True:

        choice = input("Choose> ").strip()

        if choice == "1":
            return

        elif choice == "exit":
            return "exit"

        else:
            print("👉 Type 1 or exit.")


# ============================================================
# HELP
# ============================================================

def show_help():

    print("""
╔════════════════════════════════════════════════════╗
║                    MY SHELL HELP                 ║
╚════════════════════════════════════════════════════╝

If you are a beginner, don't worry.

MyShell will guide you.

MAIN OPTIONS:

    1 → Start / continue learning
    2 → View roadmap
    3 → View progress
    4 → Learn a command
    5 → Practice a command

You can also use:

    help
        Show this help.

    exit
        Exit MyShell.

Normal Linux commands:

    pwd
    ls
    cd
    ps

If you don't know what to do:

    Type 1

to start learning.
""")


# ============================================================
# COMMAND EXECUTION
# ============================================================

def execute_command(command_line):

    try:

        # ----------------------------------------------------
        # PIPE
        # ----------------------------------------------------

        if "|" in command_line:

            parts = command_line.split("|")

            if len(parts) != 2:

                print("Currently MyShell supports one pipe.")
                return

            left = shlex.split(parts[0].strip())
            right = shlex.split(parts[1].strip())

            if not left or not right:
                print("Invalid pipe command.")
                return

            first = subprocess.Popen(
                left,
                stdout=subprocess.PIPE
            )

            second = subprocess.Popen(
                right,
                stdin=first.stdout
            )

            first.stdout.close()

            second.wait()

            return

        # ----------------------------------------------------
        # NORMAL COMMAND
        # ----------------------------------------------------

        args = shlex.split(command_line)

        if not args:
            return

        # cd must change THIS shell's directory
        if args[0] == "cd":

            if len(args) < 2:

                print("Usage: cd <directory>")
                return

            try:

                os.chdir(args[1])

            except FileNotFoundError:

                print("❌ Directory not found.")

            except NotADirectoryError:

                print("❌ That is not a directory.")

            return

        subprocess.run(args)

    except FileNotFoundError:

        print("""
❌ Command not found.

👉 Type help if you need help.
""")

    except Exception as error:

        print("Error:", error)


# ============================================================
# START PROGRAM
# ============================================================

welcome()

update_streak()


while True:

    choice = input("MyShell> ").strip()

    if choice == "1":

        result = main_menu()

        if result is False:
            break

    elif choice == "2":

        result = show_roadmap()

        if result == "exit":
            break

    elif choice == "3":

        result = show_progress()

        if result == "exit":
            break

    elif choice == "help":

        show_help()

    elif choice == "exit":

        print("\nThanks for learning Linux with MyShell! 👋")
        save_data()
        break

    elif choice == "":

        continue

    else:

        print("""
❌ I don't recognize that command.

Remember:

    1     → Start learning
    2     → View roadmap
    3     → View progress
    help  → Get help
    exit  → Exit MyShell

👉 Start with:

    1
""")
