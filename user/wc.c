#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

// option flag 
int flag_l = 0, flag_w = 0, flag_c = 0;

void
wc(int fd, char *name) // fd is the file descriptor, and print results with optional name
{
  int i, n; // i is index for iterating through buffer, n is number of bytes read
  int l = 0, w = 0, c = 0; // line, word, byte count
  int inword = 0; // whether currently in a word

  while((n = read(fd, buf, sizeof(buf))) > 0){ // read file in chunks of 512 bytes 
    for(i=0; i<n; i++){ // iterate through buffer after each read
      c++;
      if(buf[i] == '\n')
        l++;
      if(strchr(" \r\t\n\v", buf[i])) // if current character is a whitespace character
        inword = 0; // if we were in a word, we are now not in a word
      else if(!inword){ // if we encounter a non-whitespace character and we were not in a word, then we have found the start of a new word
        w++; // increment word count
        inword = 1; // we are now in a word until we encounter another whitespace character
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

  if(name && name[0]) // if name is not null and not empty, print the name of the file after the counts
    printf("%s", name);

  printf("\n");

}

int
main(int argc, char *argv[])
{
  int fd, i;

  // parse options
  for(i = 1; i < argc; i++){ // iterate through command line arguments starting from index 1 (index 0 is the program name)
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
  if(i == argc){ // if there are no file arguments, read from standard input
    wc(0, ""); // fd 0 is standard input, and print empty name since we are reading from stdin
    exit(0);
  }

  for(; i < argc; i++){ // iterate through file arguments
    if((fd = open(argv[i], O_RDONLY)) < 0){ // open file for reading, if open fails print error and exit
      printf("wc: cannot open %s\n", argv[i]);
      exit(1);
    }
    wc(fd, argv[i]); // call wc on the file descriptor and print the name of the file
    close(fd);
  }
  exit(0);
}

/*
cat README | wc -l
cat read from README and pipe convert output of cat to input of wc, 
which will read from standard input and count the number of lines, words, and bytes in the input. 
The -l option tells wc to only print the line count.

wc -w README
wc will read from the file README and count the number of lines, words, and bytes in the file. 
The -w option tells wc to only print the word count.
*/
