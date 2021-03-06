/*
  Copyright (c) 2012, Cosmin Gorgovan
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met: 

  1. Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer. 
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution. 

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
  ptmux: Terminal device multiplexer
   
  Each input char from the source terminal gets displayed either on a specific
  pseudoterminal device if following a char in range [0...PT_COUNT] or on the
  default pts otherwise. Input chars in range [0...PT_COUNT] are consumed 
  internally and they're not forwarded to pseudoterminals.
   
  Input from all pseudoterminals is collected and sent to the source terminal.
*/

#define _XOPEN_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>

#define PT_COUNT     2
#define DEFAULT_PT   0
#define BUFSIZE       255

// Settings
int pt_count = PT_COUNT;
int default_pt = DEFAULT_PT;
int fork_to_bg = 0;

void print_syntax() {
  printf("Usage: ptmux [OPTIONS] DEVICE\n\n");
  printf("Valid options are:\n");
  printf("  -b    fork to background after printing pseudoterminal pathnames\n");
  printf("  -d    default pseudoterminal index\n");
  printf("  -h    display this help and exit\n");
  printf("  -n    number of pseudoterminals\n");
}

int load_settings(int argc, char **argv) {
  int c;
  
  while ((c = getopt (argc, argv, "bhd:n:")) != -1) {
    switch (c) {
      case 'b':
        fork_to_bg = 1;
        break;
      case 'd':
        default_pt = atoi(optarg);
        break;
      case 'n':
        pt_count = atoi(optarg);
        break;
      case 'h':
        print_syntax();
        exit(EXIT_SUCCESS);
        break;
      case '?':
        print_syntax();
        // fall through
      default:
        exit(EXIT_FAILURE);
    }
  }
  
  if ((argc - optind) != 1) {
    print_syntax();
    exit(EXIT_FAILURE);
  }
  
  if (pt_count <= 0) {
    printf("Error: Invalid pseudoterminal count\n");
    exit(EXIT_FAILURE);
  }
  
  if (default_pt < 0 || default_pt >= pt_count) {
    printf("Error: Invalid default pseudoterminal\n");
    exit(EXIT_FAILURE);
  }
}

int open_pt() {
  int pt = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (pt == -1 || grantpt (pt) == -1 || unlockpt (pt) == -1) {
    return -1;
  }
  return pt;
}

int main(int argc, char **argv) {
  int source;
  int *pt;
  fd_set pts;

  char *ptname;
  int maxfd;
  int size;
  char buf[BUFSIZE];
  int i;
  int selected_pt;
  
  load_settings(argc, argv);
  
  pt = malloc(sizeof(int) * pt_count);
  if (pt == NULL) {
    printf("Error allocating memory\n");
    exit(EXIT_FAILURE);
  }
  
  source = open(argv[argc-1], O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (source == -1) {
    printf("Error opening source terminal %s\n", argv[argc-1]);
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < pt_count; i++) {
    if ((pt[i] = open_pt()) == -1 || ((ptname = ptsname(pt[i])) == NULL)) {
      printf("Error creating pseudoterminal\n");
      exit(EXIT_FAILURE);
    }
    printf("%s\n", ptname);
  }

  if (fork_to_bg && fork() != 0) {
    exit(EXIT_SUCCESS);
  }

  maxfd = 0;
  for (i = 0; i < pt_count; i++) {
    if (pt[i] > maxfd) maxfd = pt[i];
  }
  if (source > maxfd)
    maxfd = source;
  maxfd++;
  
  while(1) {
    FD_SET(source, &pts);
    for (i = 0; i < pt_count; i++) {
      FD_SET(pt[i], &pts);
    }
    
    select(maxfd, &pts, NULL, NULL, NULL);

    if (FD_ISSET(source, &pts)) {
      size = read(source, &buf, BUFSIZE);

      for (i = 0; i < size; i++) {
        if (buf[i] < pt_count) {
          selected_pt = buf[i];
        } else {
          write(pt[selected_pt], &buf[i], 1);
          selected_pt = default_pt;
        }
      }

      for (i = 0; i < pt_count; i++) {
        fsync(pt[i]);
      }
    }

    for (i = 0; i < pt_count; i++) {
      if (FD_ISSET(pt[i], &pts)) {
        size = read(pt[i], &buf, BUFSIZE);
        write(source, buf, size);
        fsync(source);
      }
    }

  } // while(1)

}
