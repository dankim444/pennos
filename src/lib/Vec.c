#include "./Vec.h"
#include <stdio.h>
#include <stdlib.h>

Vec vec_new(size_t initial_capacity, ptr_dtor_fn ele_dtor_fn) {
  Vec vector;
  vector.capacity = initial_capacity;
  vector.data = malloc(sizeof(void*) * initial_capacity);
  vector.ele_dtor_fn = ele_dtor_fn;
  vector.length = 0;
  return vector;
}

void vec_destroy(Vec* self) {
  if (self->ele_dtor_fn) {
    for (int i = 0; i < self->length; i++) {
      self->ele_dtor_fn(self->data[i]);
    }
  }
  free(self->data);
}

/* Erases all elements from the container.
 * After this, the length of the vector is zero.
 * Capacity of the vector is unchanged.
 *
 * @param self a pointer to the vector we want to clear.
 * @pre Assumes self points to a valid vector.
 * @post The removed elements are destructed (cleaned up).
 */
void vec_clear(Vec* self) {
  if (self->ele_dtor_fn) {
    for (int i = 0; i < self->length; i++) {
      self->ele_dtor_fn(self->data[i]);
    }
  }

  self->length = 0;
}

void vec_resize(Vec* self, size_t new_capacity) {
  if (new_capacity * sizeof(void*) < new_capacity) {
    perror("vec_resize: new capacity too large");
  }
  if (new_capacity > self->length) {
    self->capacity = new_capacity;
    ptr_t* new_data = malloc(sizeof(void*) * self->capacity);

    // Copy over old elements
    for (int i = 0; i < self->length; i++) {
      new_data[i] = self->data[i];
    }

    free(self->data);

    self->data = new_data;
  }
}

void vec_erase(Vec* self, size_t index) {
  if (index >= self->length) {
    perror("vec_erase: index >= vec length");
  }

  if (self->ele_dtor_fn) {
    self->ele_dtor_fn(self->data[index]);
  }

  for (unsigned int i = index; i < self->length - 1; i++) {
    self->data[i] = self->data[i + 1];
  }

  self->length--;
}

void vec_erase_no_deletor(Vec* self, size_t index) {
  if (index >= self->length) {
    perror("vec_erase: index >= vec length");
  }

  for (unsigned int i = index; i < self->length - 1; i++) {
    self->data[i] = self->data[i + 1];
  }

  self->length--;
}

void vec_insert(Vec* self, size_t index, ptr_t new_ele) {
  if (index > self->length) {
    perror("vec_insert: index > vec length");
  }

  if (index == self->length) {  // Insertion at end = Adding at end
    vec_push_back(self, new_ele);
  } else {  // Inserting not at the end
    // Vector is full
    if (self->length == self->capacity) {
      vec_resize(self, self->capacity * 2);
    }
    // Insertion + Displacement
    for (unsigned int i = self->length; i > index; i--) {
      self->data[i] = self->data[i - 1];
    }
    self->data[index] = new_ele;

    self->length++;
  }
}

bool vec_pop_back(Vec* self) {
  if (self->length == 0) {
    return false;
  }
  if (self->ele_dtor_fn) {
    self->ele_dtor_fn(self->data[--self->length]);
  } else {
    self->length--;
  }
  return true;
}

void vec_push_back(Vec* self, ptr_t new_ele) {
  if (self->capacity == self->length) {
    if (self->capacity == 0) {
      vec_resize(self, 1);
    } else {
      vec_resize(self, self->capacity * 2);
    }
  }

  if (self->capacity == self->length) {
    perror("vec_push_back: resize failed");
  }

  // The array is 0 indexed
  self->data[self->length++] = new_ele;
}

void vec_set(Vec* self, size_t index, ptr_t new_ele) {
  if (index >= self->length) {
    perror("vec_set: idx >= len");
  }
  if (self->ele_dtor_fn) {
    self->ele_dtor_fn(self->data[index]);
  }
  self->data[index] = new_ele;
}

ptr_t vec_get(Vec* self, size_t index) {
  if (index >= self->length) {
    perror("vec_get: index greater than length");
  }
  return self->data[index];
}
