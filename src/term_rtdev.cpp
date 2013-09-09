#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <rtdm/rtdm.h>
#include <native/task.h>
#include <sys/mman.h>

using namespace std;



void usage() {
  cout << "term_dev <fd_start> <fd_stop>" << endl;
  cout << " - fd_start: first file descriptor to close (integer)." << endl;
  cout << " - fd_stop: first file descriptor to close (integer)." << endl;
}


int main(int argc, const char* argv[]) {

  cout << "term_dev 1.0" << endl << "RT open handles cleanup utility." << endl;

  if(argc < 3) {
    usage();
    exit(0);
  }

  int fd_start = 0, fd_stop = 0;
  sscanf(argv[1], "%d", &fd_start);
  sscanf(argv[2], "%d", &fd_stop);

  cout << "Trying to close file descriptors from " << fd_start << " to " << fd_stop << "..." << endl;

  mlockall(MCL_CURRENT | MCL_FUTURE);
  
  rt_task_shadow(NULL, "term_dev", 0, 0);
  
  for(int i = fd_start; i <= fd_stop; i++) {
    rt_dev_close(i);
  }
}
