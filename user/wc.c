#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

// option flag 
int flag_l = 0, flag_w = 0, flag_c = 0;

void
wc(int fd, char *name)
{
  int i, n;
  int l = 0, w = 0, c = 0;
  int inword = 0;

  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i=0; i<n; i++){
      c++;
      if(buf[i] == '\n')
        l++;
      if(strchr(" \r\t\n\v", buf[i]))
        inword = 0;
      else if(!inword){
        w++;
        inword = 1;
      }
    }
  }
  if(n < 0){
    printf("wc: read error\n");
    exit(1);
  }
  
  if(flag_l)
    printf("%d ", l);
  if(flag_w)
    printf("%d ", w);
  if(flag_c)
    printf("%d ", c);

  if(name && name[0])
    printf("%s", name);

  printf("\n");

}

int
main(int argc, char *argv[])
{
  int fd, i;

  // parse options
  for(i = 1; i < argc; i++){
    if(argv[i][0] != '-')
      break; 

    if(strcmp(argv[i], "-l") == 0)
      flag_l = 1;
    else if(strcmp(argv[i], "-w") == 0)
      flag_w = 1;
    else if(strcmp(argv[i], "-c") == 0)
      flag_c = 1;
    else{
      printf("wc: invalid option %s\n", argv[i]);
      exit(1);
    }
  }

  // no option -> print all
  if(!flag_l && !flag_w && !flag_c){
    flag_l = flag_w = flag_c = 1;
  }

  // stdin
  if(i == argc){
    wc(0, "");
    exit(0);
  }

  for(; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      printf("wc: cannot open %s\n", argv[i]);
      exit(1);
    }
    wc(fd, argv[i]);
    close(fd);
  }
  exit(0);
}
