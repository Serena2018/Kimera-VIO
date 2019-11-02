/* -----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   Frame.h
 * @brief  Class describing a pair of stereo images
 * @author Antoni Rosinol, Luca Carlone
 */

#pragma once

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <opencv2/highgui/highgui_c.h>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>

#include <gtsam/geometry/Cal3_S2.h>
#include <gtsam/geometry/StereoPoint2.h>

#include "kimera-vio/frontend/Frame.h"
#include "kimera-vio/frontend/StereoFrame-definitions.h"
#include "kimera-vio/utils/UtilsGeometry.h"

namespace VIO {

class StereoFrame {
 public:
  StereoFrame(const FrameId& id,
              const Timestamp& timestamp,
              const cv::Mat& left_image,
              const CameraParams& cam_param_left,
              const cv::Mat& right_image,
              const CameraParams& cam_param_right,
              const gtsam::Pose3& L_Pose_R,
              const StereoMatchingParams& stereo_matching_params);

 public:
  struct LandmarkInfo {
    KeypointCV keypoint;
    double score;
    int age;
    gtsam::Vector3 keypoint_3d;
  };

  cv::Mat left_img_rectified_;
  cv::Mat right_img_rectified_;

  // TODO these guys are flying around the code, as they are publicly accessible
  // by anyone... To make this self-contained and thread-safe, there should be
  // getters for each one of these, but this has the caveat of making copies
  // everytime a getter is called. Better would be to add them to a
  // Output queue.
  KeypointsCV left_keypoints_rectified_;
  KeypointsCV right_keypoints_rectified_;
  std::vector<Kstatus> right_keypoints_status_;
  std::vector<double> keypoints_depth_;
  std::vector<Vector3>
      keypoints_3d_;  // in the ref frame of the UNRECTIFIED left frame

 public:
  /* ------------------------------------------------------------------------ */
  void setIsKeyframe(bool is_kf);

  /* ------------------------------------------------------------------------ */
  void setIsRectified(bool is_rectified);

  /* ------------------------------------------------------------------------ */
  // THIS IS NOT THREAD-SAFE
  // Create a 2D mesh only including triangles corresponding to obstables
  // (planar surfaces) min_elongation_ratio = 0.5 (max 1): check on the
  // elongation of the triangle (TODO: this check is conservative) if mesh2Dtype
  // = VALIDKEYPOINTS: 2D triangulation is computed form keypoints with VALID
  // right match and valid lmk id (!=-1) if mesh2Dtype = DENSE: to the keypoints
  // mentioned in the sparse case, we add other points without lmk id (but with
  // valid stereo) pointsWithIdStereo is optional, and represents the landmarks
  // corresponding to the keypoints used to create the 2d mesh.
  void createMesh2dStereo(std::vector<cv::Vec6f>* triangulation_2D,
                          std::vector<std::pair<LandmarkId, gtsam::Point3>>*
                              pointsWithIdStereo = nullptr,
                          const Mesh2Dtype& mesh2Dtype = VALIDKEYPOINTS,
                          const bool& useCanny = true) const;

  /* ------------------------------------------------------------------------ */
  // THIS IS NOT THREAD-SAFE
  void createMesh2dVIO(std::vector<cv::Vec6f>* triangulation_2D,
                       const std::unordered_map<LandmarkId, gtsam::Point3>&
                           pointsWithIdVIO) const;

  /* ------------------------------------------------------------------------ */
  // Removes triangles in the 2d mesh that have more than "max_keypoints_with_
  // gradient" keypoints with higher gradient than "gradient_bound".
  // Input the original triangulation: original_triangulation_2D
  // Output the filtered triangulation wo high-gradient triangles:
  // filtered_triangulation_2D.
  // gradient_bound = 50 (max 255): if pixels in triangle have at least
  // max_keypoints _with_gradient grad smaller than gradient_bound, triangle is
  // rejected
  void filterTrianglesWithGradients(
      const std::vector<cv::Vec6f>& original_triangulation_2D,
      std::vector<cv::Vec6f>* filtered_triangulation_2D,
      const float& gradient_bound = 50.0,
      const size_t& max_keypoints_with_gradient = 0) const;

  /* ------------------------------------------------------------------------ */
  // Copy rectification parameters from another stereo camera.
  void cloneRectificationParameters(const StereoFrame& sf);

  /* ------------------------------------------------------------------------ */
  // For each keypoint in the left frame, get
  // (i) keypoint in right frame,
  // (ii) depth,
  // (iii) corresponding 3D point.
  void sparseStereoMatching(const int verbosity = 0);

  /* ------------------------------------------------------------------------ */
  void checkStereoFrame() const;

  /* ------------------------------------------------------------------------ */
  LandmarkInfo getLandmarkInfo(const LandmarkId& i) const;

  /* ------------------------------------------------------------------------ */
  // Compute rectification parameters.
  static void computeRectificationParameters(
      const gtsam::Pose3& camL_Pose_camR,
      CameraParams* left_cam_params,  // left_frame_.cam_param_
      CameraParams* right_cam_params,
      gtsam::Pose3* B_Pose_camLrect,
      double* baseline);

  // TODO the functions below are just public for testing... fix that.
  /* ------------------------------------------------------------------------ */
  void undistortRectifyPoints(
      const KeypointsCV& left_keypoints_unrectified,
      const CameraParams& cam_param,
      const gtsam::Cal3_S2& rectCameraMatrix,
      StatusKeypointsCV* left_keypoints_rectified) const;

  /* ------------------------------------------------------------------------ */
  // TODO do not return containers by value.
  StatusKeypointsCV getRightKeypointsRectified(
      const cv::Mat left_rectified,
      const cv::Mat right_rectified,
      const StatusKeypointsCV& left_keypoints_rectified,
      const double& fx,
      const double& getBaseline) const;

  StatusKeypointsCV getRightKeypointsRectifiedRGBD(
      const cv::Mat left_rectified,
      const cv::Mat right_rectified,
      const StatusKeypointsCV& left_keypoints_rectified,
      const double& fx,
      const double& getBaseline,
      const double& getDepthMapFactor,
      const double& getMinDepth) const;

  /* ------------------------------------------------------------------------ */
  std::vector<double> getDepthFromRectifiedMatches(
      StatusKeypointsCV& left_keypoints_rectified,
      StatusKeypointsCV& right_keypoints_rectified,
      const double& fx,
      const double& getBaseline) const;

  /* ------------------------------------------------------------------------ */
  static std::pair<KeypointsCV, std::vector<Kstatus>> distortUnrectifyPoints(
      const StatusKeypointsCV& keypoints_rectified,
      const cv::Mat map_x,
      const cv::Mat map_y);

  /* ------------------------------------------------------------------------ */
  std::pair<StatusKeypointCV, double> findMatchingKeypointRectified(
      const cv::Mat left_rectified,
      const KeypointCV& left_rectified_i,
      const cv::Mat right_rectified,
      const int templ_cols,
      const int templ_rows,
      const int stripe_cols,
      const int stripe_rows,
      const double tol_corr,
      const bool debugStereoMatching = false) const;

 public:
  /// Getters
  // Thread-safe.
  inline FrameId getFrameId() const { return id_; }
  inline Timestamp getTimestamp() const { return timestamp_; }

  // NOT THREAD-SAFE, needs critical section.
  inline bool isRectified() const { return is_rectified_; }
  inline bool isKeyframe() const { return is_keyframe_; }
  inline gtsam::Pose3 getBPoseCamLRect() const {
    CHECK(is_rectified_);
    return B_Pose_camLrect_;
  }
  inline double getBaseline() const { return baseline_; }
  inline StereoMatchingParams getSparseStereoParams() const {
    return sparse_stereo_params_;
  }
  inline double getMinDepthFactor() const {
    return sparse_stereo_params_.min_depth_factor_;
  }
  inline double getMapDepthFactor() const {
    return sparse_stereo_params_.map_depth_factor_;
  }
  inline gtsam::Cal3_S2 getLeftUndistRectCamMat() const {
    return left_undistRectCameraMatrix_;
  }
  inline gtsam::Cal3_S2 getRightUndistRectCamMat() const {
    return right_undistRectCameraMatrix_;
  }

  // NON-THREAD SAFE.
  inline const Frame& getLeftFrame() const { return left_frame_; }
  inline const Frame& getRightFrame() const { return right_frame_; }
  // NON-THREAD SAFE, and potentially very hazardous, giving away rights to
  // modify class members is EVIL.
  inline Frame* getLeftFrameMutable() { return &left_frame_; }
  inline Frame* getRightFrameMutable() { return &right_frame_; }

 private:
  const FrameId id_;
  const Timestamp timestamp_;

  Frame left_frame_;
  Frame right_frame_;

  bool is_rectified_;  // make sure to do that on each captured image
  bool is_keyframe_;

  StereoMatchingParams sparse_stereo_params_;

  // RELATIVE POSE BEFORE RECTIFICATION
  //! Relative pose between left and right camera:
  const gtsam::Pose3 camL_Pose_camR_;

  // QUANTITIES AFTER RECTIFICATION
  // Note: rectification is something that belongs to a stereo camera,
  // and that's why these are stored here!
  gtsam::Cal3_S2 left_undistRectCameraMatrix_;
  gtsam::Cal3_S2 right_undistRectCameraMatrix_;
  //! Pose of the left camera wrt the body frame after rectification!
  gtsam::Pose3 B_Pose_camLrect_;
  double baseline_;

 private:
  /* ------------------------------------------------------------------------ */
  // Given an image img, computes its gradients in img_grads.
  void computeImgGradients(const cv::Mat& img, cv::Mat* img_grads) const;

  /* ------------------------------------------------------------------------ */
  // Use optical flow to get right frame correspondences.
  // deprecated
  void getRightKeypointsLKunrectified();

  /* ------------------------------------------------------------------------ */
  // Get disparity image:
  // https://github.com/opencv/opencv/blob/master/samples/cpp/tutorial_code/calib3d/stereoBM/SBM_Sample.cpp
  // TODO imshow has to be called in the main thread.
  cv::Mat getDisparityImage() const;

  /* ------------------------------------------------------------------------ */
  void print() const;

  /* ------------------------------------------------------------------------ */
  void showOriginal(const int verbosity) const;

  /* ------------------------------------------------------------------------ */
  // TODO visualization (aka imshow/waitKey) must be done in the main thread...
  void showRectified(const int verbosity) const;

  /* ------------------------------------------------------------------------ */
  // TODO visualization (aka imshow/waitKey) must be done in the main thread...
  void showImagesSideBySide(const cv::Mat imL,
                            const cv::Mat imR,
                            const std::string& title,
                            const int& verbosity = 0) const;

  /* ------------------------------------------------------------------------ */
  cv::Mat drawEpipolarLines(const cv::Mat img1,
                            const cv::Mat img2,
                            const int& numLines = 20,
                            const int& verbosity = 0) const;

  /* ------------------------------------------------------------------------ */
  // TODO visualization (aka imshow/waitKey) must be done in the main thread...
  void displayLeftRightMatches() const;

  /* ------------------------------------------------------------------------ */
  // Visualize statistics on the performance of the sparse stereo matching
  void displayKeypointStats(
      const StatusKeypointsCV& right_keypoints_rectified) const;
};

}  // namespace VIO
