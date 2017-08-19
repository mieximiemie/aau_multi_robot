#include <battery_simulate.h>

#define INFINITE_ENERGY false

using namespace std;

//TODO(minor) comments, debugs, and so on...

battery_simulate::battery_simulate() //TODO the constructor should require as argument an instance of TimeManagerInterface
{
    // read parameters
    nh.getParam("energy_mgmt/speed_avg", speed_avg_init);
//    ROS_INFO("speed_avg_init: %.2f", speed_avg_init);    
    nh.getParam("energy_mgmt/power_charging", power_charging); // W (i.e, watt)
    nh.getParam("energy_mgmt/power_per_speed", power_per_speed);     // W/(m/s)
    nh.getParam("energy_mgmt/power_moving_fixed_cost", power_moving_fixed_cost);     // W/(m/s)
    nh.getParam("energy_mgmt/power_sonar", power_sonar); // W
    nh.getParam("energy_mgmt/power_laser", power_laser); // W
    nh.getParam("energy_mgmt/power_microcontroller", power_microcontroller); // W
    nh.getParam("energy_mgmt/power_basic_computations", power_basic_computations);  // W
    nh.getParam("energy_mgmt/power_advanced_computations", power_advanced_computations);  // W
    nh.getParam("energy_mgmt/max_linear_speed", max_speed_linear); // m/s
    nh.getParam("energy_mgmt/maximum_traveling_distance", maximum_traveling_distance); // m/s
    advanced_computations_bool = true;
    
//    ROS_ERROR("%.1f", power_moving);    
//    ROS_ERROR("%f, %f, %f, %f, %f", power_charging, power_moving, power_standing, charge_max, speed_avg_init);

    // initialize private variables
    speed_angular = 0;
    time_moving = 0;   //TODO(minor) useless?
    time_standing = 0; //TODO(minor) useless?
    perc_moving = 0.5; //TODO(minor) useless?
    perc_standing = 0.5; //TODO(minor) useless?
    output_shown = false; //TODO(minor) useless?
    speed_avg = speed_avg_init;
    idle_mode = false; 
//    do_not_consume_battery = true; //important, since at the beginning we have a lot of unsused time...
    do_not_consume_battery = false;
    initializing = true;
    counter_moving_to_frontier = 0;
    consumed_energy_A = 0;
    consumed_energy_B = 0;
    speed_linear = 0;
    last_x = 0, last_y = 0;
    traveled_distance = 0;
    last_traveled_distance = 0;
    total_traveled_distance = 0;
    
    //ROS_ERROR("maximum_running_time: %f", maximum_running_time);
    
//    if(INFINITE_ENERGY) {
//        total_energy_A = 10000;
//        remaining_energy_A = 10000;
//        power_standing = 0.0;  
//        power_moving   = 0.0;  
//        power_charging = 100.0;
//        maximum_running_time = 100000000000000000;
//    }

    // initialize battery state
    state.charging = false;
    state.soc = 1; // (adimensional) // TODO(minor) if we assume that the robot starts fully_charged
    state.remaining_time_charge = 0; // since the robot is assumed to be fully charged when the exploration starts
    state.remaining_distance = maximum_traveling_distance;
    state.remaining_time_run = maximum_traveling_distance * speed_avg_init; //s //TODO(minor) "maximum" is misleading: use "estimated"...
    state.maximum_traveling_distance = maximum_traveling_distance;
    state.fully_charged = true;

    // advertise topics
    pub_battery = nh.advertise<explorer::battery_state>("battery_state", 1);
    //pub_charging_completed = nh.advertise<std_msgs::Empty>("charging_completed", 1);
    
    pub_full_battery_info = nh.advertise<explorer::battery_state>("full_battery_info", 1, true);

    // subscribe to topics
    sub_speed = nh.subscribe("avg_speed", 1, &battery_simulate::cb_speed, this);
    sub_cmd_vel = nh.subscribe("cmd_vel", 1, &battery_simulate::cb_cmd_vel, this);
    sub_robot = nh.subscribe("explorer/robot", 100, &battery_simulate::cb_robot, this);    
//    sub_time = nh.subscribe("totalTime", 1, &battery_simulate::totalTime, this); // TODO(minor) do we need this?

    //ROS_ERROR("remaining distance: %f", state.remaining_distance);
    pub_full_battery_info.publish(state);
    
    ros::NodeHandle nh_tilde("~");
    nh_tilde.param<std::string>("log_path", log_path, "");
    nh_tilde.param<string>("robot_prefix", robot_prefix, "");
    /* Initialize robot name */
    if (robot_prefix.empty())
    {
        /* Empty prefix: we are on an hardware platform (i.e., real experiment) */
        ROS_INFO("Real robot");

        /* Set robot name and hostname */
        char hostname[1024];
        hostname[1023] = '\0';
        gethostname(hostname, 1023);
        robot_name = string(hostname);

        /* Set robot ID based on the robot name */
        std::string bob = "bob";
        std::string marley = "marley";
        std::string turtlebot = "turtlebot";
        std::string joy = "joy";
        std::string hans = "hans";
        if (robot_name.compare(turtlebot) == 0)
            robot_id = 0;
        if (robot_name.compare(joy) == 0)
            robot_id = 1;
        if (robot_name.compare(marley) == 0)
            robot_id = 2;
        if (robot_name.compare(bob) == 0)
            robot_id = 3;
        if (robot_name.compare(hans) == 0)
            robot_id = 4;
    }
    else
    {
        /* Prefix is set: we are in a simulation */
        ROS_INFO("Simulation");
        robot_name = robot_prefix;
    }
    
    pose_sub = nh.subscribe("amcl_pose", 1000, &battery_simulate::poseCallback, this);
    
    set_robot_state_sc = nh.serviceClient<robot_state::SetRobotState>("robot_state/set_robot_state");
    get_robot_state_sc = nh.serviceClient<robot_state::GetRobotState>("robot_state/get_robot_state");
    try_to_lock_robot_state_sc = nh.serviceClient<robot_state::TryToLockRobotState>("robot_state/try_to_lock_robot_state");
    unlock_robot_state_sc = nh.serviceClient<robot_state::UnlockRobotState>("robot_state/unlock_robot_state");
    
    robot_state = initializing;
        
}

void battery_simulate::initializeSimulationTime() {
    if(time_manager == NULL)
        ROS_ERROR("Instance of TimeManager not set!");
    else
        time_last = time_manager->simulationTimeNow();
        
}

void battery_simulate::cb_robot(const adhoc_communication::EmRobot::ConstPtr &msg)
{
    ROS_ERROR("this should not be called anymore!!");
}

void battery_simulate::get_state() {
    robot_state::GetRobotState get_msg;
    while(!get_robot_state_sc.call(get_msg))
        ROS_ERROR("call to get_robot_state failed!");
    if(get_msg.response.robot_state != robot_state)
        handle_new_state(get_msg.response.robot_state);
}

void battery_simulate::handle_new_state(int new_state) {
    if(initializing && new_state == robot_state::MOVING_TO_FRONTIER) {
        if(counter_moving_to_frontier == 0)
            counter_moving_to_frontier++;
        else {
            initializing = false;
            ROS_INFO("Finished initialization procedure");
        }
        return;

        //TODO use this code
        //if(initializing) {
        //    initializing = false;
         //   ROS_INFO("Finished initialization procedure");
        //}
        //return;  
    } 
    
    if(initializing)
        return;

    if (new_state == robot_state::CHARGING)
    {
        ROS_DEBUG("Start recharging");
        state.charging = true;
        prev_consumed_energy_A = consumed_energy_A;
    }
    else {
        /* The robot is not charging; if the battery was previously under charging, it means that the robot aborted the
        recharging process */
        if (state.charging == true)
        {
            ROS_DEBUG("Recharging aborted");
            state.charging = false;
            state.fully_charged = false; //TODO reduntant?
        }
    }
    
    if(new_state == robot_state::CHARGING_COMPLETED) //when the robot is fully_charged, it is left a little bit at the DS to allow him to compute the next DS without consuming energy, so that the check of the reachability of frontiers also using the Ds graph makes sense
        do_not_consume_battery = true;
    else
        do_not_consume_battery = false;
    
//    if(new_state == fully_charged || new_state == exploring || new_state == exploring_for_graph_navigation)
    if(new_state == robot_state::COMPUTING_NEXT_GOAL || new_state == robot_state::exploring_for_graph_navigation)
        advanced_computations_bool = true;
    else
        advanced_computations_bool = false;
    
    if(new_state == robot_state::IN_QUEUE)
        idle_mode = true;
    else
        idle_mode = false;
        
    robot_state = new_state;
        
}

void battery_simulate::compute()
{
    /* Compute the number of elapsed seconds since the last time that we updated the battery state (since we use
     * powers) */
    ros::Duration time_diff = time_manager->simulationTimeNow() - time_last;
    double time_diff_sec = time_diff.toSec();
    elapsed_time = time_diff_sec; //TODO for debugging, should be removed...

    double power_idle = power_microcontroller + power_basic_computations;
    
    /* If there is no time difference to last computation, there is nothing to do */
    if (time_diff_sec <= 0)
        return;

    state.fully_charged = false;
    /* If the robot is charging, increase remaining battery life, otherwise compute consumed energy and decrease remaining battery life */
    if (state.charging)
    {
        ROS_INFO("Recharging battery");
        double ratio_A = -1, ratio_B = -1;
        if(consumed_energy_A < 0 && consumed_energy_B < 0) {
            ROS_FATAL("this should not happen...");
            return;
        }
        else if(consumed_energy_A <= 0) {
            ratio_A = 0.0;
            ratio_B = 1.0;
            consumed_energy_A = 0;
            ROS_ERROR("this should not happen..."); //FIXME actually this could happen since B is also consumed while charging...
        }   
        else if(consumed_energy_B <= 0) {
            ratio_A = 1.0;
            ratio_B = 0.0;
            consumed_energy_B = 0;
            ROS_ERROR("this should not happen...");
        }
        else {
            ratio_A = consumed_energy_A / (consumed_energy_A + consumed_energy_B);
            ratio_B = consumed_energy_B / (consumed_energy_A + consumed_energy_B);
        }
        
        if(ratio_A < 0 || ratio_A > 1 || ratio_B < 0 || ratio_B > 1 || fabs(ratio_A + ratio_B - 1.0) > 0.01 ) {
            ROS_FATAL("strange ratio");
            return;
        }
        
        consumed_energy_A -= ratio_A * power_charging * time_diff_sec;
        consumed_energy_B -= ratio_B * power_charging * time_diff_sec;
        consumed_energy_B += power_idle * time_diff_sec;
        
        state.remaining_distance = (prev_consumed_energy_A - consumed_energy_A) / prev_consumed_energy_A * maximum_traveling_distance;
        if(state.remaining_distance > maximum_traveling_distance)
            state.remaining_distance = maximum_traveling_distance;

        /* Check if the battery is now fully charged; notice that SOC could be higher than 100% due to how we increment
         * the remaing_energy during the charging process */
//        if (state.soc >= 1)
        if(consumed_energy_A <=0 && consumed_energy_B <= 0)
        {
            ROS_INFO("Recharging completed");
            
            state.charging = false;
            state.fully_charged = true;
             
            consumed_energy_A = 0;
            consumed_energy_B = 0;
            
            // Set battery state to its maximum values 
            state.remaining_time_run = maximum_traveling_distance * speed_avg;
            state.remaining_distance = maximum_traveling_distance;
            state.remaining_time_charge = 0;
            state.soc = 1; // since SOC cannot be higher than 100% in real life, force it to be 100%
            
            setRobotStateToChargingCompleted();
        }
        else {
            ROS_DEBUG("Recharging...");
            state.remaining_time_charge = (consumed_energy_A + consumed_energy_B) / power_charging ;
            
            mutex_traveled_distance.lock();
            state.remaining_distance -= traveled_distance;
//            ROS_ERROR("%.2f", traveled_distance);

            last_traveled_distance = traveled_distance;
            total_traveled_distance += traveled_distance;

            traveled_distance = 0;
            mutex_traveled_distance.unlock();
            
            state.remaining_time_run = state.remaining_distance * speed_avg;
            state.soc = state.remaining_distance / maximum_traveling_distance;
            
            }
    }
    else if (do_not_consume_battery) {
        time_last = time_manager->simulationTimeNow();
        return;
    } 
    else if(idle_mode) {
        consumed_energy_B += power_idle * time_diff_sec; // J
//        
//        state.soc = remaining_energy / total_energy;
//        state.remaining_time_charge = (total_energy - remaining_energy) / power_charging ;
//        state.remaining_time_run = remaining_energy / (power_moving * max_speed_linear + power_standing + power_basic_computations + power_advanced_computation);  
//        state.remaining_distance = state.remaining_time_run * speed_avg;
        
    } else {
    
        /* If the robot is moving, than we have to consider also the energy consumed for moving, otherwise it is
         * sufficient to consider the fixed power cost.
         * Notice that this is clearly an approximation, since we are not sure that the robot was moving for the whole
         * interval of time: moreover, since we do not know the exact speed profile during this interval of time, we
         * overestimate the consumed energy by assuming that the robot moved at the maximum speed for the whole period.
         */
        
        if (speed_linear > 0) { //TODO we should check also the robot state (e.g.: if the robot is in 'exploring', speed_linear should be zero...); notice that 
            consumed_energy_A += (power_moving_fixed_cost + power_per_speed * speed_linear) * time_diff_sec; // J
        }
        
        consumed_energy_B += (power_sonar + power_laser + power_microcontroller + power_basic_computations + power_advanced_computations) * time_diff_sec; // J
        
        /* Update battery state */
        state.remaining_time_charge = (consumed_energy_A + consumed_energy_B) / power_charging ;
        
        mutex_traveled_distance.lock();
        state.remaining_distance -= traveled_distance;
//        ROS_ERROR("%.2f", traveled_distance);

        last_traveled_distance = traveled_distance;
        total_traveled_distance += traveled_distance;

        traveled_distance = 0;
        mutex_traveled_distance.unlock();
        
        state.remaining_time_run = state.remaining_distance * speed_avg;
        state.soc = state.remaining_distance / maximum_traveling_distance;
        
    }

//    ROS_ERROR("SOC: %.0f%%; remaining distance: %.2fm", state.soc * 100, state.remaining_distance);
    
    /* Store the time at which this battery state update has been perfomed, so that next time we can compute againg the elapsed time interval */
    time_last = time_manager->simulationTimeNow();
}

void battery_simulate::log()
{
    std::string state_std;
    if(initializing)
        state_std = "initializing";
    else if(idle_mode)
        state_std = "idle";
    else if(state.charging) 
        state_std = "charging";
    else if(advanced_computations_bool)
        state_std = "computing";
    else if(speed_linear > 0 || speed_angular > 0)
        state_std = "moving";
    else if(do_not_consume_battery)
        state_std = "at_ds_for_computation";
    else
        state_std = "standing";
        
    ros::Duration sim_time = time_manager->simulationTimeNow() - sim_time_start;
    ros::WallDuration wall_time = ros::WallTime::now() - wall_time_start;
    
    battery_state_fs.open(battery_state_filename.c_str(), std::fstream::in | std::fstream::app | std::fstream::out);
    battery_state_fs << sim_time.toSec() << "," << wall_time.toSec() << "," << state.remaining_time_run << "," << state.remaining_time_charge << "," << state.remaining_distance << "," << state_std << "," << consumed_energy_A << "," << consumed_energy_B << "," << last_traveled_distance << "," << total_traveled_distance << "," << time_manager->simulationTimeNow() << "," << ros::WallTime::now() << std::endl;
    battery_state_fs.close();
}

void battery_simulate::publish()
{
    ROS_INFO("publishing battery state");
    pub_battery.publish(state);
    ROS_INFO("%.1f, %.1f", state.remaining_distance, state.maximum_traveling_distance);
}

void battery_simulate::cb_cmd_vel(const geometry_msgs::Twist &msg)
{
    ROS_DEBUG("Received speed");
    speed_linear = msg.linear.x;
    speed_angular = msg.angular.z;
}

void battery_simulate::cb_speed(const explorer::Speed &msg)  // unused for the moment, but maybe with a better estimate
                                                             // of the battery curve...
{
    // if the average speed is very low, there is probably something wrong, set it to the value from the config file
    if (msg.avg_speed > speed_avg_init)
    {
        speed_avg = msg.avg_speed;
    }
    else
    {
        speed_avg = speed_avg_init;
    }
}

// TODO(minor) - DO WE NEED THIS???
void battery_simulate::cb_soc(const std_msgs::Float32::ConstPtr &msg)
{
    // ROS_INFO("Received SOC!!!");
}

// TODO(minor) do we need this?
//void battery_simulate::totalTime(const std_msgs::Float32::ConstPtr &msg)
//{
//    total_time = ("%F", msg->data);
//}

void battery_simulate::run() {
    while(ros::ok()) {
        ros::Duration(1).sleep(); //TODO(minor) rates???
        ros::spinOnce();
        
        get_state();
        
        // compute new battery state
        compute();

        // log battery
        log();

        // publish battery state
        publish();
    }
}

void battery_simulate::setTimeManager(TimeManagerInterface *time_manager) {
    this->time_manager = time_manager;
}

void battery_simulate::createLogDirectory() {
    /* Create directory */
    log_path = log_path.append("/energy_mgmt");
    log_path = log_path.append(robot_name);
    boost::filesystem::path boost_log_path(log_path.c_str());
    if (!boost::filesystem::exists(boost_log_path))
    {
        ROS_INFO("Creating directory %s", log_path.c_str());
        try
        {
            if (!boost::filesystem::create_directories(boost_log_path))
            {
                ROS_ERROR("Cannot create directory %s: aborting node...", log_path.c_str());
                exit(-1);
            }
        }
        catch (const boost::filesystem::filesystem_error &e)
        {
            ROS_ERROR("Cannot create path %saborting node...", log_path.c_str());
            exit(-1);
        }
    }
    else
    {
        ROS_INFO("Directory %s already exists: log files will be saved there", log_path.c_str());
    }
}

void battery_simulate::createLogFiles() {
    /* Create file names */
    log_path = log_path.append("/");
    info_file = log_path + std::string("metadata_battery.csv");
    battery_state_filename = log_path + std::string("battery_state.csv");

    fs_info.open(info_file.c_str(), std::fstream::in | std::fstream::app | std::fstream::out);
    fs_info << "#power_sonar, power_laser, power_basic_computations, power_advanced_computations, power_microcontroller, power_moving_fixed_cost, power_per_speed, power_charging,max_linear_speed,initial_speed_avg" << std::endl;
    fs_info << power_sonar << "," << power_laser << "," << power_basic_computations << "," << power_advanced_computations << "," << power_microcontroller << "," << power_moving_fixed_cost << "," << power_per_speed << "," << power_charging << "," << max_speed_linear << "," << speed_avg_init << std::endl;
    fs_info.close();
    
    battery_state_fs.open(battery_state_filename.c_str(), std::fstream::in | std::fstream::app | std::fstream::out);
    battery_state_fs << "#elapsed_sim_time,elapsed_wall_time,state.remaining_time_run,state.remaining_time_charge,state.remaining_distance,state,consumed_energy_A,consumed_energy_B,last_traveled_distance,total_traveled_distance,sim_time,wall_time" << std::endl;
    battery_state_fs.close();
    
    sim_time_start = ros::Time::now();
    wall_time_start = ros::WallTime::now();
}

void battery_simulate::poseCallback(const geometry_msgs::PoseWithCovarianceStampedConstPtr &pose) {
    pose_x = pose->pose.pose.position.x;
    pose_y = pose->pose.pose.position.y;
    
    mutex_traveled_distance.lock();
    traveled_distance += sqrt( (last_x-pose_x)*(last_x-pose_x) + (last_y-pose_y)*(last_y-pose_y) );
    mutex_traveled_distance.unlock();
    
    last_x = pose_x;
    last_y = pose_y;
}

void battery_simulate::setRobotStateToChargingCompleted() {

    robot_state::TryToLockRobotState try_msg;
    bool repeat, call_succeeded;
    do {
        ROS_INFO("trying to acquire lock on robot_state");
        call_succeeded = try_to_lock_robot_state_sc.call(try_msg);
        if(!call_succeeded) {
            ROS_ERROR("failed call to try_lock");
            repeat = true;
        }
        if(!try_msg.response.lock_acquired) {
            ROS_INFO("lock not acquired: retrying");
            repeat = true;
        }
    } while(repeat);

    robot_state::GetRobotState get_msg;
    while(!get_robot_state_sc.call(get_msg))
        ROS_ERROR("call to get_robot_state failed!");
    if(get_msg.response.robot_state != robot_state::CHARGING)
        handle_new_state(get_msg.response.robot_state);

    robot_state::SetRobotState set_msg;
    set_msg.request.robot_state = robot_state::CHARGING_COMPLETED;
    while(!set_robot_state_sc.call(set_msg))
        ROS_ERROR("call to set_robot_state failed!");

    robot_state::UnlockRobotState unlock_msg;
    while(!unlock_robot_state_sc.call(unlock_msg))
        ROS_ERROR("call to unlock_robot_state failed!");
}

/*************************
 ** Debugging functions **
 *************************/
void battery_simulate::set_last_time() {
    time_last = time_manager->simulationTimeNow();
}

double battery_simulate::last_time_secs() {
    return time_last.toSec();
}

double battery_simulate::getElapsedTime() {
    return elapsed_time;
}

void battery_simulate::spinOnce() {
    ros::spinOnce();
}

double battery_simulate::getConsumedEnergyA() {
    return consumed_energy_A;
}

double battery_simulate::getConsumedEnergyB() {
    return consumed_energy_B;
}

double battery_simulate::getRemainingDistance() {
    return state.remaining_distance;
}

double battery_simulate::getMaximumTravelingDistance() {
    return maximum_traveling_distance;
}

double battery_simulate::getTotalTraveledDistance() {
    return total_traveled_distance;
}