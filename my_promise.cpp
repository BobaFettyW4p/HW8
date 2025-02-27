#include"my_promise.h"
#include<thread>
#include<iostream>
#include<stdexcept>
#include<exception>
using namespace mpcs;
using namespace std;
int main()
{
  MyPromise<int> mpi;
  thread thr{ [&]() { 
	  try {
		  cout << mpi.get_future().get() << endl;
	  } catch(exception &e) {
		  cout << e.what() << endl;
	  }}
  };
#if 1
  mpi.set_value(7);
#else
  try {
	  throw runtime_error("Some runtime error");
  }
  catch (exception &) {
	  mpi.set_exception(current_exception());
  }
#endif
  thr.join();
  return 0;
}