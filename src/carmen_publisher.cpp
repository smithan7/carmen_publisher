///////////////////////////////////////////////////////////////////////////////
// this program just uses sicktoolbox to get laser scans, and then publishes
// them as ROS messages
//
// Copyright (C) 2008, Morgan Quigley
//
// I am distributing this code under the BSD license:
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice, 
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright 
//     notice, this list of conditions and the following disclaimer in the 
//     documentation and/or other materials provided with the distribution.
//   * Neither the name of Stanford University nor the names of its 
//     contributors may be used to endorse or promote products derived from 
//     this software without specific prior written permission.
//   
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE.


// https://github.com/cognitiveRobot/carmen_publisher

#include <iostream>
#include <csignal>
#include <cstdio>
#include <math.h>
#include <limits>
#include <fstream>
#include <iomanip>
#include <vector>
#include <dirent.h>



#include "ros/ros.h"
#include "sensor_msgs/LaserScan.h"
#include <diagnostic_updater/diagnostic_updater.h> // Publishing over the diagnostics channels.
#include <diagnostic_updater/publisher.h>


#include "geometry_msgs/Twist.h"
#include "geometry_msgs/Pose.h"
#include "geometry_msgs/PoseStamped.h"
#include "tf/tf.h"
#include "tf/transform_listener.h"  //for tf::getPrefixParam
#include <tf/transform_broadcaster.h>
#include "tf/transform_datatypes.h"

using namespace std;

void publish_scan(diagnostic_updater::DiagnosedPublisher<sensor_msgs::LaserScan> *pub, float *range_values,
        uint32_t n_range_values, uint32_t *intensity_values,
        uint32_t n_intensity_values, double scan_time, float angle_min,
        float angle_max, std::string frame_id, ros::Time time) {
    static int scan_count = 0;
    sensor_msgs::LaserScan scan_msg;
    scan_msg.header.frame_id = frame_id;
    scan_count++;

    scan_msg.angle_min = angle_min;
    scan_msg.angle_max = angle_max;

    scan_msg.angle_increment = (scan_msg.angle_max - scan_msg.angle_min) / (double) (n_range_values - 1);
    scan_msg.scan_time = scan_time;
    scan_msg.time_increment = scan_time / (2 * M_PI) * scan_msg.angle_increment;
    scan_msg.range_min = 0;

    scan_msg.range_max = 8.1;

    scan_msg.ranges.resize(n_range_values);
    scan_msg.header.stamp = ros::Time::now();//(ros::Time)time;//same time as odo
    for (size_t i = 0; i < n_range_values; i++) {
        // Check for overflow values, see pg 124 of the Sick LMS telegram listing
        /*switch (range_values[i]) {
                // 8m or 80m operation
            case 8191: // Measurement not valid
                scan_msg.ranges[i] = numeric_limits<float>::quiet_NaN();
                break;
            case 8190: // Dazzling
                scan_msg.ranges[i] = numeric_limits<float>::quiet_NaN();
                break;
            case 8189: // Operation Overflow
                scan_msg.ranges[i] = numeric_limits<float>::quiet_NaN();
                break;
            case 8187: // Signal to Noise ratio too small
                scan_msg.ranges[i] = numeric_limits<float>::quiet_NaN();
                break;
            case 8186: // Erorr when reading channel 1
                scan_msg.ranges[i] = numeric_limits<float>::quiet_NaN();
                break;
            case 8183: // Measured value > maximum Value
                scan_msg.ranges[i] = numeric_limits<float>::infinity();
                break;

            default:
                scan_msg.ranges[i] = range_values[i];
        }*/
        scan_msg.ranges[i] = range_values[i];
    }
    scan_msg.intensities.resize(n_intensity_values);
    for (size_t i = 0; i < n_intensity_values; i++) {
        scan_msg.intensities[i] = (float) intensity_values[i];
    }


    pub->publish(scan_msg);
}

int main(int argc, char **argv) {
	if(argc != 2) {
		cout<<"Usage: rosrun carmen_publisher carmen_publisher <filename-with-path>"<<endl;
		return 0;
	}
    ros::init(argc, argv, "carmen_publisher");
    ros::NodeHandle nh;
    ros::NodeHandle nh_ns("~");

    bool inverted;

    vector<string> odoms;
    int scanID = 0;
    double rX, rY,theta,lastX, lastY;
    string data;

    //for odom->base_link transform
    tf::TransformBroadcaster odom_broadcaster;
    geometry_msgs::TransformStamped odom_trans;

    //for resolving tf names.
    std::string tf_prefix;
    std::string frame_id_odom;
    std::string frame_id_base_link;

    tf_prefix = tf::getPrefixParam(nh);
    frame_id_odom = tf::resolve(tf_prefix, "odom");
    frame_id_base_link = tf::resolve(tf_prefix, "base_link");

    // publishing transform odom->base_link

    odom_trans.header.frame_id = frame_id_odom;
    odom_trans.child_frame_id = frame_id_base_link;

    std::string frame_id = "laser";
    double scan_time = 1.0 / 100;

    const int nScans = 360;
    float range_values[nScans] = {0};
    uint32_t intensity_values[nScans] = {0};
    uint32_t n_range_values = nScans;
    uint32_t n_intensity_values = 0;
    double sleepTime = 50000;

    float angle_min = -1.562070;
    float angle_max = 1.562070;
    double angle_increment = (angle_max - angle_min) / double(n_range_values);

    // Diagnostic publisher parameters
    double desired_freq;
    nh_ns.param<double>("desired_frequency", desired_freq, 10.0);
    double min_freq;
    nh_ns.param<double>("min_frequency", min_freq, desired_freq);
    double max_freq;
    nh_ns.param<double>("max_frequency", max_freq, desired_freq);
    double freq_tolerance; // Tolerance before error, fractional percent of frequency.
    nh_ns.param<double>("frequency_tolerance", freq_tolerance, 0.3);
    int window_size; // Number of samples to consider in frequency
    nh_ns.param<int>("window_size", window_size, 30);
    double min_delay; // The minimum publishing delay (in seconds) before error.  Negative values mean future dated messages.
    nh_ns.param<double>("min_acceptable_delay", min_delay, 0.0);
    double max_delay; // The maximum publishing delay (in seconds) before error.
    nh_ns.param<double>("max_acceptable_delay", max_delay, 0.2);
    std::string hardware_id;
    nh_ns.param<std::string>("hardware_id", hardware_id, "SICK LMS");
    double time_offset_sec;
    nh_ns.param<double>("time_offset", time_offset_sec, 0.0);
    ros::Duration time_offset(time_offset_sec);

    // Set up diagnostics
    diagnostic_updater::Updater updater;
    updater.setHardwareID(hardware_id);
    diagnostic_updater::DiagnosedPublisher<sensor_msgs::LaserScan> scan_pub(nh.advertise<sensor_msgs::LaserScan>("scan", 10), updater,
            diagnostic_updater::FrequencyStatusParam(&min_freq, &max_freq, freq_tolerance, window_size),
            diagnostic_updater::TimeStampStatusParam(min_delay, max_delay));

    const char* addForLogFile = argv[1];//"/home/albot1/rosData/carmen.log";//logFile6-ClockW-sLoop-robot1.log
    ifstream inputFile(addForLogFile, ios::in); //open to read

    double time;

    try {
        while (!inputFile.eof()) {

        	inputFile >> data;

        	if (data.compare("FLASER") == 0 || data.compare("FLASER") == 0){

	            if (data.compare("ODOM") == 0) {
	                 inputFile >> data;
	                 rX = atof( data.c_str());
	                 inputFile >> data;
	                 rY = atof( data.c_str());
	                 inputFile >> data;
	                 theta = atof( data.c_str());            
	                
	                for(int i=0;i<3;i++){                   
	                    inputFile >> data;//ignoring next 3 data
	                }
	                inputFile >> data; //for time
					time = atof(data.c_str());

					//publishing tf
		            odom_trans.header.stamp = ros::Time::now();//(ros::Time)time;//ros::Time::now();
		                
		            odom_trans.transform.translation.x =rX;
		            odom_trans.transform.translation.y = rY;
		            odom_trans.transform.translation.z = 0.0;
		            odom_trans.transform.rotation = tf::createQuaternionMsgFromYaw(theta);

		            odom_broadcaster.sendTransform(odom_trans);
		            usleep(sleepTime/5);//50000usec = 50ms
	            }

	            if (data.compare("FLASER") == 0) {
	            	// taken from: http://carmen.sourceforge.net/logger_playback.html
	                inputFile >> data;
	       			int n_samples = atoi(data.c_str());
	                for (int i = 0; i < nScans; i++) {
	                    //scan from carmen file
	                    inputFile >> data;
	                    range_values[i] = atof(data.c_str());
	                }

	                 inputFile >> data;
	                 rX = atof( data.c_str());
	                 inputFile >> data;
	                 rY = atof( data.c_str());
	                 inputFile >> data;
	                 theta = atof( data.c_str());

	                scanID++;

		                	            //publishing laserScan
		            publish_scan(&scan_pub, range_values, n_range_values, intensity_values,
		                    n_intensity_values, scan_time, angle_min, angle_max, frame_id,ros::Time::now());
		            ros::spinOnce();
		            // Update diagnostics
		            updater.update();
			
			    	cout<<"Scan "<<scanID<<" published"<<endl;
				    usleep(sleepTime);//50000usec = 50ms	

				    	            //publishing tf
		            odom_trans.header.stamp = ros::Time::now();//(ros::Time)time;//ros::Time::now();
		                
		            odom_trans.transform.translation.x =rX;
		            odom_trans.transform.translation.y = rY;
		            odom_trans.transform.translation.z = 0.0;
		            odom_trans.transform.rotation = tf::createQuaternionMsgFromYaw(theta);

		            //odom_broadcaster.sendTransform(odom_trans);
	            }
        	}
    	} 
    }
    catch (...) {
        ROS_ERROR("Unknown error.");
        return 1;
    }

    
    ROS_INFO("Success.\n");

    return 0;
}

