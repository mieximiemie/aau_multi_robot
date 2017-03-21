#include <battery_simulate.h>

using namespace std;

battery_simulate::battery_simulate()
{
    // read parameters
    nh.getParam("energy_mgmt/speed_avg", speed_avg_init);
    nh.getParam("energy_mgmt/power_charging", power_charging);
    nh.getParam("energy_mgmt/power_moving", power_moving);
    nh.getParam("energy_mgmt/power_standing", power_standing);
    nh.getParam("energy_mgmt/charge_max", charge_max);

    charge_max = 1000000;

    // initialize private variables
    charge = charge_max;
    speed_linear = 0;
    speed_angular = 0;
    time_moving = 0;
    time_standing = 0;
    perc_moving = 0.5;
    perc_standing = 0.5;
    output_shown = false;
    time_last = ros::Time::now();
    
    
    max_speed_linear = 0.8;
    total_power = 60; // NB: do not put it under 50-60, because otherwise robots do not have enough time to make computations...
    remaining_power = total_power;
    speed_linear = max_speed_linear;
    power_standing = 1;
    power_moving = 1.3;
    power_charging = 5;
    
    
    
 


    // initialize message
    state.charging = false;
    state.soc = 100;
    state.remaining_time_charge = 0;
  //  state.remaining_time_run = charge_max * (perc_moving / power_moving + perc_standing / power_standing) * 3600;
 //   state.remaining_distance = speed_avg_init * charge_max / power_moving * 3600;


    bool debugShown = false;

    // advertise topics
    pub_battery = nh.advertise<energy_mgmt::battery_state>("battery_state", 1);
    pub_charging_completed = nh.advertise<std_msgs::Empty>("charging_completed", 1);

    // subscribe to topics
    sub_charge = nh.subscribe("going_to_recharge", 1, &battery_simulate::cb_charge, this);
    sub_speed = nh.subscribe("avg_speed", 1, &battery_simulate::cb_speed, this);
    sub_cmd_vel = nh.subscribe("cmd_vel", 1, &battery_simulate::cb_cmd_vel, this);

    //TODO: do we need this?
    sub_time = nh.subscribe("totalTime", 1, &battery_simulate::totalTime, this);
    
    
    state.remaining_time_run = 1000;
    state.remaining_distance = 1000;

}

void battery_simulate::compute()
{
    if(state.charging) {
        //printf("%c[1;34mRecharging...\e[0m\n", 27);
        remaining_power += power_charging;
        state.soc = remaining_power / total_power;
        state.remaining_time_charge = 10;
        //printf("%c[1;34mSOC: %f\e[0m\n", 27, state.soc);
        if(state.soc * 100 >= 100) {
            state.soc = 100;
            state.charging = false;
            state.remaining_time_charge = 0;
            remaining_power = total_power;
            state.remaining_time_run = 60;
            state.remaining_distance = 60;
            //ROS_ERROR("%c[1;34mRecharging complete!\e[0m\n", 27);
            std_msgs::Empty msg;
            pub_charging_completed.publish(msg);

        }
    } else {
        // calculate time difference
        ros::Duration time_diff = ros::Time::now() - time_last;
        time_last = ros::Time::now();
        double time_diff_sec = time_diff.toSec();
        // if there is no time difference to last computation then there is nothing to do
        if(time_diff_sec <= 0)
            return;

        if(speed_linear > 0)
            remaining_power -= power_standing;
        else
            remaining_power -= power_moving * max_speed_linear + power_standing;
        
        state.soc = remaining_power / total_power;

        /*
        if(speed_lienar > 0)
            //state.remaining_time_run = (total_time * state.soc) / (100 * ((0.7551 * max_speed_linear) + 0.3959));
            //state.remaining_time_run = (total_time * state.soc) / (100 * ((0.7551 * max_speed_linear) + 0.3959));
        else
            state.remaining_time_run = (total_time * state.soc) / standing_consumption(100 * ((0.7551 * max_speed_linear) + 0.3959));    
        */
        
        
        
        state.remaining_time_run = state.soc * total_power;
        state.remaining_distance = state.remaining_time_run;
        
        //state.remaining_time_run = 1000;
        //state.remaining_distance = 1000;
        //state.soc = 90;
        //ROS_INFO("%f", state.remaining_distance);
    //     if you don't need the max. time left and the max. remaining distance, add 1 to the variable j of array[][j] in
    //     battery_measure.cpp, line 87 row 46 before publishing stateOfCharge (stoch.data)
    }
}

void battery_simulate::output()
{
    if((int(state.soc) % 10) == 0)
    {
        if (output_shown == false)
        {
            ROS_INFO("Battery: %.0f%%  ---  remaining distance: %.2fm", state.soc, state.remaining_distance);
            output_shown = true;
        }
    }
    else
        output_shown = false;
}

void battery_simulate::publish()
{
    pub_battery.publish(state);
}

void battery_simulate::cb_charge(const std_msgs::Empty::ConstPtr &msg)
{
    //ROS_INFO("Starting to recharge");
    state.charging = true;
    state.soc = 0;
    remaining_power = 0;
    state.remaining_time_run = 0;
    state.remaining_distance = 0;
}

void battery_simulate::cb_cmd_vel(const geometry_msgs::Twist &msg)
{
    //ROS_INFO("Received speed");
    speed_linear = msg.linear.x;
    speed_angular = msg.angular.z;
}

void battery_simulate::cb_speed(const explorer::Speed &msg)
{

    // if the average speed is very low, there is probably something wrong, set it to the value from the config file
    if(msg.avg_speed > speed_avg_init){
        speed_avg = msg.avg_speed;
    }
    else{
        speed_avg = speed_avg_init;
    }
}

// TODO - DO WE NEED THIS???
void battery_simulate::cb_soc(const std_msgs::Float32::ConstPtr& msg)
{
    //ROS_INFO("Received SOC!!!");
    state.soc = ("%F", msg->data);
}

//TODO: do we need this?
void battery_simulate::totalTime(const std_msgs::Float32::ConstPtr& msg)
{
    total_time = ("%F", msg->data);
}


