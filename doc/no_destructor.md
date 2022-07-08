# 简介

NoDestructor类解析，使用了定位new运算符，不同与常规的new运算符，定位new运算符不需要相应的delete运算符来释放内存，因为它本身就不开辟新的内存。new对象时会调用构造函数，delete时会调用析构函数,所以这种实现方式不会调用类的析构函数。

单例通过static语义实现(c++11规定了local static在多线程条件下的初始化行为，要求编译器保证了内部静态变量的线程安全性)

使用上：

```cpp
const Comparator* BytewiseComparator() {
    static NoDestructor<BytewiseComparatorImpl> singleton;
    return singleton.get();
}
```

```cpp
// 包装一个从不调用析构函数的实例。适用于函数级静态变量。
template <typename InstanceType>
class NoDestructor {
 public:
  template <typename... ConstructorArgTypes>
  explicit NoDestructor(ConstructorArgTypes&&... constructor_args) {
    static_assert(sizeof(instance_storage_) >= sizeof(InstanceType),
                  "instance_storage_ is not large enough to hold the instance");
    static_assert(
        alignof(decltype(instance_storage_)) >= alignof(InstanceType),
        "instance_storage_ does not meet the instance's alignment requirement");
    new (&instance_storage_)
        InstanceType(std::forward<ConstructorArgTypes>(constructor_args)...);
  }

  ~NoDestructor() = default;

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;

  InstanceType* get() {
    return reinterpret_cast<InstanceType*>(&instance_storage_);
  }

 private:
  typename std::aligned_storage<sizeof(InstanceType),
                                alignof(InstanceType)>::type instance_storage_;
};
```



# 相关知识点

## 1.C++11可变模板参数

可变参数模板和普通模板的语义是一样的，只是写法上稍有区别，声明可变参数模板时需要在typename或class后面带上省略号“...”。比如我们常常这样声明一个可变模版参数：template<typename...>或者template<class...>，一个典型的可变模版参数的定义是这样的：

```cpp
template <typename... T>
void f(T... args);
```

上面的可变模版参数的定义当中，省略号的作用有两个：
1.声明一个参数包T... args，这个参数包中可以包含0到任意个模板参数；
2.在模板定义的右边，可以将参数包展开成一个一个独立的参数。

## 2.static_assert 静态期断言

static_assert用于编译期检查，若比较无法通过，那么编译期报错。静态期语义决定了static_assert只能使用编译器可见的常数

## 3.alignof 返回对齐地址字节数

sizeof ： 获取内存存储的大小。
alignof : 获取地址对其的大小，使用pragma pack可以指定对齐子节数

## 4.std::aligned_storage 对齐存储

通过提供一个数据类型和该类型对应的对齐字节数，获取一个对齐大小的存储，通过type类型成员声明

```cpp
void func() {
    std::aligned_storage<3, 2>::type a;
    cout << sizeof(a) << endl;
}
```

这里原始类型大小为3字节，按照2字节对齐，因此得到一个4字节大小的存储，注意这里a的类型不是原始类型，而是std::aligned_storage<T1, T2>::type类型，使用时需要用原始类型强制转换.

## 5.定位new运算符

定位new在指定位置分配内存，一般用于栈空间分配内存，**定位new的一个特点是无法显式delete，因此通过定位new分配出来的对象没有调用析构函数的机会，这也是NoDesturctor类不会调用析构函数的核心实现**。

跳表的实现中 NewNode函数 也使用了这种方式*placement new*。

## 6.std::forward完美转发

forward的作用是完美转发参数，若函数传入左值，那么转发到其他函数时仍保留左值属性；若函数传入右值，那么转发到其他函数时仍保留右值属性，forward通用范式如下：

```cpp
template <typename T>
void foo(T&& value) {
    bar(std::forward<T>(value));
}
```

编译器通过判断&的组合方式决定变量是左值还是右值