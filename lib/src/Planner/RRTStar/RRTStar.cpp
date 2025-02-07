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

#include <Planner/RRTStar/RRTStar.h>

namespace planner {
    RRTStar::RRTStar(const uint32_t& dim,
                     const uint32_t& max_sampling_num,
                     const double&   goal_sampling_rate,
                     const double&   expand_dist,
                     const double&   R) :
        base::PlannerBase(dim),
        max_sampling_num_(max_sampling_num),
        expand_dist_(expand_dist),
        R_(R) {
        setGoalSamplingRate(goal_sampling_rate);
    }

    RRTStar::~RRTStar() {
    }

    void RRTStar::setMaxSamplingNum(const uint32_t& max_sampling_num) {
        max_sampling_num_ = max_sampling_num;
    }

    void RRTStar::setGoalSamplingRate(const double& goal_sampling_rate) {
        if(!(0.0 <= goal_sampling_rate && goal_sampling_rate <= 1.0)) {
            throw std::invalid_argument("[" + std::string(__PRETTY_FUNCTION__) + "] " +
                                        "Rate of Sampling goal state is invalid");
        }

        goal_sampling_rate_ = goal_sampling_rate;
    }

    void RRTStar::setExpandDist(const double& expand_dist) {
        expand_dist_ = expand_dist;
    }

    void RRTStar::setR(const double& R) {
        R_ = R;
    }

    bool RRTStar::solve(const State& start, const State& goal) {
        // definition of set of node
        std::vector<std::shared_ptr<Node>> node_list;
        node_list.reserve(max_sampling_num_);
        node_list.push_back(std::make_shared<Node>(start, nullptr, 0));

        // sampling on euclidean space
        for(size_t i = 0; i < max_sampling_num_; i++) {
            auto rand_node = std::make_shared<Node>(goal, nullptr, 0);
            if(goal_sampling_rate_ < sampler_->getUniformUnitRandomVal()) {
                rand_node->state = sampler_->run(Sampler::Mode::WholeArea);

                // resample when node dose not meet constraint
                if(constraint_->checkConstraintType(rand_node->state) == ConstraintType::NOENTRY) {
                    continue;
                }
            }

            // get index of node that nearest node from sampling node
            auto nearest_node_index = getNearestNodeIndex(rand_node, node_list);

            // generate new node
            auto new_node = generateSteerNode(node_list[nearest_node_index], rand_node, expand_dist_);

            // add to list if new node meets constraint
            if(constraint_->checkCollision(node_list[nearest_node_index]->state, new_node->state)) {
                // Find nodes that exist on certain domain
                auto near_node_indexes = findNearNodes(new_node, node_list);

                // Choose parent node from near node
                new_node = chooseParentNode(new_node, node_list, near_node_indexes);

                // add node to list
                node_list.push_back(new_node);

                // redefine parent node of near node
                rewireNearNodes(node_list, near_node_indexes);

                auto cost_to_goal = new_node->state.distanceFrom(goal);
                if(cost_to_goal < expand_dist_ &&
                   new_node->cost + cost_to_goal < terminate_search_cost_) {
                    break;
                }
            }
        }

        result_.clear();

        // store the result
        auto best_last_index = getBestNodeIndex(goal, expand_dist_, node_list);
        if(best_last_index < 0) {
            return false;
        }
        else {
            std::shared_ptr<base::NodeBase> result_node = node_list[best_last_index];

            result_cost_ = node_list[best_last_index]->cost + result_node->state.distanceFrom(goal);
            if(result_node->state != goal) {
                result_.push_back(goal);
            }

            while(true) {
                result_.insert(result_.begin(), result_node->state);
                if(result_node->parent == nullptr) {
                    break;
                }

                result_node = result_node->parent;
            }
        }

        // store the node list
        node_list_.clear();
        std::move(node_list.begin(), node_list.end(), std::back_inserter(node_list_));

        return true;
    }

    size_t RRTStar::getNearestNodeIndex(const std::shared_ptr<Node>& target_node,
                                        const std::vector<std::shared_ptr<Node>>& node_list) const {
        auto min_dist_index = 0;
        auto min_dist       = std::numeric_limits<double>::max();
        for(size_t i = 0; i < node_list.size(); i++) {
            auto dist = node_list[i]->state.distanceFrom(target_node->state);
            if(dist < min_dist) {
                min_dist = dist;
                min_dist_index = i;
            }
        }

        return min_dist_index;
    }

    std::shared_ptr<RRTStar::Node> RRTStar::generateSteerNode(const std::shared_ptr<Node>& src_node,
                                                              const std::shared_ptr<Node>& dst_node,
                                                              const double& expand_dist) const {
        auto steered_node    = std::make_shared<Node>(src_node->state, src_node, src_node->cost);
        auto dist_src_to_dst = src_node->state.distanceFrom(dst_node->state);
        if(dist_src_to_dst < expand_dist) {
            steered_node->cost  += dist_src_to_dst;
            steered_node->state  = dst_node->state;
        }
        else {
            steered_node->cost += expand_dist;
            steered_node->state = src_node->state + ((dst_node->state - src_node->state) / dist_src_to_dst) * expand_dist;
        }
        return steered_node;
    }

    std::vector<size_t> RRTStar::findNearNodes(const std::shared_ptr<Node>&              target_node,
                                               const std::vector<std::shared_ptr<Node>>& node_list) const {
        std::vector<size_t> near_node_indexes;

        auto num_node = node_list.size();
        if(num_node != 0) {
            auto radius = R_ * std::pow((std::log(num_node) / num_node), 1.0 / constraint_->space.getDim());
            for(size_t i = 0; i < num_node; i++) {
                auto dist = node_list[i]->state.distanceFrom(target_node->state);
                if(dist < radius) {
                    near_node_indexes.push_back(i);
                }
            }
        }

        return near_node_indexes;
    }

    std::shared_ptr<RRTStar::Node> RRTStar::chooseParentNode(const std::shared_ptr<Node>&              target_node,
                                                             const std::vector<std::shared_ptr<Node>>& node_list,
                                                             const std::vector<size_t>&                near_node_indexes) const {
        auto min_cost_parent_node = target_node->parent;
        auto min_cost             = std::numeric_limits<double>::max();
        for(const auto& near_node_index : near_node_indexes) {
            auto dist = target_node->state.distanceFrom(node_list[near_node_index]->state);
            auto cost = node_list[near_node_index]->cost + dist;
            if(cost < min_cost) {
                if(constraint_->checkCollision(target_node->state, node_list[near_node_index]->state)) {
                    min_cost_parent_node = node_list[near_node_index];
                    min_cost             = cost;
                }
            }
        }

        if(min_cost != std::numeric_limits<double>::max()) {
            target_node->parent = min_cost_parent_node;
            target_node->cost   = min_cost;
        }

        return target_node;
    }

    void RRTStar::rewireNearNodes(std::vector<std::shared_ptr<Node>>& node_list,
                                  const std::vector<size_t>&          near_node_indexes) const {
        auto new_node = node_list.back();
        for(const auto& near_node_index : near_node_indexes) {
            auto near_node = node_list[near_node_index];
            auto new_cost  = new_node->cost + near_node->state.distanceFrom(new_node->state);
            if(new_cost < near_node->cost) {
                if(constraint_->checkCollision(new_node->state, near_node->state)) {
                    near_node->parent = new_node;
                    near_node->cost   = new_cost;
                }
            }
        }
    }

    int RRTStar::getBestNodeIndex(const State&                              target_state,
                                  const double&                             radius,
                                  const std::vector<std::shared_ptr<Node>>& node_list) const {
        auto best_index = -1;
        auto min_cost   = std::numeric_limits<double>::max();
        for(size_t i = 0; i < node_list.size(); i++) {
            auto dist_from_target = target_state.distanceFrom(node_list[i]->state);
            if(dist_from_target < radius) {
                if(node_list[i]->cost + dist_from_target < min_cost) {
                    best_index = i;
                    min_cost   = node_list[i]->cost + dist_from_target;
                }
            }
        }

        return best_index;
    }
}
