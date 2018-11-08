#include <string>
#include <type_traits>

// ugly, but c++ doesn't have overload by return type

template<typename T>
typename std::enable_if<!std::is_same<T, bool>::value && std::is_integral<T>::value, T>::type
fromString(const std::string&);

template<typename T>
typename std::enable_if<std::is_same<T, bool>::value, T>::type
fromString(const std::string&);

template<typename T>
typename std::enable_if<std::is_same<T, std::string>::value, T>::type
fromString(const std::string&);

template<typename T>
typename std::enable_if<std::is_same<T, float>::value, T>::type
fromString(const std::string&);

template<typename T>
typename std::enable_if<std::is_same<T, double>::value, T>::type
fromString(const std::string&);
