#include <iostream>
#include <memory>

#include "shared_ptr.hpp"
using namespace lockfree;

struct MyObj {
  MyObj() { std::cout << "MyObj constructed\n"; }

  ~MyObj() { std::cout << "MyObj destructed\n"; }
};

struct Container
    : std::enable_shared_from_this<Container> // note: public inheritance
{
  shared_ptr<MyObj> memberObj;

  void CreateMember() { memberObj = make_shared<MyObj>(); }

  shared_ptr<MyObj> GetAsMyObj() {
    // Use an alias shared ptr for member
    return shared_ptr<MyObj>(shared_from_this(), memberObj.get());
  }
};

#define COUT(str) std::cout << '\n' << str << '\n'

#define DEMO(...) std::cout << #__VA_ARGS__ << " = " << __VA_ARGS__ << '\n'

int main() {
  COUT("Creating shared container");
  shared_ptr<Container> cont = std::make_shared<Container>();
  DEMO(cont.use_count());
  DEMO(cont->memberObj.use_count());

  COUT("Creating member");
  cont->CreateMember();
  DEMO(cont.use_count());
  DEMO(cont->memberObj.use_count());

  COUT("Creating another shared container");
  shared_ptr<Container> cont2 = cont;
  DEMO(cont.use_count());
  DEMO(cont->memberObj.use_count());
  DEMO(cont2.use_count());
  DEMO(cont2->memberObj.use_count());

  COUT("GetAsMyObj");
  shared_ptr<MyObj> myobj1 = cont->GetAsMyObj();
  DEMO(myobj1.use_count());
  DEMO(cont.use_count());
  DEMO(cont->memberObj.use_count());
  DEMO(cont2.use_count());
  DEMO(cont2->memberObj.use_count());

  COUT("Copying alias obj");
  shared_ptr<MyObj> myobj2 = myobj1;
  DEMO(myobj1.use_count());
  DEMO(myobj2.use_count());
  DEMO(cont.use_count());
  DEMO(cont->memberObj.use_count());
  DEMO(cont2.use_count());
  DEMO(cont2->memberObj.use_count());

  COUT("Resetting cont2");
  cont2.reset();
  DEMO(myobj1.use_count());
  DEMO(myobj2.use_count());
  DEMO(cont.use_count());
  DEMO(cont->memberObj.use_count());

  COUT("Resetting myobj2");
  myobj2.reset();
  DEMO(myobj1.use_count());
  DEMO(cont.use_count());
  DEMO(cont->memberObj.use_count());

  COUT("Resetting cont");
  cont.reset();
  DEMO(myobj1.use_count());
  DEMO(cont.use_count());
}