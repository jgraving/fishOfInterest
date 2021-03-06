
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <iostream>
#include <netcdfcpp.h>

using namespace cv;
using namespace std;

int setUpNetCDF(NcFile* dataFile, int nFrames, int nFish);

int main( int argc, char** argv )
{
    // **************************************************************************************************
    // set up the parameters for blob detection
    // **************************************************************************************************
    SimpleBlobDetector::Params params;
    params.minDistBetweenBlobs = 5.0f;
    params.filterByInertia = false;
    params.filterByConvexity = false;
    params.filterByColor = false;
    params.filterByCircularity = false;
    params.filterByArea = true;
    params.minArea = 10.0f;
    params.maxArea = 500.0f;
    params.minThreshold = 215;
    params.thresholdStep = 15;
    params.maxThreshold = 250;

    // set up and create the detector using the parameters
    // Set up detector with params
    Ptr<SimpleBlobDetector> blob_detector = SimpleBlobDetector::create(params);

    vector<KeyPoint> keypoints;
    string dataDir = "/home/ctorney/data/fishPredation/";


    std::string trialName;
    if (argc > 1) 
        trialName =  argv[1];
    else
    {
        cout<<"trial name missing!"<<endl;
        return 0;
    }
            
 //   trialName =  "MVI_" + trialName;

    // **************************************************************************************************
    // create mask image
    // **************************************************************************************************
    string mask = dataDir + "masks/mask-" + trialName + ".png";
    Mat imMask = imread( mask, IMREAD_GRAYSCALE );

    // **************************************************************************************************
    // open the movie
    // **************************************************************************************************
    string movie = dataDir + "allVideos/" + trialName + ".MOV";
    VideoCapture cap(movie);

    if (!cap.isOpened())
    {
        cout << "Failed to open avi file: " << movie << endl;
        return -1;
    }

    int fCount = 7500;//cap.get(CAP_PROP_FRAME_COUNT );
    int fStart = 750;
    int nFrames = fCount - fStart;
    int nFish = 2*atoi(argv[2]) + 2;

    int fishCounts[nFish+1];
    for (int nf=0;nf<nFish+1;nf++)
        fishCounts[nf]=0;

    // **************************************************************************************************
    // create & save background image
    // **************************************************************************************************
    string strImBk = dataDir + "backGrounds/bk-" + trialName + ".png";
    Mat imBk = imread( strImBk);

    
    Mat frame, fgMaskMOG2;
    if (imBk.empty())
    {
        Ptr<BackgroundSubtractor> pMOG2 = createBackgroundSubtractorMOG2(fCount, 16, true);

        for(int f=0;f<fCount;f++)
        {
            if (!cap.grab())             
                break;
            if ((f<fStart)||(f%25!=0))
                continue;
            cap.retrieve(frame);             
            pMOG2->apply(frame, fgMaskMOG2, 0.025);

        }
        pMOG2->getBackgroundImage(imBk);

        imwrite( strImBk, imBk );
    }
    cvtColor( imBk, imBk, COLOR_BGR2GRAY );

    // **************************************************************************************************
    // create the netcdf file
    // **************************************************************************************************
    string ncFileName = dataDir + "tracked/" + trialName + ".nc";
    NcFile dataFile(ncFileName.c_str(), NcFile::Replace);

    if (!dataFile.is_valid())
    {
        cout << "Couldn't open netcdf file!\n";
        return -1;
    }

    setUpNetCDF(&dataFile, nFrames, nFish );

    // get the variable to store the positions
    NcVar* pxy = dataFile.get_var("pxy");
    NcVar* frNum = dataFile.get_var("frNum");

    // **************************************************************************************************
    // loop over all frames and record positions
    // **************************************************************************************************

    bool visuals = 1;
    if (visuals)
        namedWindow("Positions", 1);
    Mat gsFrame;
    cap.set(CAP_PROP_POS_FRAMES,0);
    for(int f=0;f<fCount;f++)
    {
        if (!cap.read(frame))             
            break;
        if (f<fStart-1)
            continue;

        // convert to grayscale
        cvtColor( frame, gsFrame, COLOR_BGR2GRAY );
        // subtract background
        absdiff(imBk, gsFrame, gsFrame);
        // select region of interest using mask
        bitwise_and(gsFrame, imMask, gsFrame);
        threshold(gsFrame, gsFrame, 20, 255, THRESH_BINARY );
        bitwise_not(gsFrame, gsFrame);
        if (f<fStart)
            continue;
  //      bitwise_or(gsFrame, lastFrame, gsFrame);
        // find the blobs
        blob_detector->detect(gsFrame, keypoints);
        // create array for output
        float dataOut[nFish][2];
        for (int i=0;i<nFish;i++)
            dataOut[i][0]=dataOut[i][1]=-1.0f;

        // extract the x y coordinates of the keypoints
        // use only the first 4 as keypoints are sorted according to quality 
        int foundPoints = keypoints.size();
        if (foundPoints>nFish)
            foundPoints = nFish;
        for (int i=0; i<foundPoints; i++)
        {
            dataOut[i][0] = keypoints[i].pt.x; 
            dataOut[i][1] = keypoints[i].pt.y;
            if (visuals)
                circle( gsFrame,keypoints[i].pt, 8, Scalar( 25, 125, 125 ), -1, 8);
        }
        fishCounts[foundPoints]++;

        pxy->set_cur(f - fStart);
        frNum->set_cur(f - fStart);
        pxy->put(&dataOut[0][0], 1, nFish, 2);
        frNum->put(&f, 1);


        if (visuals)
        {
            pyrDown(gsFrame, gsFrame) ;
            imshow("Positions", gsFrame);


            char key = waitKey(10);
            if (key == 27) // ESC
                break;
        }
    }
    cout<<trialName<<" : ";
    for (int nf=0;nf<nFish+1;nf++)
        cout<<fishCounts[nf]<<" : ";
    cout<<endl;

    return 0;
}

int setUpNetCDF(NcFile* dataFile, int nFrames, int nFish)
{
    // dimension for each frame
    NcDim* frDim = dataFile->add_dim("frame", nFrames);
    // dimension for individual fish
    NcDim* iDim = dataFile->add_dim("fish", nFish);
    // xy dimension for vectors
    NcDim* xyDim = dataFile->add_dim("xy", 2);

    // define a netCDF variable for the positions of individuals
    dataFile->add_var("pxy", ncFloat, frDim, iDim, xyDim);
    // define a netCDF variable for the frame number to account 
    // for any offset needed to remove experimental set-up
    dataFile->add_var("frNum", ncInt, frDim);

    return 0;

}



