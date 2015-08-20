// RUN: %python %S/check_clang_tidy.py %s readability-redundant-smartptr-get %t

#define NULL __null

namespace std {

template <typename T>
struct unique_ptr {
  T& operator*() const;
  T* operator->() const;
  T* get() const;
};

template <typename T>
struct shared_ptr {
  T& operator*() const;
  T* operator->() const;
  T* get() const;
};

}  // namespace std

struct Bar {
  void Do();
  void ConstDo() const;
};
struct BarPtr {
  Bar* operator->();
  Bar& operator*();
  Bar* get();
};
struct int_ptr {
  int* get();
  int* operator->();
  int& operator*();
};

struct Fail1 {
  Bar* get();
};
struct Fail2 {
  Bar* get();
  int* operator->();
  int& operator*();
};

void Positive() {
  BarPtr u;
  // CHECK-FIXES: BarPtr u;
  BarPtr().get()->Do();
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: Redundant get() call on smart pointer. [readability-redundant-smartptr-get]
  // CHECK-MESSAGES: BarPtr().get()->Do();
  // CHECK-FIXES: BarPtr()->Do();

  u.get()->ConstDo();
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: u.get()->ConstDo();
  // CHECK-FIXES: u->ConstDo();

  Bar& b = *BarPtr().get();
  // CHECK-MESSAGES: :[[@LINE-1]]:13: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: Bar& b = *BarPtr().get();
  // CHECK-FIXES: Bar& b = *BarPtr();

  Bar& b2 = *std::unique_ptr<Bar>().get();
  // CHECK-MESSAGES: :[[@LINE-1]]:14: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: Bar& b2 = *std::unique_ptr<Bar>().get();
  // CHECK-FIXES: Bar& b2 = *std::unique_ptr<Bar>();

  (*BarPtr().get()).ConstDo();
  // CHECK-MESSAGES: :[[@LINE-1]]:5: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: (*BarPtr().get()).ConstDo();
  // CHECK-FIXES: (*BarPtr()).ConstDo();

  (*std::unique_ptr<Bar>().get()).ConstDo();
  // CHECK-MESSAGES: :[[@LINE-1]]:5: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: (*std::unique_ptr<Bar>().get()).ConstDo();
  // CHECK-FIXES: (*std::unique_ptr<Bar>()).ConstDo();

  std::unique_ptr<Bar>* up;
  (*up->get()).Do();
  // CHECK-MESSAGES: :[[@LINE-1]]:5: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: (*up->get()).Do();
  // CHECK-FIXES: (**up).Do();

  int_ptr ip;
  int i = *ip.get();
  // CHECK-MESSAGES: :[[@LINE-1]]:12: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: int i = *ip.get();
  // CHECK-FIXES: int i = *ip;

  std::unique_ptr<int> uu;
  std::shared_ptr<double> *ss;
  bool bb = uu.get() == nullptr;
  // CHECK-MESSAGES: :[[@LINE-1]]:13: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: uu.get() == nullptr;
  // CHECK-FIXES: bool bb = uu == nullptr;

  bb = nullptr != ss->get();
  // CHECK-MESSAGES: :[[@LINE-1]]:19: warning: Redundant get() call on smart pointer.
  // CHECK-MESSAGES: nullptr != ss->get();
  // CHECK-FIXES: bb = nullptr != *ss;
}

void Negative() {
  struct NegPtr {
    int* get();
    int* operator->() {
      return &*this->get();
    }
    int& operator*() {
      return *get();
    }
  };

  std::unique_ptr<Bar>* u;
  u->get()->Do();

  Fail1().get()->Do();
  Fail2().get()->Do();
  const Bar& b = *Fail1().get();
  (*Fail2().get()).Do();

  int_ptr ip;
  bool bb = std::unique_ptr<int>().get() == NULL;
  bb = ip.get() == nullptr;
  bb = u->get() == NULL;
}
