/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2025 Jose Vicente Campos Martinez, <josevcm@gmail.com>

  HCE-LABORATORY is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  HCE-LABORATORY is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with HCE-LABORATORY. If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef RT_BUFFER_H
#define RT_BUFFER_H

#include <cassert>
#include <cstring>
#include <memory>
#include <functional>

#include <rt/Heap.h>

#define ALLOC_ALIGNMENT 128

namespace rt {

template <class T>
class Buffer
{
   public:

      enum Direction
      {
         Left = 0,
         Right = 1
      };

   protected:

      std::shared_ptr<Alloc<T>> alloc;

      struct State
      {
         unsigned int position; // current data position
         unsigned int capacity; // buffer data capacity
         unsigned int limit; // buffer data limit
      } state;

      struct Attrs
      {
         unsigned int type; // data type
         unsigned int stride; // data stride, how many data has in one chunk for all channels
         unsigned int interleave; // data interleave, how many consecutive data has per channel
         void *context; // context payload
      } attrs;

   public:

      static Heap<T> heap;

      /**
       * Creates an empty/invalid buffer with no allocation.
       */
      Buffer() : state {0, 0, 0}, attrs {0, 0, 0, nullptr}
      {
      }

      /**
       * Creates a shallow copy that shares the same allocation/state snapshot.
       * @param other Source buffer.
       */
      Buffer(const Buffer &other) : alloc(other.alloc), state(other.state), attrs(other.attrs)
      {
      }

      /**
       * Creates a buffer from raw data and flips it for read mode.
       * @param data Source pointer.
       * @param capacity Number of elements to allocate/copy.
       * @param type Optional payload type tag.
       * @param stride Elements per logical chunk.
       * @param interleave Channel interleave.
       * @param context Optional user context pointer.
       */
      explicit Buffer(const T *data, unsigned int capacity, unsigned int type = 0, unsigned int stride = 1, unsigned int interleave = 1, void *context = nullptr) : Buffer(capacity, type, stride, interleave, context)
      {
         if (data && capacity)
         {
            put(data, capacity);
            flip();
         }
      }

      /**
       * Creates a buffer with allocated storage.
       * @param capacity Number of elements to allocate.
       * @param type Optional payload type tag.
       * @param stride Elements per logical chunk.
       * @param interleave Channel interleave.
       * @param context Optional user context pointer.
       */
      explicit Buffer(unsigned int capacity, unsigned int type = 0, unsigned int stride = 1, unsigned int interleave = 1, void *context = nullptr) : state {0, capacity, capacity}, attrs {type, stride, interleave, context}
      {
         alloc = heap.alloc(capacity, ALLOC_ALIGNMENT);
      }

      /**
       * Creates a buffer from an initializer list and flips it for read mode.
       * @param data Source elements.
       * @param type Optional payload type tag.
       * @param stride Elements per logical chunk.
       * @param interleave Channel interleave.
       * @param context Optional user context pointer.
       */
      Buffer(std::initializer_list<T> data, unsigned int type = 0, unsigned int stride = 1, unsigned int interleave = 1, void *context = nullptr) : Buffer(data.size(), type, stride, interleave, context)
      {
         put(data);
         flip();
      }

      /**
       * Destroys the buffer wrapper (shared allocation is released automatically).
       */
      ~Buffer() = default;

      /**
       * Assigns from another buffer (shared allocation semantics).
       * @param other Source buffer.
       * @return Reference to this buffer.
       */
      Buffer &operator=(const Buffer &other)
      {
         if (&other == this)
            return *this;

         alloc = other.alloc;
         state = other.state;
         attrs = other.attrs;

         return *this;
      }

      /**
       * Compares readable ranges for equality.
       * @param other Buffer to compare.
       * @return True if readable bytes/elements are equal.
       */
      bool operator==(const Buffer &other) const
      {
         if (this == &other)
            return true;

         if (remaining() != other.remaining())
            return false;

         if (alloc == other.alloc)
            return true;

         return std::memcmp(ptr(), other.ptr(), remaining()) == 0;
      }

      /**
       * Compares readable ranges for inequality.
       * @param other Buffer to compare.
       * @return True if readable ranges differ.
       */
      bool operator!=(const Buffer &other) const
      {
         return !operator==(other);
      }

      /**
       * Releases allocation and resets all state/attributes.
       */
      void reset()
      {
         alloc.reset();
         state = {0, 0, 0};
         attrs = {0, 0, 0, nullptr};
      }

      /** @return True if buffer has a valid allocation. */
      bool isValid() const
      {
         return alloc != nullptr;
      }

      /** @return True if position reached limit. */
      bool isEmpty() const
      {
         return state.position == state.limit;
      }

      /** @return True if position reached capacity. */
      bool isFull() const
      {
         return state.position == state.capacity;
      }

      /** @return Current cursor position. */
      unsigned int position() const
      {
         return state.position;
      }

      /** @return Current logical limit. */
      unsigned int limit() const
      {
         return state.limit;
      }

      /** @return Total allocated capacity (elements). */
      unsigned int capacity() const
      {
         return state.capacity;
      }

      /** @return Elements available to read/write until limit. */
      unsigned int remaining() const
      {
         return state.limit - state.position;
      }

      /** @return Logical element count considering stride/interleave metadata. */
      unsigned int elements() const
      {
         return state.limit * attrs.interleave / attrs.stride;
      }

      /** @return Stride metadata value. */
      unsigned int stride() const
      {
         return attrs.stride;
      }

      /** @return Interleave metadata value from allocation. */
      unsigned int interleave() const
      {
         assert(alloc != nullptr);
         return alloc->interleave;
      }

      /** @return Readable size in bytes. */
      unsigned int size() const
      {
         return state.limit * sizeof(T);
      }

      /** @return Chunk size in bytes (stride * sizeof(T)). */
      unsigned int chunk() const
      {
         return attrs.stride * sizeof(T);
      }

      /** @return User-defined type tag. */
      unsigned int type() const
      {
         return attrs.type;
      }

      /** @return User context pointer. */
      void *context() const
      {
         return attrs.context;
      }

      /** @return Pointer to start of allocated storage. */
      T *data() const
      {
         assert(alloc != nullptr);
         assert(alloc->data != nullptr);
         return alloc->data;
      }

      /** @return Pointer at current position. */
      T *ptr() const
      {
         assert(alloc != nullptr);
         assert(alloc->data != nullptr);
         return alloc->data + state.position;
      }

      /**
       * Resizes storage preserving up to min(newCapacity, current limit) elements.
       * @param newCapacity New capacity in elements.
       * @return Reference to this buffer.
       */
      Buffer &resize(unsigned int newCapacity)
      {
         assert(alloc != nullptr);

         const int count = std::min(newCapacity, state.limit);

         Alloc<T> newAlloc = heap.acquire(newCapacity, ALLOC_ALIGNMENT);

         std::memcpy(newAlloc->data, alloc->data, count * sizeof(T));

         heap.release(alloc);

         alloc = newAlloc;
         state.limit = newCapacity > state.limit ? state.limit : newCapacity;
         state.capacity = newCapacity;

         return *this;
      }

      /**
       * Sets write/read mode to full capacity and rewinds position.
       * @return Reference to this buffer.
       */
      Buffer &clear()
      {
         assert(alloc != nullptr);

         state.limit = state.capacity;
         state.position = 0;

         return *this;
      }

      /**
       * Writes repeated value at current position.
       * @param value Value to write.
       * @param count Number of elements.
       * @return Reference to this buffer.
       */
      Buffer &fill(T value, unsigned int count)
      {
         assert(alloc != nullptr);
         assert(state.position + count <= state.limit);

         memset(alloc->data + state.position, value, count * sizeof(T));

         state.position += count;

         return *this;
      }

      /**
       * Switches from write mode to read mode (limit=position, position=0).
       * @return Reference to this buffer.
       */
      Buffer &flip()
      {
         assert(alloc != nullptr);

         state.limit = state.position;
         state.position = 0;

         return *this;
      }

      /**
       * Rewinds position to the start without changing limit.
       * @return Reference to this buffer.
       */
      Buffer &rewind()
      {
         assert(alloc != nullptr);

         state.position = 0;

         return *this;
      }

      /**
       * Extends current limit by a number of elements.
       * @param size Elements to add to limit.
       * @return Reference to this buffer.
       */
      Buffer &room(unsigned int size)
      {
         assert(alloc != nullptr);
         assert(state.limit + size <= state.capacity);

         state.limit += size;

         return *this;
      }

      /**
       * Extracts one element from the head (position++)
       * @return Extracted element.
       */
      T get()
      {
         assert(alloc != nullptr);
         assert(state.position < state.limit);

         return alloc->data[state.position++];
      }

      /**
       * Reads one element from the head without moving position.
       * @return Peeked element.
       */
      T peek() const
      {
         assert(alloc != nullptr);
         assert(state.position < state.limit);

         return alloc->data[state.position];
      }

      /**
       * Extracts one element from the tail (limit--).
       * @return Extracted element.
       */
      T pop()
      {
         assert(alloc != nullptr);
         assert(state.position < state.limit);

         return alloc->data[--state.limit];
      }

      /**
       * Writes one element at current position (position++).
       * @param value Element value.
       * @return Reference to this buffer.
       */
      Buffer &put(const T &value)
      {
         assert(alloc != nullptr);
         assert(state.position < state.limit);

         alloc->data[state.position++] = value;

         return *this;
      }

      /**
       * Writes an initializer list at current position.
       * @param data Source values.
       * @return Reference to this buffer.
       */
      Buffer &put(std::initializer_list<T> data)
      {
         for (auto b: data)
            put(b);

         return *this;
      }

      /**
       * Extracts multiple elements from head.
       * @param data Destination pointer.
       * @param elements Number of elements to read.
       * @return Reference to this buffer.
       */
      Buffer &get(T *data, unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(elements <= state.limit - state.position);

         std::memcpy(data, alloc->data + state.position, elements * sizeof(T));

         state.position += elements;

         return *this;
      }

      /**
       * Reads multiple elements from head without moving position.
       * @param data Destination pointer.
       * @param elements Number of elements to read.
       * @return Const reference to this buffer.
       */
      const Buffer &peek(T *data, unsigned int elements) const
      {
         assert(alloc != nullptr);
         assert(elements <= state.limit - state.position);

         std::memcpy(data, alloc->data + state.position, elements * sizeof(T));

         return *this;
      }

      /**
       * Extracts multiple elements from tail.
       * @param data Destination pointer.
       * @param elements Number of elements to pop.
       * @return Reference to this buffer.
       */
      Buffer &pop(T *data, unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(elements <= state.limit - state.position);

         std::memcpy(data, alloc->data + state.limit - elements, elements * sizeof(T));

         state.limit -= elements;

         return *this;
      }

      /**
       * Writes multiple elements from pointer.
       * @param data Source pointer.
       * @param elements Number of elements to write.
       * @return Reference to this buffer.
       */
      Buffer &put(const T *data, unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(elements <= state.limit - state.position);

         std::memcpy(alloc->data + state.position, data, elements * sizeof(T));

         state.position += elements;

         return *this;
      }

      /**
       * Extracts elements from this buffer into another buffer.
       * @param data Destination buffer.
       * @param elements Number of elements to transfer.
       * @return Reference to this buffer.
       */
      Buffer &get(Buffer &data, unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(data.remaining() >= elements);

         int count = std::min(elements, state.limit - state.position);
         data.put(alloc->data + state.position, count);
         data.flip();

         state.position += count;

         return *this;
      }

      /**
       * Reads elements into another buffer without moving this position.
       * @param data Destination buffer.
       * @param elements Number of elements to copy.
       * @return Const reference to this buffer.
       */
      const Buffer &peek(Buffer &data, unsigned int elements) const
      {
         assert(alloc != nullptr);
         assert(data.remaining() >= elements);

         int count = std::min(elements, state.limit - state.position);
         data.put(alloc->data + state.position, count);
         data.flip();

         return *this;
      }

      /**
       * Pops elements from this tail into destination buffer.
       * @param data Destination buffer.
       * @param elements Number of elements to pop.
       * @return Reference to this buffer.
       */
      Buffer &pop(Buffer &data, unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(elements <= state.limit - state.position);
         assert(data.remaining() >= elements);

         pop(data.ptr(), elements);

         return *this;
      }

      /**
       * Writes elements from source buffer into this buffer.
       * @param data Source buffer.
       * @param elements Number of elements to copy.
       * @return Reference to this buffer.
       */
      Buffer &put(const Buffer &data, unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(elements <= state.limit - state.position);
         assert(data.remaining() >= elements);

         put(data.ptr(), elements);

         return *this;
      }

      /**
       * Extracts up to destination remaining elements from head.
       * @param data Destination buffer.
       * @return Reference to this buffer.
       */
      Buffer &get(Buffer &data)
      {
         return get(data, data.remaining());
      }

      /**
       * Reads up to destination remaining elements without moving position.
       * @param data Destination buffer.
       * @return Const reference to this buffer.
       */
      const Buffer &peek(Buffer &data) const
      {
         return peek(data, data.remaining());
      }

      /**
       * Pops destination remaining elements from tail.
       * @param data Destination buffer.
       * @return Reference to this buffer.
       */
      Buffer &pop(Buffer &data)
      {
         return pop(data, data.remaining());
      }

      /**
       * Writes source remaining elements to this buffer.
       * @param data Source buffer.
       * @return Reference to this buffer.
       */
      Buffer &put(const Buffer &data)
      {
         return put(data, data.remaining());
      }

      /**
       * Reserves space at head side and returns writable pointer.
       * @param elements Number of elements to reserve.
       * @param clear If true, zero-initialize reserved range.
       * @return Pointer to the reserved range start.
       */
      T *push(unsigned int elements, bool clear = false)
      {
         assert(alloc != nullptr);
         assert(state.position + elements <= state.capacity);

         if (clear)
            std::memset(alloc->data + state.position, 0, elements * sizeof(T));

         state.position += elements;

         return alloc->data + state.position - elements;
      }

      /**
       * Moves position backward and returns pointer to restored region.
       * @param elements Number of elements to release.
       * @param clear If true, zero the released range.
       * @return Pointer to the new current position.
       */
      T *pull(unsigned int elements, bool clear = false)
      {
         assert(alloc != nullptr);
         assert(state.position - elements >= 0);

         state.position -= elements;

         if (clear)
            std::memset(alloc->data + state.position, 0, elements * sizeof(T));

         return alloc->data + state.position;
      }

      /**
       * Moves position forward by a number of elements.
       * @param elements Number of elements to skip.
       * @return Shallow copy of this buffer state after skip.
       */
      Buffer skip(unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(state.position + elements <= state.limit);

         state.position += elements;

         return *this;
      }

      /**
       * Rotates underlying storage left/right by count.
       * @param dir Rotation direction.
       * @param count Number of positions.
       * @return Reference to this buffer.
       */
      Buffer &rotate(Direction dir, unsigned int count = 1)
      {
         if (count > state.capacity)
            count %= state.capacity;

         T tmp[count];

         switch (dir)
         {
            case Left:
            {
               for (int i = 0; i < state.capacity; ++i)
               {
                  if (i < count)
                     tmp[i] = alloc->data[i];

                  if (i < state.capacity - count)
                     alloc->data[i] = alloc->data[i + count];
                  else
                     alloc->data[i] = tmp[i - state.capacity + count];
               }

               break;
            }

            case Right:
            {
               for (int i = state.capacity - 1; i >= 0; --i)
               {
                  if (i >= state.capacity - count)
                     tmp[i - state.capacity + count] = alloc->data[i];

                  if (i >= count)
                     alloc->data[i] = alloc->data[i - count];
                  else
                     alloc->data[i] = tmp[i];
               }

               break;
            }
         }

         return *this;
      }

      /**
       * Shifts underlying storage left/right filling emptied elements with zero.
       * @param dir Shift direction.
       * @param count Number of positions.
       * @return Reference to this buffer.
       */
      Buffer &shift(Direction dir, unsigned int count = 1)
      {
         if (count > state.capacity)
            count %= state.capacity;

         switch (dir)
         {
            case Left:
            {
               for (int i = 0; i < state.capacity; ++i)
               {
                  if (i < state.capacity - count)
                     alloc->data[i] = alloc->data[i + count];
                  else
                     alloc->data[i] = 0;
               }

               break;
            }

            case Right:
            {
               for (int i = state.capacity - 1; i >= 0; --i)
               {
                  if (i >= count)
                     alloc->data[i] = alloc->data[i - count];
                  else
                     alloc->data[i] = 0;
               }

               break;
            }
         }

         return *this;
      }

      /**
       * Writes a slice from source buffer into this buffer at offset.
       * @param data Source buffer.
       * @param offset Destination offset in this buffer.
       * @param elements Number of elements to copy.
       * @return Reference to this buffer.
       */
      Buffer &set(const Buffer &data, unsigned int offset, unsigned int elements)
      {
         assert(alloc != nullptr);
         assert(offset + elements <= state.capacity);
         assert(data.remaining() >= elements);

         T *src = data.ptr();

         for (int i = 0; i < elements; i++)
            alloc->data[offset + i] = src[i];

         return *this;
      }

      /**
       * Writes all source remaining elements into this buffer at offset.
       * @param data Source buffer.
       * @param offset Destination offset.
       * @return Reference to this buffer.
       */
      Buffer &set(const Buffer &data, unsigned int offset)
      {
         return set(data, offset, data.remaining());
      }

      /**
       * Shrinks limit by size elements from tail.
       * @param size Number of elements to trim.
       * @return Reference to this buffer.
       */
      Buffer &trim(unsigned int size)
      {
         assert(alloc != nullptr);
         assert(state.position <= state.limit - size);

         state.limit = state.limit - size;

         return *this;
      }

      /**
       * Reduces readable range into an accumulator value.
       * @tparam E Accumulator type.
       * @param value Initial accumulator value.
       * @param handler Reduction function(acc, element) -> acc.
       * @return Final reduced accumulator.
       */
      template <typename E>
      E reduce(E value, const std::function<E(E, T)> &handler) const
      {
         assert(alloc != nullptr);

         for (int i = state.position; i < state.limit; ++i)
            value = handler(value, alloc->data[i]);

         return value;
      }

      /**
       * Streams data by stride-sized chunks.
       * @param handler Callback receiving chunk pointer and chunk element count.
       */
      void stream(const std::function<void(const T *, unsigned int)> &handler) const
      {
         assert(alloc != nullptr);

         for (int i = state.position; i < state.limit; i += attrs.stride)
            handler(alloc->data + i, attrs.stride);
      }

      /**
       * Random-access element by absolute index.
       * @param index Absolute index.
       * @return Mutable element reference.
       */
      T &operator[](unsigned int index)
      {
         assert(alloc != nullptr);

         return alloc->data[index];
      }

      /**
       * Random-access element by absolute index.
       * @param index Absolute index.
       * @return Const element reference.
       */
      const T &operator[](unsigned int index) const
      {
         assert(alloc != nullptr);

         return alloc->data[index];
      }
};

template <class T>
Heap<T> Buffer<T>::heap;

}

#endif
