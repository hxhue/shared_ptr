#include "shared_ptr.hpp"
#include <iostream>
using namespace lockfree;

// 测试辅助对象
struct TestObj {
  static int constructed;
  static int destroyed;

  TestObj() { ++constructed; }
  ~TestObj() {
    --constructed;
    ++destroyed;
  }

  int value = 42;
  void foo() { /* 测试方法 */ }
};
int TestObj::constructed = 0;
int TestObj::destroyed = 0;

// 继承测试用类
struct Base {
  virtual ~Base() = default;
  virtual int get() { return 1; }
};

struct Derived : Base {
  int get() override { return 2; }
};

// 自定义删除器
bool custom_deleter_called = false;
void custom_deleter(TestObj *p) {
  delete p;
  custom_deleter_called = true;
}

// 测试用例
void test_shared_ptr_basic() {
  // 基础构造测试
  {
    shared_ptr<TestObj> p1(new TestObj);
    assert(p1.use_count() == 1);
    assert(TestObj::constructed == 1);

    // 解引用测试
    assert((*p1).value == 42);
    assert(p1->value == 42);
    assert(p1.get() != nullptr);
  }
  assert(TestObj::destroyed == 1);
}

void test_copy_semantics() {
  // 拷贝构造测试
  {
    shared_ptr<TestObj> p1(new TestObj);
    shared_ptr<TestObj> p2(p1);
    assert(p1.use_count() == 2);
    assert(p2.use_count() == 2);
    assert(p1.get() == p2.get());
  }
  assert(TestObj::destroyed == 1);
}

void test_assignment_operator() {
  // 赋值操作测试
  {
    shared_ptr<TestObj> p1(new TestObj);
    shared_ptr<TestObj> p2;
    p2 = p1;
    assert(p1.use_count() == 2);

    // 自赋值测试
    p2 = p2;
    assert(p1.use_count() == 2);
  }
  assert(TestObj::destroyed == 1);
}

void test_reset() {
  // reset 功能测试
  {
    shared_ptr<TestObj> p(new TestObj);
    p.reset();
    assert(p.get() == nullptr);
    assert(p.use_count() == 0);
    assert(TestObj::destroyed == 1);

    p.reset(new TestObj);
    assert(p.use_count() == 1);
  }
  assert(TestObj::destroyed == 2);
}

void test_custom_deleter() {
  // 自定义删除器测试
  custom_deleter_called = false;
  { shared_ptr<TestObj> p(new TestObj, custom_deleter); }
  assert(custom_deleter_called);
}

void test_polymorphism() {
  // 多态类型测试
  {
    shared_ptr<Derived> d(new Derived);
    shared_ptr<Base> b = d;
    assert(b->get() == 2);
    assert(d.use_count() == 2);
  }
}

void test_move_semantics() {
  // 移动语义测试
  {
    shared_ptr<TestObj> p1(new TestObj);
    shared_ptr<TestObj> p2 = std::move(p1);
    assert(p1.get() == nullptr);
    assert(p2.use_count() == 1);
  }
  assert(TestObj::destroyed == 1);
}

void test_edge_cases() {
  // 边界条件测试
  {
    // 空指针构造
    shared_ptr<TestObj> p(nullptr);
    assert(p.use_count() == 0);

    // bool 转换测试
    if (p)
      assert(false);
  }

  // 数组测试（如果支持的话）
  // 注意：标准库 shared_ptr 默认不支持数组，需要自定义删除器
  // TODO:
  // {
  //     shared_ptr<int[]> arr(new int[5], [](int* p) { delete[] p; });
  // }
}

void clean() {
  TestObj::constructed = 0;
  TestObj::destroyed = 0;
}

// 运行所有测试
int main() {

  clean();
  test_shared_ptr_basic();

  clean();
  test_copy_semantics();

  clean();
  test_assignment_operator();

  clean();
  test_reset();

  clean();
  test_custom_deleter();

  clean();
  test_polymorphism();

  clean();
  test_move_semantics();

  clean();
  test_edge_cases();

  std::cout << "All tests passed!" << std::endl;
  return 0;
}