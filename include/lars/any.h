#pragma once

#include <lars/visitor.h>

#include <string>
#include <memory>
#include <exception>
#include <functional>
#include <utility>

namespace lars {
  /**
   * Error raised when calling `get` on empty Any object.
   */
  struct UndefinedAnyException: public std::exception{
    const char * what() const noexcept override {
      return "called get() on undefined Any";
    }
  };
  
  template <class T> struct AnyVisitable;
  
  class Any;
  struct AnyReference;
  
  namespace any_detail {
    template<typename T> struct is_shared_ptr : std::false_type { using value_type = void; };
    template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type { using value_type = T; };
    template< class T > struct remove_cvref { typedef std::remove_cv_t<std::remove_reference_t<T>> type; };
    
    template <class T> constexpr static bool NotDerivedFromAny = !std::is_base_of<Any,typename std::decay<T>::type>::value;

    template <class T> struct CapturedSharedPtr: public std::shared_ptr<T>{
      CapturedSharedPtr(const std::shared_ptr<T> &d):std::shared_ptr<T>(d){ }
      operator T & () { return **this; }
      operator const T & () const { return **this; }
    };
  }

  /**
   * A class that can hold an arbitrary value of any type.
   */
  class Any {
  protected:
    std::shared_ptr<VisitableBase> data;

    Any(const Any &) = default;
    Any &operator=(const Any &) = default;

  public:
    
    Any(){}
    template <
      class T,
      typename = typename std::enable_if<any_detail::NotDerivedFromAny<T>>::type
    > Any(T && v){ set<typename any_detail::remove_cvref<T>::type>(std::forward<T>(v)); }
    Any(Any &&) = default;
    Any &operator=(Any &&) = default;
    
    template <
      class T,
      typename = typename std::enable_if<!std::is_base_of<Any,typename std::decay<T>::type>::value>::type
    > Any & operator=(T && o) {
      set<typename any_detail::remove_cvref<T>::type>(std::forward<T>(o));
      return *this;
    }
    
    /**
     * Sets the stored object to an object of type `T`, constructed with the arguments provided.
     * The `VisitableType` templated paramter defines the internal type used for storing and
     * casting the object. The default is `lars::AnyVisitable<T>::type` which can be specialized
     * for usertypes.
     */
    template <
      class T,
      class VisitableType = typename AnyVisitable<T>::type,
      typename ... Args
    > auto & set(Args && ... args) {
      static_assert(!std::is_base_of<Any,T>::value);
      if constexpr (std::is_base_of<VisitableBase,typename any_detail::is_shared_ptr<T>::value_type>::value) {
        auto value = T(args...);
        data = value;
        return *value;
      } else {
        auto value = std::make_shared<VisitableType>(std::forward<Args>(args)...);
        data = value;
        return static_cast<typename VisitableType::Type &>(*value);
      }
    }

    /**
     * Same as `Any::set`, but uses an internal type that can be visitor_casted to the base types.
     */
    template <class T, typename ... Bases, typename ... Args> T & setWithBases(Args && ... args){
      return set<T, DataVisitableWithBases<T,Bases...>>(std::forward<Args>(args)...);
    }
    
    /**
     * Captures the value from another any object
     */
    void setReference(const Any & other){
      data = other.data;
    }
    
    /**
     * Casts the internal data to `T` using `visitor_cast`.
     * A `InvalidVisitor` exception will be raised if the cast is unsuccessful.
     */
    template <class T> T get() const {
      if constexpr (std::is_same<typename std::decay<T>::type, Any>::value) {
        return *this;
      } else if constexpr (any_detail::is_shared_ptr<T>::value) {
        using Value = typename any_detail::is_shared_ptr<T>::value_type;
        if (!data) { throw UndefinedAnyException(); }
        return std::shared_ptr<Value>(data, &get<Value&>());
      } else {
        if (!data) { throw UndefinedAnyException(); }
        return visitor_cast<T>(*data);
      }
    }

    /**
     * Casts the internal data to `T *` using `visitor_cast`.
     * `nullptr` will be returned if the cast is unsuccessful.
     */
    template <class T> T * tryGet() const {
      if(!data) { return nullptr; }
      return visitor_cast<T*>(data.get());
    }
    
    /**
     * Returns a shared pointer containing the result of `Visitor.tryGet<T>()`.
     * Will return an empty `shared_ptr` if unsuccessful.
     */
    template <class T> std::shared_ptr<T> getShared() const {
      if (auto ptr = tryGet<T>()) {
        return std::shared_ptr<T>(data, ptr);
      } else {
        return std::shared_ptr<T>();
      }
    }

    /**
     * `true`, when contains value, `false` otherwise
     */
    operator bool()const{
      return bool(data);
    }
    
    /**
     * resets the value
     */
    void reset(){
      data.reset();
    }
    
    /**
     * the type of the stored value
     */
    TypeIndex type()const{
      if (!data) { return lars::getTypeIndex<void>(); }
      return data->visitableType();
    }

    /**
     * Accept visitor
     */
    void accept(VisitorBase &visitor){
      if (!data) { throw UndefinedAnyException(); }
      data->accept(visitor);
    }

    void accept(VisitorBase &visitor) const {
      if (!data) { throw UndefinedAnyException(); }
      std::as_const(*data).accept(visitor);
    }
    
    bool accept(RecursiveVisitorBase &visitor){
      if (!data) { throw UndefinedAnyException(); }
      return data->accept(visitor);
    }
    
    bool accept(RecursiveVisitorBase &visitor) const {
      if (!data) { throw UndefinedAnyException(); }
      return std::as_const(*data).accept(visitor);
    }

    /**
     * creation methods
     */
    
    template <
      class T,
      class VisitableType = typename AnyVisitable<T>::type,
      typename ... Args
    > static Any create(Args && ... args){
      Any a;
      a.set<T,VisitableType>(std::forward<Args>(args) ...);
      return a;
    }
    
    template <typename ... Bases, typename ... Args> static Any withBases(Args && ... args){
      Any a;
      a.setWithBases<Bases...>(std::forward<Args>(args) ...);
      return a;
    }

  };

  template <class T, typename ... Args> Any makeAny(Args && ... args){
    Any v;
    v.set<T>(std::forward<Args>(args)...);
    return v;
  }
  
  /**
   * An Any object that can implicitly capture another Any by reference.
   */
  struct AnyReference: public Any {
    AnyReference(){ }
    AnyReference(const Any &other):Any(other){ }
    AnyReference(const AnyReference &other):Any(other){ }

    template <
      class T,
      typename = typename std::enable_if<any_detail::NotDerivedFromAny<T>>::type
    > AnyReference(T && v):Any(std::forward<T>(v)){}
    
    AnyReference &operator=(const AnyReference &other){
      Any::operator=(other);
      return *this;
    }
    
    AnyReference &operator=(const Any &other){
      Any::operator=(other);
      return *this;
    }
    
  };
    
  /**
   * Defines the default internal type for Any<T>.
   * Specialize this class to support implicit conversions usertypes.
   */
  template <class T> struct AnyVisitable {
    using type = typename std::conditional<
      std::is_base_of<VisitableBase, T>::value,
      T,
      DataVisitable<T>
    >::type;
  };

}

/**
 * Numeric any conversions.
 */
#define LARS_ANY_DEFINE_SCALAR_TYPE(Type,Conversions) \
template <> struct lars::AnyVisitable<Type>{\
  using Types = typename lars::TypeList<Type &, const Type &>::template Merge<Conversions>; \
  using ConstTypes = typename lars::TypeList<const Type &>::template Merge<Conversions>; \
  using type = lars::DataVisitablePrototype<Type, Types, ConstTypes>; \
}

#ifndef LARS_ANY_NUMERIC_TYPES
#define LARS_ANY_NUMERIC_TYPES ::lars::TypeList<char, int, long, long long, unsigned char, unsigned int, unsigned long, unsigned long long, float, double, long double>
#endif

LARS_ANY_DEFINE_SCALAR_TYPE(char, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(int, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(long, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(long long, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(unsigned char, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(unsigned int, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(unsigned long, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(unsigned long long, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(float, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(double, LARS_ANY_NUMERIC_TYPES);
LARS_ANY_DEFINE_SCALAR_TYPE(long double, LARS_ANY_NUMERIC_TYPES);

/**
 * Char arrays are captured as strings.
 */
template <size_t N> struct lars::AnyVisitable<char[N]> {
  using type = lars::AnyVisitable<std::string>::type;
};

template <size_t N> struct lars::AnyVisitable<const char[N]> {
  using type = lars::AnyVisitable<std::string>::type;
};

/**
 * Capture values as reference through `std::reference_wrapper`.
 */
template <class T> struct lars::AnyVisitable<std::reference_wrapper<T>> {
  using type = lars::DataVisitablePrototype<
    std::reference_wrapper<T>,
    typename AnyVisitable<T>::type::Types,
    typename AnyVisitable<T>::type::ConstTypes,
    typename AnyVisitable<T>::type::Type
  >;
};

template <class T> struct lars::AnyVisitable<std::reference_wrapper<const T>> {
  using type = lars::DataVisitablePrototype<
    std::reference_wrapper<const T>,
    typename AnyVisitable<T>::type::ConstTypes,
    typename AnyVisitable<T>::type::ConstTypes,
    const typename AnyVisitable<T>::type::Type
  >;
};

/**
 * Allow casts of `shared_ptr` to value references.
 * Note: the origin `shared_ptr` cannot be reconstructed from the value.
 * Instead new `shared_ptr`s will be created for every call to `Any::get<std::shared_ptr<T>>()`.
 */
template <class T> struct lars::AnyVisitable<std::shared_ptr<T>> {
  using type = lars::DataVisitablePrototype<
    lars::any_detail::CapturedSharedPtr<T>,
    typename TypeList<std::shared_ptr<T> &>::template Merge<typename AnyVisitable<T>::type::Types>,
    typename TypeList<const std::shared_ptr<T> &, std::shared_ptr<T>>::template Merge<typename AnyVisitable<T>::type::ConstTypes>,
    T
  >;
};
