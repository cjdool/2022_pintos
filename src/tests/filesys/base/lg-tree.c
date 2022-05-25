/* Library function for creating a tree of directories. */

#include <stdio.h>
#include <syscall.h>
#include "tests/main.h"
#include "tests/lib.h"
#include <random.h>

#define MAX_PATHLEN 300

char pathname[MAX_PATHLEN];
int max_idx = 0; 

void test1 (void);
void test2 (void);
void test_mkdir (char *path);
void test_open (char *path);

void test_mkdir (char *path) 
{
  if (mkdir(path) <= 0) 
    exit(-1);
}

void test_open (char *path) 
{
  int fd; 
  if ((fd = open(path)) <=0)
    exit(-1);

  close(fd);
}

void 
test1 (void) {
  int i;

  for (i = 0; i*2+2 < MAX_PATHLEN; i++ ) {
    pathname[i*2] = '/';
    pathname[i*2+2] = '\0';

    // Create directory
    pathname[i*2+1] = 'a';
    test_mkdir(pathname);
  
    pathname[i*2+1] = 'b';
    test_mkdir(pathname);
   
    pathname[i*2+1] = 'c';
    test_mkdir(pathname);
  }

  max_idx = i; 

  msg("Test 1 Done");
}

void
test2 (void) {
  unsigned long r;
  char tmp; int i;

  for (i = 0; i < 100; i++) {
    r = random_ulong() % max_idx;

    tmp = pathname[r*2+2]; 
    pathname[r*2+2] = '\0';

    //msg("%s\n", pathname);
    test_open(pathname);

    pathname[r*2+2] = tmp;
  }
  msg("Test 2 Done");
}

void 
test_main (void) 
{
  test1 ();
  test2 ();
}
