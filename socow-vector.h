#pragma once

#include <cstddef>
#include <array>
#include <algorithm>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
public:
  using iterator = T*;
  using const_iterator = T const*;

  socow_vector();
  socow_vector(socow_vector const &);
  socow_vector& operator=(socow_vector const& other);

  ~socow_vector();

  T& operator[](size_t i);
  T const& operator[](size_t i) const;

  T* data();
  T const* data() const;
  size_t size() const;

  T& front();
  T const& front() const;

  T& back();
  T const& back() const;

  void push_back(T const&);
  void pop_back();

  bool empty() const;

  size_t capacity() const;
  void reserve(size_t);
  void shrink_to_fit();

  void clear();

  void swap(socow_vector& other);

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;

  iterator insert(const_iterator pos, T const&);

  iterator erase(const_iterator pos);

  iterator erase(const_iterator first, const_iterator last);

private:
  struct dynamic_buffer {
    size_t ref_count;
    size_t capacity_;
    T dynamic_data[0];
  };
  const_iterator cbegin();
  void reset();
  void unshare();
  void clear_dynamic();
  void change_capacity(size_t new_capacity);
  static void swap_static_and_dynamic(socow_vector& stat, socow_vector& dynamic);
  static void copy_range(T const* from, T* to, size_t start, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      try {
        new (to + start + i) T(from[start + i]);
      } catch (...) {
        clear_range(to, start, i);
        throw;
      }
    }
  }
  static void copy_from_begin(T const* from, T* to, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      try {
        new (to + i) T(from[i]);
      } catch (...) {
        clear_prefix(to, i);
        throw;
      }
    }
  }
  static void clear_range(T* array_, size_t start, size_t length) {
    for (size_t i = start + length; i > start; --i) {
      array_[i - 1].~T();
    }
  }
  static void clear_prefix(T* array_, size_t prefix_length) {
    clear_range(array_, 0, prefix_length);
  }
  static dynamic_buffer* allocate_buffer(size_t new_cap) {
    dynamic_buffer* new_dynamic_buffer = reinterpret_cast<dynamic_buffer*>(
        operator new(sizeof(dynamic_buffer) + sizeof(T) * new_cap));
    new_dynamic_buffer->capacity_ = new_cap;
    new_dynamic_buffer->ref_count = 1;
    return new_dynamic_buffer;
  }

  size_t size_{0};
  bool dynamic_{false};
  union {
    T static_data[SMALL_SIZE];
    dynamic_buffer* dynamic_buffer_;
  };
};

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::clear_dynamic() {
  if (dynamic_buffer_->ref_count > 1) {
    dynamic_buffer_->ref_count--;
  } else {
    clear_prefix(dynamic_buffer_->dynamic_data, size_);
    operator delete(dynamic_buffer_);
  }
}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::socow_vector() = default;;

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::socow_vector(const socow_vector& other)
    : size_(other.size_),
      dynamic_(other.dynamic_)
{
  if (other.dynamic_) {
    dynamic_buffer_ = other.dynamic_buffer_;
    dynamic_buffer_->ref_count++;
  } else {
    copy_from_begin(other.static_data, static_data, other.size_);
  }
}

template <typename T, size_t SMALL_SIZE> socow_vector<T, SMALL_SIZE>&
socow_vector<T, SMALL_SIZE>::operator=(const socow_vector& other) {
  if (this != &other) {
    socow_vector(other).swap(*this);
  }
  return *this;
}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::~socow_vector() {
  reset();
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::operator[](size_t i) {
  return data()[i];
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::operator[](size_t i) const {
  return data()[i];
}

template <typename T, size_t SMALL_SIZE>
T* socow_vector<T, SMALL_SIZE>::data() {
  unshare();
  return dynamic_ ? dynamic_buffer_->dynamic_data
                  : static_data;
}

template <typename T, size_t SMALL_SIZE>
T const* socow_vector<T, SMALL_SIZE>::data() const {
  return dynamic_ ? dynamic_buffer_->dynamic_data
                  : static_data;
}

template <typename T, size_t SMALL_SIZE>
size_t socow_vector<T, SMALL_SIZE>::size() const {
  return size_;
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::front() {
  return data()[0];
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::front() const {
  return data()[0];
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::back() {
  return data()[size_ - 1];
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::back() const {
  return data()[size_ - 1];
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::push_back(const T& value) {
  if (size() == capacity()) {
    dynamic_buffer* tmp = allocate_buffer(capacity() * 2);
    try {
      copy_from_begin(data(), tmp->dynamic_data, size_);
    } catch (...) {
      operator delete(tmp);
      throw;
    }
    try {
      new (&tmp->dynamic_data[size_]) T(value);
    } catch (...) {
      clear_prefix(tmp->dynamic_data, size_);
      operator delete(tmp);
      throw;
    }
    reset();
    dynamic_buffer_ = tmp;
    dynamic_ = true;
  } else {
    new (data() + size_) T(value);
  }
  size_++;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::pop_back() {
  data()[--size_].~T();
}

template <typename T, size_t SMALL_SIZE>
bool socow_vector<T, SMALL_SIZE>::empty() const {
  return size_ == 0;
}

template <typename T, size_t SMALL_SIZE>
size_t socow_vector<T, SMALL_SIZE>::capacity() const {
  return dynamic_ ? dynamic_buffer_->capacity_ : SMALL_SIZE;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::reserve(size_t new_capacity) {
  if (capacity() < new_capacity) {
    change_capacity(new_capacity);
  } else {
    unshare();
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::shrink_to_fit() {
  if (size_ != capacity()) {
    change_capacity(size_);
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::clear() {
  unshare();
  if (dynamic_) {
    clear_prefix(dynamic_buffer_->dynamic_data, size_);
  } else {
    clear_prefix(static_data, size_);
  }
  size_ = 0;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::swap(socow_vector& other) {
  if (dynamic_ && other.dynamic_) {
    std::swap(dynamic_buffer_, other.dynamic_buffer_);
  } else if (dynamic_ && !other.dynamic_) {
    swap_static_and_dynamic(other, *this);
  } else if (!dynamic_ && other.dynamic_) {
    swap_static_and_dynamic(*this, other);
  } else {
    using std::swap;
    for (size_t i = 0; i < std::min(size_, other.size_); ++i) {
      swap(static_data[i], other.static_data[i]);
    }
    if (size_ < other.size_) {
      copy_range(other.static_data, static_data, size_, other.size_ - size_);
      clear_range(other.static_data, size_, other.size_ - size_);
    } else {
      copy_range(static_data, other.static_data, other.size_, size_ - other.size_);
      clear_range(static_data, other.size_, size_ - other.size_);
    }
  }
  std::swap(size_, other.size_);
  std::swap(dynamic_, other.dynamic_);
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::swap_static_and_dynamic(
    socow_vector& stat, socow_vector& dynamic) {
  dynamic_buffer* tmp_dynamic_buffer = dynamic.dynamic_buffer_;
  dynamic.dynamic_buffer_ = nullptr;
  try {
    copy_from_begin(stat.static_data, dynamic.static_data, stat.size_);
  } catch (...) {
    dynamic.dynamic_buffer_ = tmp_dynamic_buffer;
    throw;
  }
  clear_prefix(stat.static_data, stat.size_);
  stat.dynamic_buffer_ = tmp_dynamic_buffer;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>
    ::iterator socow_vector<T, SMALL_SIZE>::begin() {
  return data();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>
    ::iterator socow_vector<T, SMALL_SIZE>::end() {
  return data() + size_;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>
    ::const_iterator socow_vector<T, SMALL_SIZE>::begin() const {
  return data();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>
    ::const_iterator socow_vector<T, SMALL_SIZE>::end() const {
  return data() + size_;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::insert(socow_vector::const_iterator pos,
                                    const T& value) {
  size_t ind = std::distance(cbegin(),
                             pos);
  push_back(value);
  T* cur_data = data();
  using std::swap;
  for (size_t i = size_ - 1; i > ind; --i) {
    swap(cur_data[i - 1], cur_data[i]);
  }
  return cur_data + ind;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::erase(socow_vector::const_iterator pos) {
  return erase(pos, pos + 1);
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::erase(socow_vector::const_iterator first,
                                   socow_vector::const_iterator last) {
  const_iterator cbegin_ = cbegin();
  size_t ind1 = std::distance(cbegin_,
                             first);
  size_t ind2 = std::distance(cbegin_,
                              last);
  T* cur_data = data();
  using std::swap;
  for (size_t i = 0; i < size() - ind2; ++i) {
    swap(cur_data[ind1 + i], cur_data[ind2 + i]);
  }
  for (size_t i = 0; i < ind2 - ind1; i++) {
    pop_back();
  }
  return cur_data + ind1;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::unshare() {
  if (dynamic_ && dynamic_buffer_->ref_count > 1) {
    if (size_ > SMALL_SIZE) {
      change_capacity(dynamic_buffer_->capacity_);
    } else {
      change_capacity(SMALL_SIZE);
    }
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::change_capacity(size_t new_capacity) {
  if (new_capacity <= SMALL_SIZE && dynamic_) {
    dynamic_buffer* tmp = dynamic_buffer_;
    try {
      copy_from_begin(tmp->dynamic_data, static_data, size_);
      if (--tmp->ref_count == 0) {
        for (size_t i = size_; i > 0; --i) {
          tmp->dynamic_data[i - 1].~T();
        }
        operator delete(tmp);
      }
      dynamic_ = false;
    } catch (...) {
      dynamic_buffer_ = tmp;
      throw;
    }
  } else if (new_capacity > SMALL_SIZE) {
    dynamic_buffer* new_dynamic_buffer = allocate_buffer(new_capacity);
    if (dynamic_) {
      try {
        copy_from_begin(dynamic_buffer_->dynamic_data, new_dynamic_buffer->dynamic_data,
             size_);
        clear_dynamic();
      } catch (...) {
        operator delete(new_dynamic_buffer);
        throw;
      }
    } else {
      try {
        copy_from_begin(static_data, new_dynamic_buffer->dynamic_data, size_);
        clear_prefix(static_data, size_);
      } catch (...) {
        operator delete(new_dynamic_buffer);
        throw;
      }
    }
    dynamic_buffer_ = new_dynamic_buffer;
    dynamic_ = true;
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::reset() {
  if (dynamic_) {
    clear_dynamic();
  } else {
    clear_prefix(static_data, size_);
  }
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::const_iterator
socow_vector<T, SMALL_SIZE>::cbegin() {
  return dynamic_ ? dynamic_buffer_->dynamic_data : static_data;
}
