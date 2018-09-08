#include <ConnectionProcessor.hpp>

template<typename ProcessorType, typename... Args>
ProcessorType& ConnectionManager::addToBeg(Args&&... args) {
	std::unique_ptr<ConnectionProcessor> proc(std::make_unique<ProcessorType>(std::forward<Args>(args)...));

	ProcessorType& procRef = *static_cast<ProcessorType*>(proc.get());
	processorTypeMap.emplace(typeid(ProcessorType), std::ref(*proc.get()));

	processors.emplace_front(std::move(proc));

	return procRef;
}

template<typename ProcessorType>
ProcessorType& ConnectionManager::getProcessor() {
	return processorTypeMap.at(typeid(ProcessorType));
}
