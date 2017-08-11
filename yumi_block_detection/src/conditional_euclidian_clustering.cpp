// Dataset: https://sourceforge.net/projects/pointclouds/files/PCD%20datasets/Trimble/Outdoor1/Statues_4.pcd.zip/download
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/console/time.h>
#include <pcl/impl/point_types.hpp>

#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/segmentation/conditional_euclidean_clustering.h>
#include <pcl/visualization/pcl_visualizer.h>

typedef pcl::PointXYZI PointTypeIO;
typedef pcl::PointXYZINormal PointTypeFull;


bool
enforceIntensitySimilarity (const PointTypeFull& point_a, const PointTypeFull& point_b, float squared_distance)
{
  if (fabs (point_a.intensity - point_b.intensity) < 1.0f)
    return (true);
  else
    return (false);
}

bool
enforceCurvatureOrIntensitySimilarity (const PointTypeFull& point_a, const PointTypeFull& point_b, float squared_distance)
{
  Eigen::Map<const Eigen::Vector3f> point_a_normal (point_a.normal);
  Eigen::Map<const Eigen::Vector3f> point_b_normal (point_b.normal);
  if (fabs (point_a.intensity - point_b.intensity) < 1.0f)
    return (true);
  if (fabs (point_a_normal.dot (point_b_normal)) < 0.05)
    return (true);
  return (false);
}

bool
customRegionGrowing (const PointTypeFull& point_a, const PointTypeFull& point_b, float squared_distance)
{
  Eigen::Map<const Eigen::Vector3f> point_a_normal (point_a.normal);
  Eigen::Map<const Eigen::Vector3f> point_b_normal (point_b.normal);
  if (squared_distance < 10000)
  {
    if (fabs (point_a.intensity - point_b.intensity) < 1.0f)
      return (true);
    if (fabs (point_a_normal.dot (point_b_normal)) < 0.06)
      return (true);
  }
  else
  {
    if (fabs (point_a.intensity - point_b.intensity) < 3.0f)
      return (true);
  }
  return (false);
}

int
main (int argc, char** argv)
{
  // Data containers used
  pcl::PointCloud<PointTypeIO>::Ptr cloud_in (new pcl::PointCloud<PointTypeIO>), cloud_out (new pcl::PointCloud<PointTypeIO>);
  pcl::PointCloud<PointTypeFull>::Ptr cloud_with_normals (new pcl::PointCloud<PointTypeFull>);
  pcl::IndicesClustersPtr clusters (new pcl::IndicesClusters), small_clusters (new pcl::IndicesClusters), large_clusters (new pcl::IndicesClusters);
  pcl::search::KdTree<PointTypeIO>::Ptr search_tree (new pcl::search::KdTree<PointTypeIO>);
  pcl::console::TicToc tt;

  // Load the input point cloud
  std::cerr << "Loading...\n", tt.tic ();
  pcl::io::loadPCDFile ("intensity_scene4.pcd", *cloud_in);
  std::cerr << ">> Done: " << tt.toc () << " ms, " << cloud_in->points.size () << " points\n";

  // Downsample the cloud using a Voxel Grid class
  std::cerr << "Downsampling...\n", tt.tic ();
  pcl::VoxelGrid<PointTypeIO> vg;
  vg.setInputCloud (cloud_in);
  vg.setLeafSize (0.005f, 0.005f, 0.005f);
  vg.setDownsampleAllData (true);
  vg.filter (*cloud_out);
  std::cerr << ">> Done: " << tt.toc () << " ms, " << cloud_out->points.size () << " points\n";

  //cloud_out = cloud_in;

  // Set up a Normal Estimation class and merge data in cloud_with_normals
  std::cerr << "Computing normals...\n", tt.tic ();
  pcl::copyPointCloud (*cloud_out, *cloud_with_normals);
  pcl::NormalEstimation<PointTypeIO, PointTypeFull> ne;
  ne.setInputCloud (cloud_out);
  ne.setSearchMethod (search_tree);
  ne.setRadiusSearch (1.0);
  ne.compute (*cloud_with_normals);
  std::cerr << ">> Done: " << tt.toc () << " ms\n";

  //pcl::PointCloud<pcl::PointXYZI>::Ptr cloud_act (new pcl::PointCloud<pcl::PointXYZI>);
  //pcl::PointCloudXYZRGBtoXYZI(*filCloud, *cloud_act);

  // Set up a Conditional Euclidean Clustering class
  std::cerr << "Segmenting to clusters...\n", tt.tic ();
  pcl::ConditionalEuclideanClustering<PointTypeFull> cec (true);
  cec.setInputCloud (cloud_with_normals);
  cec.setConditionFunction (&enforceCurvatureOrIntensitySimilarity);
  cec.setClusterTolerance (10.0); //?
  cec.setMinClusterSize (5);
  cec.setMaxClusterSize (2000);
  cec.segment (*clusters);
  cec.getRemovedClusters (small_clusters, large_clusters);
  std::cerr << ">> Done: " << tt.toc () << " ms\n";
  std::cerr << "Number of clusters...\n" << clusters->size();

  pcl::PointCloud<PointTypeIO>::Ptr cloud_act (new pcl::PointCloud<PointTypeIO>);

  // Using the intensity channel for lazy visualization of the output
  for (int i = 0; i < small_clusters->size (); ++i)
    for (int j = 0; j < (*small_clusters)[i].indices.size (); ++j)
      cloud_out->points[(*small_clusters)[i].indices[j]].intensity = -2.0;
  for (int i = 0; i < large_clusters->size (); ++i)
    for (int j = 0; j < (*large_clusters)[i].indices.size (); ++j)
    {
      cloud_out->points[(*large_clusters)[i].indices[j]].intensity = +10.0;
      cloud_act->push_back(cloud_out->points[(*large_clusters)[i].indices[j]]);
    }
  for (int i = 0; i < clusters->size (); ++i)
  {
    int label = rand () % 8;
    for (int j = 0; j < (*clusters)[i].indices.size (); ++j)
    {
      cloud_out->points[(*clusters)[i].indices[j]].intensity = label;
      //cloud_act->push_back(cloud_out->points[(*clusters)[i].indices[j]]);
    }
  }

  pcl::visualization::PCLVisualizer *viewer (new pcl::visualization::PCLVisualizer ("3D Viewer"));
  pcl::visualization::PointCloudColorHandlerGenericField<PointTypeIO>::PointCloudColorHandlerGenericField rgb(cloud_out,"intensity");
  viewer->addPointCloud<PointTypeIO>(cloud_out, rgb, "sample cloud");
  viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud");
  viewer->setBackgroundColor (0, 0, 0);
  viewer->spin();

  // Save the output point cloud
  std::cerr << "Saving...\n", tt.tic ();
  pcl::io::savePCDFile ("output.pcd", *cloud_act);
  std::cerr << ">> Done: " << tt.toc () << " ms\n";

  return (0);
}
