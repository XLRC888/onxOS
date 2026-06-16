<h2><pre>
  ___    _   _   _   _   ___    ____
 / _ \  | \ | | \ \ / / / _ \  / ___|
| | | | |  \| |  \ | / | | | | \___	\
| |_| | | |\  |  / | \ | |_| |  ___) |
 \___/  |_| \_| /_/ \_\ \___/  |____/
</pre></h2>

32-bit x86 OS from scratch. Primary target is real hardware: old laptops,
netbooks, anything with an i686 CPU. QEMU is just for development.

Persistent filesystem, text editor, shell with ~30 commands. No graphics stack,
no audio, no networking. Boot time in seconds even on spinning rust.

Grab the latest ISO from Releases, or build it:

```
  make iso
  make run
```

Ctrl+A then X to exit QEMU. For serial: -serial stdio.

Install to disk from the live ISO: boot, run `setup`, then `exit`. This writes
the bootloader (first 6 sectors), kernel (next 122), and filesystem (LBA 128+).
After that you can boot the disk directly with `make run-hdd`.

Runs on machines with 1GB RAM and Atom CPUs. Not trying to replace Linux.
Just a self contained environment for editing text, writing code, or tinkering.

### BUILD REQUIREMENTS

  - gcc (i686 cross-compiler or native with -m32)
  - nasm, ld, python3
  - grub-mkimage, xorriso
  - qemu-system-i386 (for testing)

On Arch:

```
pacman -S base-devel nasm grub libisoburn python qemu-system-x86
```

### SHELL

Tab completion, history (!!/!n/arrows), ~30 commands:

```
  / $ help

  onxOS shell help

    ls/cd/pwd        navigate directories
    touch/mkdir      create files / directories
    rm/rmdir         remove files (rm -r for dirs)
    cat              print file contents
    echo             print text
    cp/mv            copy / move files
    find <pat>       find files by name
    grep <pat>       search file contents
    head <f>         first 10 lines
    tail <f>         last 10 lines (-n N)
    wc <f>           count lines, words, bytes
    history          show command history
    uname            system info
    df/du            disk usage / directory size
    sort <f>         sort file lines
    yes [msg]        repeat a string
    sleep <ms>       delay milliseconds
    seq <end>        number sequence
    rev <f>          reverse chars per line
    which <f>        locate a file
    tac <f>          reverse line order
    base64 <f>       base64 encode a file
    uniq <f>         filter duplicate lines
    stat <f>         file info
    hex <f>          hex dump
    tree [dir]       display directory tree
    tau <f>          text editor (esc to exit)
    setup            install onxOS to this disk
    cowsay [msg]     ascii cow
    clear/ver/reboot clear / version / reboot
    help             you are here
    !! or !5         re-run from history

  note: exit saves the filesystem and halts
```

```
  / $ ver

  onxIM v0.0.5
  built for i686
```

```
  / $ cowsay onxOS

    _______
   < onxOS >
    -------
          \   ^__^
           \  (oo)\_______
              (__)\       )\/\
                  ||----w |
                  ||     ||
```

### SESSION EXAMPLE

```
  / $ pwd
  /

  / $ echo hello world
  hello world

  / $ touch test.txt
  / $ mkdir docs
  / $ ls
    docs/

  / $ cd docs
  /docs $ touch readme.md

  / $ history
    1  pwd
    2  echo hello world
    3  touch test.txt
    4  mkdir docs
    5  ls
    6  cd docs
    7  touch readme.md
    8  history

  / $ seq 5
  1
  2
  3
  4
  5
```

### EDITOR

tau is the built in text editor. Open with `tau filename.txt`.
Type to insert, escape to exit.

### LICENSE

Do whatever you want with it.

### DISCLAIMER
Use at your own risk.
