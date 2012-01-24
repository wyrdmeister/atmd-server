/*
 * Xenomai Vector class
 * (Inspired to STL vector class - It implements only a subset of complete STL functionality)
 *
 * Copyright (C) Michele Devetta 2012 <michele.devetta@unimi.it>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XENO_VEC_H
#define XENO_VEC_H

#include <string.h>
#include <native/heap.h>

template <typename T> class xenovec {
public:
  xenovec(RT_HEAP *heap) : _heap(heap), _data(NULL), _cap(0), _sz(0) {};
  ~xenovec() { rt_heap_free(_heap, _data); };

public:
  // push_back method: add one element at the end of the array
  void push_back(const T& value) {
    if(_cap <= _sz) {
      _realloc(_sz+1);
    }
    _data[_sz] = value;
    _sz++;
  };

  // clear method: delete all elements (it does not deallocate memory)
  void clear() { _sz = 0; };

  // reserve() method: preallocate storage for vector
  void reserve(size_t sz) {
    if(sz > _cap)
      _realloc(sz);
  };

  // size() method: return the number of elements in the array
  size_t size()const { return _sz; };

  // capacity() method: return the available storage in the vector object
  size_t capacity()const { return _cap; };

  // [] operator: select an element given it's index (throw an exception if the index is too high)
  T& operator[](size_t i) { if(i >= _sz) throw(-1); return _data[i]; };
  const T& operator[](size_t i)const { if(i >= _sz) throw(-1); return _data[i]; };
  
private:
  // Private function to increase buffer size
  void _realloc(size_t sz) {
    T* newptr = NULL;
    int retval = 0;
    retval = rt_heap_alloc(_heap, sz * sizeof(T), TM_NONBLOCK, (void**)&newptr);
    if(retval)
      throw(retval);
    if(_data != NULL && _sz != 0) {
      // NOTE: this memcpy may be a problem with objects that needs to be copy constructed!
      memcpy(newptr, _data, _sz * sizeof(T));
      retval = rt_heap_free(_heap, _data);
      if(retval)
        throw(retval);
    }
    _data = newptr;
    _cap = sz;
  };

  // Heap descriptor
  RT_HEAP * _heap;

  // Data pointer
  T* _data;

  // Size and capacity counters
  size_t _cap;
  size_t _sz;
};

#endif
