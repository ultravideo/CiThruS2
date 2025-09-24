#pragma once

namespace TemplateUtility
{
	// For loop where the indices are passed as template parameters, allowing you
	// evaluate the index at compile time and pass it to other templates.
	// 
	// Example usage:
	// 
	// TemplateUtility::For<>(std::integer_sequence<uint8_t, 1, 2, 3>{}, [&]<uint8_t i>()
	// {
	//     exampleArray[i] = std::get<i>(exampleTuple);
    // });
	template <typename T, typename F, T... Is>
	void For(std::integer_sequence<T, Is...>, F func)
	{
		(func.template operator()<Is>(), ...);
	}

	// For loop from 0 to N where the indices are passed as template
	// parameters, allowing you evaluate the index at compile time and pass it
	// to other templates.
	// 
	// Example usage:
	// 
	// TemplateUtility::For<3>([&]<uint8_t i>()
	// {
	//     exampleArray[i] = std::get<i>(exampleTuple);
	// });
	template <auto N, typename F>
	void For(F func)
	{
		For(std::make_integer_sequence<decltype(N), N>(), func);
	}
}

// This is useful for simplifying some template function calls so that you
// don't have to explicitly cast things
template<typename T>
struct ConstWrapper
{
public:
	ConstWrapper() : Value() {}
	ConstWrapper(T value)
		: Value(value) {}

	const T Value;
};
