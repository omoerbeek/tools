//#include <cstddef>
//#include <utility>
//#include <type_traits>
//#include <new>
#include <sys/mman.h>
#include <err.h>
#include <pthread.h>

#include <algorithm>
#include <random>
#include <vector>
#include <iostream>

// On OpenBSD mem used as stack should be marked MAP_STACK
#if !defined(MAP_STACK)
#define MAP_STACK 0
#endif

template <typename T>
struct mmap_allocator {
  using value_type = T;
  using pointer = T*;
  using size_type = std::size_t;
  static_assert (std::is_trivial<T>::value,
                 "mmap_allocator must only be used with trivial types");

  pointer
  allocate (size_type const n) {
    void *p = mmap(nullptr, n * sizeof(value_type),
                   PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_STACK, -1, 0);
    if (p == MAP_FAILED)
      throw std::bad_alloc();
    return static_cast<pointer>(p);
  }

  void
  deallocate (pointer const ptr, size_type const n) noexcept {
    munmap(ptr, n * sizeof(value_type));
  }

  void construct (T*) const noexcept {}

  template <typename X, typename... Args>
  void
  construct (X* place, Args&&... args) const noexcept {
    new (static_cast<void*>(place)) X (std::forward<Args>(args)...);
  }
};

template <typename T> inline
bool operator== (mmap_allocator<T> const&, mmap_allocator<T> const&) noexcept {
  return true;
}

template <typename T> inline
bool operator!= (mmap_allocator<T> const&, mmap_allocator<T> const&) noexcept {
  return false;
}

template <typename T>
struct malloc_allocator {
  using value_type = T;
  using pointer = T*;
  using size_type = std::size_t;
  static_assert (std::is_trivial<T>::value,
                 "malloc_allocator must only be used with trivial types");

  pointer
  allocate (size_type const n) {
    void *p = malloc(n * sizeof(value_type));
    if (p == nullptr)
      throw std::bad_alloc();
    return static_cast<pointer>(p);
  }

  void
  deallocate (pointer const ptr, size_type const n) noexcept {
    free(ptr);
  }

  void construct (T*) const noexcept {}

  template <typename X, typename... Args>
  void
  construct (X* place, Args&&... args) const noexcept {
    new (static_cast<void*>(place)) X (std::forward<Args>(args)...);
  }
};

template <typename T> inline
bool operator== (malloc_allocator<T> const&, malloc_allocator<T> const&) noexcept {
  return true;
}

template <typename T> inline
bool operator!= (malloc_allocator<T> const&, malloc_allocator<T> const&) noexcept {
  return false;
}

template <typename A>
std::vector<char, A>* alloc(void) {
  auto v = new(std::vector<char, A>);
  v->resize(200001);
  return v;
}

template <typename A>
void exercise(size_t count) {
  auto allocs = std::vector<std::vector<char, A>*>(count);
  for (size_t i = 0; i < count; i++) {
    allocs[i] = alloc<A>();
  }
/*
  for (auto i = allocs.begin(); i != allocs.end(); ++i) {
    for (auto j = (*i)->begin(); j != (*i)->end(); ++j) {
      *j = 99;
    }
  }
*/
  auto rng = std::default_random_engine {};
  std::shuffle(std::begin(allocs), std::end(allocs), rng);

  for (size_t i = 0; i < count/2; i++) {
    delete allocs[i];
  }
  for (size_t i = 0; i < count/2; i++) {
    allocs[i] = alloc<A>();
  }
/*
  for (auto i = allocs.begin(); i != allocs.end(); ++i) {
    for (auto j = (*i)->begin(); j != (*i)->end(); ++j) {
      *j = 99;
    }
  }
*/
  for (size_t i = 0; i < count; i++) {
    delete allocs[i];
  }
}

void *f1(void *) {
	for (int i = 0; i < 100; i++)
		exercise<mmap_allocator<char>>(400);
	return nullptr;
}

void *f2(void *) {
	for (int i = 0; i < 100; i++)
		exercise<malloc_allocator<char>>(400);
	return nullptr;
}

void start(size_t count, void *(*f)(void *)) {

  pthread_t p[count];
  for (int i = 0; i < count; i++) {
    if (pthread_create(&p[i], nullptr, f, nullptr) != 0)
      err(1, "pthread_create");
  }
  for (int i = 0; i < count; i++) {
    if (pthread_join(p[i], nullptr) != 0)
      err(1, "pthread_join");
  }
}

int
main(int argc, char *argv[]) {
  extern char *__progname;

  if (argc < 2) {
    errx(1, "usage: %s [1|2]\n", __progname);
  }
  switch (atoi(argv[1])) {
    case 1:
      std::cout << "Method 1: mmap_allocator" << std::endl;
      start(10, f1);
      break;
    case 2:
      std::cout << "Method 2: malloc_allocator" << std::endl;
      start(10, f2);
      break;
    default:
      errx(1, "usage: %s [1|2|...]\n", __progname);
  }
}
