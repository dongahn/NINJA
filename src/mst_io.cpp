/* ==========================NINJA:LICENSE==========================================   
  Copyright (c) 2016, Lawrence Livermore National Security, LLC.                     
  Produced at the Lawrence Livermore National Laboratory.                            
                                                                                    
  Written by Kento Sato, kento@llnl.gov. LLNL-CODE-713637.                           
  All rights reserved.                                                               
                                                                                    
  This file is part of NINJA. For details, see https://github.com/PRUNER/NINJA      
  Please also see the LICENSE.TXT file for our notice and the LGPL.                      
                                                                                    
  This program is free software; you can redistribute it and/or modify it under the 
  terms of the GNU General Public License (as published by the Free Software         
  Foundation) version 2.1 dated February 1999.                                       
                                                                                    
  This program is distributed in the hope that it will be useful, but WITHOUT ANY    
  WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or                  
  FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU          
  General Public License for more details.                                           
                                                                                    
  You should have received a copy of the GNU Lesser General Public License along     
  with this program; if not, write to the Free Software Foundation, Inc., 59 Temple 
  Place, Suite 330, Boston, MA 02111-1307 USA                                 
  ============================NINJA:LICENSE========================================= */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <getopt.h>
#include <errno.h>

#include <mpi.h>

#define MST_OPEN_TRIES (30)
#define MST_OPEN_USLEEP (100000)

int mst_open(const char* file, int flags, mode_t  mode)
{
  int fd = -1;
  if (mode) { 
    fd = open(file, flags, mode);
  } else {
    fd = open(file, flags, S_IRUSR | S_IWUSR);
  }

  if (fd < 0) {
    fprintf(stderr, "Opening file: open(%s) errno=%d @ %s:%d %m\n",
            file, errno, __FILE__, __LINE__);

    /* try again */
    int tries = MST_OPEN_TRIES;
    while (tries && fd < 0) {
      usleep(MST_OPEN_USLEEP);
      if (mode) { 
        fd = open(file, flags, mode);
      } else {
        fd = open(file, flags, S_IRUSR | S_IWUSR);
      }
      tries--;
    }

    /* if we still don't have a valid file, consider it an error */
    if (fd < 0) {
      fprintf(stderr, "Opening file: open(%s) errno=%d @ %s:%d %m\n",
              file, errno, __FILE__, __LINE__
	      );
    }
  }

  return fd;
}


int mst_close(const char* file, int fd)
{
  /* fsync first */
  fsync(fd);

  /* now close the file */
  if (close(fd) != 0) {
    /* hit an error, print message */
    fprintf(stderr, "Closing file descriptor %d for file %s: errno=%d @ %s:%d %m",
            fd, file, errno, __FILE__, __LINE__
	    );
    return 1;
  }

  return 0;
}

ssize_t mst_write(const char* file, int fd, const void* buf, size_t size)
{
  ssize_t n = 0;
  int retries = 10;
  while (n < size)
    {
      ssize_t rc = write(fd, (char*) buf + n, size - n);
      if (rc > 0) {
	n += rc;
      } else if (rc == 0) {
	/* something bad happened, print an error and abort */
	fprintf(stderr, "Error writing %s: write(%d, %p, %ld) returned 0 @ %s:%d",
		file, fd, (char*) buf + n, size - n, __FILE__, __LINE__
		);
	exit(1);
      } else { /* (rc < 0) */
	/* got an error, check whether it was serious */
	if (errno == EINTR || errno == EAGAIN) {
	  continue;
	}

	/* something worth printing an error about */
	retries--;
	if (retries) {
	  /* print an error and try again */
	  fprintf(stderr, "Error writing %s: write(%d, %p, %ld) errno=%d @ %s:%d %m",
		  file, fd, (char*) buf + n, size - n, errno, __FILE__, __LINE__
		  );
	} else {
	  /* too many failed retries, give up */
	  fprintf(stderr, "Giving up write to %s: write(%d, %p, %ld) errno=%d @ %s:%d %m",
		  file, fd, (char*) buf + n, size - n, errno, __FILE__, __LINE__
		  );
	  exit(1);
	}
      }
    }
  return n;
}


/* reliable read from file descriptor (retries, if necessary, until hard error) */
ssize_t mst_read(const char* file, int fd, void* buf, size_t size)
{
  ssize_t n = 0;
  int retries = 10;
  while (n < size)
    {
      int rc = read(fd, (char*) buf + n, size - n);
      if (rc  > 0) {
	n += rc;
      } else if (rc == 0) {
	/* EOF */
	return n;
      } else { /* (rc < 0) */
	/* got an error, check whether it was serious */
	if (errno == EINTR || errno == EAGAIN) {
	  continue;
	}

	/* something worth printing an error about */
	retries--;
	if (retries) {
	  /* print an error and try again */
	  fprintf(stderr, "Error reading %s: read(%d, %p, %ld) errno=%d @ %s:%d %m",
		  file, fd, (char*) buf + n, size - n, errno, __FILE__, __LINE__
		  );
	} else {
	  /* too many failed retries, give up */
	  fprintf(stderr, "Giving up read of %s: read(%d, %p, %ld) errno=%d @ %s:%d %m",
		  file, fd, (char*) buf + n, size - n, errno, __FILE__, __LINE__
		  );
	  exit(1);
	}
      }
    }
  return n;
}
