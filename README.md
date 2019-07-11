# Third Semester - Computer Networks

Tasks from the laboratory: [__Computer Networks__](https://usosweb.umk.pl/kontroler.php?_action=katalog2/przedmioty/pokazPrzedmiot&kod=1000-I1SKO).

Tasks come from [moodle](plas.mat.umk.pl), from the course site.

I used the __functional programming__ paradigm and the __ANSI C__ standard with flags: `-wall`, `-pedantic`, `-ansi`

---

### Issues discussed during the course:

- static and dynamic compilation with additional libraries, `Makefile` file structure
- primitive communicatin between processes - pipes in command line and in C programms
- error handling - `errno` variable, and `perror()`, `strerror()` functions
- getting information about:
  - environment variables: `getenv()`, `setenv()`, `putenv()`, `unsetenv()`
  - files: `open()`, `read()`, `write()`, `close()`, `lseek()`, `unlink()`, `creat()`, `stat()`/`lstat()`, struct `stat`
  - catalogs: `opendir()`, `readdir()`, `closedir()`, `rmdir()`, struct `dirent`
  - users and groups, file system permissions, processes: structs `passwd` and `group` and `get...()` functions
- creating and completing processes: `exec()` family, `fork()`, `wait()`/`waitpid()`, `exit()`
- zombies and orphans processes
- communication betweenn processes
  - signals
  - client and server programms
  - named pipes: `mkfifo()`, `mknod`
- IPC objects for System V and POSIX (only in lecture)
  - message quaues: `msgget()`, `msgctl()`, `msgsnd()`, `msgrcv()`
  - semaphores: `semget()`, `semctl()`, `semop()`
  - shared memory: `shmget()`, `shmctl()`, `shmat()`, `shmdt()`
- addressing on the network: MAC, IP, domain addresses
- DNS, BSD sockets, UDP/IP, TCP/IP: `socket()`, `bind()`, `sendto()`, `recvfrom()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`

--- 

While labs, I created three programs:
1. My own version of `ls -la` command
2. Book of complaints and applications
3. UDP chat between two users
