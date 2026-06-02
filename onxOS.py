import difflib
import re
import os
import sys
import json
import termios
import tty
import shutil
import select
import io
import readline
import threading

VERSION = "2.0.0"
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
    "ls", "cd", "mkdir", "touch", "rm", "cp", "mv", "find", "grep", "cat", "tau", "pwd",
    "man help", "man exit", "man clear", "man let()", "man print()",
    "man ls", "man cd", "man mkdir", "man touch", "man rm",
    "man cp", "man mv", "man find", "man grep", "man cat", "man tau", "man pwd",
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
    "cp":      (
        "Copies files or directories.\n"
        "  Usage:\n"
        "    cp source dest          - Copy a file\n"
        "    cp -r source dest       - Copy a directory recursively"
    ),
    "mv":      (
        "Moves or renames files and directories.\n"
        "  Usage:\n"
        "    mv source dest          - Move/rename a file or directory"
    ),
    "find":    (
        "Searches for files matching a name pattern.\n"
        "  Usage:\n"
        "    find -name pattern      - Search current dir\n"
        "    find path -name pattern - Search specific dir\n"
        "  Supports * and ? wildcards."
    ),
    "history": (
        "Shows command history.\n"
        "  Usage:\n"
        "    history                 - Display all history entries\n"
        "    !n                      - Re-run history entry #n"
    ),
    "source":  (
        "Executes commands from a file.\n"
        "  Usage:\n"
        "    source filepath         - Run commands from file"
    ),
    "chmod":   (
        "Changes file permissions.\n"
        "  Usage:\n"
        "    chmod mode file         - Set permissions (e.g. 755, u+x, +x)"
    ),
    "alias":   (
        "Defines or lists command aliases.\n"
        "  Usage:\n"
        "    alias                   - List aliases\n"
        "    alias name='value'      - Define an alias"
    ),
    "unalias": (
        "Removes an alias.\n"
        "  Usage:\n"
        "    unalias name            - Remove alias"
    ),
    "export":  (
        "Sets or lists environment variables.\n"
        "  Usage:\n"
        "    export                  - List variables\n"
        "    export VAR=value        - Set a variable\n"
        "  Use $VAR to expand in commands."
    ),
    "echo":    (
        "Prints text to the terminal.\n"
        "  Usage:\n"
        "    echo text               - Print text\n"
        "  Environment variables ($VAR) are expanded."
    ),
    "which":   (
        "Locates a command.\n"
        "  Usage:\n"
        "    which command           - Show if command is built-in"
    ),
    "type":    (
        "Describes a command.\n"
        "  Usage:\n"
        "    type name               - Show if built-in, alias, etc."
    ),
    "sort":    (
        "Sorts lines from a file or stdin.\n"
        "  Usage:\n"
        "    sort [path]             - Sort lines alphabetically"
    ),
    "head":    (
        "Outputs the first N lines of a file or stdin.\n"
        "  Usage:\n"
        "    head [-N] [path]        - Default 10 lines"
    ),
    "tail":    (
        "Outputs the last N lines of a file or stdin.\n"
        "  Usage:\n"
        "    tail [-N] [path]        - Default 10 lines"
    ),
    "wc":      (
        "Counts lines, words, and characters.\n"
        "  Usage:\n"
        "    wc [path]               - Count from file or stdin"
    ),
    "jobs":    (
        "Lists background jobs.\n"
        "  Usage:\n"
        "    jobs                    - Show all background jobs\n"
        "  Append & to run a command in the background."
    ),
    "fg":      (
        "Brings a background job to the foreground.\n"
        "  Usage:\n"
        "    fg [job_id]             - Foreground a job (default: last)"
    ),
    "kill":    (
        "Terminates a background job.\n"
        "  Usage:\n"
        "    kill job_id             - Kill a background job"
    ),
    "grep":    (
        "Searches file contents for a pattern.\n"
        "  Usage:\n"
        "    grep pattern            - Search current dir files\n"
        "    grep pattern path       - Search specific file or dir"
    ),
    "cat":     (
        "Prints the contents of a file.\n"
        "  Usage:\n"
        "    cat filename.ext\n"
        "    cat ~/some/path/filename.ext"
    ),
    "tau":     (
        "Full-screen terminal editor with nvim-like controls.\n"
        "  Usage:\n"
        "    tau filename.ext\n"
        "    tau ~/some/path/filename.ext\n"
        "  Normal mode:\n"
        "    j/k or arrows  - Move cursor up/down\n"
        "    h/l or arrows  - Move cursor left/right\n"
        "    0/$             - Jump to start/end of line\n"
        "    gg/G            - Jump to top/bottom\n"
        "    i/I             - Insert at cursor/line start\n"
        "    a/A             - Append after cursor/line end\n"
        "    o/O             - Insert new line below/above\n"
        "    x               - Delete character at cursor\n"
        "    dd              - Delete current line\n"
        "    yy              - Yank (copy) current line\n"
        "    p/P             - Paste below/above\n"
        "    u/Ctrl+R        - Undo/Redo\n"
        "    r<chr>          - Replace character at cursor\n"
        "    :               - Enter command mode\n"
        "  Insert mode: type to edit, Esc to return\n"
        "  Command mode:\n"
        "    :w   - Save\n"
        "    :q   - Quit (prompts if unsaved)\n"
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
        print(f"  {'─' * (len(key) + 4)}")
        print(f"   {key.upper()}")
        print(f"  {'─' * (len(key) + 4)}")
        print(f"\n  {MAN_PAGES[key]}")
    else:
        print(f"  No manual entry for '{arg}'. Try 'help' to see all commands.")

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

_aliases = {}
_env = {}
_jobs = {}
_next_job_id = 1

def cmd_alias(args: str):
    if not args.strip():
        for name, val in sorted(_aliases.items()):
            print(f"  alias {name}='{val}'")
        return
    parts = args.split("=", 1)
    if len(parts) != 2:
        print(f"alias: Invalid syntax. Use: alias name=value")
        return
    name = parts[0].strip()
    val = parts[1].strip().strip("'\"")
    _aliases[name] = val

def cmd_unalias(args: str):
    name = args.strip()
    if not name:
        print("unalias: Missing name")
        return
    if name in _aliases:
        del _aliases[name]
    else:
        print(f"unalias: '{name}': not found")

def cmd_export(args: str):
    if not args.strip():
        for k, v in sorted(_env.items()):
            print(f"  export {k}={v}")
        return
    parts = args.split("=", 1)
    if len(parts) != 2:
        print(f"export: Invalid syntax. Use: export VAR=value")
        return
    _env[parts[0].strip()] = parts[1].strip()

def cmd_echo(args: str):
    result = args.strip()
    for k, v in _env.items():
        result = result.replace(f"${k}", v)
    print(result)

def cmd_which(args: str):
    name = args.strip()
    if not name:
        print("which: Missing command name")
        return
    if name in COMMANDS:
        print(f"  {name} (built-in)")
    else:
        print(f"  {name}: not found")

def cmd_type(args: str):
    name = args.strip()
    if not name:
        print("type: Missing name")
        return
    if name in COMMANDS:
        print(f"  {name} is a shell built-in")
    elif name in _aliases:
        print(f"  {name} is aliased to '{_aliases[name]}'")
    else:
        print(f"  {name}: not found")

def _read_lines(fs, cwd, args):
    path = args.strip()
    stdin = _env.get("_STDIN", "")
    if not stdin and not path:
        buf = io.StringIO()
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            buf.write(line)
        stdin = buf.getvalue()
    if path:
        target = normalize_path(path, cwd)
        node = resolve(fs, target)
        if node and node["type"] == "file":
            return [l for l in node.get("content", "").split("\n") if l != ""]
        print(f"  No such file: {path}")
        return None
    elif stdin:
        return [l for l in stdin.split("\n") if l != ""]
    else:
        print("  Missing input (stdin or filename)")
        return None

def cmd_sort(fs, cwd, args: str):
    lines = _read_lines(fs, cwd, args)
    if lines is not None:
        for line in sorted(lines):
            print(line)

def cmd_head(fs, cwd, args: str):
    parts = args.strip().split()
    n = 10
    path = ""
    for p in parts:
        if p.startswith("-"):
            try:
                n = int(p[1:])
            except ValueError:
                pass
        else:
            path = p
    lines = _read_lines(fs, cwd, path)
    if lines is not None:
        for line in lines[:n]:
            print(line)

def cmd_tail(fs, cwd, args: str):
    parts = args.strip().split()
    n = 10
    path = ""
    for p in parts:
        if p.startswith("-"):
            try:
                n = int(p[1:])
            except ValueError:
                pass
        else:
            path = p
    lines = _read_lines(fs, cwd, path)
    if lines is not None:
        for line in lines[-n:]:
            print(line)

def cmd_kill(args: str):
    spec = args.strip()
    if not spec:
        print("kill: Missing argument. Usage: kill job_id")
        return
    try:
        jid = int(spec)
    except ValueError:
        print(f"kill: Invalid job id '{spec}'")
        return
    if jid not in _jobs:
        print(f"kill: Job [{jid}] not found")
        return
    t = _jobs.pop(jid, None)
    if t and t.is_alive():
        print(f"  [{jid}] terminated")
    else:
        print(f"  [{jid}] already done")

def cmd_jobs(args: str):
    if not _jobs:
        print("  (no background jobs)")
        return
    for jid, t in sorted(_jobs.items()):
        alive = t.is_alive()
        print(f"  [{jid}] {'running' if alive else 'done'}")

def cmd_fg(args: str):
    if not _jobs:
        print("  (no background jobs)")
        return
    if args.strip():
        try:
            jid = int(args.strip())
        except ValueError:
            print(f"fg: Invalid job id '{args.strip()}'")
            return
    else:
        jid = max(_jobs.keys())
    if jid not in _jobs:
        print(f"  Job [{jid}] not found")
        return
    t = _jobs.pop(jid)
    if t.is_alive():
        t.join()

def cmd_wc(fs, cwd, args: str):
    path = args.strip()
    stdin = _env.get("_STDIN", "")
    if not stdin and not path:
        buf = io.StringIO()
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            buf.write(line)
        stdin = buf.getvalue()
    if path:
        target = normalize_path(path, cwd)
        node = resolve(fs, target)
        if node and node["type"] == "file":
            content = node.get("content", "")
            non_empty = [l for l in content.split("\n") if l != ""]
            word_count = sum(len(l.split()) for l in non_empty)
            char_count = len(content)
            print(f"  {len(non_empty)} {word_count} {char_count} {path}")
            return
        print(f"  No such file: {path}")
    elif stdin:
        non_empty = [l for l in stdin.split("\n") if l != ""]
        word_count = sum(len(l.split()) for l in non_empty)
        char_count = len(stdin)
        print(f"  {len(non_empty)} {word_count} {char_count}")
    else:
        print("  Missing input (stdin or filename)")

def cmd_chmod(fs, cwd, args: str):
    parts = args.strip().split(None, 2)
    if len(parts) < 2:
        print("chmod: Missing operand. Usage: chmod mode file")
        return
    mode_str = parts[0]
    target = normalize_path(parts[1], cwd)
    node = resolve(fs, target)
    if not node:
        print(f"chmod: '{parts[1]}': No such file or directory")
        return
    if mode_str.startswith("u+"):
        node["perm"] = node.get("perm", 0o644) | {"r": 0o400, "w": 0o200, "x": 0o100}.get(mode_str[2:], 0)
    elif mode_str.startswith("g+"):
        node["perm"] = node.get("perm", 0o644) | {"r": 0o040, "w": 0o020, "x": 0o010}.get(mode_str[2:], 0)
    elif mode_str.startswith("o+"):
        node["perm"] = node.get("perm", 0o644) | {"r": 0o004, "w": 0o002, "x": 0o001}.get(mode_str[2:], 0)
    elif mode_str.startswith("a+"):
        node["perm"] = node.get("perm", 0o644) | {"r": 0o444, "w": 0o222, "x": 0o111}.get(mode_str[2:], 0)
    elif mode_str == "+x":
        node["perm"] = node.get("perm", 0o644) | 0o111
    else:
        try:
            node["perm"] = int(mode_str, 8)
        except ValueError:
            print(f"chmod: Invalid mode '{mode_str}'")
            return
    print(f"  chmod {parts[1]} -> {oct(node['perm'])}")
    save_fs(fs)

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

def _deep_copy_node(node):
    if node["type"] == "dir":
        return {"type": "dir", "children": {k: _deep_copy_node(v) for k, v in node.get("children", {}).items()}}
    return {"type": "file", "content": node.get("content", "")}

def cmd_cp(fs, cwd, args: str):
    parts = args.strip().split()
    recursive = False
    if "-r" in parts:
        recursive = True
        parts.remove("-r")
    if len(parts) < 2:
        print("cp: Missing operand. Usage: cp [-r] source dest")
        return

    src_path, dst_path = parts[0], parts[1]
    src_target = normalize_path(src_path, cwd)
    dst_target = normalize_path(dst_path, cwd)

    src_node = resolve(fs, src_target)
    if src_node is None:
        print(f"cp: No such file or directory: {src_path}")
        return

    dst_parent = resolve_parent(fs, dst_target)
    if dst_parent is None:
        print(f"cp: Destination parent directory does not exist: {dst_path}")
        return

    dst_name = dst_target[-1]
    if dst_name in dst_parent.get("children", {}):
        print(f"cp: '{dst_path}' already exists")
        return

    if src_node["type"] == "dir":
        if not recursive:
            print(f"cp: -r not specified; omitting directory '{src_path}'")
            return
        dst_parent.setdefault("children", {})[dst_name] = _deep_copy_node(src_node)
        print(f"copied directory '{src_path}' -> '{dst_path}'")
    else:
        dst_parent.setdefault("children", {})[dst_name] = {"type": "file", "content": src_node.get("content", "")}
        print(f"copied '{src_path}' -> '{dst_path}'")
    save_fs(fs)

def cmd_source(fs, cwd_in, args: str):
    path = args.strip()
    if not path:
        print("source: Missing filename")
        return
    target = normalize_path(path, cwd_in)
    node = resolve(fs, target)
    if not node or node["type"] != "file":
        print(f"source: '{path}': No such file")
        return
    content = node.get("content", "")
    cwd = list(cwd_in)
    for line in content.split("\n"):
        line = line.strip()
        if line and not line.startswith("#"):
            cwd = _exec_line(line, fs, cwd)
    cwd_in[:] = cwd

def cmd_mv(fs, cwd, args: str):
    parts = args.strip().split()
    if len(parts) < 2:
        print("mv: Missing operand. Usage: mv source dest")
        return

    src_path, dst_path = parts[0], parts[1]
    src_target = normalize_path(src_path, cwd)
    dst_target = normalize_path(dst_path, cwd)

    if src_target == dst_target:
        print(f"mv: '{src_path}' and '{dst_path}' are the same")
        return

    src_node = resolve(fs, src_target)
    if src_node is None:
        print(f"mv: No such file or directory: {src_path}")
        return

    src_parent = resolve_parent(fs, src_target)
    src_name = src_target[-1]

    dst_parent = resolve_parent(fs, dst_target)
    if dst_parent is None:
        print(f"mv: Destination parent directory does not exist: {dst_path}")
        return

    dst_name = dst_target[-1]
    if dst_name in dst_parent.get("children", {}):
        print(f"mv: '{dst_path}' already exists")
        return

    node_copy = _deep_copy_node(src_node)
    del src_parent["children"][src_name]
    dst_parent.setdefault("children", {})[dst_name] = node_copy
    print(f"moved '{src_path}' -> '{dst_path}'")
    save_fs(fs)

def cmd_find(fs, cwd, args: str):
    parts = args.strip().split()
    name_pattern = None
    search_dir = None
    i = 0
    while i < len(parts):
        if parts[i] == "-name":
            i += 1
            if i < len(parts):
                name_pattern = parts[i]
        elif not parts[i].startswith("-") and search_dir is None:
            search_dir = parts[i]
        i += 1
    if name_pattern is None:
        print("find: Missing -name pattern. Usage: find [path] -name pattern")
        return
    regex = "^" + re.escape(name_pattern).replace(r"\*", ".*").replace(r"\?", ".") + "$"
    target = normalize_path(search_dir, cwd) if search_dir else cwd
    node = resolve(fs, target)
    if node is None or node["type"] != "dir":
        print(f"find: '{search_dir or path_to_str(cwd)}': No such directory")
        return

    def _search(n, path_parts):
        r = []
        for nm, ch in n.get("children", {}).items():
            p = path_parts + [nm]
            if re.match(regex, nm):
                r.append(path_to_str(p))
            if ch["type"] == "dir":
                r.extend(_search(ch, p))
        return r

    results = _search(node, target)
    if results:
        for r in sorted(results):
            print(f"  {r}")
    else:
        print("  (no matches)")

def cmd_grep(fs, cwd, args: str):
    parts = args.strip().split()
    if not parts:
        print("grep: Missing pattern. Usage: grep pattern [path]")
        return
    pattern, search_path = parts[0], parts[1] if len(parts) > 1 else ""
    nodes_to_search = []
    if search_path:
        target = normalize_path(search_path, cwd)
        node = resolve(fs, target)
        if node is None:
            print(f"grep: No such file or directory: {search_path}")
            return
        if node["type"] == "dir":
            nodes_to_search = [(n, c) for n, c in node.get("children", {}).items() if c["type"] == "file"]
        else:
            nodes_to_search = [(search_path.split("/")[-1] or search_path, node)]
    else:
        cur = resolve(fs, cwd)
        if cur:
            nodes_to_search = [(n, c) for n, c in cur.get("children", {}).items() if c["type"] == "file"]
    found = False
    for fname, fnode in sorted(nodes_to_search):
        for ln, line in enumerate(fnode.get("content", "").split("\n"), 1):
            if pattern in line:
                print(f"  {fname}:{ln}: {line}")
                found = True
    if not found:
        print("  (no matches)")

def cmd_history(args: str):
    hlen = readline.get_current_history_length()
    if hlen == 0:
        print("  (no history)")
        return
    for i in range(1, hlen + 1):
        item = readline.get_history_item(i)
        if item:
            print(f"  {i:>4}  {item}")

def _run_one(rawcmd, fs, cwd, stdin_str=None):
    first = rawcmd.split(None, 1)[0]
    if first in _aliases and first not in ("alias", "unalias", "export"):
        suffix = rawcmd[len(first):]
        rawcmd = _aliases[first] + suffix
    for k, v in sorted(_env.items(), key=lambda x: -len(x[0])):
        rawcmd = rawcmd.replace(f"${k}", v)
    tokens = rawcmd.split(None, 1)
    cmd = tokens[0].lower()
    args = tokens[1] if len(tokens) > 1 else ""

    if rawcmd == "help":
        print(HELP_TEXT)
    elif rawcmd == "clear":
        print("\033[2J\033[H", end="", flush=True)
    elif rawcmd == "pwd":
        cmd_pwd(cwd)
    elif cmd == "history":
        cmd_history(args)
    elif cmd == "alias":
        cmd_alias(args)
    elif cmd == "unalias":
        cmd_unalias(args)
    elif cmd == "export":
        cmd_export(args)
    elif cmd == "echo":
        cmd_echo(args)
    elif cmd == "which":
        cmd_which(args)
    elif cmd == "type":
        cmd_type(args)
    elif cmd == "sort":
        cmd_sort(fs, cwd, args)
    elif cmd == "head":
        cmd_head(fs, cwd, args)
    elif cmd == "tail":
        cmd_tail(fs, cwd, args)
    elif cmd == "wc":
        cmd_wc(fs, cwd, args)
    elif cmd == "jobs":
        cmd_jobs(args)
    elif cmd == "fg":
        cmd_fg(args)
    elif cmd == "kill":
        cmd_kill(args)
    elif cmd == "chmod":
        cmd_chmod(fs, cwd, args)
    elif cmd == "source":
        cmd_source(fs, cwd, args)
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
    elif cmd == "cp":
        cmd_cp(fs, cwd, args)
    elif cmd == "mv":
        cmd_mv(fs, cwd, args)
    elif cmd == "find":
        cmd_find(fs, cwd, args)
    elif cmd == "grep":
        cmd_grep(fs, cwd, args)
    elif cmd == "cat":
        cmd_cat(fs, cwd, args)
    elif cmd == "tau":
        cmd_tau(fs, cwd, args)
    elif rawcmd.startswith("let("):
        cmd_let(rawcmd)
    elif rawcmd.startswith("print("):
        cmd_print(rawcmd)
    elif rawcmd == "exit" or rawcmd == "quit":
        pass
    else:
        suggestion = get_suggestion(rawcmd)
        if suggestion:
            print(f"Unknown command '{cmd}'. did you mean '{suggestion}'?")
        else:
            print(f"Unknown command '{cmd}'. type 'help' to see all commands.")
    return cwd

def _exec_line(rawcmd, fs, cwd):
    global _next_job_id
    bg = rawcmd.strip().endswith("&")
    if bg:
        rawcmd = rawcmd.strip()[:-1].strip()
    if "|" in rawcmd:
        parts = [p.strip() for p in rawcmd.split("|")]
        output = None
        for part in parts:
            buf = io.StringIO()
            old_stdout = sys.stdout
            old_stdin = sys.stdin
            old_stdin_env = _env.get("_STDIN")
            sys.stdout = buf
            if output is not None:
                sys.stdin = io.StringIO(output)
                _env["_STDIN"] = output
            try:
                cwd = _run_one(part, fs, cwd, output)
            finally:
                sys.stdout = old_stdout
                sys.stdin = old_stdin
                if old_stdin_env is None:
                    _env.pop("_STDIN", None)
                else:
                    _env["_STDIN"] = old_stdin_env
            output = buf.getvalue()
        if output:
            print(output, end="")
        return cwd
    def _do_run(rawcmd, fs, cwd):
        redirect_map = {
            ">>": ("append", "stdout"),
            ">":  ("write", "stdout"),
            "2>>": ("append", "stderr"),
            "2>":  ("write", "stderr"),
            "<":  ("read", "stdin"),
        }
        for sym, (mode, stream) in redirect_map.items():
            if sym in rawcmd:
                left, right = rawcmd.split(sym, 1)
                right = right.strip()
                if stream == "stdin":
                    target = normalize_path(right, cwd)
                    node = resolve(fs, target)
                    if node and node["type"] == "file":
                        _env["_STDIN"] = node.get("content", "")
                        try:
                            cwd = _run_one(left.strip(), fs, cwd)
                        finally:
                            _env.pop("_STDIN", None)
                    else:
                        print(f"Redirect: '{right}': No such file")
                elif stream == "stdout":
                    target = normalize_path(right, cwd)
                    buf = io.StringIO()
                    old_stdout = sys.stdout
                    sys.stdout = buf
                    try:
                        cwd = _run_one(left.strip(), fs, cwd)
                    finally:
                        sys.stdout = old_stdout
                    text = buf.getvalue()
                    parent, child_key, _ = get_node_and_parent(fs, target)
                    if parent is not None and child_key is not None:
                        if mode == "write":
                            parent.setdefault("children", {})[child_key] = {"type": "file", "content": text}
                        else:
                            existing = parent["children"].get(child_key, {}).get("content", "")
                            parent["children"][child_key] = {"type": "file", "content": existing + text}
                        save_fs(fs)
                return cwd
        return _run_one(rawcmd, fs, cwd)

    if bg:
        jid = _next_job_id
        _next_job_id += 1
        def _bg():
            _do_run(rawcmd, fs, list(cwd))
        t = threading.Thread(target=_bg)
        t.start()
        _jobs[jid] = t
        print(f"  [{jid}]")
        return cwd
    return _do_run(rawcmd, fs, cwd)

def cmd_cat(fs, cwd, args: str):
    path = args.strip()
    if not path:
        stdin = _env.get("_STDIN")
        if stdin:
            print(stdin, end="")
            return
        print("cat: Missing operand. Correct usage: cat filename.ext")
        return
    target = normalize_path(path, cwd)
    node = resolve(fs, target)
    if node is None:
        print(f"cat: No such file: {path}")
        return
    if node["type"] != "file":
        print(f"cat: '{path}' is a directory")
        return
    content = node.get("content", "")
    if content:
        print(content)
    else:
        print("  (Empty file)")

def _tau_get_key():
    fd = sys.stdin.fileno()
    b = os.read(fd, 1)
    if not b:
        return ''
    if b[0] == 0x1b:
        r, _, _ = select.select([fd], [], [], 0.03)
        if r:
            b2 = os.read(fd, 1)
            if b2 and b2[0] in (0x5b, 0x4f):
                r, _, _ = select.select([fd], [], [], 0.03)
                if r:
                    b2 += os.read(fd, 1)
                if b2[0] == 0x4f:
                    b2 = b'\x5b' + b2[1:2]
                return (b'\x1b' + b2).decode('utf-8', errors='replace')
            return (b'\x1b' + b2).decode('utf-8', errors='replace') if b2 else '\x1b'
        return '\x1b'
    if b[0] & 0xE0 == 0xC0:
        b += os.read(fd, 1)
    elif b[0] & 0xF0 == 0xE0:
        b += os.read(fd, 2)
    elif b[0] & 0xF8 == 0xF0:
        b += os.read(fd, 3)
    return b.decode('utf-8', errors='replace')


def _tau_render(state):
    rows, cols = shutil.get_terminal_size()
    lines = state["lines"]
    cursor_ln = state["cursor_line"]
    cursor_col = state["cursor_col"]
    mode = state["mode"]
    scroll = state["scroll"]

    content_rows = rows - 2
    if content_rows < 1:
        content_rows = 1

    total = max(len(lines), 1)
    ln_w = len(str(total))

    if cursor_ln < scroll:
        scroll = cursor_ln
    elif cursor_ln >= scroll + content_rows:
        scroll = cursor_ln - content_rows + 1
    if scroll < 0:
        scroll = 0
    state["scroll"] = scroll

    mode_str = {"normal": "NORMAL", "insert": "INSERT", "command": "CMD"}[mode]
    mod = " [+]" if state["dirty"] else ""
    status = f" {state['filename']}{mod}  {mode_str}  {cursor_ln+1}:{cursor_col+1} "
    if len(status) > cols:
        status = status[:cols]
    else:
        status = status.ljust(cols)

    max_content = cols - ln_w - 2
    if max_content < 0:
        max_content = 0

    parts = []
    parts.append(f"\033[1;1H{status}\033[K")

    for i in range(content_rows):
        row = i + 2
        idx = scroll + i
        if idx < len(lines):
            line = lines[idx]
            display = line[:max_content]
            ln_str = str(idx + 1).rjust(ln_w)
            if idx == cursor_ln:
                parts.append(f"\033[{row};1H>{ln_str} {display}\033[K")
            else:
                parts.append(f"\033[{row};1H {ln_str} {display}\033[K")
        elif not lines and idx == 0:
            ln_str = "1".rjust(ln_w)
            if idx == cursor_ln:
                parts.append(f"\033[{row};1H>{ln_str}\033[K")
            else:
                parts.append(f"\033[{row};1H {ln_str} \033[K")
        else:
            parts.append(f"\033[{row};1H {'~'.rjust(ln_w, ' ')} \033[K")

    last_row = content_rows + 2
    if mode == "command":
        cmd = state["cmd_buf"]
        parts.append(f"\033[{last_row};1H:{cmd}\033[K")
    else:
        parts.append(f"\033[{last_row};1H\033[K")

    if mode == "command":
        cur_row = rows
        cur_col = 2 + len(state["cmd_buf"])
    else:
        cur_row = 2 + (cursor_ln - scroll)
        cur_col = 2 + ln_w + 1 + cursor_col

    if cur_row < 1: cur_row = 1
    if cur_row > rows: cur_row = rows
    if cur_col < 1: cur_col = 1
    if cur_col > cols: cur_col = cols

    parts.append(f"\033[{cur_row};{cur_col}H\033[?25h")

    sys.stdout.write("".join(parts))
    sys.stdout.flush()


def _tau_fallback(node, name, lines, fs):
    dirty = False
    print(f"  -- tau: {name} ({len(lines)} lines) --")
    print(f"  Commands: :w :q :wq :q!")
    for i, line in enumerate(lines, 1):
        print(f"  {i:>3} | {line}")
    while True:
        try:
            inp = input("tau> ")
        except (EOFError, KeyboardInterrupt):
            print()
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
                confirm = input("  Unsaved changes. Quit? [y/n] ").strip().lower()
                if confirm == "y":
                    break
            else:
                print("  Exiting.")
                break
        elif inp.startswith(":"):
            print(f"  Unknown command '{inp}'.")
        else:
            lines.append(inp)
            dirty = True


def cmd_tau(fs, cwd, args):
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
    elif node["type"] == "dir":
        print(f"tau: '{path}' is a directory")
        return

    name = target[-1]
    content = node.get("content", "")
    lines = content.split("\n") if content else []
    if lines == [""]:
        lines = []
    if not lines and not content:
        lines = []
    try:
        fd = sys.stdin.fileno()
        old_term = termios.tcgetattr(fd)
    except (termios.error, io.UnsupportedOperation, AttributeError):
        _tau_fallback(node, name, lines, fs)
        return

    state = {
        "lines": lines,
        "cursor_line": 0,
        "cursor_col": 0,
        "mode": "normal",
        "cmd_buf": "",
        "dirty": False,
        "filename": name,
        "scroll": 0,
        "node": node,
        "fs": fs,
        "undo_stack": [],
        "redo_stack": [],
        "yank": None,
        "insert_snapshot": None,
    }

    try:
        tty.setraw(fd)
        sys.stdout.write("\033[?25l\033[2J\033[H")
        sys.stdout.flush()

        while True:
            _tau_render(state)
            key = _tau_get_key()

            if state["mode"] == "command":
                if key in ('\r', '\n'):
                    cmd = state["cmd_buf"]
                    if cmd == "w":
                        state["node"]["content"] = "\n".join(state["lines"])
                        save_fs(state["fs"])
                        state["dirty"] = False
                    elif cmd == "q":
                        if state["dirty"]:
                            state["cmd_buf"] = "q!"
                            continue
                        break
                    elif cmd == "wq":
                        state["node"]["content"] = "\n".join(state["lines"])
                        save_fs(state["fs"])
                        break
                    elif cmd == "q!":
                        break
                    elif cmd == "w!":
                        state["node"]["content"] = "\n".join(state["lines"])
                        save_fs(state["fs"])
                        state["dirty"] = False
                    state["mode"] = "normal"
                    state["cmd_buf"] = ""
                elif key == '\x1b':
                    state["mode"] = "normal"
                    state["cmd_buf"] = ""
                elif key in ('\x7f', '\b'):
                    state["cmd_buf"] = state["cmd_buf"][:-1]
                elif len(key) == 1:
                    state["cmd_buf"] += key

            elif state["mode"] == "insert":
                if key == '\x1b':
                    if state["insert_snapshot"] is not None and state["lines"] != state["insert_snapshot"]:
                        state["undo_stack"].append(state["insert_snapshot"])
                        state["redo_stack"].clear()
                    state["insert_snapshot"] = None
                    state["mode"] = "normal"
                elif key in ('\r', '\n'):
                    if not state["lines"]:
                        state["lines"] = ["", ""]
                        state["cursor_line"] = 1
                    else:
                        line = state["lines"][state["cursor_line"]]
                        before = line[:state["cursor_col"]]
                        after = line[state["cursor_col"]:]
                        state["lines"][state["cursor_line"]] = before
                        state["lines"].insert(state["cursor_line"] + 1, after)
                        state["cursor_line"] += 1
                    state["cursor_col"] = 0
                    state["dirty"] = True
                elif key in ('\x7f', '\b'):
                    if not state["lines"]:
                        pass
                    elif state["cursor_col"] > 0:
                        line = state["lines"][state["cursor_line"]]
                        state["lines"][state["cursor_line"]] = line[:state["cursor_col"]-1] + line[state["cursor_col"]:]
                        state["cursor_col"] -= 1
                        state["dirty"] = True
                    elif state["cursor_line"] > 0:
                        prev_line = state["lines"].pop(state["cursor_line"] - 1)
                        cur_line = state["lines"][state["cursor_line"] - 1]
                        state["cursor_col"] = len(prev_line)
                        state["lines"][state["cursor_line"] - 1] = prev_line + cur_line
                        state["cursor_line"] -= 1
                        state["dirty"] = True
                elif key == '\t':
                    if not state["lines"]:
                        state["lines"] = [""]
                    line = state["lines"][state["cursor_line"]]
                    state["lines"][state["cursor_line"]] = line[:state["cursor_col"]] + "    " + line[state["cursor_col"]:]
                    state["cursor_col"] += 4
                    state["dirty"] = True
                elif key == '\x1b[A':
                    if state["cursor_line"] > 0:
                        state["cursor_line"] -= 1
                        state["cursor_col"] = min(state["cursor_col"], len(state["lines"][state["cursor_line"]]))
                elif key == '\x1b[B':
                    if state["cursor_line"] < len(state["lines"]) - 1:
                        state["cursor_line"] += 1
                        state["cursor_col"] = min(state["cursor_col"], len(state["lines"][state["cursor_line"]]))
                elif key == '\x1b[C':
                    if state["lines"]:
                        line_len = len(state["lines"][state["cursor_line"]])
                        if state["cursor_col"] < line_len:
                            state["cursor_col"] += 1
                elif key == '\x1b[D':
                    if state["cursor_col"] > 0:
                        state["cursor_col"] -= 1
                elif key == '\x1b[H':
                    state["cursor_col"] = 0
                elif key == '\x1b[F':
                    if state["lines"]:
                        state["cursor_col"] = len(state["lines"][state["cursor_line"]])
                elif len(key) == 1:
                    if not state["lines"]:
                        state["lines"] = [""]
                    line = state["lines"][state["cursor_line"]]
                    state["lines"][state["cursor_line"]] = line[:state["cursor_col"]] + key + line[state["cursor_col"]:]
                    state["cursor_col"] += 1
                    state["dirty"] = True

            else:
                if key == ':':
                    state["mode"] = "command"
                    state["cmd_buf"] = ""
                elif key == 'i':
                    if not state["lines"]:
                        state["lines"] = [""]
                    state["insert_snapshot"] = state["lines"][:]
                    state["mode"] = "insert"
                elif key == 'I':
                    if not state["lines"]:
                        state["lines"] = [""]
                    state["insert_snapshot"] = state["lines"][:]
                    state["cursor_col"] = 0
                    state["mode"] = "insert"
                elif key == 'a':
                    if not state["lines"]:
                        state["lines"] = [""]
                        state["cursor_col"] = 0
                    state["insert_snapshot"] = state["lines"][:]
                    state["cursor_col"] = min(state["cursor_col"] + 1, len(state["lines"][state["cursor_line"]]))
                    state["mode"] = "insert"
                elif key == 'A':
                    if not state["lines"]:
                        state["lines"] = [""]
                    state["insert_snapshot"] = state["lines"][:]
                    state["cursor_col"] = len(state["lines"][state["cursor_line"]])
                    state["mode"] = "insert"
                elif key == 'o':
                    if not state["lines"]:
                        state["lines"] = ["", ""]
                        state["cursor_line"] = 0
                    state["undo_stack"].append(state["lines"][:])
                    state["redo_stack"].clear()
                    state["lines"].insert(state["cursor_line"] + 1, "")
                    state["cursor_line"] += 1
                    state["cursor_col"] = 0
                    state["mode"] = "insert"
                    state["dirty"] = True
                elif key == 'O':
                    if not state["lines"]:
                        state["lines"] = [""]
                        state["cursor_line"] = 0
                    state["undo_stack"].append(state["lines"][:])
                    state["redo_stack"].clear()
                    state["lines"].insert(state["cursor_line"], "")
                    state["cursor_col"] = 0
                    state["mode"] = "insert"
                    state["dirty"] = True
                elif key == 'x':
                    if state["lines"]:
                        line = state["lines"][state["cursor_line"]]
                        if state["cursor_col"] < len(line):
                            state["undo_stack"].append(state["lines"][:])
                            state["redo_stack"].clear()
                            state["lines"][state["cursor_line"]] = line[:state["cursor_col"]] + line[state["cursor_col"]+1:]
                            state["dirty"] = True
                elif key == 'd':
                    nk = _tau_get_key()
                    if nk == 'd':
                        if state["lines"]:
                            state["undo_stack"].append(state["lines"][:])
                            state["redo_stack"].clear()
                            state["lines"].pop(state["cursor_line"])
                            if not state["lines"]:
                                state["lines"] = [""]
                            if state["cursor_line"] >= len(state["lines"]):
                                state["cursor_line"] = max(0, len(state["lines"]) - 1)
                            state["cursor_col"] = 0
                            state["dirty"] = True
                elif key == 'y':
                    nk = _tau_get_key()
                    if nk == 'y' and state["lines"]:
                        state["yank"] = state["lines"][state["cursor_line"]]
                elif key == 'p':
                    if state["yank"] is not None:
                        state["undo_stack"].append(state["lines"][:])
                        state["redo_stack"].clear()
                        state["lines"].insert(state["cursor_line"] + 1, state["yank"])
                        state["cursor_line"] += 1
                        state["cursor_col"] = 0
                        state["dirty"] = True
                elif key == 'P':
                    if state["yank"] is not None:
                        state["undo_stack"].append(state["lines"][:])
                        state["redo_stack"].clear()
                        state["lines"].insert(state["cursor_line"], state["yank"])
                        state["cursor_col"] = 0
                        state["dirty"] = True
                elif key == 'j' or key == '\x1b[B':
                    if state["cursor_line"] < len(state["lines"]) - 1:
                        state["cursor_line"] += 1
                        state["cursor_col"] = min(state["cursor_col"], len(state["lines"][state["cursor_line"]]))
                elif key == 'k' or key == '\x1b[A':
                    if state["cursor_line"] > 0:
                        state["cursor_line"] -= 1
                        state["cursor_col"] = min(state["cursor_col"], len(state["lines"][state["cursor_line"]]))
                elif key == 'h' or key == '\x1b[D':
                    if state["cursor_col"] > 0:
                        state["cursor_col"] -= 1
                elif key == 'l' or key == '\x1b[C':
                    if state["lines"]:
                        line_len = len(state["lines"][state["cursor_line"]])
                        if state["cursor_col"] < line_len:
                            state["cursor_col"] += 1
                elif key == '0' or key == '\x1b[H':
                    state["cursor_col"] = 0
                elif key == '$' or key == '\x1b[F':
                    if state["lines"]:
                        state["cursor_col"] = len(state["lines"][state["cursor_line"]])
                elif key == 'g':
                    nk = _tau_get_key()
                    if nk == 'g':
                        state["cursor_line"] = 0
                        state["cursor_col"] = 0
                elif key == 'G':
                    state["cursor_line"] = max(0, len(state["lines"]) - 1)
                    state["cursor_col"] = 0
                elif key == 'u':
                    if state["undo_stack"]:
                        state["redo_stack"].append(state["lines"][:])
                        state["lines"] = state["undo_stack"].pop()
                        if state["cursor_line"] >= len(state["lines"]):
                            state["cursor_line"] = max(0, len(state["lines"]) - 1)
                        if state["lines"]:
                            state["cursor_col"] = min(state["cursor_col"], len(state["lines"][state["cursor_line"]]))
                        state["dirty"] = True
                elif key == '\x12':
                    if state["redo_stack"]:
                        state["undo_stack"].append(state["lines"][:])
                        state["lines"] = state["redo_stack"].pop()
                        if state["cursor_line"] >= len(state["lines"]):
                            state["cursor_line"] = max(0, len(state["lines"]) - 1)
                        if state["lines"]:
                            state["cursor_col"] = min(state["cursor_col"], len(state["lines"][state["cursor_line"]]))
                        state["dirty"] = True
                elif key == 'r':
                    nk = _tau_get_key()
                    if state["lines"] and len(nk) == 1:
                        line = state["lines"][state["cursor_line"]]
                        if state["cursor_col"] < len(line):
                            state["undo_stack"].append(state["lines"][:])
                            state["redo_stack"].clear()
                            state["lines"][state["cursor_line"]] = line[:state["cursor_col"]] + nk + line[state["cursor_col"]+1:]
                            state["dirty"] = True

    finally:
        try:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_term)
        except Exception:
            pass
        sys.stdout.write("\033[?25h\033[2J\033[H")
        sys.stdout.flush()

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
    cp [-r] src dest        Copy files or directories
    find [path] -name p     Search files by name pattern
    grep pattern [path]     Search file contents
    chmod mode file         Change file permissions
    alias [name=value]      Define or list aliases
    unalias name            Remove an alias
    export [VAR=value]      Set or list environment variables
    echo text               Print text with $VAR expansion
    which command           Locate a built-in command
    type name               Describe a command
    sort [path]             Sort lines
    head [-N] [path]        First N lines (default 10)
    tail [-N] [path]        Last N lines (default 10)
    wc [path]               Count lines, words, chars
    command &               Run command in background
    jobs                    List background jobs
    fg [id]                 Bring job to foreground
    kill id                 Terminate a background job
    source path             Execute commands from a file
    history                 Show & re-run (!n) command history
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

    def _completer(text, state):
        try:
            completions = []
            for c in COMMANDS:
                if c.startswith(text) and " " not in c and "(" not in c:
                    completions.append(c + " ")
            node = resolve(fs, cwd)
            if node:
                for nm, ch in node.get("children", {}).items():
                    if nm.startswith(text):
                        completions.append(nm + ("/" if ch["type"] == "dir" else " "))
            completions = sorted(set(completions))
            return completions[state] if state < len(completions) else None
        except Exception:
            return None

    readline.set_completer(_completer)
    readline.parse_and_bind("tab: complete")
    readline.set_completer_delims(" \t\n;")

    rc_node = resolve(fs, normalize_path(".onxrc", cwd))
    if rc_node and rc_node["type"] == "file":
        for rc_line in rc_node.get("content", "").split("\n"):
            rc_line = rc_line.strip()
            if rc_line and not rc_line.startswith("#"):
                cwd = _exec_line(rc_line, fs, cwd)

    print(f"  onxOS v{VERSION}, type 'help' to get started")

    while True:
        prompt_cwd = path_to_str(cwd)
        p = _env.get("PROMPT")
        if p:
            prompt_str = p.replace("\\w", prompt_cwd).replace("\\s", "onxOS").replace("\\v", VERSION) + " "
        else:
            prompt_str = f"{prompt_cwd}>> "
        try:
            rawcmd = input(prompt_str).strip()
        except (EOFError, KeyboardInterrupt):
            print("\nexiting.")
            break
        if not rawcmd:
            continue

        if rawcmd.startswith("!"):
            try:
                n = int(rawcmd[1:])
                rawcmd = readline.get_history_item(n)
                if rawcmd:
                    print(rawcmd)
                else:
                    print(f"  No history entry #{n}")
                    continue
            except ValueError:
                print(f"  Unknown command '{rawcmd}'")
                continue

        if rawcmd in ("exit", "quit"):
            print("goodbye.")
            break
        cwd = _exec_line(rawcmd, fs, cwd)

if __name__ == "__main__":
    run_terminal()