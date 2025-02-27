#ifndef MY_PROMISE_H
#  define MY_PROMISE_H
#include<memory>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<exception>
#include<stdexcept>
//utilizing the preprocessor to gain access to std::variant, std::optional, and overload (contained in the <utility> header)
#include <variant>
#include <optional>
#include <utility>
using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;
using std::make_unique;
using std::move;
using std::mutex;
using std::condition_variable;
using std::lock_guard;
using std::unique_lock;
using std::exception_ptr;
using std::rethrow_exception;
using std::runtime_error;

/**
 * Below, we will be using the overloaded function to handle the two possible variant types: a unique_ptr<T> or an exception_ptr
 * As such, we need to define a helper class template to handle the overloaded function before we can use it
 * The first class is a variadic template class that can accept any number of types; it inherits from all types provided
 * This is important because when we pass lambdas to it, each will be a unique type, and we need to be able to handle all of them
 * The second class helps the compiler deduce the correct type for the overloaded function
 */
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace mpcs {
template<class T> class MyPromise;

/**This class, while necessary in the initial implementation, is not needed in our implementation
 * The variant in the SharedState struct tracks the state of the future object instead
 * The rest of the codebase has been updated to utilize the variant to track the state instead
 * I have left the State enum in the codebase (commented out) for clarity and to explain how use of the variant simplified our code
 */
// enum class State { empty, val, exc };

template<class T>
struct SharedState {
  /** the value and exception fields have been combined into an optional FutureValue field which encapsulates a variant
  *if the future does not have a value, the FutureValue variant will be empty
  *we have updated the code in the rest of the header file to handle the variant as a value if it is of the type unique_ptr<T>
  *and as an exception if it is of the type exception_ptr
  */
  std::optional<std::variant<unique_ptr<T>, exception_ptr>> FutureValue;
  mutex mtx;
  condition_variable cv;
};

template<typename T>
class MyFuture {
public:
  MyFuture(MyFuture const &) = delete; // Injected class name
  MyFuture(MyFuture &&) = default;
    T get() {
    unique_lock lck{sharedState->mtx};
    sharedState->cv.wait(lck, 
        [&] { return sharedState->FutureValue.has_value(); });
    /**After we've confirmed the FutureValue is not empty (which we did in the above line), we can visit the variant to get the value or exception
     * We overload the visit function to handle the two possible variant types: a unique_ptr<T> or an exception_ptr
     * If it's a unique_ptr<T>, we derefernce it to get the value and return it
     * If it's an exception_ptr, we rethrow the exception it points to
     * We pass the FutureValue field to the overloaded function by dereferencing it
     */

    return std::visit(overloaded{
        [](unique_ptr<T>& ptr) -> T { return move(*ptr); },
        [](exception_ptr& exc) -> T { rethrow_exception(exc); }
    }, *sharedState->FutureValue);
  }
private:
  friend class MyPromise<T>;
  MyFuture(shared_ptr<SharedState<T>> &sharedState) 
	  : sharedState(sharedState) {}
  shared_ptr<SharedState<T>> sharedState;
};

template<typename T>
class MyPromise
{
public:
  MyPromise() : sharedState{make_shared<SharedState<T>>()} {}

  void set_value(T const &value) {
    lock_guard lck(sharedState->mtx);
    //since FutureValue is a variant, we can directly assign it a unique_ptr<T> and the variant will automatically handle the type
    sharedState->FutureValue = std::variant<unique_ptr<T>, exception_ptr>(make_unique<T>(value));
    sharedState->cv.notify_one();
  }

  void set_exception(exception_ptr exc) {
    lock_guard lck(sharedState->mtx);
    //similar to set_value(), we can directly assign the FutureValue field an exception_ptr and the variant will automatically handle the type
    sharedState->FutureValue = std::variant<unique_ptr<T>, exception_ptr>(exc);
    sharedState->cv.notify_one();
  }

  MyFuture<T> get_future() {
    return sharedState;
  }
private:
  shared_ptr<SharedState<T>> sharedState; 
};
}
#endif
