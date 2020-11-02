
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>

using namespace std;
using namespace cv;


class BlemishRemover
{
public:
	BlemishRemover();

	void process(const char* inputFile);

private:
	//string inputFile;
};	// BlemishRemover


BlemishRemover::BlemishRemover()
{

}	// ctor

void BlemishRemover::process(const char *inputFilePath)
{
	Mat imSrc = imread(inputFilePath, IMREAD_COLOR);
	CV_Assert(!imSrc.empty());

	imshow("Blemish Removal", imSrc);
	waitKey();
}	// process

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		cout << "Usage: reblemish <input image>" << endl;
		return -1;
	}

	try
	{
		BlemishRemover blemishRemover;
		blemishRemover.process(argv[1]);
	}
	catch (const std::exception& e)		// cv::Exception inherits std::exception
	{
		cerr << e.what() << endl;
		return -2;
	}
		

	return 0;
}