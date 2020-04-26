/// \file
/// \brief Draws Each Obstacle in RViz using MarkerArrays
///
/// PARAMETERS:
/// PUBLISHES:
/// SUBSCRIBES:
/// FUNCTIONS:

#include <ros/ros.h>

#include <math.h>
#include <string>
#include <vector>

#include "map/map.hpp"
#include "map/prm.hpp"
#include "global_planner/heuristic.hpp"

#include "nuslam/TurtleMap.h"

#include <functional>  // To use std::bind
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <geometry_msgs/Point.h>

// Used to deal with YAML list of lists
#include <xmlrpcpp/XmlRpcValue.h> // catkin component


int main(int argc, char** argv)
/// The Main Function ///
{
    ROS_INFO("STARTING NODE: astar");

    // Vars
    double frequency = 10.0;
    // Scale by which to transform marker poses (10 cm/cell so we use 10)
    double SCALE = 10.0;
    std::string frame_id = "base_footprint";
    XmlRpc::XmlRpcValue xml_obstacles;

    // PRM Parameters
    int n = 10;
    int k = 5;
    double thresh = 0.01;
    double inflate = 0.1;

    // store Obstacle(s) here to create Map
    std::vector<map::Obstacle> obstacles_v;

    ros::init(argc, argv, "astar_node"); // register the node on ROS
    ros::NodeHandle nh; // get a handle to ROS
    ros::NodeHandle nh_("~"); // get a handle to ROS
    // Parameters
    nh_.getParam("frequency", frequency);
    nh_.getParam("obstacles", xml_obstacles);
    nh_.getParam("map_frame_id", frame_id);
    nh_.getParam("n", n);
    nh_.getParam("k", k);
    nh_.getParam("thresh", thresh);
    nh_.getParam("inflate", inflate);
    nh_.getParam("scale", SCALE);

    rigid2d::Vector2D start(7.0/SCALE, 3.0/SCALE);
    rigid2d::Vector2D goal(7.0/SCALE, 26.0/SCALE);

    // 'obstacles' is a triple-nested list.
    // 1st level: obstacle (Obstacle), 2nd level: vertices (std::vector), 3rd level: coordinates (Vector2D)

    // std::vector<Obstacle>
    if(xml_obstacles.getType() != XmlRpc::XmlRpcValue::TypeArray)
    {
    ROS_ERROR("There is no list of obstacles");
    } else
    {
        for(int i = 0; i < xml_obstacles.size(); ++i)
        {
          // Obstacle contains std::vector<Vector2D>
          // create Obstacle with empty vertex vector
          map::Obstacle obs;
          if(xml_obstacles[i].getType() != XmlRpc::XmlRpcValue::TypeArray)
          {
              ROS_ERROR("obstacles[%d] has no vertices", i);
          } else {
              for(int j = 0; j < xml_obstacles[i].size(); ++j)
              {
                // Vector2D contains x,y coords
                if(xml_obstacles[i][j].size() != 2)
                {
                   ROS_ERROR("Vertex[%d] of obstacles[%d] is not a pair of coordinates", j, i);
                } else if(
                  xml_obstacles[i][j][0].getType() != XmlRpc::XmlRpcValue::TypeDouble or
                  xml_obstacles[i][j][1].getType() != XmlRpc::XmlRpcValue::TypeDouble)
                {
                  ROS_ERROR("The coordinates of vertex[%d] of obstacles[%d] are not doubles", j, i);
                } else {
                  // PASSED ALL THE TESTS: push Vector2D to vertices list in Obstacle object
                  rigid2d::Vector2D vertex(xml_obstacles[i][j][0], xml_obstacles[i][j][1]);
                  // NOTE: SCALE DOWN
                  vertex.x /= SCALE;
                  vertex.y /= SCALE;
                  obs.vertices.push_back(vertex);
                }
              }
          }
          // push Obstacle object to vector of Object(s) in Map
          obstacles_v.push_back(obs);
        }
    }

    // Initialize Marker
    // Init Marker Array Publisher
    ros::Publisher line_pub = nh.advertise<visualization_msgs::MarkerArray>("line_map", 1);
    ros::Publisher sph_pub = nh.advertise<visualization_msgs::MarkerArray>("sph_map", 1);

    ros::Publisher line_path_pub = nh.advertise<visualization_msgs::MarkerArray>("line_path", 1);
    ros::Publisher sph_path_pub = nh.advertise<visualization_msgs::MarkerArray>("sph_path", 1);

    // Init Marker Array
    visualization_msgs::MarkerArray line_arr;
    visualization_msgs::MarkerArray sph_arr;

    visualization_msgs::MarkerArray line_path;
    visualization_msgs::MarkerArray sph_path;

    // Init Marker
    visualization_msgs::Marker marker;
    marker.header.frame_id = frame_id;
    marker.header.stamp = ros::Time::now();
    // marker.ns = "my_namespace";
    // marker.id = 0;
    marker.type = visualization_msgs::Marker::LINE_STRIP;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.z = 0;
    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;
    marker.scale.x = 0.02;
    marker.color.r = 0.96f;
    marker.color.g = 0.475f;
    marker.color.b = 0.0f;
    marker.color.a = 1.0;
    marker.lifetime = ros::Duration();

    visualization_msgs::Marker sph_mkr;
    sph_mkr = marker;
    sph_mkr.type = visualization_msgs::Marker::SPHERE;
    sph_mkr.scale.x = 0.02;
    sph_mkr.scale.y = 0.02;
    sph_mkr.scale.z = 0.02;

    // Color
    sph_mkr.color.r = 0.5f;
    sph_mkr.color.g = 0.0f;
    sph_mkr.color.b = 0.5f;
    int sph_mkr_id = 0;

    // LINE_STRIP relative to this pose
    marker.pose.position.x = 0.0;
    marker.pose.position.y = 0.0;


    // PATH MARKERS
    visualization_msgs::Marker path_marker;
    path_marker = marker;
    // More visible than PRM
    path_marker.scale.x = 0.05;
    path_marker.color.r = 0.0;
    path_marker.color.g = 1.0;
    path_marker.color.b = 0.0;

    visualization_msgs::Marker path_sph_mkr;
    path_sph_mkr = sph_mkr;
    // More visible than PRM
    path_sph_mkr.scale.x = 0.05;
    path_sph_mkr.scale.y = 0.05;
    path_sph_mkr.scale.z = 0.05;
    path_sph_mkr.color.r = 0.0f;
    path_sph_mkr.color.g = 0.0f;
    path_sph_mkr.color.b = 0.0f;

    int path_sph_mkr_id = 0;

    // Build PRM
    map::PRM prm(obstacles_v, inflate);
    prm.build_map(n, k, thresh);
    // DRAW PRM
    auto configurations = prm.return_prm();
    for (auto node_iter = configurations.begin(); node_iter != configurations.end(); node_iter++)
    {
        marker.points.clear();
        marker.id = std::distance(configurations.begin(), node_iter);

        // Check if a node has edges before plotting
        if (node_iter->id_set.size() > 0)
        {
            for (auto id_iter = node_iter->id_set.begin(); id_iter != node_iter->id_set.end(); id_iter++)
                {
                  // Add node as first marker vertex
                  geometry_msgs::Point first_vertex;
                  first_vertex.x = node_iter->coords.x;
                  first_vertex.y = node_iter->coords.y;
                  first_vertex.z = 0.0;
                  marker.points.push_back(first_vertex);

                  // Find Vertex for each ID
                  auto neighbor_iter = configurations.at(*id_iter);

                  geometry_msgs::Point new_vertex;
                  new_vertex.x = neighbor_iter.coords.x;
                  new_vertex.y = neighbor_iter.coords.y;
                  new_vertex.z = 0.0;
                  marker.points.push_back(new_vertex);

                  // Also push back cylinders
                  sph_mkr.pose.position.x = node_iter->coords.x;
                  sph_mkr.pose.position.y = node_iter->coords.y;
                  sph_mkr.id = sph_mkr_id;
                  sph_mkr_id++;
                  sph_arr.markers.push_back(sph_mkr);
                }
            // Push to Marker Array
            line_arr.markers.push_back(marker);
        }
    }

    // PLAN on PRM using A*
    global::Astar astar(obstacles_v);

    auto path = astar.plan(start, goal, configurations);

    for (auto path_iter = path.begin(); path_iter != path.end(); path_iter++)
    {

        // Add node as marker vertex
        geometry_msgs::Point vtx;
        vtx.x = path_iter->vertex.coords.x;
        vtx.y = path_iter->vertex.coords.y;
        vtx.z = 0.0;
        path_marker.points.push_back(vtx);

        // Also push back cylinders
        path_sph_mkr.pose.position.x = path_iter->vertex.coords.x;
        path_sph_mkr.pose.position.y = path_iter->vertex.coords.y;
        path_sph_mkr.id = sph_mkr_id;
        path_sph_mkr_id++;
        sph_path.markers.push_back(sph_mkr);

    }

    line_path.markers.push_back(path_marker);

    ros::Rate rate(frequency);

    // Main While
    while (ros::ok())
    {
        ros::spinOnce();

        // Publish Map
        line_pub.publish(line_arr);
        sph_pub.publish(sph_arr);

        // Publish Path
        line_path_pub.publish(line_path);
        sph_path_pub.publish(sph_path);

        rate.sleep();
    }

    return 0;
}