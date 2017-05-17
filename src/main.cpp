#include <iostream>
#include <stdlib.h>     /* malloc, calloc, realloc, free */

#include <opencv2/core/core.hpp>        // Basic OpenCV structures (cv::Mat)
#include <opencv2/videoio/videoio.hpp>  // Video write
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "supportfunc.hpp"
#include <fstream>
//#include <neural-cam-ros/Object.h>
//#include <neural-cam-ros/ObjectStack.h>
//#include <thread>

#include <ctime>
#include "ros/ros.h"
#include "std_msgs/String.h"
#include <sstream>
#include <geometry_msgs/Point32.h>
#include <tf/transform_listener.h>
#include <laser_geometry/laser_geometry.h>

using namespace std;
using namespace cv;

extern "C" {
#include "detector.h"
}

using namespace std;
using namespace cv;

extern "C" {
#include "detector.h"
}

// ********************************************************
// ********* support functions ****************************
// ********************************************************

VideoCapture cap_un(0);
Mat img_cpp;

// temp storage for detected objects
std::vector<detectedBox> detectedobjects;

// convert IplImage to Mat   (NOT IN USE)
void convert_frame(IplImage* input){
    img_cpp = cv::cvarrToMat(input);
}

// draw boxes
extern "C" void label_func(int tl_x, int tl_y, int br_x, int br_y, char *names){

   string str(names);
   Scalar color;
   bool keep = false;

   if(str == "person"){  //index 01
     color = Scalar(255, 0, 0);  //coral color
     keep = true;
   }else if (str == "bike"){ //index 02
     color = Scalar(0, 0, 255);     //orange color
     keep = true;
   }else if (str == "vehicle"){ //index 03
     color = Scalar(0,255,0);      //gold color
     keep = true;
   }else{
     color = Scalar(0,0,0);          //black
   }


   if(keep){
     detectedBox tempstorage;

     if(tl_x < 0)
        tl_x = 0;
     if(tl_y < 0)
        tl_y = 0;

     if(br_x > img_cpp.cols)
        br_x = img_cpp.cols;
     if(br_y > img_cpp.rows)
        br_y = img_cpp.rows;

     tempstorage.topLeft = Point(tl_x,tl_y);
     tempstorage.bottomRight = Point(br_x,br_y);
     tempstorage.name = str;
     tempstorage.objectColor = color;

     detectedobjects.push_back(tempstorage);  //rmb to destory this
   }

}

vector<detectedBox> display_frame_cv(bool display){

    vector<detectedBox> pass_objects(detectedobjects);

    if(display){
    for(int j = 0; j < detectedobjects.size(); j++){
          Point namePos(detectedobjects[j].topLeft.x,detectedobjects[j].topLeft.y-10);  //position of name
            rectangle(img_cpp, detectedobjects[j].topLeft, detectedobjects[j].bottomRight, detectedobjects[j].objectColor, 2, CV_AA);                  //draw bounding box
            putText(img_cpp, detectedobjects[j].name, namePos, FONT_HERSHEY_PLAIN, 2.0, detectedobjects[j].objectColor, 1.5);                          //write the name of the object
        }

        imshow("detected results", img_cpp); //display as external window
    }

    detectedobjects.clear();  //clear vector for next cycle

    return pass_objects;
}

// capture from camera stream
extern "C" image load_stream_cv()
{
    cap_un >> img_cpp;

    if (img_cpp.empty()){
       cout << "Warning: frame is empty! Check camera setup" << endl;
       return make_empty_image(0,0,0);
    }

    image im = mat_to_image(img_cpp);
    rgbgr_image(im);
    return im;
}


// initialization of network
bool init_network_param(bool train){

     char *datacfg;
     char *cfg;
     char *weights;
     float thresh_desired;
     
     //default
     string datafile = "cfg/voc.data";
     string archfile = "cfg/tiny-yolo-voc.cfg";
     string weightfile = "tiny-yolo-voc.weights";

     ifstream confFile("setup.cfg");

     string line;
     int cnt = 0;
     if(confFile.is_open()){
       while( std::getline(confFile, line) ){
           istringstream is_line(line);
           string key;

           if(getline(is_line, key, '=')){
               string value;
               if(getline(is_line, value)){
                  if(cnt == 0){
                    datacfg = new char[value.length() + 1];
                    strcpy(datacfg, value.c_str());
                  }
                  else if(cnt == 1){
                    cfg = new char[value.length() + 1];
                    strcpy(cfg, value.c_str());
                  }
                  else if(cnt == 2){

                    if(value.length() == 0)
                       weights = 0;
        else{
                       weights = new char[value.length() + 1];
                       strcpy(weights, value.c_str());
        }

                  }
                  else if(cnt == 3){
                    thresh_desired = strtof((value).c_str(),0); // string to float
                  }
                  cnt++;
               }
           }
       }
     }else{

         datacfg = new char[datafile.length() + 1];
         strcpy(datacfg, datafile.c_str());

         cfg = new char[archfile.length() + 1];
         strcpy(cfg, archfile.c_str());
  
         weights = new char[weightfile.length() + 1];
         strcpy(weights, weightfile.c_str());

         thresh_desired = 0.35;

         cout << "Error: Unable to open setup.cfg, make sure it exists in the parent directory" << endl;
         
         return 0;
     }

     //initialize c api
     if(train)
  setup_detector_training(datacfg, cfg, weights);
     else{
        if(!weights){
     cout << "Error: No weights file specified in setup.cfg! Abort" <<endl;
        }else    
           setup_proceedure(datacfg, cfg, weights, thresh_desired);
     }

     delete [] datacfg;
     delete [] cfg;
     delete [] weights;

     return 1;
}

// initialize camera setup
bool init_camera_param(int cam_id){

      cap_un.open(cam_id);
      if(!cap_un.isOpened()){
         cout << "camera stream failed to open!" <<endl;
   return false;
      }else
         return true;
}

// run this in a loop
void process_camera_frame(bool display){
     camera_detector();      //draw frame from img_cpp;
     display_frame_cv(display);
}

//---------------------------->
//<---------------------- main ---------------------------->
//---------------------------->

int main(int argc, char* argv[]){

  ros::init(argc, argv, "talker");
  ros::NodeHandle n;

   if(argc < 2){
      cerr <<"Usage detail: "<< argv[0] << " OPTION"<< endl;
      return 0;
   }

   if(strcmp(argv[1], "train") == 0){        //training

      init_network_param(1);        
      execute_detector_training();

   }else if (strcmp(argv[1], "eval") == 0){  //evaluation using camera

      if(!init_camera_param(0))
          return -1;

       init_network_param(0);       //initialize the CNN parameters from cfg files

       for(;;){  //process and show everyframe
          process_camera_frame(true);
          if(waitKey (1) >= 0)  //break upon anykey
             break;
       }

   }else{
      cerr << "Invalid options, either train or eval" << endl;
      return 0;
   }

   return 0;
}
