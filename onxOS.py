import difflib
import re
import os
import sys
import json

VERSION = "0.2.0"
FS_SAVE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "onx_fs.json")

def default_fs():
    return {"type": "dir", "children": {}}

def load_fs():
    if os.path.exists(FS_SAVE_PATH):
        try:
            with open(FS_SAVE_PATH, "r") as f:
                return json.load(f)
        except Exception:
            pass
    return default_fs()

def save_fs(fs):
    try:
        with open(FS_SAVE_PATH, "w") as f:
            json.dump(fs, f, indent=2)
    except Exception as e:
        print(f"Warning: Couldn't save filesystem: {e}")

def normalize_path(path: str, cwd: list) -> list:
    path = path.strip()

    if path in ("~", "~/"):
        return ["~"]

    if path.startswith("~/"):
        parts = path[2:].split("/")
        base = ["~"]
    elif path.startswith("~"):
        parts = path[1:].split("/")
        base = ["~"]
    else:
        parts = path.split("/")
        base = list(cwd)

    result = list(base)
    for part in parts:
        part = part.strip()
        if part in ("", "."):
            continue
        elif part == "..":
            if len(result) > 1:
                result.pop()
        else:
            result.append(part)
    return result

def path_to_str(node_list: list) -> str:
    if node_list == ["~"]:
        return "~/"
    return "~/" + "/".join(node_list[1:]) + "/"

def get_node(fs, node_list):
    cur = fs
    for key in node_list:
        if cur.get("type") == "dir" and key in cur.get("children", {}):
            cur = cur["children"][key]
        elif key == "~" and cur.get("type") == "dir":
            cur = cur
        else:
            return None
    return cur

def get_node_and_parent(fs, node_list):
    if len(node_list) == 1:
        return (None, node_list[0], fs)
    parent = get_node(fs, node_list[:-1])
    if parent is None:
        return (None, None, None)
    child_key = node_list[-1]
    child = parent.get("children", {}).get(child_key)
    return (parent, child_key, child)

def resolve(fs, node_list):
    if node_list[0] != "~":
        return None
    if len(node_list) == 1:
        return fs
    cur = fs
    for key in node_list[1:]:
        if cur.get("type") != "dir":
            return None
        children = cur.get("children", {})
        if key not in children:
            return None
        cur = children[key]
    return cur

def resolve_parent(fs, node_list):
    if len(node_list) <= 1:
        return None
    return resolve(fs, node_list[:-1])

variables = {}

COMMANDS = [
    "help", "exit", "clear", "man",
    "let()", "print()",
    "ls", "cd", "mkdir", "touch", "rm", "cat", "tau", "pwd",
    "man help", "man exit", "man clear", "man let()", "man print()",
    "man ls", "man cd", "man mkdir", "man touch", "man rm",
    "man cat", "man tau", "man pwd",
]

def get_suggestion(raw: str) -> str | None:
    stripped = re.sub(r'\(.*', '()', raw)
    first_word = stripped.split()[0] if stripped.split() else stripped
    matches = difflib.get_close_matches(first_word, COMMANDS, n=1, cutoff=0.5)
    return matches[0] if matches else None

MAN_PAGES = {
    "help":    "Displays the available commands and version info.\n  usage: help",
    "exit":    "Exits onxOS.\n  usage: exit",
    "clear":   "Clears the terminal screen.\n  usage: clear",
    "man":     "Shows the manual for a specific command.\n  usage: man [COMMAND]",
    "pwd":     "Prints the current working directory.\n  usage: pwd",
    "ls":      (
        "Lists files and folders in the current (or specified) directory.\n"
        "  usage:\n"
        "    ls              - List current dir\n"
        "    ls ~/somedir/   - List a specific dir"
    ),
    "cd":      (
        "Changes the current directory.\n"
        "  Usage:\n"
        "    cd foldername     - Go into a folder\n"
        "    cd ..             - Go up one level\n"
        "    cd ~              - Go back to root\n"
        "    cd ~/some/path/   - Absolute path navigation"
    ),
    "mkdir":   (
        "Creates a new folder.\n"
        "  Usage:\n"
        "    mkdir foldername\n"
        "    mkdir ~/some/path/newfolder"
    ),
    "touch":   (
        "Creates a new empty file.\n"
        "  Usage:\n"
        "    touch filename.ext\n"
        "    touch ~/some/path/filename.ext\n"
        "  Note:\n"
        "    If no path is given, file is created in the current directory."
    ),
    "rm":      (
        "Removes a file or empty folder.\n"
        "  Usage:\n"
        "    rm filename.ext\n"
        "    rm foldername\n"
        "    rm -r foldername    - Removes a folder and all its contents"
    ),
    "cat":     (
        "Prints the contents of a file.\n"
        "  Usage:\n"
        "    cat filename.ext\n"
        "    cat ~/some/path/filename.ext"
    ),
    "tau":     (
        "Opens the oNx built-in file editor.\n"
        "  Usage:\n"
        "    tau filename.ext\n"
        "    tau ~/some/path/filename.ext\n"
        "  Controls (inside editor):\n"
        "    Type lines and press enter to add them\n"
        "    :w   - Save and stay\n"
        "    :q   - Quit (prompts if unsaved changes)\n"
        "    :wq  - Save and quit\n"
        "    :q!  - Quit without saving"
    ),
    "let()":   (
        "Defines a variable.\n"
        "  Usage:\n"
        "    let(varname) = value\n"
        "  Examples:\n"
        '    let(eggcount) = 5\n'
        '    let(hw) = "Hello World!"\n'
        "    let(a) = b             (copies b's value into a)\n"
        "  Note:\n"
        "    Variable references are copied by value, not by reference.\n"
        "    Changing b later won't update a."
    ),
    "print()": (
        "Prints a variable or a literal string.\n"
        "  Usage:\n"
        '    print("some text")   - Prints literal text\n'
        "    print(varname)       - Prints a variable's value"
    ),
}

def cmd_man(arg: str):
    key = arg.strip().lower()
    if re.match(r"let\(.*\)", key):
        key = "let()"
    elif re.match(r"print\(.*\)", key):
        key = "print()"
    if key in MAN_PAGES:
        print(f"  {key}\n    {MAN_PAGES[key]}")
    else:
        print(f"No manual entry for '{arg}'. Try 'help' to see all commands.")

def cmd_ls(fs, cwd, args: str):
    if args.strip():
        target = normalize_path(args.strip(), cwd)
    else:
        target = cwd

    node = resolve(fs, target)
    if node is None:
        print(f"ls: No such directory: {args.strip() or path_to_str(cwd)}")
        return
    if node["type"] != "dir":
        print(f"ls: Not a directory: {args.strip()}")
        return

    children = node.get("children", {})
    if not children:
        print("  (empty)")
        return
    for name, child in sorted(children.items()):
        if child["type"] == "dir":
            print(f"  {name}/")
        else:
            size = len(child.get("content", ""))
            print(f"  {name}  ({size}B)")

def cmd_cd(fs, cwd, args: str) -> list:
    path = args.strip()
    if not path or path in ("~", "~/"):
        return ["~"]
    target = normalize_path(path, cwd)
    node = resolve(fs, target)
    if node is None:
        print(f"cd: No such directory: {path}")
        return cwd
    if node["type"] != "dir":
        print(f"cd: Not a directory: {path}")
        return cwd
    return target

def cmd_mkdir(fs, cwd, args: str):
    path = args.strip()
    if not path:
        print("mkdir: Missing operand. Correct usage: mkdir foldername")
        return

    if path.endswith("/"):
        path = path[:-1]

    target = normalize_path(path, cwd)
    parent = resolve_parent(fs, target)
    if parent is None:
        print(f"mkdir: Parent directory does not exist for: {path}")
        return
    name = target[-1]
    if name in parent.get("children", {}):
        print(f"mkdir: '{name}' already exists")
        return
    parent.setdefault("children", {})[name] = {"type": "dir", "children": {}}
    print(f"Created directory '{name}'")
    save_fs(fs)

def cmd_touch(fs, cwd, args: str):
    path = args.strip()
    if not path:
        print("touch: Missing operand. Correct usage: touch filename.ext")
        return

    target = normalize_path(path, cwd)
    parent = resolve_parent(fs, target)
    if parent is None:
        print(f"touch: Parent directory does not exist: {path}")
        return
    name = target[-1]
    if name in parent.get("children", {}):
        print(f"touch: '{name}' already exists")
        return
    parent.setdefault("children", {})[name] = {"type": "file", "content": ""}
    print(f"created '{name}'")
    save_fs(fs)

def cmd_rm(fs, cwd, args: str):
    parts = args.strip().split()
    recursive = False
    if "-r" in parts:
        recursive = True
        parts.remove("-r")
    if not parts:
        print("rm: Missing operand. Correct usage: rm [-r] target")
        return

    path = parts[0]
    target = normalize_path(path, cwd)
    if target == ["~"]:
        print("rm: Cannot remove root directory")
        return

    parent = resolve_parent(fs, target)
    if parent is None:
        print(f"rm: No such file or directory: {path}")
        return
    name = target[-1]
    if name not in parent.get("children", {}):
        print(f"rm: No such file or directory: {path}")
        return

    node = parent["children"][name]
    if node["type"] == "dir":
        if node.get("children") and not recursive:
            print(f"rm: '{name}' is a non-empty directory. Use rm -r to remove it.")
            return
    del parent["children"][name]
    print(f"removed '{name}'")
    save_fs(fs)

def cmd_cat(fs, cwd, args: str):
    path = args.strip()
    if not path:
        print("cat: Missing operand. Correct usage: cat filename.ext")
        return
    target = normalize_path(path, cwd)
    node = resolve(fs, target)
    if node is None:
        print(f"cat: no such file: {path}")
        return
    if node["type"] != "file":
        print(f"cat: '{path}' is a directory")
        return
    content = node.get("content", "")
    if content:
        print(content)
    else:
        print("  (empty file)")

def cmd_tau(fs, cwd, args: str):
    path = args.strip()
    if not path:
        print("tau: Missing operand. Correct usage: tau filename.ext")
        return

    target = normalize_path(path, cwd)
    node = resolve(fs, target)

    if node is None:
        parent = resolve_parent(fs, target)
        if parent is None:
            print(f"tau: Parent directory does not exist: {path}")
            return
        name = target[-1]
        parent.setdefault("children", {})[name] = {"type": "file", "content": ""}
        node = parent["children"][name]
        print(f"  new file: {path}")
    elif node["type"] == "dir":
        print(f"tau: '{path}' is a directory")
        return

    name = target[-1]
    lines = node.get("content", "").split("\n") if node.get("content") else []

    if lines == [""]:
        lines = []

    dirty = False

    print(f"  ── tau: {name} ──────────────────────────────")
    print(f"  {len(lines)} Line(s) loaded. Commands: :w :q :wq :q!")
    print(f"  ─────────────────────────────────────────────")

    for i, line in enumerate(lines, 1):
        print(f"  {i:>3} │ {line}")

    while True:
        try:
            inp = input("tau> ")
        except (EOFError, KeyboardInterrupt):
            print("\ntau: Interrupted")
            break

        if inp == ":wq":
            node["content"] = "\n".join(lines)
            save_fs(fs)
            print(f"  Saved '{name}'. Exiting.")
            break
        elif inp == ":w":
            node["content"] = "\n".join(lines)
            save_fs(fs)
            dirty = False
            print(f"  Saved '{name}'.")
        elif inp == ":q!":
            print("  Exiting without saving.")
            break
        elif inp == ":q":
            if dirty:
                confirm = input("  There are unsaved changes. Do you want to quit anyway? [y/n] ").strip().lower()
                if confirm == "y":
                    print("  Exiting without saving.")
                    break
            else:
                print("  Exiting.")
                break
        elif inp.startswith(":"):
            print(f"  Unknown command '{inp}'. Use :w :q :wq :q!")
        else:
            lines.append(inp)
            dirty = True

def cmd_pwd(cwd):
    print(path_to_str(cwd))

def cmd_let(rawcmd: str):
    match = re.match(r"let\((\w+)\)\s*=\s*(.+)", rawcmd)
    if not match:
        print("Improper usage of 'let'. Correct usage:\n  let(varname) = value")
        return
    varname = match.group(1)
    varvalue_raw = match.group(2).strip()

    if (varvalue_raw.startswith('"') and varvalue_raw.endswith('"')) or \
       (varvalue_raw.startswith("'") and varvalue_raw.endswith("'")):
        varvalue = varvalue_raw[1:-1]
    else:
        try:
            varvalue = int(varvalue_raw)
        except ValueError:
            try:
                varvalue = float(varvalue_raw)
            except ValueError:
                if varvalue_raw in variables:
                    varvalue = variables[varvalue_raw]
                else:
                    print(f"Undefined variable '{varvalue_raw}'")
                    return
    variables[varname] = varvalue
    print(f"Set {varname} = {repr(variables[varname])}")

def cmd_print(rawcmd: str):
    lit_match = re.match(r'print\(["\'](.*)["\']\)$', rawcmd)
    if lit_match:
        print(lit_match.group(1))
        return
    var_match = re.match(r"print\((\w+)\)$", rawcmd)
    if var_match:
        varname = var_match.group(1)
        if varname in variables:
            print(variables[varname])
        else:
            print(f"Undefined variable '{varname}'")
        return
    print("Invalid usage. See: man print()")

HELP_TEXT = f"""
┌─────────────────────────────────────────────┐
│  onxOS  v{VERSION}                              │
│  A fake OS simulator, built in Python       │
└─────────────────────────────────────────────┘

  filesystem
    pwd                     Print current directory
    ls [path]               List directory contents
    cd [path]               Change directory
    mkdir [path]            Create a directory
    touch [path]            Create an empty file
    rm [-r] [path]          Remove a file or directory
    cat [path]              Print file contents
    tau [path]              Open file editor

  variables
    let(name) = value       Define a variable
    print(name)             Print a variable
    print("text")           Print literal text

  system
    help                    Show this message
    man [command]           Show manual for command
    clear                   Clear the screen
    exit                    Exit onxOS

  Tip: use ~ for root (e.g. ~/docs/), '..' to go up
""".strip()

def run_terminal():
    fs = load_fs()
    cwd = ["~"]

    print(f"  onxOS v{VERSION}, type 'help' to get started")

    while True:
        prompt_cwd = path_to_str(cwd)
        try:
            rawcmd = input(f"{prompt_cwd}>> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nexiting.")
            break
        if not rawcmd:
            continue
        
        tokens = rawcmd.split(None, 1)
        cmd = tokens[0].lower()
        args = tokens[1] if len(tokens) > 1 else ""

        if rawcmd in ("exit", "quit"):
            print("goodbye.")
            break
        elif rawcmd == "help":
            print(HELP_TEXT)
        elif rawcmd == "clear":
            print("\033[2J\033[H", end="", flush=True)
        elif rawcmd == "pwd":
            cmd_pwd(cwd)
        elif cmd == "man":
            if not args.strip():
                print("Improper usage of 'man'. Correct usage:\n  man [COMMAND]")
            else:
                cmd_man(args)
        elif cmd == "ls":
            cmd_ls(fs, cwd, args)
        elif cmd == "cd":
            cwd = cmd_cd(fs, cwd, args)
        elif cmd == "mkdir":
            cmd_mkdir(fs, cwd, args)
        elif cmd == "touch":
            cmd_touch(fs, cwd, args)
        elif cmd == "rm":
            cmd_rm(fs, cwd, args)
        elif cmd == "cat":
            cmd_cat(fs, cwd, args)
        elif cmd == "tau":
            cmd_tau(fs, cwd, args)
        elif rawcmd.startswith("let("):
            cmd_let(rawcmd)
        elif rawcmd.startswith("print("):
            cmd_print(rawcmd)
        else:
            suggestion = get_suggestion(rawcmd)
            if suggestion:
                print(f"Unknown command '{cmd}'. did you mean '{suggestion}'?")
            else:
                print(f"Unknown command '{cmd}'. type 'help' to see all commands.")

run_terminal()