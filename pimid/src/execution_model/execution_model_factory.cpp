#include "execution_model/execution_model.h"
#include "execution_model/zsim_execution_model.h"
#include "execution_model/event_driven_execution_model.h"
#include <iostream>
#include <stdexcept>

namespace pimid {

std::shared_ptr<IExecutionModel> ExecutionModelFactory::createExecutionModel(
    ExecutionModelType type,
    const PIMIDConfig& config,
    SimulationDomain domain) {

    std::cout << "Creating execution model: ";

    switch (type) {
        case ExecutionModelType::ZSIM_EXECUTION_DRIVEN:
            std::cout << "ZSim Execution-Driven" << std::endl;
            return std::make_shared<ZSimExecutionModel>();

        case ExecutionModelType::EVENT_DRIVEN_ANALYTICAL:
            std::cout << "Event-Driven Analytical" << std::endl;
            return std::make_shared<EventDrivenExecutionModel>();

        case ExecutionModelType::HYBRID:
            // Hybrid mode deprecated - host and device should be configured independently
            std::cout << "Hybrid (deprecated, using Analytical)" << std::endl;
            return std::make_shared<EventDrivenExecutionModel>();

        case ExecutionModelType::TRACE_DRIVEN:
            std::cout << "Trace-Driven (not yet implemented)" << std::endl;
            throw std::runtime_error("Trace-driven execution model not yet implemented");

        default:
            throw std::runtime_error("Unknown execution model type");
    }
}

std::shared_ptr<IExecutionModel> ExecutionModelFactory::createFromConfig(
    const std::string& model_name,
    const PIMIDConfig& config,
    SimulationDomain domain) {

    std::cout << "Creating execution model from config: '" << model_name << "'" << std::endl;

    // Parse model name to determine type
    if (model_name == "zsim" || model_name == "execution_driven") {
        return createExecutionModel(ExecutionModelType::ZSIM_EXECUTION_DRIVEN,
                                   config, domain);
    }
    else if (model_name == "event_driven" || model_name == "analytical") {
        return createExecutionModel(ExecutionModelType::EVENT_DRIVEN_ANALYTICAL,
                                   config, domain);
    }
    else if (model_name == "hybrid") {
        return createExecutionModel(ExecutionModelType::HYBRID,
                                   config, domain);
    }
    else if (model_name == "trace" || model_name == "trace_driven") {
        return createExecutionModel(ExecutionModelType::TRACE_DRIVEN,
                                   config, domain);
    }
    else {
        throw std::runtime_error("Unknown execution model name: " + model_name);
    }
}

} // namespace pimid
