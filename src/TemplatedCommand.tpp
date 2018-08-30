template<class Tuple, std::size_t... I>
std::initializer_list<std::type_index> getTupleTypes(std::index_sequence<I...>) {
	return {typeid(std::tuple_element<I, Tuple>::type)...};
}

template<typename... Args>
TemplatedCommand<Args...>::TemplatedCommand(Server& s, std::string name, std::string desc)
: Command(s, std::move(name), std::move(desc),
	getTupleTypes<Tuple>(std::index_sequence_for<Args...>{})) { }

template<typename... Args>
bool TemplatedCommand<Args...>::checkArguments(const nlohmann::json& j) {
	if (!j.is_array() || j.size() != expectedArgumentTypes.size()) {
		return false;
	}

	try {
		Tuple t = j; // inneficient
	} catch(...) {
		return false;
	}

	return true;
}

template<typename... Args>
template<std::size_t... I>
Result TemplatedCommand<Args...>::reallyExec(Player& p, Tuple t, std::index_sequence<I...>) {
	return exec(p, std::get<I>(t)...);
}

template<typename... Args>
Result TemplatedCommand<Args...>::doExec(Player& p, nlohmann::json j) {
	return reallyExec(p, std::move(j), std::index_sequence_for<Args...>{});
}
