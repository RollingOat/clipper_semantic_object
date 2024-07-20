#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <iostream>
#include <fstream>
#include <sstream>
#include <clipper/clipper.h>
#include <clipper/utils.h>

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

// function to do delaunay triangulation


// function to compute triangle difference


// function to find matched triangles and points


// function to run clipper


// function to estimate transformation



int main(){
// instantiate the invariant function that will be used to score associations
  clipper::invariants::EuclideanDistance::Params iparams;
  iparams.sigma = 0.1;
  iparams.epsilon = 0.5;

  clipper::invariants::EuclideanDistancePtr invariant =
            std::make_shared<clipper::invariants::EuclideanDistance>(iparams);

  clipper::Params params;
  clipper::CLIPPER clipper(invariant, params);

  //
  // Data setup
  //
  // read from data file 1
  std::string data1_file_name = "/home/jiuzhou/clipper_semantic_object/examples/data/robot0Map_forest_2d.txt";
  Eigen::Matrix2Xd model = read_2d_points(data1_file_name);
  // read from data file 2
  std::string data2_file_name = "/home/jiuzhou/clipper_semantic_object/examples/data/robot1Map_forest_2d.txt";
  Eigen::Matrix2Xd data = read_2d_points("/home/jiuzhou/clipper_semantic_object/examples/data/robot1Map_forest_2d.txt");
  // create a association set of data
  clipper::Association A = clipper::utils::createAllToAll(model.cols(), data.cols());
  // an empty association set will be assumed to be all-to-all
  std::cout << "Scoring pairwise consistency" << std::endl;
  clipper.scorePairwiseConsistency(model, data, A);
  // find the densest clique of the previously constructed consistency graph
  std::cout << "Solving as maximum clique" << std::endl;
  clipper.solve();
  // check that the select clique was correct
  std::cout << "Getting Results" << std::endl;
  clipper::Association Ainliers = clipper.getSelectedAssociations();
  // print out the inliers
  std::cout << Ainliers << std::endl;
  return 0;
}