carmen_publisher
================
1. it reads a carmen logfile(.log) then publishes robot position and laserScan

adapted from:

https://github.com/cognitiveRobot/carmen_publisher

to run: get number of scans per scan, FLASER #### [Scans(1:####)]

then run: 

rosrun carmen_publisher carmen_publisher /home/andy/catkin_ws/src/carmen_publisher/data_sets/fr079-complete.gfs.log 

to run named log file

to visualize, run:

rviz rviz

to create a map run

rosrun gmapping slam_gmapping particles=250


# carmen_publisher
