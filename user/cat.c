#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[2048];

void cat(int fd)
{
  int n;

  while ((n = read(fd, buf, sizeof(buf))) > 0)
  {
    if (write(1, buf, n) != n)
    {
      fprintf(2, "cat: write error\n");
      exit(1);
    }
  }
  if (n < 0)
  {
    fprintf(2, "cat: read error\n");
    exit(1);
  }
}

int readline(int fd, char *buf, int maxlen)
{
  int n, i = 0;
  char c;

  while ((n = read(fd, &c, 1)) > 0)
  {
    if (c == '\n')
    {
      buf[i] = c;
      ++i;
      break;
    }

    if (i >= (maxlen - 1))
    {
      buf[i] = c;
      ++i;
      while ((n = read(fd, &c, 1)) > 0 && c != '\n')
        ;
      break;
    }

    buf[i] = c;
    ++i;
  }
  
  // error
  if (((n == 0) && (i == 0)) || (n < 0))
    return n;
  
  // if ok
  buf[i] = '\0';
  return i;
}

void cat_with_line(int fd)
{
  int cnt = 0;
  int n;

  while ((n = readline(fd, buf, sizeof(buf))) > 0)
  {
    ++cnt;
    if (cnt < 10)
      fprintf(1, "     %d  %s", cnt, buf);
    else if (cnt < 100)
      fprintf(1, "    %d  %s", cnt, buf);
    else if (cnt < 1000)
      fprintf(1, "   %d  %s", cnt, buf);
    else if (cnt < 10000)
      fprintf(1, "  %d  %s", cnt, buf);
    else if (cnt < 100000)
      fprintf(1, " %d  %s", cnt, buf);
    else
      fprintf(1, "%d  %s", cnt, buf);
  }

  if (n < 0)
  {
    fprintf(2, "cat: read error\n");
    exit(1);
  }
}

int main(int argc, char *argv[])
{
  int fd, i;
  int show_line_numbers = 0;
  int index = 1;

  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n')
  {
    show_line_numbers = 1;
    index = 2; // Skip -n
  }
  // no files provided after -n, then read from stdin
  if (argc <= index)
  {
    if (show_line_numbers)
    {
      cat_with_line(0);
    }
    else
    {
      cat(0);
    }
    exit(0);
  }

  // process
  for (i = index; i < argc; i++)
  {
    if ((fd = open(argv[i], O_RDONLY)) < 0)
    {
      fprintf(2, "cat: cannot open %s\n", argv[i]);
      exit(1);
    }
    if (show_line_numbers)
    {
      cat_with_line(fd);
    }
    else
    {
      cat(fd);
    }
    close(fd);
  }
  exit(0);
}

/*int main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    cat(0);
    exit(0);
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      fprintf(2, "cat: cannot open %s\n", argv[i]);
      exit(1);
    }
    cat(fd);
    close(fd);
  }
  exit(0);
}*/