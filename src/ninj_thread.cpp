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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <map>

#include "ninj_thread.h"
#include "nin_util.h"

using namespace std;

//pthread_mutex_t ninj_thread_mpi_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ninj_thread_mpi_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

nin_spsc_queue<nin_delayed_send_request*> nin_thread_input, nin_thread_output;
map<double, nin_delayed_send_request*> ordered_delayed_send_request_map;

int debug_int  = -1;

struct timespec wait;

static void sleep_wait()
{
  //  double a = NIN_get_time();
  //  clock_gettime(CLOCK_MONOTONIC, &wait);
  //  wait.tv_nsec += 1;//10 * 1000;// (us % (1000 * 1000)) * 1000; 
  //  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wait, NULL);
  usleep(1);
  //  sleep(11110);
  //  double b = NIN_get_time();
  //  NIN_DBG("%f", b - a);
}

static void add_delayed_send_request(nin_delayed_send_request *ds_req)
{
  if (!ds_req->is_final) {
    double send_time = ds_req->send_time;
    while (ordered_delayed_send_request_map.find(send_time) != 
	   ordered_delayed_send_request_map.end()) {
      send_time += 1.0/1e6;
    }
    ds_req->send_time = send_time;
    ordered_delayed_send_request_map[ds_req->send_time] = ds_req;
  } else {
    /*For now, specify big enough latecy to be abel to put this request at the end of queue/map  */
    double send_time = NIN_get_time() * 2; 
    ordered_delayed_send_request_map[send_time] = ds_req;
  }
  return;
}

void* run_delayed_send(void *args)
{
  int ret;
  nin_delayed_send_request* front_ds_req, *new_ds_req;
  map<double, nin_delayed_send_request*>::iterator it;
  int is_erased;
  double send_time;
  struct timespec stval;
  double a, b;
  stval.tv_sec = 0;
  stval.tv_nsec = 1;
  while (1) {
    /*Wait until delayed send request enqueued*/
    if (ordered_delayed_send_request_map.size() == 0) {
      while ((new_ds_req = nin_thread_input.dequeue()) == NULL) {
	sleep_wait();
      }
      send_time = new_ds_req->send_time;
      add_delayed_send_request(new_ds_req);
    }
    it = ordered_delayed_send_request_map.begin();
    front_ds_req = it->second;


    /*Wait until send time expired*/
    while(front_ds_req->send_time >= NIN_get_time() && front_ds_req->is_final == 0) {
      /*  but if new request arrive, then update the next send request */
      if ((new_ds_req = nin_thread_input.dequeue()) != NULL) {
	add_delayed_send_request(new_ds_req);
	if (new_ds_req->send_time < front_ds_req->send_time) {
	  it = ordered_delayed_send_request_map.begin();
	  front_ds_req = it->second;
	}
      }
      sleep_wait();
    }
      
    if (front_ds_req->is_final) return NULL;

    /*Then, start expired send request*/
    PMPI_WRAP(ret = PMPI_Start(&front_ds_req->request), "MPI_Start");
    front_ds_req->is_started = 1;
    //    nin_thread_output.enqueue(front_ds_req);
    // NIN_DBG("send: dest: %d: tag: %d: req: %p, size: %lu, debug_int: %d, enqueuc: %lu, deqc: %lu", 
    //   	    front_ds_req->dest, front_ds_req->tag, front_ds_req->request, ordered_delayed_send_request_map.size(), debug_int, 
    // 	    nin_thread_input.get_enqueue_count(), nin_thread_input.get_dequeue_count());
    ordered_delayed_send_request_map.erase(it);
    front_ds_req = NULL;
  }
  return NULL;
}
