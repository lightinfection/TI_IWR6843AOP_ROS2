#ifndef DBSCAN_HPP
#define DBSCAN_HPP

#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <rclcpp/rclcpp.hpp>
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/point_cloud.hpp"
#include "sensor_msgs/sensor_msgs/point_cloud_conversion.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "object_detection/common/boundingbox.hpp"

#define UNCLASSIFIED -1
#define CORE_POINT 1
#define BORDER_POINT 2
#define NOISE -2
#define SUCCESS 0
#define FAILURE -3

typedef union
{
    struct
    {
        uint8_t b, g, r;
    };
    uint32_t value;
} RGBvalue;

template <typename PointPCL, typename PointPCLRGB>
class dbscan {
public:    
    dbscan() 
    {
        out_pointcloud = std::make_shared<sensor_msgs::msg::PointCloud>();
        bboxes = std::make_shared<visualization_msgs::msg::MarkerArray>();
        container = bbox();
        outcloud_dbscan = typename pcl::PointCloud<PointPCLRGB>::Ptr (new typename pcl::PointCloud<PointPCLRGB>);
    }
    ~dbscan()
    {
        if(rgb_color) delete rgb_color;
    }

    void set_params(const float& eps, const int& min, const bool& bb, const std::string& frame_id, const std::string& ns)
    {
        m_minPoints = min;
        m_epsilon = eps;
        if_bb = bb;
        frame_id_ = frame_id;
        ns_ = ns;

        success = true;
        return;
    };

    int expandCluster(Point point, int clusterID)
    {
        std::vector<int> clusterSeeds = calculateCluster(point);

        if (int(clusterSeeds.size()) < m_minPoints)
        {
            point.clusterID = NOISE;
            return FAILURE;
        }
        else
        {
            int index = 0, indexCorePoint = 0;
            std::vector<int>::iterator iterSeeds;
            for (iterSeeds = clusterSeeds.begin(); iterSeeds != clusterSeeds.end(); ++iterSeeds)
            {
                m_points.at(*iterSeeds).clusterID = clusterID;
                if (m_points.at(*iterSeeds).x == point.x && m_points.at(*iterSeeds).y == point.y && m_points.at(*iterSeeds).z == point.z)
                //  && m_points.at(*iterSeeds).intensity == point.intensity )
                {
                    indexCorePoint = index;
                }
                ++index;
            }
            clusterSeeds.erase(clusterSeeds.begin()+indexCorePoint);

            for (std::vector<int>::size_type i = 0, n = clusterSeeds.size(); i < n; ++i)
            {
                std::vector<int> clusterNeighors = calculateCluster(m_points.at(clusterSeeds[i]));

                if (int(clusterNeighors.size()) >= m_minPoints)
                {
                    std::vector<int>::iterator iterNeighors;
                    for (iterNeighors = clusterNeighors.begin(); iterNeighors != clusterNeighors.end(); ++iterNeighors)
                    {
                        if (m_points.at(*iterNeighors).clusterID == UNCLASSIFIED || m_points.at(*iterNeighors).clusterID == NOISE)
                        {
                            if (m_points.at(*iterNeighors).clusterID == UNCLASSIFIED)
                            {
                                clusterSeeds.push_back(*iterNeighors);
                                n = clusterSeeds.size();
                            }
                            m_points.at(*iterNeighors).clusterID = clusterID;
                        }
                    }
                }
            }

            return SUCCESS;
        }
    }

    std::vector<int> calculateCluster(Point point)
    {
        int index = 0;
        std::vector<Point>::iterator iter;
        std::vector<int> clusterIndex;
        for (iter = m_points.begin(); iter != m_points.end(); ++iter)
        {
            if (calculateDistance(point, *iter) <= m_epsilon) clusterIndex.push_back(index);
            index++;
        }
        return clusterIndex;
    };

    double calculateDistance(const Point& pointCore, const Point& pointTarget )
    {
        return pow(pointCore.x - pointTarget.x,2)+pow(pointCore.y - pointTarget.y,2)+pow(pointCore.z - pointTarget.z,2);
    }

    void getpoint(const sensor_msgs::msg::PointCloud2::ConstSharedPtr& pc)
    {
        m_points.clear();
        outcloud_dbscan->clear();

        outcloud_dbscan->header.stamp = static_cast<long int>(rclcpp::Clock().now().seconds());
        outcloud_dbscan->header.frame_id = pc->header.frame_id;
        sensor_msgs::convertPointCloud2ToPointCloud(*pc, *out_pointcloud);
        for (size_t i=0; i<out_pointcloud->points.size(); i++)
        {
            p.x = out_pointcloud->points[i].x;
            p.y = out_pointcloud->points[i].y;
            p.z = out_pointcloud->points[i].z;
            // p.intensity = out_pointcloud.channels[0].values[i];
            p.clusterID = UNCLASSIFIED;
            // m_points.insert(m_points.begin(), p);
            m_points.push_back(p);
            m_pointSize = m_points.size();
        }
        out_pointcloud->points.clear();
        return;
    }

    void getpoint(const typename pcl::PointCloud<PointPCL>::Ptr& input)
    {
        m_points.clear();
        outcloud_dbscan->clear();

        outcloud_dbscan->header.frame_id = input->header.frame_id;
        for (size_t i=0; i<input->points.size(); i++)
        {
            p.x = input->points[i].x;
            p.y = input->points[i].y;
            p.z = input->points[i].z;
            p.clusterID = UNCLASSIFIED;
            m_points.push_back(p);
            m_pointSize = m_points.size();
        }
        return;
    }
    
    void getrandomcolor(RGBvalue* x)
    {
        x->b = std::rand()%255;
        x->g = std::rand()%255;
        x->b = std::rand()%255;
    };

    int cluster()
    {
        int clusterID = 1;
        std::vector<Point>::iterator iter;
        for (iter = m_points.begin(); iter != m_points.end(); ++iter)
        {
            if (iter->clusterID == UNCLASSIFIED)
            {
                if (expandCluster(*iter, clusterID) != FAILURE) clusterID += 1;
            }
        }
        return clusterID;
    }

    void yield(std::vector<Point>& points, const int& num_points)
    {
        if (outcloud_dbscan->size() > 0)
        {
            printf("the pointcloud buffer is not cleared in time, and the size of left data is %ld\n", outcloud_dbscan->size());
            return;
        }
        int n = colors.size();
        if (cluster_num > n)
        {
            for (int cluster_ind = 0; cluster_ind < cluster_num - n; cluster_ind++)
            {
                rgb_color = new RGBvalue;
                getrandomcolor(rgb_color);
                colors.push_back(*rgb_color);
                delete rgb_color;
            }
        }
        // for (size_t cluster_ind = 0; cluster_ind < n; cluster_ind++)
        //     {
        //         printf("%5.2lf, ", colors[cluster_ind]);
        //         if (cluster_ind==n-1)
        //         {
        //             printf("\n");
        //         }
        //     }
        
        if (if_bb)
        {
            if (bboxes->markers.size() > 0)
            {
                printf("uncleared cube markers removed.\n");
                bboxes->markers.clear();
            }
            try
            {
                auto it_r = std::find_if(points.begin(), points.end(), [&](Point& pt) { return pt.clusterID==UNCLASSIFIED; });
                int real_cluster_num = (it_r != points.end()) ? (cluster_num - 1) : cluster_num;
                // printf("real num is %d, total num is %d \n", real_cluster_num, cluster_num);
                auto stamp = rclcpp::Clock().now();
                for (int id = 0; id < real_cluster_num; id++)
                {
                    container.update(points, id+1, frame_id_, ns_);
                    if (container.initialized)
                    {
                        container.marker->color.r = ((float)(uint32_t)colors[id].r)/255.0;
                        container.marker->color.g = ((float)(uint32_t)colors[id].g)/255.0;
                        container.marker->color.b = ((float)(uint32_t)colors[id].b)/255.0;
                        container.marker->color.a = 0.2;
                        container.marker->header.stamp = stamp;
                        bboxes->markers.push_back(*container.marker);
                    }
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }

        outcloud_dbscan->height = 1;
        outcloud_dbscan->width = num_points;
        outcloud_dbscan->is_dense = 1;
        outcloud_dbscan->points.resize(outcloud_dbscan->height*outcloud_dbscan->width);

        int i = 0;
        int j = 0;

        // printf("Number of points: %u\n"
        //     " x     y     z     cluster_id\n"
        //     "-----------------------------\n"
        //     , num_points);

        while(i < num_points)
        {
            if (points[i].clusterID!=UNCLASSIFIED)
            {
                outcloud_dbscan->points[j].x = points[i].x;
                outcloud_dbscan->points[j].y = points[i].y;
                outcloud_dbscan->points[j].z = points[i].z;
                outcloud_dbscan->points[j].rgb = (colors[points[i].clusterID - 1].value);
                // printf("%5.2lf %5.2lf %5.2lf: %d (%5.2lf)\n",
                //         outcloud_dbscan->points[j].x, outcloud_dbscan->points[j].y, outcloud_dbscan->points[j].z, points[i].clusterID, outcloud_dbscan->points[j].rgb);
                ++j;
            }
            // printf("%5.2lf %5.2lf %5.2lf: %d\n",
            //         points[i].x, points[i].y, points[i].z, points[i].clusterID);        
            ++i;
        }     
    }

    void run(const sensor_msgs::msg::PointCloud2::ConstSharedPtr& input)
    {
        if (input->width>0)
        {
            if (success)
            {
                getpoint(input);
                if (if_bb) bboxes->markers.clear();
                if (!m_points.empty())
                {
                    cluster_num = cluster();
                    yield(m_points, m_pointSize);
                }
                m_points.clear();
            }
        }
        else printf("the input pointcloud for dbscan cluster is empty, size is %d\n", input->width);
        return;
    }
    
    void run(const typename pcl::PointCloud<PointPCL>::Ptr& input)
    {
        if (input->width>0)
        {
            if (success)
            {
                getpoint(input);
                if (!m_points.empty())
                {
                    cluster_num = cluster();
                    yield(m_points, m_pointSize);
                }
                m_points.clear();
            }
        }
        else printf("the input pointcloud for dbscan cluster is empty, size is %d\n", input->width);
        return;
    }
    
public:
    int cluster_num, m_pointSize;
    std::vector<Point> m_points;
    std::vector<RGBvalue> colors;
    RGBvalue* rgb_color;
    Point p;
    visualization_msgs::msg::MarkerArray::SharedPtr bboxes;
    bbox container;
    sensor_msgs::msg::PointCloud::SharedPtr out_pointcloud;
    typename pcl::PointCloud<PointPCLRGB>::Ptr outcloud_dbscan;
    bool success = false;

private:    
    int m_minPoints;
    double m_epsilon;
    bool if_bb;
    std::string frame_id_, ns_;
};

#endif