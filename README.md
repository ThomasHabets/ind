# ind

By Thomas Habets <thomas@habets.se>

http://www.habets.pp.se/synscan/

## Why the name 'ind'?

Indent, or "Indent to make reading output Not-Difficult".

## What does it do?

When you prefix a command with 'ind' every line that the program
outputs can be prefixed and postfixed by a short string.

Ind works best with normal line-based programs. It will work with
fullscreen programs such as less or emacs, but they will display a bit
weird. Usually it will look good after a redraw (Ctrl-L), but for ind
to work properly with fullscreen (ncurses) programs it would have to
implement a terminal emulator (such as vt100). At the moment this is
outside the scope of ind.

Note that because some programs will behave differently if stdout is
not tty. This means that if you run one of these commands:

```
$ ind ./test | cat
$ ./test
```

(where test is the test program in the ind source tree)
The order of the output will be "wrong". But it will be *the same*
order. Ind will not change anything. It could, but that could have
unintended side effects. So it doesn't. For more info on libc
buffering see the manpage for `setvbuf`.

## Example uses

### Ex1

```
$ ind ls; ind ind ls
  Makefile
  README
  ind
  ind.1
  ind.c
  ind.yodl
    Makefile
    README
    ind
    ind.1
    ind.c
    ind.yodl
```

### Ex2

```
$ ind -p '%s ' ping 2001:4860:4860::8888
1567158247 PING 2001:4860:4860::8888 (2001:4860:4860::8888): 56 data bytes
1567158247 64 bytes from 2001:4860:4860::8888: icmp_seq=0 hlim=56 time=1.827 ms
1567158248 64 bytes from 2001:4860:4860::8888: icmp_seq=1 hlim=56 time=1.685 ms
1567158249 64 bytes from 2001:4860:4860::8888: icmp_seq=2 hlim=56 time=1.674 ms
1567158250 64 bytes from 2001:4860:4860::8888: icmp_seq=3 hlim=56 time=1.866 ms
1567158251 64 bytes from 2001:4860:4860::8888: icmp_seq=4 hlim=56 time=1.651 ms
[...]
```

### Ex3

```
$ ind ./test
  fd: 0
        Size: 32x123
  fd: 1
        Size: 32x123
  fd: 2
        Not a tty!
  0.1 stdout
>>0.2 stderr
  0.3 stdout
  1.1 stdout
  1.3 stdout
>>1.2 stderr
  Progress: done
  Give me an integer: 123
  Value: 123
```

### Ex4

Let's say you're running a script that has several stages, each producing
some status output. Now you can indent the output from the substages.

```
echo "Stage 1, reconfooberating the ablamatron..."
ind -p "--> " cat blah.txt

echo "Stage 2, burning image..."
ind -p "    " growisofs -Z /dev/hdc=randomcrap.img
[...]
```

This will produce the output:

```
Stage 1, reconfooberating the ablamatron...
--> This is a text file
--> with only text in it.
Stage 2, burning image...
    ...
    ...
    ...
```

## Testing
Before a release test this on some different systems.

```
./ind ./test
./ind ./test < t
./ind ./test < t | cat
./ind ./test | cat

./ind cat
./ind cat < t
./ind cat < t > apa
./ind cat > apa
```

### Log of tested systems and versions

0.13pre-release:
   Debian GNU/kFreeBSD (squeeze) amd64
   Debian GNU/Linux (lenny) amd64
   OpenBSD 4.4 amd64
   IRIX 6.5.15
0.12:
   FreeBSD 6.1 x86
   OpenBSD 4.4 amd64
   Debian GNU/Linux (lenny) amd64
   Solaris 10 sparc
