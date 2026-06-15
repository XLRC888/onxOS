<h2><pre>
  ___    _   _   _   _   ___    ____
 / _ \  | \ | | \ \ / / / _ \  | ___|
| | | | |  \| |  \ | / | | | | |___ |
| |_| | | |\  |  / | \ | |_| |  __| |
 \___/  |_| \_| /_/ \_\ \___/  |____|
</pre></h2>

A 32-bit x86 operating system built from scratch. Boots on old hardware, slow
laptops, and QEMU. Comes with a persistent filesystem, AHCI/SATA support, a
text editor, and a shell that has way more commands than it should.

If your main machine is a decade old or you have a pile of netbooks collecting
dust, onxOS might actually be useful. It boots fast, lives entirely in memory
if needed, and has no bloat from graphics stacks, audio subsystems, or network
protocols. The kernel is about 40KB and the whole ISO fits on a floppy-era
budget at 582KB.

Download the latest ISO from Releases, or build it yourself.

### QUICK START

```
  make iso
  
  make run
```


That boots it in QEMU with a 256MB virtual disk attached. Ctrl+A then X to
exit. Connect via serial: -serial stdio.

### BUILD REQUIREMENTS

  - gcc (i686 cross-compiler or native with -m32)
  - nasm
  - ld
  - grub-mkimage
  - xorriso
  - python3
  - qemu-system-i386 (for testing)

On Arch Linux:

  
```
pacman -S base-devel nasm grub libisoburn python qemu-system-x86
```


The Makefile handles everything else. Just run make iso.

### SHELL

The shell supports tab completion, command history (!!, !n and up/down arrow), and about 30
commands. You get a filesystem, a text editor, and a cow.
(also has a scrollback feature with PgUp and PgDn)

```
  / $ help

  onxOS shell help

    ls/cd/pwd        navigate directories
    touch/mkdir      create files / directories
    rm/rmdir         remove files (rm -r for directories)
    cat              print file contents
    echo             print text
    cp/mv            copy / move files
    find <pat>       find files by name
    grep <pat>       search file contents
    head <f>         first 10 lines of file
    tail <f>         last 10 lines (-n N)
    wc <f>           count lines, words, bytes
    history          show command history
    uname            system information
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
    stat <f>         file information
    hex <f>          hex dump
    tree [dir]       display directory tree
    tau <f>          text editor (esc to exit)
    cowsay [msg]     ascii cow with a message
    clear/ver/reboot clear screen / version / reboot
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


### INTERACTIVE SESSION

Here is what a real session looks like:

```
  / $ pwd
  /

  / $ df
    files: 0
    dirs: 0
    data_lba: 81

  / $ echo hello world
  hello world

  / $ touch test.txt
  / $ mkdir docs
  / $ ls
    docs/

  / $ cd docs
  /docs $ pwd
  /docs

  /docs $ touch readme.md
  /docs $ cd /

  / $ history
    1  help
    2  ver
    3  uname
    4  ls
    5  pwd
    6  df
    7  echo hello world
    8  touch test.txt
    9  mkdir docs
    10  cd docs
    11  touch readme.md
    12  cd /
    13  ls
    14  history

  / $ seq 5
  1
  2
  3
  4
  5
```


### WHY THIS EXISTS

Old netbooks with 1GB of RAM and Atom processors can run this. There is no X
server, no audio, no networking, no bloat. The kernel lives in memory and the
boot time is measured in seconds even on spinning rust.

It is not trying to replace Linux. It is a self-contained environment for
editing text files, writing code, or tinkering with a minimal OS. If your
idea of a good time is a shell and a filesystem and nothing else, this fits.

The persistent filesystem saves to disk when you type exit. Reboot and your
files are still there. No fsck, no journaling, no overhead. It just works.

### EDITOR

tau is a basic text editor built into the shell. Open it with:

```
tau filename.txt
```


Controls are minimal. Type to insert, escape to exit, and the file is
preserved in the filesystem. It is meant for quick edits, not IDE work.

### LICENSE

Do whatever you want OR can with it.


### DISCLAIMER
Use it at your own risk.
