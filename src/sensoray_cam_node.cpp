
/*********************************************************************
*
*    File:    sensoray_cam_node
* Package:    sensoray_cam
*  Author:    Weikun Zhen (weikunz AT cmu DOT edu)
*    Date:    2015-05-26
*  Detail:    This program is a driver for sensoray model 2255
*             video capture box in ROS .
*
**********************************************************************/

#include <sensoray_cam/sensoray_cam.h>

// For communication to ROS
#include <ros/ros.h>
#include <sensor_msgs/CompressedImage.h>
#include "std_msgs/String.h"
#include <image_transport/image_transport.h>
#include <camera_info_manager/camera_info_manager.h>
#include <sstream>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>

// the main
int main(int argc, char **argv)
{
    ros::init(argc, argv, "sensoray_cam");
    ros::NodeHandle nh;

    // parameters
    std::string dev_name1, dev_name2, io_method_name, camera_name1, camera_name2, camera_info_url1, camera_info_url2;
    int image_width, image_height, framerate;
    boost::shared_ptr<camera_info_manager::CameraInfoManager> cinfo1, cinfo2;

    // shared image message
    sensor_msgs::CompressedImage img1,img2;
    image_transport::CameraPublisher img_pub1,img_pub2;


    image_transport::ImageTransport it(nh);
    ros::Publisher crapImgPub, crapImgPub2;

    crapImgPub = nh.advertise<sensor_msgs::CompressedImage>("MyImg", 10);
    crapImgPub2 = nh.advertise<sensor_msgs::Image>("MyImg_raw", 10);
    img_pub1 = it.advertiseCamera("/sensoray_cam_stereo/left/image_raw/compressed",1);
    img_pub2 = it.advertiseCamera("/sensoray_cam_stereo/right/image_raw/compressed",1);

    // grab the parameters
    nh.param("video_device1", dev_name1, std::string("/dev/video0"));
    nh.param("video_device2", dev_name2, std::string("/dev/video1"));

    // possible values: mmap, read, userptr
    nh.param("io_method", io_method_name, std::string("mmap"));
    nh.param("image_width", image_width, 640);
    nh.param("image_height", image_height, 480);
    nh.param("framerate", framerate, 10);

    // load the camera info
    nh.param("camera_frame_id1", img1.header.frame_id, std::string("stereo_left"));
    nh.param("camera_frame_id2", img2.header.frame_id, std::string("stereo_right"));
    nh.param("camera_name1", camera_name1, std::string("stereo_left"));
    nh.param("camera_name2", camera_name2, std::string("stereo_right"));
    nh.param("camera_info_url1", camera_info_url1, std::string("file://${ROS_HOME}/camera_info/calib_left.yaml"));
    nh.param("camera_info_url2", camera_info_url2, std::string("file://${ROS_HOME}/camera_info/calib_right.yaml"));
    cinfo1.reset(new camera_info_manager::CameraInfoManager(nh, camera_name1, camera_info_url1));
    cinfo2.reset(new camera_info_manager::CameraInfoManager(nh, camera_name2, camera_info_url2));

    // define sensoray camera capture object
    /*for debug, we only define one object for now*/
    SensorayCam cam1(dev_name1);
//    SensorayCam cam2(dev_name2);

    // the main loop
    int count = 0;
    ros::Publisher chatter_pub = nh.advertise<std_msgs::String>("chatter", 1000);
    while(ros::ok() && count < 10)
    {
        // read images
        if(!cam1.grab_image()){
            ROS_WARN("Device '%s' loses frame.", cam1.dev_name);
        }

        //publish image
        cv_bridge::CvImage cv_image;
        sensor_msgs::Image ros_image;
        cv::Mat imgbuf(cv::Size(480, 640), CV_8UC3, cam1.myImgPtr);
        cv_image.image = cv::imdecode(imgbuf,1);
        cv_image.encoding = "bgr8";
        cv_image.toImageMsg(ros_image);
        crapImgPub2.publish(ros_image);

        ROS_INFO("count = %d", count++);
    }
}
