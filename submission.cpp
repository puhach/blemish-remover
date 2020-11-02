
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
	// TODO: define copy and move constructors along with assignment and move-assignment operators


	void process(const char* inputFile);

	
private:

	static constexpr const char windowName[] = "Blemish Removal";

	static void onMouse(int event, int x, int y, int flags, void *data);

	//string inputFile;
	Mat imSrc, imPrev, imCur;
};	// BlemishRemover


BlemishRemover::BlemishRemover()
{
	namedWindow(BlemishRemover::windowName);

	setMouseCallback(BlemishRemover::windowName, onMouse, this);
}	// ctor

void BlemishRemover::process(const char *inputFilePath)
{
	this->imSrc = imread(inputFilePath, IMREAD_COLOR);
	CV_Assert(!this->imSrc.empty());
	
	//this->imPrev = this->imSrc.clone();
	this->imSrc.copyTo(this->imPrev);
	this->imSrc.copyTo(this->imCur);
		
	int key;
	do
	{
		imshow(BlemishRemover::windowName, imCur);
		key = waitKey(10);		
	} while (true);
}	// process


void BlemishRemover::onMouse(int event, int x, int y, int flags, void* data)
{
	BlemishRemover* br = static_cast<BlemishRemover*>(data);

	switch (event)
	{
	case EVENT_LBUTTONUP:
		circle(br->imCur, Point(x, y), 5, Scalar(0,255,0), -1);
		break;
	}
}	// onMouse

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