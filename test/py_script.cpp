#include <iostream>
#include <string>
#include <array>
#include <tuple>

#include <Python.h>

#ifndef FORCE_INLINE
	#define FORCE_INLINE __forceinline
#endif

namespace mao {
	template <int ...i>
	struct int_sequence
	{
	};

	template <char ...c>
	struct char_sequence
	{
		static constexpr char value[] = { c..., 0 };
	};

	template < typename T1, typename T2>
	struct concat;

	template <char ...c1, char ...c2>
	struct concat<char_sequence<c1...>, char_sequence<c2...>>
	{
		using type = char_sequence<c1..., c2...>;
	};

	template <typename ...Types1, typename ...Types2>
	struct concat<std::tuple<Types1...>, std::tuple<Types2...>>
	{
		using type = std::tuple<Types1..., Types2...>;
	};

	template <int ...i1, int ...i2>
	struct concat<int_sequence<i1...>, int_sequence<i2...>>
	{
		using type = int_sequence<i1..., i2...>;
	};

	template <int N>
	struct make_int_sequence
	{
		using type = typename concat<typename make_int_sequence<N - 1>::type, int_sequence<N-1>>::type;
	};

	template <>
	struct make_int_sequence<0>
	{
		using type = int_sequence<>;
	};

	template <typename ...Types>
	struct concat_deduce;

	template <>
	struct concat_deduce<>
	{
		using type = char_sequence<'\0'>;
	};

	template <typename Head, typename ...Tails>
	struct concat_deduce<Head, Tails...>
	{
		using type = typename concat<Head, typename concat_deduce<Tails...>::type >::type;
	};
	
	template <typename T>
	struct concat_result;

	template <char ...c>
	struct concat_result<char_sequence<c...>>
	{
		static constexpr const char value[] = { c..., '\0' };
	};

	template <typename ...Types>
	struct format_str
	{
		static constexpr auto value = concat_result<typename concat_deduce<typename type_infos<typename std::remove_cv<Types>::type>::format...>::type >::value;
	};

	class auto_releaser
	{
	public:
		FORCE_INLINE explicit auto_releaser(PyObject *obj) : obj_(obj)
		{
		}

		FORCE_INLINE ~auto_releaser()
		{
			Py_XDECREF(obj_);
		}

		auto_releaser operator=(const auto_releaser &) = delete;

		FORCE_INLINE operator PyObject *()
		{
			return obj_;
		}

	private:
		PyObject *obj_;
	};

	template <typename T>
	struct type_infos
	{
	};

	template <>
	struct type_infos<void>
	{
		using name = char_sequence<>;
		using format = char_sequence<>;
		using interval_type = void;

		FORCE_INLINE static void extract(PyObject *obj)
		{
		}

		FORCE_INLINE static bool check(PyObject *obj)
		{
			return true;
		}
	};

	template <>
	struct type_infos<int>
	{
		using name = char_sequence<'i', 'n', 't'>;
		using format = char_sequence<'i'>;
		using internal_type = int;

		FORCE_INLINE static int convert(int value)
		{
			return value;
		}

		FORCE_INLINE static int extract(PyObject *obj)
		{
			return int(PyLong_AsLong(obj));
		}

		FORCE_INLINE static bool check(PyObject *obj)
		{
			return PyLong_Check(obj) != 0;
		}
	};

	template <>
	struct type_infos<unsigned int>
	{
		using name = char_sequence<'u', 'i', 'n', 't'>;
		using format = char_sequence<'I'>;
		using internal_type = unsigned int;

		FORCE_INLINE static unsigned int convert(unsigned int value)
		{
			return value;
		}

		FORCE_INLINE static unsigned int extract(PyObject *obj)
		{
			return (unsigned int)PyLong_AsUnsignedLong(obj);
		}

		FORCE_INLINE static bool check(PyObject *obj)
		{
			return PyLong_Check(obj) != 0;
		}
	};

	template <>
	struct type_infos<float>
	{
		using name = char_sequence<'f', 'l', 'o', 'a', 't'>;
		using format = char_sequence<'f'>;
		using internal_type = float;

		FORCE_INLINE static float convert(float value)
		{
			return value;
		}

		FORCE_INLINE static float extract(PyObject *obj)
		{
			return (float)PyFloat_AsDouble(obj);
		}

		FORCE_INLINE static bool check(PyObject *obj)
		{
			return PyFloat_Check(obj) != 0;
		}
	};

	template <>
	struct type_infos<double>
	{
		using name = char_sequence<'d', 'o', 'u', 'b', 'l', 'e'>;
		using format = char_sequence<'d'>;
		using interval_type = double;

		// convert to use in Py_BuildValue
		FORCE_INLINE static double convert(double value)
		{
			return value;
		}

		// extrac from users
		FORCE_INLINE static double extract(PyObject *obj)
		{
			return PyFloat_AsDouble(obj);
		}

		FORCE_INLINE static bool check(PyObject *obj)
		{
			return PyFloat_Check(obj) != 0;
		}
	};

	template <>
	struct type_infos<const char *>
	{
		using name = char_sequence<'c', 'h', 'a', 'r', '*'>;
		using format = char_sequence<'s'>;
		using interval_type = char *;
		FORCE_INLINE static const char * convert(const char *value)
		{
			return value;
		}

		FORCE_INLINE static std::string extract(PyObject *obj)
		{
			return (const char *)PyUnicode_DATA(obj);
		}

		FORCE_INLINE static bool check(PyObject *obj)
		{
			return PyUnicode_Check(obj) != 0;
		}
	};

	template <>
	struct type_infos<std::string>
	{
		using name = char_sequence<'s', 't', 'r'>;
		using format = char_sequence<'O'>;
		using interval_type = std::string;

		FORCE_INLINE static auto_releaser convert(const std::string &value)
		{
			PyObject *obj = PyUnicode_FromString(value.c_str());

			return auto_releaser(obj);
		}

		FORCE_INLINE static std::string extract(PyObject *obj)
		{
			return (const char *)PyUnicode_DATA(obj);
		}

		FORCE_INLINE static bool check(PyObject *obj)
		{
			return PyUnicode_Check(obj) != 0;
		}
	};

}

namespace mao
{
	class module;

	class invoker
	{
	public:
		invoker(const char *module_name, const char *fun_name)
		{
			auto m = PyImport_ImportModule(module_name);
			func_ = PyObject_GetAttrString(m, fun_name);
			Py_DECREF(m);
		}

		invoker(PyObject *m, const char *name)
		{
			func_ = PyObject_GetAttrString(m, name);
		}

		invoker(invoker &i)
		{
			func_ = i.func_;
			Py_XINCREF(func_);
		}

		invoker(invoker &&i)
		{
			func_ = i.func_;
			i.func_ = nullptr;
		}

		~invoker()
		{
			Py_XDECREF(func_);
		}

		bool is_valid()
		{
			return func_ != nullptr;
		}

		invoker &operator=(invoker &i)
		{
			func_ = i.func_;
			Py_XINCREF(func_);
		}

		invoker &&operator=(invoker &&i)
		{
			func_ = i.func_;
			i.func_ = nullptr;
		}

		template <typename T>
		FORCE_INLINE auto convert(T &value)
		{
			return type_infos< std::remove_cv_t<T> >::convert(value);
		}

		template <typename T>
		FORCE_INLINE T & auto_convert(T &&value)
		{
			return value;
		}

		FORCE_INLINE PyObject *auto_convert(auto_releaser &&obj)
		{
			return (PyObject *)obj;
		}

		template <typename R, typename ...Args>
		FORCE_INLINE auto call(Args...args)
		{
			PyObject *ret = PyObject_CallFunction(func_, format_str<Args...>::value, auto_convert(convert(args))...);

			auto_releaser releaser(ret);
			return type_infos<R>::extract(ret);
		}

		template <typename R, typename ...Args>
		FORCE_INLINE auto pcall(Args ...args)
		{
			using ReturnType = decltype(type_infos<R>::extract(nullptr));
			PyObject *ret = PyObject_CallFunction(func_, format_str<Args...>::value, auto_convert(convert(args))...);
			if (!ret) { // error occur
				PyErr_Print();
				PyErr_Clear();
				return ReturnType();
			}

			auto_releaser releaser(ret);

			if (!type_infos<R>::check(ret)) { // error occur
				std::cout << "[error] return type \"" << type_infos<R>::name::value << "\" needed!" << std::endl;
				return ReturnType();
			}

			return type_infos<R>::extract(ret);
		}

	private:
		PyObject *func_;
	};

	class module
	{
	public:
		module(const char *name)
		{
			module_ = PyImport_ImportModule(name);
		}

		~module()
		{
			Py_XDECREF(module_);
		}

		module operator=(const module &) = delete;

		invoker get(const char *name)
		{
			return invoker(module_, name);
		}

		FORCE_INLINE bool is_valid()
		{
			return module_ != nullptr;
		}

	private:
		PyObject *module_;
		friend class invoker;
	};

	// common methods
	template <typename Id, int N, typename Fun>
	struct static_method_provider
	{
		static_method_provider(const char *name, Fun fun, const char *doc = nullptr)
			: name_(name), doc_(doc)
		{
			fun_ = fun;
		}

		static PyObject *fun(PyObject *self, PyObject *args)
		{
//			fun_(N);
			return invoke(fun_, self, args);
		}

		PyMethodDef method_def()
		{
			return PyMethodDef{ name_, fun, METH_VARARGS, doc_ };
		}

		template <typename R, typename ...Types>
		FORCE_INLINE static PyObject *invoke(R(*fun)(Types ...), PyObject *self, PyObject *args)
		{
			std::array<PyObject *, sizeof...(Types)> objs = {nullptr};

			auto obj = std::get<0>(objs);

			using sequence = typename make_int_sequence<sizeof...(Types)>::type;

			return do_invoke(sequence(), fun, args, objs);
		}

		template <int ...I, int Size, typename R, typename ...Types>
		FORCE_INLINE static PyObject *do_invoke(const int_sequence<I...> &, R(*fun)(Types ...), \
				PyObject *args, std::array<PyObject *, Size> &objs)
		{
			int size = Size;
			if (!PyArg_UnpackTuple(args, "ref", Size, Size, &std::get<I>(objs)...)) {
				PyErr_SetString(PyExc_RuntimeError, "call cpp method error!!");
				return nullptr;
			}

			auto result = fun(type_infos<Types>::extract(std::get<I>(objs))...);
			return Py_BuildValue(type_infos<std::remove_cv<R>::type>::format::value, type_infos<std::remove_cv<R>::type>::convert(result));
//			return nullptr;
		}


		const char *name_;
		const char *doc_;

		static Fun fun_;
	};

	template <typename Id, int N, typename Fun>
	Fun static_method_provider<Id, N, Fun>::fun_;

	template <typename T>
	struct def_imp
	{
		using fun_type = T;

		def_imp(const char *name, T fun, const char *doc = nullptr) :
			name_(name), fun_(fun), doc_(doc)
		{

		}

		const char *name_;
		T fun_;
		const char *doc_;
	};

	template <typename T>
	auto def(const char *name, T fun, const char *doc = nullptr)
	{
		return def_imp<T>(name, fun, doc);
	}

	template <int N, typename T1, typename ...T2>
	class module_def_imp;

	template <int N, typename ...FunTypes, typename ...Types>
	class module_def_imp<N, std::tuple<FunTypes...>, Types...>
	{
	public:
		module_def_imp(const char *name, Types ...args) 
			: name_(name)
		{
			static PyMethodDef methods[N + 1] = { FunTypes(args.name_, args.fun_, args.doc_).method_def()...,{ nullptr } };

			methods_ = methods;
		}

		~module_def_imp()
		{
		}

		void init()
		{
			auto module = PyImport_AddModule(name_.c_str());
			PyModule_AddFunctions(module, methods_);
		}

	private:
		std::string name_;

		PyMethodDef *methods_;
	};

	template <typename Id, int N, typename ...Funcs>
	struct make_static_method;

	template <typename Id, int N, typename F>
	struct make_static_method<Id, N, F>
	{
		using type = std::tuple<static_method_provider<Id, N, typename F::fun_type>>;
	};

	template <typename Id, int N, typename F, typename ...Funcs>
	struct make_static_method<Id, N, F, Funcs...>
	{
		using type = typename concat< std::tuple<static_method_provider<Id, N, typename F::fun_type> >, typename make_static_method<Id, N - 1, Funcs... >::type >::type;
	};

	template <typename T, typename ...Types>
	auto module_def(const char *name, Types ...args)
	{
		return module_def_imp<sizeof...(Types), typename make_static_method<T, sizeof...(Types), Types...>::type, Types...>(name, args...);
	}
}

const std::string cpp_fun(int iv)
{

	std::cout << "cpp fun" << iv << std::endl;

	return "cpp_fun return string!";
}

int cpp_fun2(int iv)
{

	std::cout << "cpp fun" << iv << std::endl;

	return 2;
}

class py_m {};
auto m = mao::module_def<py_m>("m", mao::def("cpp_fun", cpp_fun), mao::def("cpp_fun2", cpp_fun2));

int main() 
{
	Py_Initialize();

	m.init();

	static mao::module m("main");

	static auto init = m.get("init");

	std::string value = init.pcall<std::string>(1, 2, std::string("this is a string!xxx"));
}
