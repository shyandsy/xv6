#include "types.h"
#include "stat.h"
#include "user.h"

int sleep_time = 0;
int ready_time = 0;
int turnaround_time = 0;

int
main(int argc, char *argv[])
{
  int pro_num;
  int index, type_flag;
  int pid;

  int wait_time, run_time, io_time;

  if (argc < 1) {
    printf(1, "No input argv");
    exit();
  } else {
    pro_num = argv[0] * 3;
  }

  for (index = 0; index < pro_num; index++) {
    int type_index, dummy_index;

    pid = fork();
    if (pid == 0) {
      continue;
    } else if (pid % 3 == 0) {
      for (type_index = 0; type_index < 100; type_index++) {
        for (dummy_index = 0; dummy_index < 100000; dummy_index++);
      }
      type_flag = 1;
      break;
    } else if (pid % 3 == 1) {
      for (type_index = 0; type_index < 100; type_index++) {
        for (dummy_index = 0; dummy_index < 100000; dummy_index);
        yield();
      }
      type_flag = 2;
      break;
    } else if (pid % 3 == 2) {
      for (dummy_index = 0; dummy_index < 100; dummy_index) {
        sleep(1);
      }
      type_flag = 3;
      break;
    }
    type_flag = 0;
  }

  wait2(&wait_time, &run_time, &io_time);

  sleep_time += io_time;
  ready_time += wait_time;
  turnaround_time = turnaround_time + wait_time + run_time + io_time;

  switch (type_flag) {
  case 0:
    sleep_time = sleep_time / pro_num;
    ready_time = ready_time / pro_num;
    turnaround_time = turnaround_time / pro_num;
    printf(1, "This is the parent process \n");
    printf(3, "The wait time is %d, the run time is %d, the io time is %d\n", wait_time, run_time, io_time);
    printf(3, "The average sleep time is %d, the average ready time is %d, the average turnaround time is %d\n", sleep_time, ready_time, turnaround_time);
    break
  case 1:
    printf(1, "This is the %d process, the type is CPU \n", pid);
    printf(3, "The wait time is %d, the run time is %d, the io time is %d\n", wait_time, run_time, io_time);
    break
  case 2:
    printf(1, "This is the %d process, the type is SCPU \n", pid);
    printf(3, "The wait time is %d, the run time is %d, the io time is %d\n", wait_time, run_time, io_time);
    break
  case 3:
    printf(1, "This is the %d process, the type is IO \n", pid);
    printf(3, "The wait time is %d, the run time is %d, the io time is %d\n", wait_time, run_time, io_time);
    break
  }

  exit();
}
