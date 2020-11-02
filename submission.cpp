
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>

using namespace std;
using namespace cv;

int main(int argc, char* argv[])
{
	Mat imSrc = imread("blemish.png", IMREAD_COLOR);
	CV_Assert(!imSrc.empty());

	imshow("Blemish Removal", imSrc);
	waitKey();

	return 0;
}