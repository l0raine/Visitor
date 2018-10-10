#pragma once

#include <lars/visitor.h>
#include <lars/index_tuple.h>
#include <lars/make_function.h>
#include <lars/mutator.h>
#include <lars/type_index.h>
#include <lars/visitable_type_index.h>

#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <type_traits>
#include <iterator>
#include <string>
#include <assert.h>

namespace lars{

  struct AnyScalarBase:public lars::Visitable<AnyScalarBase>{
    virtual TypeIndex type() const = 0;
  };
  
  template<class T> struct AnyScalarData:public DerivedVisitable<AnyScalarData<T>,WithVisitableBaseClass<AnyScalarBase>>::Type{
    T data;
    template <typename ... Args> AnyScalarData(Args && ... args):data(std::forward<Args>(args)...){ }
    TypeIndex type()const override{ return get_type_index<T>(); }
  };
  
  class Any{
  private:
    std::shared_ptr<AnyScalarBase> data;
  public:
    using BadAnyCast = lars::IncompatibleVisitorException;
    
    TypeIndex type()const{ return data->type(); }
    
    template <class T> T &get(){
      struct GetVisitor:public Visitor<AnyScalarData<T>>{
        T * result;
        void visit(AnyScalarData<T> &data){ result = &data.data; }
      } visitor;
      accept_visitor(visitor);
      return *visitor.result;
    }
    
    template <class T> const T &get()const{
      struct GetVisitor:public ConstVisitor<AnyScalarData<T>>{
        const T * result;
        void visit(const AnyScalarData<T> &data){ result = &data.data; }
      } visitor;
      accept_visitor(visitor);
      return *visitor.result;
    }
    
    template <class T> T *try_to_get(){
      struct GetVisitor:public Visitor<AnyScalarBase,AnyScalarData<T>>{
        T * result;
        void visit(AnyScalarBase &){ result = nullptr; }
        void visit(AnyScalarData<T> &data){ result = &data.data; }
      } visitor;
      accept_visitor(visitor);
      return visitor.result;
    }
    
    template <class T> const T * try_to_get()const{
      struct GetVisitor:public ConstVisitor<AnyScalarBase,AnyScalarData<T>>{
        const T * result;
        void visit(const AnyScalarBase &){ result = nullptr; }
        void visit(const AnyScalarData<T> &data){ result = &data.data; }
      } visitor;
      accept_visitor(visitor);
      return visitor.result;
    }

    template <class T = double> T get_numeric()const{
      struct GetVisitor:public ConstVisitor<AnyScalarData<float>,AnyScalarData<double>,AnyScalarData<unsigned>,AnyScalarData<int>>{
        T result;
        void visit(const AnyScalarData<float> &data){ result = data.data; }
        void visit(const AnyScalarData<double> &data){ result = data.data; }
        void visit(const AnyScalarData<int> &data){ result = data.data; }
        void visit(const AnyScalarData<unsigned> &data){ result = data.data; }
      } visitor;
      accept_visitor(visitor);
      return visitor.result;
    }
    
    template <class T,typename ... Args> typename std::enable_if<!std::is_array<T>::value,void>::type set(Args && ... args){ data = std::make_unique<AnyScalarData<T>>(std::forward<Args>(args)...); }
    template <class T,typename ... Args> typename std::enable_if<std::is_array<T>::value,void>::type set(Args && ... args){ data = std::make_unique<AnyScalarData<std::basic_string<typename std::remove_extent<T>::type>>>(std::forward<Args>(args)...); }

    void accept_visitor(VisitorBase &visitor){ assert(data); data->accept(visitor); }
    void accept_visitor(ConstVisitorBase &visitor)const{ assert(data); data->accept(visitor); }
    
    operator bool()const{ return bool(data); }
  };
  
  template <class T,typename ... Args> typename std::enable_if<!std::is_same<Any, T>::value,Any>::type make_any(Args && ... args){
    Any result;
    result.set<T>(std::forward<Args>(args)...);
    return result;
  }
  
  template <class T,typename ... Args> typename std::enable_if<std::is_same<Any, T>::value,Any>::type make_any(Args && ... args){
    return Any(args...);
  }
  
  class AnyArguments:public std::vector<Any>{ using std::vector<Any>::vector; };
  
  class AnyFunctionBase{
  public:
    virtual ~AnyFunctionBase(){}
    
    virtual Any call_with_any_arguments(AnyArguments &args)const = 0;
    virtual int argument_count()const = 0;
    virtual TypeIndex return_type()const = 0;
    virtual TypeIndex argument_type(unsigned)const = 0;
  };
  
  template <class R,typename ... Args> class AnyFunctionData;
  
  template <class R,typename ... Args> class AnyFunctionData:public AnyFunctionBase{
    template <class A,class B> struct SecondType{ using Type = B; };
    
  public:
    std::function<R(Args...)> data;
    
    AnyFunctionData(const std::function<R(Args...)> &f):data(f){}

    template <class U=R> typename std::enable_if<!std::is_void<U>::value,Any>::type call_with_any_arguments(typename SecondType<Args,Any>::Type & ... args)const{
      return make_any<R>(data(args.template get< typename std::remove_const<typename std::remove_reference<Args>::type>::type >() ...));
    }
    
    template <class U=R> typename std::enable_if<std::is_void<U>::value,Any>::type call_with_any_arguments(typename SecondType<Args,Any>::Type & ... args)const{
      data(args.template get< typename std::remove_const<typename std::remove_reference<Args>::type>::type >() ...);
      return Any();
    }
    
    template <size_t ... Indices> Any call_with_arguments_and_indices(AnyArguments &args,StaticIndexTuple<Indices...> indices)const{
      return call_with_any_arguments(args[Indices] ...);
    }
    
    Any call_with_any_arguments(AnyArguments &args)const override{
      if(args.size() != sizeof...(Args)) throw std::runtime_error("invalid argument count for function call");
      return call_with_arguments_and_indices(args,lars::IndexTupleRange<sizeof...(Args)>());
    }
    
    int argument_count()const override{ return sizeof...(Args); }
    
    TypeIndex return_type()const override{ return get_type_index<R>(); }
    
    template <class D = TypeIndex> typename std::enable_if<(sizeof...(Args) > 0), D>::type _argument_type(unsigned i)const{
      constexpr std::array<TypeIndex,sizeof...(Args)> types = {{ get_type_index<typename std::remove_const<typename std::remove_reference<Args>::type>::type>()... }};
      return types[i];
    }

    template <class D = TypeIndex> typename std::enable_if<sizeof...(Args) == 0, D>::type _argument_type(unsigned i)const{
      throw std::runtime_error("invalid argument");
    }

    TypeIndex argument_type(unsigned i)const override{
      return _argument_type<>(i);
    }
    
  };
  
  template <class R> class AnyFunctionData<R,AnyArguments &>:public AnyFunctionBase{
  public:
    std::function<R(AnyArguments &)> data;
    AnyFunctionData(const std::function<R(AnyArguments &)> &f):data(f){}
    template <class T=R> typename std::enable_if<!std::is_void<T>::value,Any>::type _call_with_any_arguments(AnyArguments &args)const{ return make_any<R>(data(args)); }
    template <class T=R> typename std::enable_if<std::is_void<T>::value,Any>::type _call_with_any_arguments(AnyArguments &args)const{ data(args); return Any(); }
    Any call_with_any_arguments(AnyArguments &args)const override{ return _call_with_any_arguments(args); }
    int argument_count()const override{ return -1; }
    TypeIndex return_type()const override{ return get_type_index<R>(); }
    TypeIndex argument_type(unsigned)const override{ return get_type_index<Any>(); }
  };
  
  class AnyFunction{
  private:
    std::shared_ptr<AnyFunctionBase> data;
    template <class R,typename ... Args> void _set(const std::function<R(Args...)> &f){ data = std::make_unique<AnyFunctionData<R,Args...>>(f); }
  public:
    
    AnyFunction(){}
    template <class T> AnyFunction(const T & f){ set(f); }
    template <class F> void set(const F & f){ _set(make_function(f)); }

    // template <class T> AnyFunction(T && f){ set(f); }
    //template <class F> void set(F && f){ _set(make_function(f)); }
    
    Any call(AnyArguments &args)const{ assert(data); return data->call_with_any_arguments(args); }

    template <typename ... Args> Any operator()(Args && ... args)const{
      std::array<Any, sizeof...(Args)> tmp = {{make_any< typename std::remove_const<typename std::remove_reference<Args>::type>::type >(std::forward<Args>(args)) ...}};
      AnyArguments args_vector(std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
      return call(args_vector);
    }
    
    int argument_count()const{ return data->argument_count(); }
    TypeIndex return_type()const{ return data->return_type(); }
    TypeIndex argument_type(unsigned i)const{ return data->argument_type(i); }
    operator bool()const{ return data.operator bool(); }
    
    virtual ~AnyFunction(){}
  };
  
  template <class R,typename ... Args> AnyFunction make_any_function(const std::function<R(Args...)> &f){
    return AnyFunction(f);
  }
  
}
