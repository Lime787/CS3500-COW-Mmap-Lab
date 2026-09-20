#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

#define MAP_FAILED ((void *)-1)

static void
fail(char *message)
{
  printf("vmtest: %s failed\n", message);
  exit(1);
}

static int
makepage(char *path, char value)
{
  char block[512];
  int fd;

  unlink(path);
  fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0)
    fail("create file");

  memset(block, value, sizeof(block));
  for(int i = 0; i < PGSIZE / sizeof(block); i++)
    if(write(fd, block, sizeof(block)) != sizeof(block))
      fail("initialize file");
  return fd;
}

static void
private_fork_test(void)
{
  char *path = "vmprivate";
  int fd = makepage(path, 'A');
  char *p = mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if(p == MAP_FAILED)
    fail("private mmap");

  // Make the page resident and privately dirty before fork().
  p[0] = 'P';

  int pid = fork();
  if(pid < 0)
    fail("private fork");
  if(pid == 0){
    if(p[0] != 'P')
      exit(2);
    p[0] = 'C';
    if(p[0] != 'C')
      exit(3);
    exit(0);
  }

  int status = -1;
  if(wait(&status) != pid || status != 0)
    fail("private child");
  if(p[0] != 'P')
    fail("private isolation");
  // munmap behavior is covered by the original mmaptest; this is cleanup.
  munmap(p, PGSIZE);
  close(fd);
  unlink(path);
  printf("vmtest: private fork: OK\n");
}

static void
shared_resident_fork_test(void)
{
  char *path = "vmshared";
  int fd = makepage(path, 'A');
  char *p = mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(p == MAP_FAILED)
    fail("shared mmap");

  // The assignment requires an already-resident shared page to remain
  // physically shared across fork().
  p[0] = 'P';
  int pid = fork();
  if(pid < 0)
    fail("shared fork");
  if(pid == 0){
    if(p[0] != 'P')
      exit(2);
    p[0] = 'C';
    exit(0);
  }

  int status = -1;
  if(wait(&status) != pid || status != 0)
    fail("shared child");
  if(p[0] != 'C')
    fail("shared visibility");
  // munmap behavior is covered by the original mmaptest; this is cleanup.
  munmap(p, PGSIZE);
  close(fd);
  unlink(path);
  printf("vmtest: shared resident fork: OK\n");
}

int
main(int argc, char **argv)
{
  private_fork_test();
  shared_resident_fork_test();
  printf("vmtest: all tests succeeded\n");
  exit(0);
}
