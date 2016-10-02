/** @file      rosbuzz.cpp
 *  @version   1 
 *  @date      27.09.2016
 *  @brief     Buzz Implementation as a node in ROS. 
 *  @copyright 2016 MistLab. All rights reserved.
 */

#include "ros/ros.h"
#include "sensor_msgs/NavSatFix.h"
#include "mavros_msgs/GlobalPositionTarget.h"
#include "mavros_msgs/CommandCode.h"
#include "mavros_msgs/CommandInt.h"
#include "mavros_msgs/State.h"
#include "mavros_msgs/BatteryStatus.h"
#include "mavros_msgs/Mavlink.h"
#include "sensor_msgs/NavSatStatus.h"
#include "rosbuzz/neighbour_pos.h"
#include <sstream>
#include <buzz/buzzasm.h>
#include "buzzuav_closures.h"
#include "buzz_utility.h"
#include "uav_utility.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>


std::vector<pos_struct> pos_vect; // vector of struct to store neighbours position 
static int done = 0;
static double cur_pos[3];
uint64_t payload;

/**
 * This program implements Buzz node in ros.
 */
/*

 * Print usage information
 */
void usage(const char* path, int status) {
   fprintf(stderr, "Usage:\n");
   fprintf(stderr, "\t%s <stream> <msg_size> <file.bo> <file.bdb>\n\n", path);
   fprintf(stderr, "== Options ==\n\n");
   fprintf(stderr, "  stream        The stream type: tcp or bt\n");
   fprintf(stderr, "  msg_size      The message size in bytes\n");
   fprintf(stderr, "  file.bo       The Buzz bytecode file\n");
   fprintf(stderr, "  file.bdbg     The Buzz debug file\n\n");
   exit(status);
}
/*Set the current position of the robot callback*/
void set_cur_pos(double latitude,
		 double longitude,
		 double altitude){
/* set the current position of the robot*/
cur_pos [0] =latitude;
cur_pos [1] =longitude;
cur_pos [2] =altitude;

}

/*convert from catresian to spherical coordinate system callback */
void cvt_spherical_coordinates(){
double latitude,longitude,altitude;
for(int i=0;i<pos_vect.size();i++){
latitude=pos_vect[i].x;
longitude = pos_vect[i].y;
altitude=pos_vect[i].z;
pos_vect[i].x=sqrt(pow(latitude,2.0)+pow(longitude,2.0)+pow(altitude,2.0));
pos_vect[i].y=atan(longitude/latitude);
pos_vect[i].z=atan((sqrt(pow(latitude,2.0)+pow(longitude,2.0)))/altitude);
    ROS_INFO("[Debug] Converted for neighbour : %d radius    : [%15f]",pos_vect[i].id, pos_vect[i].x);
    ROS_INFO("[Debug] Converted for neighbour : %d azimuth   : [%15f]",pos_vect[i].id, pos_vect[i].y);
    ROS_INFO("[Debug] Converted for neighbour : %d elevation : [%15f]",pos_vect[i].id, pos_vect[i].z);
}
}

/*battery status callback*/ 
void battery(const mavros_msgs::BatteryStatus::ConstPtr& msg)
{
set_battery(msg->voltage,msg->current,msg->remaining);
}

/*current position callback*/
void current_pos(const sensor_msgs::NavSatFix::ConstPtr& msg)
{
set_cur_pos(msg->latitude,msg->longitude,msg->altitude);
}

/*payload callback*/
void payload_obt(const mavros_msgs::Mavlink::ConstPtr& msg)
{
long unsigned int message_obt[(msg->payload64.end())-(msg->payload64.begin())];
int i = 0;
	// print all the remaining numbers
	for(std::vector<long unsigned int>::const_iterator it = msg->payload64.begin(); it != msg->payload64.end(); ++it)
	{
		message_obt[i] = *it;
		i++;
        }
in_msg_process(message_obt, pos_vect);

}

/*neighbours position call back */
 
void neighbour_pos(const rosbuzz::neighbour_pos::ConstPtr& msg)
{
/*obtain the neighbours position*/
int i=0;
/*clear all the previous neighbour position*/
pos_vect.clear();
for (std::vector<sensor_msgs::NavSatFix>::const_iterator it = msg->pos_neigh.begin(); it != msg->pos_neigh.end(); ++it){
sensor_msgs::NavSatFix cur_neigh = *it;
sensor_msgs::NavSatStatus stats = cur_neigh.status;
pos_vect.push_back(pos_struct(stats.status,(cur_neigh.latitude-cur_pos[0]),(cur_neigh.longitude-cur_pos[1]),(cur_neigh.altitude-cur_pos[2])));
    ROS_INFO("[Debug]I heard neighbour: %d from latitude: [%15f]",pos_vect[i].id, pos_vect[i].x);
    ROS_INFO("[Debug]I heard neighbour: %d from longitude: [%15f]",pos_vect[i].id, pos_vect[i].y);
    ROS_INFO("[Debug]I heard neighbour: %d from altitude: [%15f]",pos_vect[i].id, pos_vect[i].z);
i++;
} 
cvt_spherical_coordinates();
}

int oldcmdID=0;
int rc_cmd;
bool rc_callback(mavros_msgs::CommandInt::Request  &req,
         mavros_msgs::CommandInt::Response &res){

if(req.command==oldcmdID)
	return true;
   else
	oldcmdID=req.command;
   switch(req.command)
   {
	case mavros_msgs::CommandCode::NAV_TAKEOFF:
   		ROS_INFO("RC_call: TAKE OFF!!!!");
		rc_cmd=mavros_msgs::CommandCode::NAV_TAKEOFF;
		rc_call(rc_cmd);
		res.success = true;
		break;
	case mavros_msgs::CommandCode::NAV_LAND:
   		ROS_INFO("RC_Call: LAND!!!!");
		rc_cmd=mavros_msgs::CommandCode::NAV_LAND;
		rc_call(rc_cmd);
		res.success = true;
		break;
	case mavros_msgs::CommandCode::NAV_RETURN_TO_LAUNCH:
   		ROS_INFO("RC_Call: GO HOME!!!!");
		rc_cmd=mavros_msgs::CommandCode::NAV_RETURN_TO_LAUNCH;
		rc_call(rc_cmd);
		res.success = true;
		break;
	case mavros_msgs::CommandCode::NAV_WAYPOINT:
   		ROS_INFO("RC_Call: GO TO!!!! x = %f , y = %f , Z = %f",req.param1,req.param2,req.param3);
		double rc_goto[3];
   		rc_goto[0]=req.param1;
		rc_goto[1]=req.param2;
		rc_goto[2]=req.param3;
                
		set_goto(rc_goto);
		rc_cmd=mavros_msgs::CommandCode::NAV_WAYPOINT;
		rc_call(rc_cmd);
		res.success = true;
		break;
	default:
		res.success = false;
		break;
   }
   return true;
}


/*controlc handler callback*/
static void ctrlc_handler(int sig) {
   done = 1;
}



int main(int argc, char **argv)
{

   /*Compile the buzz code .bzz to .bo*/
   system("rm ../catkin_ws/src/rosbuzz/src/out.basm ../catkin_ws/src/rosbuzz/src/out.bo ../catkin_ws/src/rosbuzz/src/out.bdbg");
   system("bzzparse ../catkin_ws/src/rosbuzz/src/test.bzz ../catkin_ws/src/rosbuzz/src/out.basm");
   system("bzzasm ../catkin_ws/src/rosbuzz/src/out.basm ../catkin_ws/src/rosbuzz/src/out.bo ../catkin_ws/src/rosbuzz/src/out.bdbg");
  
  /*initiate rosBuzz*/
  ros::init(argc, argv, "rosBuzz"); 
  ROS_INFO("Buzz_node");

  /*Create node Handler*/
  ros::NodeHandle n_c;

 
  /*subscribers*/
  ros::Subscriber current_position_sub = n_c.subscribe("current_pos", 1000, current_pos);

  ros::Subscriber neighbour_position_sub = n_c.subscribe("neighbour_pos", 1000, neighbour_pos);

  ros::Subscriber battery_sub = n_c.subscribe("battery_state", 1000, battery);

  ros::Subscriber payload_sub = n_c.subscribe("pay_load_in", 1000, payload_obt);


  /*publishers*/
  //ros::Publisher goto_pub = n_c.advertise<mavros_msgs::GlobalPositionTarget>("go_to", 1000);
  
  //ros::Publisher cmds_pub = n_c.advertise<mavros_msgs::State>("newstate", 1000);

  ros::Publisher payload_pub = n_c.advertise<mavros_msgs::Mavlink>("pay_load_out", 1000);

  /* Clients*/
  ros::ServiceClient mav_client = n_c.serviceClient<mavros_msgs::CommandInt>("djicmd");
  
  /*Services*/
  ros::ServiceServer service = n_c.advertiseService("djicmd_rc", rc_callback);
  ROS_INFO("Ready to receive Mav Commands from dji RC client");
  /*loop rate of ros*/
  ros::Rate loop_rate(1);

   
   /* The bytecode filename */
   char* bcfname = "../catkin_ws/src/rosbuzz/src/out.bo"; //argv[1];
   /* The debugging information file name */
   char* dbgfname = "../catkin_ws/src/rosbuzz/src/out.bdbg"; //argv[2];
   /* Set CTRL-C handler */
   signal(SIGTERM, ctrlc_handler);
   signal(SIGINT, ctrlc_handler);

   
   /* Set the Buzz bytecode */
   if(buzz_script_set(bcfname, dbgfname)) {
   fprintf(stdout, "Bytecode file found and set\n");

// buzz setting
  mavros_msgs::CommandInt cmd_srv;
  int count = 0;
  while (ros::ok() && !done && !buzz_script_done())
  {
 
      /* Main loop */
         buzz_script_step();
 
    /*prepare the goto publish message */
    
    double* goto_pos = getgoto();
    cmd_srv.request.param1 = goto_pos[0];
    cmd_srv.request.param2 = goto_pos[1];
    cmd_srv.request.param3 = goto_pos[2];
    //ROS_INFO("set value X = %f, Y =%f, Z= %f ",cmd_srv.request.param1,cmd_srv.request.param2,cmd_srv.request.param3);
    /*prepare commands for takeoff,land and gohome*/
    //char tmp[20];
    //sprintf(tmp,"%i",getcmd());
    cmd_srv.request.command =  getcmd();
    /* diji client call*/
    if (mav_client.call(cmd_srv)){ ROS_INFO("Reply: %ld", (long int)cmd_srv.response.success); }
    else{ ROS_ERROR("Failed to call service 'djicmd'"); }
    /*Prepare Pay load to be sent*/  
    unsigned long int* payload_out_ptr= out_msg_process();  
    mavros_msgs::Mavlink payload_out;
    payload_out.payload64.push_back(*payload_out_ptr);
    /*publish prepared messages in respective topic*/
    payload_pub.publish(payload_out);
    
    /*run once*/
    ros::spinOnce();
    /*sleep for the mentioned loop rate*/
    loop_rate.sleep();
    ++count;
  }
  /* Cleanup */
   buzz_script_destroy();

 /* Stop the robot */
   uav_done();
   
}
  /* All done */
   return 0;

}


