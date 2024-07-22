#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <clipper/clipper.h>
#include <clipper/utils.h>
#include <triangulation/observation.hpp>
#include <chrono>


Eigen::Matrix2Xd read_2d_points(std::string txt_file) {
  // each line of the txt file is seperrated by a comma
  // the first column is the x coordinate and the second column is the y coordinate
  std::cout << "Reading data from " << txt_file << std::endl;
  std::ifstream file(txt_file);
  std::string line;
  std::vector<std::vector<double>> points;
  while (std::getline(file, line)) {
    std::vector<double> point;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
      point.push_back(std::stod(token));
    }
    points.push_back(point);
  }
  file.close();
  Eigen::Matrix2Xd points_matrix(2, points.size());
  for (int i = 0; i < points.size(); i++) {
    points_matrix(0, i) = points[i][0];
    points_matrix(1, i) = points[i][1];
  }
  std::cout << "Read " << points.size() << " points" << std::endl;
  return points_matrix;
}

Eigen::Matrix2Xd transform_2d_points(Eigen::Matrix2Xd points, Eigen::Matrix2d rotation, Eigen::Vector2d translation) {
  Eigen::Matrix2Xd transformed_points(2, points.cols());
  for (int i = 0; i < points.cols(); i++) {
    Eigen::Vector2d point = points.col(i);
    transformed_points.col(i) = rotation * point + translation;
  }
  return transformed_points;
}

std::vector<int> argsort(const std::vector<double>& v) {
  std::vector<int> idx(v.size());
  std::iota(idx.begin(), idx.end(), 0);
  std::sort(idx.begin(), idx.end(), [&v](int i1, int i2) {return v[i1] < v[i2];});
  return idx;
}

// function to compute triangle difference
void compute_triangle_diff(const DelaunayTriangulation::Polygon& triangle_model, const DelaunayTriangulation::Polygon& triangle_data, std::vector<double>& diffs, std::vector<std::vector<double>>& matched_points_model, std::vector<std::vector<double>>& matched_points_data, double threshold) {
  // get the vertices of the triangles
  PointVector vertices_model = triangle_model.points; // 3 by 2 vector
  PointVector vertices_data = triangle_data.points;
  int num_vertices = vertices_model.size();
  // std::cout << "number of points: " << vertices_model.size() << std::endl;
  // std::cout << "size of each point: " << vertices_model[0].size() << std::endl;
  // convert the vertices to Eigen::Matrix2Xd
  Eigen::Matrix2Xd vertices_model_matrix(2, vertices_model.size()); // 2 by 3 matrix
  Eigen::Matrix2Xd vertices_data_matrix(2, vertices_data.size());
  for (int i = 0; i < vertices_model.size(); i++) {
    vertices_model_matrix(0, i) = vertices_model[i][0];
    vertices_model_matrix(1, i) = vertices_model[i][1];
    vertices_data_matrix(0, i) = vertices_data[i][0];
    vertices_data_matrix(1, i) = vertices_data[i][1];
  }
  // compute the centroid of the triangles
  Eigen::Vector2d centroid_model = vertices_model_matrix.rowwise().mean();
  Eigen::Vector2d centroid_data = vertices_data_matrix.rowwise().mean();
  // compute the euclidean distance between vertices and centroid
  Eigen::Matrix2Xd vertices_model_centered = vertices_model_matrix.colwise() - centroid_model;
  Eigen::Matrix2Xd vertices_data_centered = vertices_data_matrix.colwise() - centroid_data;
  std::vector<double> dist_model; // 3 by 1 vector
  std::vector<double> dist_data;
  for(int i = 0; i < num_vertices; i++) {
    double norm = vertices_model_centered.col(i).norm();
    dist_model.push_back(norm);
  }
  for(int i = 0; i < num_vertices; i++) {
    double norm = vertices_data_centered.col(i).norm();
    dist_data.push_back(norm);
  }
  // sort the vertices based on the distance to the centroid
  std::vector<int> arg_sort_model = argsort(dist_model);
  std::vector<int> arg_sort_data = argsort(dist_data);
  std::vector<double> sorted_dist_model; // 3 by 1 vector
  std::vector<double> sorted_dist_data;
  for (int i = 0; i < num_vertices; i++) {
    sorted_dist_model.push_back(dist_model[arg_sort_model[i]]);
    // std::cout << "sorted_dist_model: " << sorted_dist_model[i] << std::endl;
    sorted_dist_data.push_back(dist_data[arg_sort_data[i]]);
  }
  // std::cout << " " << std::endl;
  // compute the l2 norm difference between the sorted distances
  double diff = 0;
  for (int i = 0; i < num_vertices; i++) {
    diff += std::pow(sorted_dist_model[i] - sorted_dist_data[i], 2);
  }
  diff = std::sqrt(diff);

  // if the difference is less than the threshold, add the matched points to the matched_points_model and matched_points_data
  if (diff < threshold) {
    diffs.push_back(diff);
    // arrange the vertices based on the sorted indices
    for (int i = 0; i < num_vertices; i++) {
      matched_points_model.push_back(vertices_model[arg_sort_model[i]]);
      matched_points_data.push_back(vertices_data[arg_sort_data[i]]);
    }
  }
}

// function to find matched triangles and points
void match_triangles(const std::vector<DelaunayTriangulation::Polygon>& triangles_model, const std::vector<DelaunayTriangulation::Polygon>& triangles_data, std::vector<double>& diffs, std::vector<std::vector<double>>& matched_points_model, std::vector<std::vector<double>>& matched_points_data, double threshold = 0.1) {
  for (int i = 0; i < triangles_model.size(); i++) {
    for (int j = 0; j < triangles_data.size(); j++) {
      compute_triangle_diff(triangles_model[i], triangles_data[j], diffs, matched_points_model, matched_points_data, threshold);
    }
  }
}

// remove repeated points by keep the index of the unique points
// void remove_repeated_points(const Eigen::Matrix2Xd& points, std::vector<int> unique_idx){
  
// }


// function to estimate transformation
Eigen::Matrix3d estimate_tf(Eigen::Matrix2Xd matched_points_a, Eigen::Matrix2Xd matched_points_b) {
  Eigen::Vector2d centroid_a = matched_points_a.rowwise().mean();
  Eigen::Vector2d centroid_b = matched_points_b.rowwise().mean();
  Eigen::Matrix2Xd centered_points_a = matched_points_a.colwise() - centroid_a;
  Eigen::Matrix2Xd centered_points_b = matched_points_b.colwise() - centroid_b;
  Eigen::Matrix2d H = centered_points_a * centered_points_b.transpose();
  Eigen::JacobiSVD<Eigen::Matrix2d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix2d R = svd.matrixV() * svd.matrixU().transpose();
  if (R.determinant() < 0) {
    R.col(1) *= -1;
  }
  Eigen::Vector2d t = centroid_b - R * centroid_a;
  Eigen::Matrix3d tf = Eigen::Matrix3d::Identity();
  tf.block<2, 2>(0, 0) = R;
  tf.block<2, 1>(0, 2) = t;
  return tf;
}


int main(){
  /*
  Data preparation
  */
  // read from data file 1
  std::string data1_file_name = "/home/jiuzhou/clipper_semantic_object/examples/data/robot0Map_forest_2d.txt";
  Eigen::Matrix2Xd model = read_2d_points(data1_file_name);
  // convert matrix to vector of vectors
  std::vector<std::vector<double>> model_points;
  for (int i = 0; i < model.cols(); i++) {
    std::vector<double> point;
    point.push_back(model(0, i));
    point.push_back(model(1, i));
    model_points.push_back(point);
  }
  // read from data file 2
  std::string data2_file_name = "/home/jiuzhou/clipper_semantic_object/examples/data/robot1Map_forest_2d.txt";
  Eigen::Matrix2Xd data = read_2d_points(data2_file_name);
  // convert matrix to vector of vectors
  std::vector<std::vector<double>> data_points;
  for (int i = 0; i < data.cols(); i++) {
    std::vector<double> point;
    point.push_back(data(0, i));
    point.push_back(data(1, i));
    data_points.push_back(point);
  }


  /*
  Delaunay triangulation
  */
  DelaunayTriangulation::Observation observation_model(model_points);
  DelaunayTriangulation::Observation observation_data(data_points);

  std::vector<DelaunayTriangulation::Polygon> triangles_model = observation_model.triangles;
  std::cout << "Number of triangles in model: " << triangles_model.size() << std::endl;
  std::vector<DelaunayTriangulation::Polygon> triangles_data = observation_data.triangles;
  std::cout << "Number of triangles in data: " << triangles_data.size() << std::endl;

  /*
  Triangle Matching
  */
  // find matched triangles and points
  std::vector<double> diffs;
  std::vector<std::vector<double>> matched_points_model;
  std::vector<std::vector<double>> matched_points_data;
  double matching_threshold = 0.1;
  auto start = std::chrono::high_resolution_clock::now();
  match_triangles(triangles_model, triangles_data, diffs, matched_points_model, matched_points_data, matching_threshold);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
  std::cout << "Elapsed time for matching: " << elapsed_seconds.count() << "s" << std::endl;
  
  /*
  Clipper Data Association
  */
  // convert matched points to Eigen::Matrix2Xd
  Eigen::Matrix2Xd matched_points_model_matrix(2, matched_points_model.size());
  Eigen::Matrix2Xd matched_points_data_matrix(2, matched_points_data.size());
  for (int i = 0; i < matched_points_model.size(); i++) {
    matched_points_model_matrix(0, i) = matched_points_model[i][0];
    matched_points_model_matrix(1, i) = matched_points_model[i][1];
    matched_points_data_matrix(0, i) = matched_points_data[i][0];
    matched_points_data_matrix(1, i) = matched_points_data[i][1];
  }
  // create a clipper::Association object from the matched points
  int number_of_initial_matched_points = matched_points_model.size();
  std::cout << "Number of initial matched points: " << number_of_initial_matched_points << std::endl;
  clipper::Association A = clipper::Association(number_of_initial_matched_points, 2);
  for (int i = 0; i < matched_points_model.size(); i++) {
    A(i, 0) = i;
    A(i, 1) = i;
  }

  // instantiate the invariant function that will be used to score associations
  clipper::invariants::EuclideanDistance::Params iparams;
  iparams.sigma = 0.1;
  iparams.epsilon = 0.3;

  clipper::invariants::EuclideanDistancePtr invariant =
            std::make_shared<clipper::invariants::EuclideanDistance>(iparams);

  clipper::Params params;
  clipper::CLIPPER clipper(invariant, params);

  // an empty association set will be assumed to be all-to-all
  std::cout << "Scoring pairwise consistency" << std::endl;
  clipper.scorePairwiseConsistency(matched_points_model_matrix, matched_points_data_matrix, A);
  // find the densest clique of the previously constructed consistency graph
  std::cout << "Solving as maximum clique" << std::endl;
  clipper.solve();
  // check that the select clique was correct
  std::cout << "Getting Results" << std::endl;
  clipper::Association Ainliers = clipper.getSelectedAssociations();
  
  // get clipper matched points
  Eigen::Matrix2Xd clipper_matched_points_model(2, Ainliers.rows());
  Eigen::Matrix2Xd clipper_matched_points_data(2, Ainliers.rows());
  for (int i = 0; i < Ainliers.rows(); i++) {
    int idx_model = Ainliers(i, 0);
    int idx_data = Ainliers(i, 1);
    clipper_matched_points_model(0, i) = matched_points_model[idx_model][0];
    clipper_matched_points_model(1, i) = matched_points_model[idx_model][1];
    clipper_matched_points_data(0, i) = matched_points_data[idx_data][0];
    clipper_matched_points_data(1, i) = matched_points_data[idx_data][1];
  }

  // estimate transformation
  Eigen::Matrix3d tf = estimate_tf(clipper_matched_points_model, clipper_matched_points_data);
  std::cout << "Estimated transformation: " << std::endl;
  std::cout << tf << std::endl;
  return 0;
}