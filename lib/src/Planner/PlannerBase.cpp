/**
 *  MIT License
 *
 *  Copyright (c) 2019 Yuya Kudo
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <Planner/PlannerBase.h>

namespace planner {
    namespace base {
        PlannerBase::PlannerBase(uint32_t dim) :
            terminate_search_cost_(0),
            constraint_(std::make_shared<ConstraintBase>(EuclideanSpace(dim))) {
        }

        PlannerBase::~PlannerBase() {
        }

        void PlannerBase::setProblemDefinition(const std::shared_ptr<ConstraintBase>& constraint) {
            if(constraint->getDim() != constraint_->getDim()) {
                throw std::invalid_argument("[" + std::string(__PRETTY_FUNCTION__) + "] " +
                                            "Problem definitions are invalid");
            }
            constraint_ = constraint;
            sampler_    = std::make_unique<Sampler>(constraint->space);
        }

        void PlannerBase::setTerminateSearchCost(const double& terminate_search_cost) {
            terminate_search_cost_ = terminate_search_cost;
        }

        const std::vector<State>& PlannerBase::getResultRef() const {
            return result_;
        }

        double PlannerBase::getResultCost() const {
            return result_cost_;
        }

        const std::vector<std::shared_ptr<NodeBase>>& PlannerBase::getNodeListRef() const {
            return node_list_;
        }
    }
}
