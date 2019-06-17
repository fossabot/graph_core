#include <dgaco_planner/dgaco_planner.h>
#include <moveit/trajectory_processing/iterative_time_parameterization.h>.h>

namespace ha_planner
{

DgacoPlanner::DgacoPlanner ( const std::string& name,
                             const std::string& group,
                             const moveit::core::RobotModelConstPtr& model ) :
  PlanningContext ( name, group )
{
  if (!m_nh.getParam(name+ "/tree_stall_generation",m_max_stall_rrt))
  {
    m_max_stall_rrt=3;
    ROS_WARN_STREAM(name+ "/tree_stall_generation is not defined, set equal to 3");
  }
  if (!m_nh.getParam(name+ "/number_of_nodes",number_of_nodes))
  {
    number_of_nodes=300;
    ROS_WARN_STREAM(name+ "/number_of_nodes is not defined, set equal to 30");
  }
  if (!m_nh.getParam(name+ "/ants_number",n_ants))
  {
    n_ants=80;
    ROS_WARN_STREAM(name+ "/ants_number is not defined, set equal to 80");
  }
  if (!m_nh.getParam(name+ "/ants_stall_generation",max_stall_gen))
  {
    max_stall_gen=50;
    ROS_WARN_STREAM(name+ "/ants_stall_generation is not defined, set equal to 50");
  }
  if (!m_nh.getParam(name+ "/refinement",refinement))
  {
    refinement=true;
    ROS_WARN_STREAM(name+ "/refinement is not defined, set true");
  }
  if (!m_nh.getParam(name+ "/tree_max_time",rrt_time))
  {
    rrt_time=0.8;
    ROS_WARN_STREAM(name+ "/tree_max_time is not defined, set equal to 0.8");
  }
  if (!m_nh.getParam(name+ "/max_time",max_time))
  {
    max_time=1.5;
    ROS_WARN_STREAM(name+ "/max_time is not defined, set equal to 1.5");
  }
  robot_model_=model;

  joint_names_=robot_model_->getJointModelGroup(group)->getActiveJointModelNames();

  m_dof=joint_names_.size();
  std::vector<double> max_velocity(m_dof,0);
  m_scaling.resize(m_dof,0);

  m_lb.resize(6,-M_PI);
  m_ub.resize(6,M_PI);

  for (unsigned int idx=0;idx<m_dof;idx++)
  {
    const robot_model::VariableBounds& bounds = robot_model_->getVariableBounds(joint_names_.at(idx));

    double vmax=100;
    if (bounds.velocity_bounded_)
      vmax = std::min(bounds.max_velocity_,std::abs(bounds.min_velocity_));
    assert(vmax>0);

    max_velocity.at(idx)=vmax;
    m_scaling.at(idx)=1.0/max_velocity.at(idx);
  }

  m_net=std::make_shared<Net>(m_dof,group,planning_scene_,m_scaling,m_lb,m_ub);





}

void DgacoPlanner::setPlanningScene ( const planning_scene::PlanningSceneConstPtr& planning_scene )
{
  planning_scene_=planning_scene;
  m_net->setPlanningScene(planning_scene);
}


bool DgacoPlanner::canServiceRequest(const moveit_msgs::MotionPlanRequest &req) const
{
  if(req.group_name != getGroupName())
  {
    ROS_ERROR("Unsupported planning group '%s' requested", req.group_name.c_str());
    return false;
  }
  
  if (req.goal_constraints[0].joint_constraints.size() == 0)
  {
    ROS_ERROR("Can only handle joint space goals.");
    return false;
  }
  
  return true;
}

void DgacoPlanner::clear()
{

}


bool DgacoPlanner::solve ( planning_interface::MotionPlanDetailedResponse& res )
{
  ros::WallTime start_time = ros::WallTime::now();
  m_net=std::make_shared<Net>(m_dof,group_,planning_scene_,m_scaling,m_lb,m_ub);
  // initializing response
  res.description_.resize(1,"");
  res.processing_time_.resize(1);
  res.error_code_.val = moveit_msgs::MoveItErrorCodes::SUCCESS;
  
  bool success = false;
  
  bool planning_success=false;
  moveit::core::RobotState start_state(robot_model_);
  moveit::core::robotStateMsgToRobotState(request_.start_state,start_state);
  if (request_.start_state.joint_state.position.size()==0)
    start_state=planning_scene_->getCurrentState();
  else
    moveit::core::robotStateMsgToRobotState(request_.start_state,start_state);

  start_state.update();
  if (planning_scene_->isStateColliding(start_state,request_.group_name))
  {
    ROS_ERROR("Start point is in collision");
    res.error_code_.val=moveit_msgs::MoveItErrorCodes::START_STATE_IN_COLLISION;
    return false;
  }

  std::vector<double> start_point;
  start_state.copyJointGroupPositions(group_,start_point);
  std::map<double,moveit_msgs::Constraints> ordered_goals;
  
  std::vector<std::vector<double>> end_points;

  // computing minimum time
  for (unsigned int iGoal=0;iGoal<request_.goal_constraints.size();iGoal++)
  {
    moveit_msgs::Constraints goal=request_.goal_constraints.at(iGoal);
    ROS_DEBUG("Processing goal %u",iGoal++);

    std::vector<double> final_configuration( goal.joint_constraints.size() );

    moveit::core::RobotState end_state(robot_model_);
    for (auto c: goal.joint_constraints)
    {
      end_state.setJointPositions(c.joint_name,&c.position);
    }
    end_state.copyJointGroupPositions(group_,final_configuration);


    ROS_DEBUG("check collision on goal %u",iGoal);

    if (planning_scene_->isStateColliding(end_state,request_.group_name))
    {
      ROS_WARN("goal %u is in collision",iGoal);
      continue;
    }

    end_points.push_back(final_configuration);

  }
  if (end_points.size()==0)
  {
    res.error_code_.val=moveit_msgs::MoveItErrorCodes::GOAL_IN_COLLISION;
    return false;
  }

  ROS_DEBUG("generate start and stop grid");
  m_net->generateNodesFromStartAndEndPoints(start_point,end_points);

  double cost=m_net->getBestCost();

  if (m_net->isSolutionFound())
  {
    ROS_INFO("there is a direct solution");
    ROS_DEBUG("direct path");
    res.error_code_.val=moveit_msgs::MoveItErrorCodes::SUCCESS;
  }
  else
  {
    ROS_DEBUG("run RRT algorithms");
    unsigned int stall_gen=0;
    double last_cost=m_net->getBestCost();

    for (unsigned int idx=0;idx<20000;idx++)
    {

      ros::WallTime act_time = ros::WallTime::now();
      if ((act_time-start_time).toSec()>rrt_time && last_cost<std::numeric_limits<double>::infinity())
        break;

      if (m_stop)
        break;

      m_net->runRRTConnect();

      cost=m_net->getBestCost();
      if (cost<last_cost*0.999)
      {
        last_cost=cost;
        stall_gen=0;
      }
      else
      {
        stall_gen++;
      }
      if (stall_gen>m_max_stall_rrt)
        break;

    }


    if (m_net->getBestCost()>=std::numeric_limits<double>::infinity())
    {
      res.error_code_.val=moveit_msgs::MoveItErrorCodes::PLANNING_FAILED;
      return false;
    }

    m_net->warpPath2(20);
//    m_net->splitPath2(5);

    if (refinement)
    {
      unsigned int removed_node=0;
      unsigned int rem;
      do
      {
        rem=m_net->removeUnconnectedNodes();
        removed_node+=rem;
      }
      while (rem>0);

      m_net->updateNodeHeuristic();
      unsigned int itrial=0;
      stall_gen=0;
      last_cost=m_net->getBestCost();
      for (itrial=0;itrial<2000;itrial++)
      {

        ros::WallTime act_time = ros::WallTime::now();
        if ((act_time-start_time).toSec()>max_time)
          break;

        if (m_net->runAntCycle(n_ants))
        {
          m_net->warpPath2(20);
//          m_net->splitPath2(20);
        }

        m_net->evaporatePheromone();
        m_net->distributePheromone(1);



          m_net->removeLowPheromoneConnections(m_net->getNodeNumber()*1);


          removed_node=0;
          do
          {
            rem=m_net->removeUnconnectedNodes();
            removed_node+=rem;
          }
          while (rem>0);

          unsigned int add_node_number=0;
          if (m_net->getNodeNumber()<number_of_nodes)
            add_node_number=number_of_nodes-m_net->getNodeNumber();

          m_net->generateNodesFromEllipsoid(add_node_number);

          m_net->updateNodeHeuristic();


        if (m_net->getBestCost()<(last_cost*0.9999))
        {
          stall_gen=0;
          last_cost=m_net->getBestCost();
        }
        else
        {
          stall_gen++;
        }

        if (stall_gen>=max_stall_gen)
          break;
      }
    }

  }
  m_net->pruningPath(m_net->getBestPathRef());
  m_net->warpPath2(20);
  std::vector<std::vector<double>> waypoints=m_net->getBestPath();

  robot_trajectory::RobotTrajectoryPtr trj(new robot_trajectory::RobotTrajectory(robot_model_,group_));



  for (const std::vector<double>& waypoint: waypoints )
  {


    moveit::core::RobotState wp_state=start_state;
    wp_state.setJointGroupPositions(group_,waypoint);
    wp_state.update();
    trj->addSuffixWayPoint(wp_state,0);


  }

  trajectory_processing::IterativeParabolicTimeParameterization ipt;
  ipt.computeTimeStamps(*trj);
  ros::WallDuration wd = ros::WallTime::now() - start_time;

  res.processing_time_.push_back(wd.toSec());
  res.description_.emplace_back("plan");
  res.trajectory_.push_back(trj);

  res.error_code_.val=moveit_msgs::MoveItErrorCodes::SUCCESS;
  return true;
}

bool DgacoPlanner::solve ( planning_interface::MotionPlanResponse& res )
{
  ros::WallTime start_time = ros::WallTime::now();
  planning_interface::MotionPlanDetailedResponse detailed_res;
  bool success = solve(detailed_res);
  if(success)
  {
    res.trajectory_ = detailed_res.trajectory_.at(0);
  }
  ros::WallDuration wd = ros::WallTime::now() - start_time;
  res.planning_time_ = wd.toSec();
  res.error_code_ = detailed_res.error_code_;
  
  return success;
}

bool DgacoPlanner::terminate()
{
  m_stop=true;
}


}
