/*
 * FreeGuard: A Faster Secure Heap Allocator
 * Copyright (C) 2017 Sam Silvestro, Hongyu Liu, Corey Crosser, 
 *                    Zhiqiang Lin, and Tongping Liu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * 
 * @file   liblight.cpp: main file, includes memory interception functions.
 * @author Tongping Liu <http://www.cs.utsa.edu/~tongpingliu/>
 * @author Sam Silvestro <sam.silvestro@utsa.edu>
 */
#include <dlfcn.h>
#include <sys/mman.h>
#include <numa.h>
#include <string.h>
#include "real.hh"
#include "xthread.hh"

thread_local thread_t * current;

__attribute__((constructor)) void initializer() {
	xthread::getInstance().initialize();
  Real::initializer();
}

// Intercept thread creation
int pthread_create(pthread_t * tid, const pthread_attr_t * attr,
  void *(*start_routine)(void *), void * arg) {

  return xthread::getInstance().thread_create(tid, attr, start_routine, arg);
}
int pthread_join(pthread_t tid, void** retval) {
	return xthread::getInstance().thread_join(tid, retval);
}
